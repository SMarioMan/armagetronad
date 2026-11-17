#!/bin/sh
# Start an Xvfb that answers GLX, for the desktop capture runs (natpb.sh).
#
# Two things stand in the way on the host.  Its mesa 24 dropped the legacy drisw
# entry points, so a stock Xvfb exposes no GLX visuals at all; the Debian
# bullseye X/mesa closure that webgl/harness/fetchpkgs.py assembles into
# prefix does.  And Xvfb needs xkbcomp plus the xkeyboard-config data, which the
# host does not keep where Xvfb looks for them - so the server runs inside a
# user+mount namespace with the prefix bind-mounted over the system locations,
# which is what nsbind.sh arranges.  (/var/lib/xkb, the compiled keymap cache, is
# not writable here either; Xvfb then compiles the keymap on every start and the
# capture runs are unaffected.)
#
#   sh webgl/harness/native_x.sh [DISPLAY] [NPREFIX]   # defaults :97, $NPREFIX
#
# It runs in the foreground: start it as a background service and leave it up
# for the whole capture session.  It is up when the socket appears -
#
#   ls /tmp/.X11-unix/X97
#
# Xvfb's banner is buffered and often never reaches a captured log, so do not
# gate a service start on its output.  Whether GLX actually answers is what the
# probe says; compile it once
#
#   cc -I"$P/usr/include" -L"$P/usr/lib/x86_64-linux-gnu" \
#      -o /tmp/glxprobe webgl/harness/glxprobe.c -lGL -lX11
#
# and ask it, with the same environment the client needs:
#
#   DISPLAY=:97 LD_LIBRARY_PATH="$P/usr/lib/x86_64-linux-gnu:$P/usr/lib" \
#   LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
#   LIBGL_DRIVERS_PATH="$P/usr/lib/x86_64-linux-gnu/dri" /tmp/glxprobe
#
# which answers  GLX present=1 version 1.4 FBConfigs=840
#
# Stop it with  pkill -f "[X]vfb :97"  (the brackets keep the pattern from
# matching the shell that is running the pkill).
set -e
here="$(cd "$(dirname "$0")" && pwd)"
disp="${1:-:97}"
P="${2:-${NPREFIX:-$(cd "$here/../.." && pwd)/nativeprefix}}"

[ -x "$P/usr/bin/Xvfb" ] || {
    echo "no Xvfb under $P/usr/bin - build the prefix first, see webgl/ENVIRONMENT.md" >&2
    exit 1
}

exec env LD_LIBRARY_PATH="$P/usr/lib/x86_64-linux-gnu:$P/usr/lib" \
    LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe \
    /usr/bin/unshare -rm --propagation private \
    "$here/nsbind.sh" \
        --bind "$P/usr/lib"   /usr/lib \
        --bind "$P/usr/bin"   /usr/bin \
        --bind "$P/usr/share" /usr/share \
        --bind "$P/etc/X11"   /etc/X11 \
    -- /usr/bin/Xvfb "$disp" -screen 0 1280x800x24 +extension GLX +iglx -nolisten tcp
