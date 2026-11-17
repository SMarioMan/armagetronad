# Rebuilding the environment that produced the measurements

The repository carries every source change, patch, replay and measurement tool.
What used to exist only in a scratch directory is written down here: what has to
be built, in which order, with which exact flags, and which traps cost hours
the first time. Nothing below needs root.

## 0. The reference host

WSL2 Linux x86_64, 3 GB RAM, 8 cores, uid 1001, **no root**, `/usr/bin` not
writable, no `/dev/dri`. So there is no GPU anywhere in the loop: every number
in `webgl/notes/` comes from **llvmpipe** on the desktop side and **SwiftShader**
inside Chrome on the port side, with the context attributes deliberately matched
(no MSAA, anisotropy 1). A difference that only appears on real hardware will
not show up in these sheets - that is what `webgl/site/lodtest.html` is for.

Git pushes go over SSH (`git config url.git@github.com:.insteadOf
https://github.com/` plus `GIT_SSH_COMMAND` pointing at a key); the HTTPS remote
has no credentials here.

## 1. The two builds

| build | what it is | needs |
|---|---|---|
| wasm | `sh webgl/build.sh` -> `webgl/dist/` | emsdk, a wasm libxml2 |
| desktop | plain `./configure && make` against a private prefix | the prefix of §1.4 |

### 1.1 emscripten

```
git clone https://github.com/emscripten-core/emsdk
cd emsdk && ./emsdk install tot && ./emsdk use tot && ./emsdk env
```

The reference machine used an emsdk checkout from 2026-11-12 (`708dcbc`,
clang 24.0.0git). `webgl/build.sh` takes `EMSDK_DIR` (default `/opt/emsdk`).

Do **not** add `-s ASSERTIONS=2` or `-g3` to the wasm link: they change the
link output that `webgl/patch_gl_emulation.py` rewrites and the patch then
fails to match (§1.3).

### 1.2 wasm libxml2 (a link dependency, not a feature)

```
tar xf libxml2-2.9.14.tar.xz && cd libxml2-2.9.14
emconfigure ./configure --prefix=$LIBXML2_PREFIX \
    --without-python --without-zlib --without-lzma --without-threads \
    --enable-static --disable-shared --with-http --without-ftp --without-iconv \
    CFLAGS=-O2 CXXFLAGS=-O2
make -j"$(nproc)" && make install
```

`webgl/build.sh` takes `LIBXML2_PREFIX` (default `/opt/libxml2-wasm`) and passes
`XML2_CFLAGS=-I$LIBXML2_PREFIX/include/libxml2` itself.

### 1.3 wasm build

```
EMSDK_DIR=$A/emsdk LIBXML2_PREFIX=$A/libxml2-wasm sh webgl/build.sh
```

`build.sh` configures with a cache file (`webgl/emscripten.cache`), links with
`-s USE_SDL=1 -s STB_IMAGE=1 -s USE_ZLIB=1 -s USE_LIBPNG=1 -s FILESYSTEM=1
-s LEGACY_GL_EMULATION=1 -s GL_UNSAFE_OPTS=0 -s ALLOW_MEMORY_GROWTH=1`, and then
applies the three sanctioned post-link patches - `patch_sdl_convert.py`,
`patch_gl_emulation.py`, `patch_light_colormaterial.py`, `patch_line_eyeclip.py`,
`patch_texture_projective_coords.py`. **It wipes `webgl/dist/` on every run**, so
anything you put there (the `glhook.js` injection of §2.5, a replay you want the
page to be able to open) has to be put back afterwards:

```
sh webgl/harness/inject_hook.sh webgl/harness/perp2.aarec
```

Serve it with `PORT=8080 python3 webgl/serve.py` (the `.wasm` MIME type matters).

### 1.4 The desktop prefix (`nativeprefix`)

The host's Ubuntu mesa 24 removed the legacy `drisw` entry points, so a stock
Xvfb there advertises GLX but hands out no visuals. The Debian **bullseye**
X11/xorg/mesa closure (mesa 20.3.5) still does, and an unprivileged user can
only get it by unpacking .debs into a private prefix:

