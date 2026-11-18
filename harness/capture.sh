#!/bin/sh
# capture.sh <tag>  - drive the running game through the single player menu,
# take in-game screenshots and file them under /workspace/shots/<tag>-*.png
set -e
tag="$1"
cd /home/agent/agtk/run360
python3 drive.py key Return wait 1500 key Down wait 1200 key Return \
    wait 6000 shot wait 4000 shot wait 5000 shot
n=0
for f in /home/agent/agtk/cfg360/screenshot/screenshot_*.png; do
    n=$((n+1))
    cp "$f" "/workspace/shots/${tag}-${n}.png"
    chmod 644 "/workspace/shots/${tag}-${n}.png"
done
rm -f /home/agent/agtk/cfg360/screenshot/screenshot_*.png
echo "captured $n shots as ${tag}-*.png"
