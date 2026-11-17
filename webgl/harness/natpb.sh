#!/bin/bash
# Desktop-side capture for the parity pairs (webgl/harness/cdpd.py is the port
# side).  Plays a recording in a native build under an Xvfb with GLX and takes
# an in-game screenshot the moment the game clock reaches each listed gate, so
# the PNG sits on an exact stamped frame instead of "whenever the X grab
# happened to land".
#
#   natpb.sh OUTPREFIX REPLAY GATE[,GATE...]
#
# Environment:
#   NBIN      the armagetronad client                 (default
#             ../..//src/armagetronad_main relative to this script's repo)
#   NUSERDIR  config dir holding autoexec.cfg         (required)
#   NVARDIR   var dir                                 (default $NUSERDIR)
#   NDISP     X display                               (default :97)
#   NPREFIX   prefix with the SDL/X libs, if they are not system-wide
#   NLOG      where the frame stamps go               (default OUTPREFIX-log.txt)
#
# Both builds print the same stamp line (webgl/harness/parity-stamp.patch,
# harness-only, never committed):
#
#   PF FRAME <n> state 6 gt <game time> ncam <c> pos <x> <y> h <h> dir <dx> <dy> menu <m>
#
# and the manifest line written next to each PNG is what
# webgl/harness/parity_report.py and shots/tools/pair_diff.py pair on:
#
#   <png> 0 <x> <y> <w> <h> n<n> gt <g> pos <x> <y> h <h> dir <dx> <dy>
#
# The client build has to carry the same probe; the gate trigger below it
# (AA_SHOT_GT) is part of that probe and sets sr_screenshotIsPlanned, which
# rSysdep.cpp:690 turns into make_screenshot() inside the same SwapGL as the
# stamped frame, i.e. pixels and stamp describe one frame.
set -e

here="$(cd "$(dirname "$0")" && pwd)"
pre="${1:?usage: natpb.sh OUTPREFIX REPLAY GATE[,GATE...]}"
rec="${2:?no replay file}"
gates="${3:?no game-time gates}"
NBIN="${NBIN:-$here/../../src/armagetronad_main}"
NUSERDIR="${NUSERDIR:?NUSERDIR must point at the dir holding autoexec.cfg}"
NVARDIR="${NVARDIR:-$NUSERDIR}"
NDISP="${NDISP:-:97}"
NLOG="${NLOG:-$pre-log.txt}"
shotdir="$PWD/screenshot"

[ -x "$NBIN" ] || { echo "no client at $NBIN (set NBIN)"; exit 1; }

# The probe only prints when the variable is set, and the screenshot list is
# read once, at the first stamped frame.
export ARMA_PARITY_STAMP=1 AA_SHOT_GT="$gates"
export DISPLAY="$NDISP"
[ -n "$NPREFIX" ] && export LD_LIBRARY_PATH="$NPREFIX/usr/lib:$NPREFIX/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# llvmpipe with GLX over Xvfb: the same fixed-function path a software player
# gets, and the string tests in rScreen.cpp:869-894 still classify it as
# hardware, so nothing is silently clamped.
export LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe SDL_AUDIODRIVER=dummy
# Mesa only finds its software driver when the DRI search path points at it.
# Without this the context is created, the client runs, and every grabbed
# screenshot is the same 3 kB black PNG - so the path has to be explicit.
if [ -n "$NPREFIX" ] && [ -d "$NPREFIX/usr/lib/x86_64-linux-gnu/dri" ]; then
    export LIBGL_DRIVERS_PATH="$NPREFIX/usr/lib/x86_64-linux-gnu/dri"
fi


rm -f "$NLOG"
rm -f "$shotdir"/screenshot_*.png 2>/dev/null || true