```
wget -O Packages-bullseye.gz http://deb.debian.org/debian/dists/bullseye/main/binary-amd64/Packages.gz
python3 webgl/harness/fetchpkgs.py Packages-bullseye.gz xvfb xserver-common \
    libgl1-mesa-dri libgl1-mesa-dev xkb-data x11-utils
for f in debs/*.deb; do dpkg -x "$f" nativeprefix; done
```

`fetchpkgs.py` resolves the dependency closure (skipping the base-system
packages listed in its `SKIP` set) and downloads into `./debs`.
`webgl/harness/debclosure.txt` is the exact 222-package list the reference
prefix ended up with - the seeds above are the important ones, the rest is
dependency closure; both the noble (24.0.5) and bullseye (20.3.5) mesa appear in
it, and the **prefix must keep the bullseye one on its library path** (§1.5).

SDL 1.2 and SDL_image are built into the same prefix - the game needs the SDL 1.2
headers and libs, and the closure provides the X side they link against:

```
(cd SDL-1.2.15 && ../sdlbuild/../SDL-1.2.15/configure \
    --prefix=$A/nativeprefix/usr --enable-shared --disable-static \
    --disable-alsa --disable-arts --disable-esd --disable-esdshared \
    --disable-nas --disable-pulse --disable-alsamixer \
    --x-includes=$A/nativeprefix/usr/include \
    --x-libraries=$A/nativeprefix/usr/lib/x86_64-linux-gnu && make -j8 install)
(cd SDL_image-1.2.12 && ./configure --prefix=$A/nativeprefix/usr \
    --enable-shared --disable-static --enable-image-png --enable-image-jpg \
    --enable-loading-yes --enable-lists-yes && make -j8 install)
```

(Those two configure lines are the ones actually used, taken from the
`config.log` of the working trees; both trees were built out-of-tree in
`$A/sdlbuild` and `$A/sdlimage`.)

Then the game itself, in a second checkout so the working tree stays clean:

```
git clone <this repo> nativesrc && cd nativesrc
PKG_CONFIG_PATH=$A/nativeprefix/usr/lib/x86_64-linux-gnu/pkgconfig:$A/nativeprefix/usr/lib/pkgconfig \
CPPFLAGS=-I$A/nativeprefix/usr/include \
LDFLAGS="-L$A/nativeprefix/usr/lib/x86_64-linux-gnu -L$A/nativeprefix/usr/lib" \
./configure --prefix=$A/nativeprefix/usr/local --disable-curl \
    --disable-sysinstall --disable-etc --disable-desktop --disable-initscripts
make -j8
```

### 1.5 An X server that answers GLX

```
sh webgl/harness/native_x.sh :97 $A/nativeprefix        # run as a background service
ls /tmp/.X11-unix/X97                                   # up when the socket exists
```

`native_x.sh` runs `Xvfb +extension GLX +iglx` inside a user+mount namespace and
bind-mounts the prefix over `/usr/lib`, `/usr/bin`, `/usr/share`, `/etc/X11` -
which is what `nsbind.sh` arranges (Xvfb needs `xkbcomp` and the
`xkeyboard-config` data, and `/usr/bin` has to be shadowed *and* still provide
`mount`, which is why `nsbind.sh` copies the mount tool aside first). Verified
answer from `webgl/harness/glxprobe.c` on this setup:
`GLX present=1 version 1.4 FBConfigs=840`.

Stop it with `pkill -f "[X]vfb :97"`.

## 2. Taking a matched pair of screenshots

### 2.1 The capture stamp (never shipped)

```
git apply webgl/harness/parity-stamp.patch      # in BOTH builds
```

It prints, every 11th frame, `PF FRAME <n> state 6 gt <game time> ncam <c> pos
<x> <y> h <heading> dir <dx> <dy>` - to stderr on the desktop, to the browser
console (and thus into the CDP log) on the port - and it defines the
`AA_SHOT_GT` setting: a comma-separated list of game times that turns into a
screenshot taken inside the same `SwapGL` as the stamped frame, so pixels and
stamp describe one frame. `webgl/harness/parity-stamp.patch` is harness-only:
**do not commit it**, and the published artifact therefore differs from a local
capture build by exactly that probe.

