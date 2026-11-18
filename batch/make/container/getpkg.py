#!/usr/bin/env python3
"""Fetch and unpack named Ubuntu packages into the local prefix."""
import subprocess, os, lzma, sys

# Everything lives under $AGTK (default: ~/agtk).
BASE = os.environ.get("AGTK") or os.path.expanduser("~/agtk")
IDX = {}
for comp in ("main", "universe"):
    block = {}; out = {}
    with lzma.open(f"{BASE}/debs/noble-{comp}.Packages.xz", "rt", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                if "Package" in block and "Filename" in block:
                    out.setdefault(block["Package"], []).append(block["Filename"])
                block = {}; continue
            if ":" in line[:20]:
                k, v = line.split(":", 1)
                block[k.strip()] = v.strip()
    IDX[comp] = out

for p in sys.argv[1:]:
    fn = None
    for c in ("main", "universe"):
        g = IDX[c].get(p)
        if g:
            fn = g[-1]; break
    if not fn:
        print("MISS", p, flush=True); continue
    dest = f"{BASE}/debs/" + os.path.basename(fn)
    if not os.path.exists(dest):
        subprocess.run(["curl", "-sfL", "-o", dest, "http://archive.ubuntu.com/ubuntu/" + fn], check=True)
    subprocess.run(["dpkg", "-x", dest, f"{BASE}/nativeprefix"], check=True)
    print("ok", p, flush=True)