# Xvfb accepts connections on one thread; on a loaded machine the accept can
# time out and SDL then dies with "Couldn't open X11 display".  Retry instead
# of publishing a run that never drew anything.
for attempt in 1 2 3 4 5; do
    "$NBIN" --playback "$rec" \
        --userconfigdir "$NUSERDIR" --configdir "$NUSERDIR" --vardir "$NVARDIR" \
        > "$pre-stdout.txt" 2> "$NLOG" &
    game=$!
    for _ in $(seq 1 15); do
        if grep -q "Couldn't open X11 display" "$NLOG" 2>/dev/null; then break; fi
        if grep -q "PF FRAME" "$NLOG" 2>/dev/null; then break; fi
        sleep 1
    done
    if ! grep -q "Couldn't open X11 display" "$NLOG" 2>/dev/null; then break; fi
    echo "attempt $attempt: the X display refused the client, retrying" >&2
    kill "$game" 2>/dev/null || true
    sleep 5
done

# Wait for the last gate, not for wall clock: playback advances the game clock
# one recorded frame at a time, so a slow renderer only takes longer, it never
# lands the shots on a different state.
last="${gates##*,}"
for _ in $(seq 1 900); do
    kill -0 "$game" 2>/dev/null || break
    if grep -qE "gt [0-9]+\." "$NLOG" 2>/dev/null &&
       "${PYTHON:-python3}" - "$NLOG" "$last" <<'EOF'
import re, sys
g = [float(m) for m in re.findall(r" gt (-?[\d.]+)", open(sys.argv[1], errors="replace").read())]
sys.exit(0 if g and g[-1] >= float(sys.argv[2]) else 1)
EOF
    then break; fi
    sleep 1
done
kill "$game" 2>/dev/null || true
wait "$game" 2>/dev/null || true

"${PYTHON:-python3}" - "$NLOG" "$shotdir" "$pre" "$gates" <<'EOF'
import hashlib, re, shutil, sys
log, shotdir, pre, gates = sys.argv[1:5]
text = open(log, errors="replace").read()
if "libGL error" in text:
    sys.exit("the client could not load a GL driver (see the log): it renders "
             "nothing, so every grabbed screenshot would be black.  Point "
             "LIBGL_DRIVERS_PATH at a DRI directory and run again.")
recs = []
for l in text.splitlines():
    m = re.search(r"PF FRAME (\d+) state 6 gt (-?[\d.]+) ncam (\d+) pos (-?[\d.]+) "
                  r"(-?[\d.]+) h (-?[\d.]+) dir (-?[\d.]+) (-?[\d.]+)", l)
    if m:
        recs.append(m.groups())
if not recs:
    sys.exit("no stamps in %s - the parity-stamp probe is not in this build, or "
             "ARMA_PARITY_STAMP was not picked up" % log)
rows = []
for i, g in enumerate([float(g) for g in gates.split(",") if g], 1):
    r = next((r for r in recs if float(r[1]) >= g), None)
    if not r:
        sys.exit("gate %s never reached in %d stamped frames" % (g, len(recs)))
    n, gt, ncam, px, py, h, dx, dy = r
    png = "%s-%02d-gt%s.png" % (pre, i, gt)
    shutil.copyfile("%s/screenshot_%d.png" % (shotdir, i), png)
    rows.append((png, n, gt, px, py, h, dx, dy))
    print("%s  frame %s  gt %s  pos %s %s  h %s  dir %s %s" % (png, n, gt, px, py, h, dx, dy))
if len({hashlib.md5(open(p, "rb").read()).hexdigest() for p, *_ in rows}) < len(rows):
    sys.exit("the gate PNGs are byte-identical - the framebuffer grab came back "
             "empty, so nothing here is a real frame")
with open(pre + "-shots.txt", "w") as f:
    for png, n, gt, px, py, h, dx, dy in rows:
        f.write("%s 0 0 0 1280 800 n%s gt %s pos %s %s h %s dir %s %s\n"
                % (png, n, gt, px, py, h, dx, dy))
EOF