### 2.2 Desktop capture

```
cd $A/nativesrc && NBIN=$A/nativesrc/src/armagetronad_main \
  NUSERDIR=$A/nativerun/user NVARDIR=$A/nativerun/var \
  NPREFIX=$A/nativeprefix NDISP=:97 \
  bash webgl/harness/natpb.sh OUTPREFIX webgl/harness/perp2.aarec 2.4637,4.3519,5.5099
```

`$NUSERDIR` and `$NVARDIR` must contain `webgl/harness/autoexec.cfg` - it pins
`ARMAGETRON_WINDOWSIZE 8` (1280x800), `FLOOR_DETAIL 3`, `TEXTURES_HI 1`,
`ALPHA_BLEND 1`, `TEXT_OUT 0`, `SP_NUM_AIS 10`, `SP_WALLS_STAY_UP_DELAY 600`,
`SP_WALLS_LENGTH 1000`, `SP_AI_IQ 30`, and **`MAX_FPS 60`**. The fps cap is not
cosmetic: the port replays one frame per CDP step, so an uncapped recording
(~29 k frames for the same wall-clock run) is unaffordable there; capped, the
same replay is ~1.4 k frames.

Output: `OUTPREFIX-NN-gt<g>.png` plus `OUTPREFIX-shots.txt`, one line per frame:
`<png> 0 0 <x> <y> <w> <h> n<frame> gt <g> pos <x> <y> h <h> dir <dx> <dy>`.

To make a *new* recording, drive the menus with `webgl/harness/natkey.c` (an
XTEST key sender - there is no `xdotool` here and a window-manager-less Xvfb
will not route XTEST to the client unless events go to the window) and start the
client with `--record <file>` as a **separate** argv token.

### 2.3 Browser capture

```
/usr/bin/google-chrome --headless=new --remote-debugging-port=9223 \
    --user-data-dir=$A/chrome --no-sandbox --disable-dev-shm-usage \
    --enable-unsafe-swiftshader --window-size=1280,800 about:blank &
PORT=8080 python3 webgl/serve.py &
CDPPORT=9223 SHOTDIR=$A/shots python3 webgl/harness/cdpd.py \
    http://127.0.0.1:8080/ TAG <steps> [BOOT]
```

That Chrome line is what produced the verified CDP endpoint (Chrome 157).
`cdpd.py`'s step language: `q e d u l r b t g` = keys, `w:MS` = wait, `s` =
screenshot, `G:<game time>` / `G:<x>,<y>` = freeze the wasm main loop
(`Module._pause`/`_step`) and advance one frame at a time until the stamped pose
or game time reaches the target, then shoot. `--playback <file>` can only open a
replay that is already inside `webgl/dist/` (see `inject_hook.sh`).

Boot traps: the first-run "First Setup" wizard is answered once and persists in
IndexedDB (IDBFS), so a fresh profile costs `M:5,e:2000` before anything else;
the canvas can be slow to appear, so `BOOT` is a timeout, not a sleep; a
cancelled capture leaves the page tab alive and burning SwiftShader CPU (it will
wedge CDP on a loaded box) - close page targets through
`curl http://127.0.0.1:9223/json/list` + `/json/close/<id>`.

### 2.4 The sheet tools

```
cd <somewhere writable>            # SHOTS defaults to the cwd
SHOTS=$PWD python3 $REPO/webgl/harness/sheets/mkpair49.py PORTMANIFEST GATECOUNT
SHOTS=$PWD python3 $REPO/webgl/harness/sheets/sheet49.py   NATMANIFEST PORTMANIFEST
SHOTS=$PWD python3 $REPO/webgl/harness/sheets/mkreadme49.py NATMANIFEST PORTMANIFEST
```

