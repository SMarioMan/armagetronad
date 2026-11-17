#!/bin/sh
# Re-arm the WebGL GL-state hook after webgl/build.sh regenerates webgl/dist.
# build.sh wipes dist/, so the hook file and its <script> tag have to be put
# back after every build before a GLHOOK capture will produce anything.
#
#   sh webgl/harness/inject_hook.sh [aarec ...]
#
# Afterwards webgl/serve.py serves webgl/dist, and a page load pulls glhook.js
# in before Module boots: it prints GLHOOK ... lines to the browser console,
# which cdpd.py captures into <tag>-console.txt.  The hook is only ever
# injected into a local dist; a shipped page never carries it.
#
# Any .aarec named on the command line is copied into dist/ too, because the
# page's --playback argument can only open a file that is already in the
# preloaded filesystem.
set -e
here="$(cd "$(dirname "$0")" && pwd)"
D="$(cd "$here/.." && pwd)/dist"

cp -f "$here/glhook.js" "$D/glhook.js"

# drop any previous injection, then re-add it right after <head>
python3 - "$D/index.html" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
tag = '<script src="glhook.js"></script>'
if tag in s:
    print("hook already injected")
    sys.exit(0)
i = s.find("<head>")
if i < 0:
    sys.exit("no <head> in " + p)
i += len("<head>")
open(p, "w").write(s[:i] + "\n" + tag + s[i:])
print("hook injected")
PY

for f in "$@"; do
    cp -f "$f" "$D/$(basename "$f")"
    echo "served $(basename "$f") $(stat -c %s "$D/$(basename "$f")") bytes"
done
