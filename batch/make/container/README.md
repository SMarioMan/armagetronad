# Native build without root

Everything in this directory builds and tests the native client inside a
container that has no root and no writable `/usr`: the dependency prefix is
unpacked from Ubuntu archives with `dpkg -x`, and the tools that insist on
system paths are made to see that prefix through a private user plus mount
namespace.  Nothing here is installed; it is only distributed with the sources.

| file | what it does |
| --- | --- |
| `fetchpkgs.py` | indexes the noble and jammy package lists, downloads the SDL 1.2 plus Mesa/X11/Xvfb set and unpacks it into `$AGTK/nativeprefix` (the SDL 1.2 stack only exists up to jammy) |
| `getpkg.py` | same, for packages wanted later (autoconf, automake, m4, libtool, pkg-config, libpulse0); reads the noble index only |
| `runbootstrap.py` | namespace runner: tmpfs over `/usr/share` with symlinks to the prefix's autoconf/automake data, plus `PERL5LIB`, `M4`, `AUTOM4TE`, `PATH`, then execs the command it is given |
| `native_build.sh` | `bootstrap.sh`, `configure` with the flags that work here, build the client, `make check` |
| `runxvfb.py`, `runxephyr.py` | X servers from the prefix, with the prefix shadowing `/usr/bin`, `/usr/share`, `/usr/lib`, `/etc/X11` |
| `x11/xorg.conf` | the `/etc/X11` content the two X servers bind to; fix `ModulePath` to point at `$AGTK/nativeprefix/usr/lib/xorg/modules` |

## Use

`AGTK` is the scratch root, default `~/agtk`; `AGTK_SRC` is the source tree,
default the current directory.

```sh
export AGTK=$HOME/agtk
mkdir -p $AGTK/debs
python3 batch/make/container/fetchpkgs.py          # about 1.4 GB of prefix
AGTK_SRC=$AGTK/src unshare -rm --propagation private \
    python3 $AGTK_SRC/batch/make/container/runbootstrap.py \
    sh $AGTK_SRC/batch/make/container/native_build.sh
python3 batch/make/container/getpkg.py autoconf automake m4 libtool   # extras
```

## What was measured

Ubuntu 24.04 (noble) x86_64, one container, 8 cores, 3 GB, no root, `/usr/bin`
not writable.  `native_build.sh` bootstraps the autotools files, configures,
compiles 103 objects, links `src/armagetronad_main` (2 749 416 bytes) and runs
the suite:

```
PASS: wvLensMapTest
# TOTAL: 1  PASS: 1  FAIL: 0
aspect 1.000 globe cube:  2993 px, 292 black, 0 mismatch, mean |du| 0.00000, max |du| 0.00000, max |dv| 0.00000
aspect 1.333 globe cube:  2993 px, 650 black, 0 mismatch, mean |du| 0.00000, max |du| 0.00000, max |dv| 0.00000
```

## Gotchas the scripts already work around

- `autoheader` calls `/usr/bin/autom4te` by absolute path.  Shadowing
  `/usr/bin` with a symlink farm fixes that but breaks gcc, which resolves its
  own prefix through the symlink and then cannot find `cc1plus`; set
  `AUTOM4TE` instead.
- `autom4te` reads `$M4`.  There is no `--m4` option.
- `aclocal` refuses to run unless `/usr/share/aclocal-1.16` exists.
- The `config.guess`/`config.sub` in the automake package are symlinks into
  `/usr/share/misc`, which the prefix does not carry; `native_build.sh` copies
  the real ones out of `autoconf/build-aux`.
- `PKG_PROG_PKG_CONFIG` needs `pkg.m4` in one of aclocal's directories.
- `configure` needs the SDL prefix pointed at the unpacked prefix
  (`--with-sdl-prefix=$AGTK/nativeprefix/usr`), and `libpulse` pulls
  `libpulsecommon` out of a subdirectory, so `LD_LIBRARY_PATH` has to include
  `$AGTK/nativeprefix/usr/lib/x86_64-linux-gnu/pulseaudio` at run time.

## X11 caveat

`runxvfb.py` brings up a 1280x800 display, but on noble the packaged xserver
does not load its glx module under these binds: `xdpyinfo` reports 22
extensions and GLX is not one of them, and `glxinfo` ends with `couldn't find
RGB GLX visual`.  So the native client cannot render against it; check with
`xdpyinfo | grep -i glx` before assuming a display is usable for GL work.
