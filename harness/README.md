# Native capture harness

What a fresh container needs to build, run and photograph this branch on a
headless Linux box without root. The scripts assume the layout below; change
`A` at the top of each script if you move it.

    $A/                        e.g. /home/agent/agtk
      repo360/                 this checkout, branch wide-view-360
      nativeprefix/            unpacked Ubuntu packages: SDL 1.2, Mesa, Xvfb, autotools
      debs/                    downloaded .deb files, ~130 MB, not committed
      etx11/                   /etc/X11 content bind mounted into the namespace
      cfg360/                  game user dir: HOME, userdatadir, screenshot/
      run360/                  run dir: var/autoexec.cfg, logs
    /workspace/armagetronad-linux64
                               upstream 0.2.9.3 alpha data bundle (language/,
                               themes/, ...), used as --datadir/--configdir
      etx11/                   /etc/X11 content bind mounted into the namespace
                               (its two files are kept in harness/x11/)

## 1. Build the native prefix

    python3 harness/fetchpkgs.py              # index noble+jammy Packages.xz, fetch, dpkg -x
    python3 harness/getpkg.py <package> ...   # add single packages afterwards

SDL 1.2 only exists up to jammy (22.04); the rest is taken from noble. This
gives `libsdl1.2-dev`, SDL_image/mixer, libpng/jpeg/tiff, Mesa + llvmpipe,
Xvfb, xdotool, autotools and m4.

## 2. X server with GLX

Xvfb has to run inside a private user+mount namespace so the prefix can shadow
`/usr/bin`, `/usr/share` and `/usr/lib`:

    /usr/bin/unshare -rm --propagation private python3 $A/repo360/harness/xvfb.py

It is up when the log reports a bound socket or AIGLX. Confirm GLX:

    LD_LIBRARY_PATH=$A/nativeprefix/usr/lib/x86_64-linux-gnu DISPLAY=:97 \
      $A/nativeprefix/usr/bin/xdotool getdisplaygeometry      # 1280 800

## 3. Build the game

    sh harness/build.sh                       # bootstrap, configure, full make
    sh harness/make.sh armagetronad_main      # incremental

`make.sh` exists because `configure` records `-I/usr/include/SDL` from the
prefix's own `sdl-config`, and that path does not exist on the host: the SDL
1.2 and GL headers live under `nativeprefix/usr/include`. Passing the include
paths explicitly is what makes a one-file rebuild work.

## 4. Lens map check

    sh harness/check.sh

One line per lens/globe/fov/aspect, then `EVERY LENS AGREES WITH THE
WORLD-SPACE MODEL`. The `LD_LIBRARY_PATH` in `check.sh` is required: the test
binary links the prefix copies of `libSDL_image` and `libpulsecommon`, and
without them it dies in the loader and `make check` reports a false FAIL.

Reading the output: `0 lens` counts pixels outside the projection's own
domain (blinky leaves those to the background), `0 hole` counts pixels inside
the domain but outside every plate frustum. Holes mean the plates stopped
tiling the sphere.

## 5. Run and photograph

    cd $A/run360
    cp $A/repo360/harness/autoexec.cfg var/autoexec.cfg
    /usr/bin/unshare -rm --propagation private python3 launch.py
    python3 drive.py wait 6000 key Return wait 1000 key Down wait 600 key Return wait 6000 shot

Screenshots land in `$A/cfg360/screenshot/screenshot_N.png`. `capture.sh <tag>`
and `lensrun.sh <lens> [globe] [tag]` file them into `/workspace/shots` as
`<tag>-N.png`. `lensrun.sh` restarts the game cleanly with one lens, starts a
local game and takes two in-game shots; run it per lens for a comparison set.

Driving notes, each learned from a wrong run:

- The main menu cursor is remembered in `cfg360/user.cfg`. Delete
  `cfg360/user.cfg` and `cfg360/var/user.cfg` for a known start: cursor on
  Play Game, Return opens the submenu, Down selects Local Game, Return starts
  the round.
- Escape on the main menu selects Exit Game, and the process exits 0 with no
  message. Menu lists wrap, so never press Up or Escape blind.
- Keys only reach the game once xdotool has focus; `drive.py` retries
  `windowfocus` and checks `getwindowfocus`. A window id stays valid in
  `search` output after the game has exited, which shows up as BadWindow from
  the focus query, not from the target window.
- The game needs 8-10 s after start before the menu takes keys; driving
  earlier produces menu or black frames (a good shot is 300-700 KB, a menu
  frame ~80-105 KB).
- Kill the client with an anchored pattern so the matching shell does not kill
  itself: `pkill -f "[a]rmagetronad_main"`.

Frame-identical comparisons between two builds: record with
`--record <file>` and replay with `--playback <file>`, each as a *separate*
argv token, with `MAX_FPS 60` in autoexec so the recording stays short. The
sim is deterministic; the same `gt` line comes back byte for byte.

## 6. Data directory

`launch.py` uses `/workspace/armagetronad-linux64` (the upstream alpha Linux
bundle) for `--datadir`/`--configdir`. Language strings are read from there,
not from the checkout, so after changing `language/english_base.txt`:

    cp $A/repo360/language/english_base.txt /workspace/armagetronad-linux64/language/
