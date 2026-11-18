#!/usr/bin/env python3
"""Drive the running game window with xdotool and take in-game screenshots.

Actions are given as arguments: wait <ms>, key <keysym>, shot (F10, bound to
the SCREENSHOT command in autoexec.cfg).  The window is focused first; on a
server with no window manager SetInputFocus can race the game creating its
window, so it is retried until the focus query agrees.
"""
import os, subprocess, sys, time

A = "/home/agent/agtk"
P = f"{A}/nativeprefix/usr/lib/x86_64-linux-gnu"
env = dict(os.environ)
env.update({"LD_LIBRARY_PATH": P, "DISPLAY": ":97"})
XDO = f"{A}/nativeprefix/usr/bin/xdotool"

def sh(*a):
    r = subprocess.run([XDO] + list(a), env=env, capture_output=True, text=True)
    if r.returncode != 0:
        print("xdotool", *a, "-> rc", r.returncode, r.stderr.strip()[:160])
    return r.stdout.strip()

win = ""
for probe in (["search", "--class", "armagetronad"], ["search", "--name", "Armagetron"],
              ["search", "--name", "armagetron"]):
    out = sh(*probe)
    if out:
        win = out.split()[-1]
        break
print("window:", win or "none found")
if not win:
    sys.exit(1)

for attempt in range(12):
    sh("windowfocus", "--sync", win)
    if sh("getwindowfocus") == win:
        break
    time.sleep(0.5)
else:
    print("could not focus the game window")
print("focus:", sh("getwindowfocus"))

args = sys.argv[1:]
i = 0
while i < len(args):
    a = args[i]
    if a == "wait":
        time.sleep(int(args[i + 1]) / 1000.0)
        i += 2
    elif a == "key":
        sh("key", "--window", win, args[i + 1])
        time.sleep(0.12)
        i += 2
    elif a == "shot":
        sh("key", "--window", win, "F10")
        time.sleep(0.4)
        i += 1
    else:
        print("unknown action", a)
        i += 1
print("drive done")
