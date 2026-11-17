#!/bin/sh
# harness: set up mounts inside a user/mount namespace, then exec the real
# command.  Xvfb needs /usr/bin/xkbcomp, the xkeyboard-config data and a
# writable /var/lib/xkb; none of those are usable by an unprivileged user on
# this host.  Usage: nsbind.sh [--bind SRC DST] [--tmpfs DST] ... -- CMD ...
set -e
MOUNT=/usr/bin/mount
# /usr/bin is itself a bind destination, and the mount tool has to survive that
# shadowing: keep a copy of it somewhere nothing shadows.
mkdir -p /tmp/.nsbind 2>/dev/null || true
if [ -x "$MOUNT" ] && [ ! -x /tmp/.nsbind/mount ]; then
    cp "$MOUNT" /tmp/.nsbind/mount 2>/dev/null || true
fi
if [ -x /tmp/.nsbind/mount ]; then MOUNT=/tmp/.nsbind/mount; fi
mkdir -p /tmp/.X11-unix
tmpfs=''
pairs=''
while [ $# -gt 0 ] && [ "$1" != "--" ]; do
    case "$1" in
        --tmpfs) tmpfs="$tmpfs
$2"; shift 2;;
        --bind)  pairs="$pairs
$2 $3"; shift 3;;
        *) echo "nsbind: bad argument $1" >&2; exit 2;;
    esac
done
shift
printf '%s\n' "$tmpfs" | while read -r dst; do
    [ -n "$dst" ] || continue
    # /var/lib/xkb, the keymap cache Xvfb wants to write, does not exist on the
    # host: create it while the host's coreutils are still visible
    mkdir -p "$dst" 2>/dev/null || true
    $MOUNT -t tmpfs tmpfs "$dst"
done
# /usr/bin may itself be a destination: bind it last.
printf '%s\n' "$pairs" | while read -r src dst; do
    [ -n "$src" ] || continue
    mkdir -p "$dst" 2>/dev/null || true
    $MOUNT --bind "$src" "$dst"
done
exec "$@"