They emit `NN-pairN.png`, `NN-all.png` and the matching `README-NN.md`, taking
the numbers straight from `webgl/harness/parity_report.py` and
`webgl/harness/gridprofile.py`. `webgl/notes/README.md` explains what the notes
assume and how to re-measure them.

### 2.5 GL state inside the emulation

`webgl/harness/glhook.js` wraps `bindTexture`/`texImage2D`/`texParameteri`/
`generateMipmap` and prints, per texture object, the uploaded mip chain, the
filters, the applied anisotropy and the shader sources the emulation compiles.
`inject_hook.sh` puts it into `webgl/dist` (it must be re-injected after every
build). It answered the question the C++ side cannot: whether the floor texture
really has mip levels and `GL_LINEAR_MIPMAP_LINEAR` at draw time.

## 3. Regenerating the generated tables

```
python3 webgl/gen_sdl12_keymap.py \
    $A/nativeprefix/include/SDL/SDL_keysym.h \
    $A/emsdk/upstream/emscripten/system/include/SDL/SDL_keycode.h \
    $A/emsdk/upstream/emscripten/system/include/SDL/SDL_scancode.h
```

Emits the `{file, runtime}` key-sym pairs for `src/render/rSDL12KeySyms.h`
(the reference run: 233 SDL 1.2 keysyms, 239 scancodes, 56 differing, 177
identical, 110 SDL2-only). `gen_sdl12_keynames.py` does the same for the key
labels in `src/ui/uSDL12KeyNames.h`. Replays store SDL 1.2 numbers while the
runtime SDL is SDL 2 - that translation is why the arrow keys in a recorded
replay move the menu at all.

## 4. Deploy

`git push origin webgl` rebuilds and republishes `https://smarioman.github.io/`.
Check what is live:

```
curl -sSI https://smarioman.github.io/armagetronad/armagetronad_main.wasm | grep -iE "last-modified|content-length"
curl -sS  https://smarioman.github.io/armagetronad/index.html | wc -c
```

As of 2026-11-17 the live artifact is 2207882 bytes, last-modified 17 Nov 2026
20:15:55 GMT. The page HTML is served `cache-control: max-age=600`, so a
comparison against a fresh push needs a hard reload.

## 5. Traps, in the order they hurt

1. **Replay keys are SDL 1.2 numbers**, the runtime is SDL 2: unreplayed menus
   work, arrow keys in a replay do nothing (§3).
2. **Two playbacks of one replay diverge.** With identical inputs the stamped
   camera pose agrees to 0.01 world units at gt 2.5 and 4.4, 0.18 at gt 5.5, and
   is 7.7 units apart by gt 7.8 - the AI and the spark particles are simulated
   per frame. Pairs beyond ~gt 5.5 are not the same arena, and nothing
   particle-based lines up at all.
3. **Floor metrics are camera-sensitive.** The desktop build alone, gated 0.05 s
   apart while moving ~19 units/s, reports band-0 line widths between 1.33 and
   3.31 px. Only *line positions* (or pairs matched to ~0.01 units) are strong
   evidence; widths and "number of lines on a scanline" are not. The `lines`
   column also counts sparks and debris streaks.
4. `webgl/build.sh` **wipes `webgl/dist/`**; re-inject hooks, re-copy replays.
5. `-s ASSERTIONS` / `-g3` break the GL-emulation patch's anchors.
6. `--record` / `--playback` take the filename as a **separate** argv token.
7. On a loaded machine Xvfb's accept can stall: start the server as a long-lived
   service, not per run, and keep the capture machine's load down (a stray tab
   at 9 CPU brings CDP and X both to their knees).
8. Kill patterns need care: `pkill -f "^\./src/armagetronad_main"` (unanchored
   matches the shell that is running the pkill), `pkill -f "[X]vfb :97"`.
9. Screenshots of a GLX window via `xwd` come back black; capture the **root**
   window (Xvfb composites the front buffer there) and crop, which is what
   `natpb.sh`/`parity_report.py` do.
10. Interactive shells here run with errexit: a `grep -c` that returns 0 matches
    aborts a script silently.
