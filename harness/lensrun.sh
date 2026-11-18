#!/bin/sh
# One lens comparison run: write the config, restart the game cleanly, start a
# local game and take two shots.  Usage: lensrun.sh <lens> [globe] [tag]
A=/home/agent/agtk
P=$A/nativeprefix/usr/lib/x86_64-linux-gnu
XDO=$A/nativeprefix/usr/bin/xdotool
L=$1
G=${2:-0}
T=${3:-lens$L-$G}

cat > $A/run360/var/autoexec.cfg <<EOF
PNG_SCREENSHOT 1
ARMAGETRON_SCREENMODE 0
ARMAGETRON_WINDOWSIZE 8
MAX_FPS 60
WIDE_VIEW_LENS $L
WIDE_VIEW_FOV 360
WIDE_VIEW_GLOBE $G
WIDE_VIEW_PLATE_RES 0.5
WIDE_VIEW_LENS_RES 0.5
KEYBOARD 291 PLAYER_BIND SCREENSHOT 0
KEYBOARD 290 PLAYER_BIND MESS_UP 0
KEYBOARD 292 PLAYER_BIND MESS_DOWN 0
KEYBOARD 293 PLAYER_BIND MESS_END 0
EOF

pkill -f "[a]rmagetronad_main" 2>/dev/null
sleep 2
rm -f $A/cfg360/user.cfg $A/cfg360/var/user.cfg $A/cfg360/screenshot/*.png

cd $A/run360
setsid /usr/bin/unshare -rm --propagation private /usr/bin/python3 launch.py \
    > $A/run360/run-$T.log 2>&1 < /dev/null &

# Wait for the game window, then for the menu to settle.
w=""
i=0
/usr/bin/sleep 10
while [ $i -lt 40 ]; do
    i=$((i+1))
    w=$(LD_LIBRARY_PATH=$P DISPLAY=:97 $XDO search --class armagetronad 2>/dev/null | /usr/bin/tail -1)
    [ -n "$w" ] && break
    sleep 1
done
echo "window: ${w:-none} after ${i}s"
[ -z "$w" ] && { tail -5 $A/run360/run-$T.log; exit 1; }

/usr/bin/python3 drive.py key Return wait 1000 key Down wait 600 key Return wait 6000 shot wait 4000 shot
n=0
for f in $(/usr/bin/ls $A/cfg360/screenshot/*.png 2>/dev/null); do
    n=$((n+1))
    /usr/bin/cp "$f" "/workspace/shots/$T-$n.png"
done
/usr/bin/chmod 644 /workspace/shots/$T-*.png 2>/dev/null
/usr/bin/ls -la /workspace/shots/$T-*.png 2>/dev/null | /usr/bin/awk '{print $9,$5}'
