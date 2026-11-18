#!/usr/bin/env python3
"""Extract the native build/run prefix (SDL 1.2 + Mesa/X11 + Xvfb) without root.

Indexes the Ubuntu noble (24.04) and jammy (22.04) package lists, downloads the
named packages and unpacks them under $PREFIX with dpkg -x.
"""
import os, subprocess, sys, lzma, shutil

PREFIX = "/home/agent/agtk/nativeprefix"
CACHE = "/home/agent/agtk/debs"

RELEASES = {
    "noble": ["main", "universe"],
    "jammy": ["main", "universe"],
}

# (release, package) pairs; SDL 1.2 stack only exists up to jammy.
WANT = [
    ("jammy", name) for name in [
        "libsdl1.2debian", "libsdl1.2-dev",
        "libsdl-image1.2", "libsdl-image1.2-dev",
        "libsdl-mixer1.2", "libsdl-mixer1.2-dev",
        "libpng16-16", "libjpeg8", "libtiff5",
    ]
] + [
    ("noble", name) for name in [
        "xvfb", "xserver-xorg-core", "xserver-common", "x11-common",
        "libgl1", "libglx0", "libglx-mesa0", "libgl1-mesa-dri", "libgl-dev",
        "libglu1-mesa", "libglu1-mesa-dev",
        "libdrm2", "libdrm-common", "libgbm1", "libwayland-server0",
        "libexpat1", "libllvm20", "libpixman-1-0", "libunwind8",
        "libx11-6", "libx11-data", "libx11-dev", "x11proto-dev", "xorg-sgml-doctools",
        "libxext6", "libxext-dev", "libxfixes3", "libxdamage1", "libxxf86vm1",
        "libxrandr2", "libxrender1", "libxi6", "libxkbfile1", "libxau6", "libxdmcp6",
        "libxcb1", "libxcb-dri3-0", "libxcb-glx0", "libxcb-present0", "libxcb-render0",
        "libxcb-shm0", "libxcb-shape0", "libxcb-sync1", "libxcb-xfixes0",
        "libxcb-dri2-0", "libxshmfence1", "libdrm-amdgpu1", "libdrm-nouveau2",
        "libdrm-radeon1", "libedit2", "libelf1", "libgcrypt20", "libgpg-error0",
        "libnss3", "libnspr4", "libdbus-1-3", "libudev1", "libudev0-shim",
        "libasound2t64", "libasyncns0", "libatomic1", "libbsd0", "libice6",
        "libsm6", "libxt6", "libice-dev", "libsm-dev", "libxt-dev",
        "libglvnd-core-dev", "libglvnd0", "libegl1", "libegl-dev", "libopengl0",
    ]
]
def index_url(rel, comp):
    return f"http://archive.ubuntu.com/ubuntu/dists/{rel}/{comp}/binary-amd64/Packages.xz"
def load_index(rel, comp):
    fn = CACHE + f"/{rel}-{comp}.Packages.xz"
    if not os.path.exists(fn):
        subprocess.run(["curl", "-sfL", "-o", fn, index_url(rel, comp)], check=True)
    src = fn
    opener = lzma.open if src.endswith(".xz") else (
        __import__("gzip").open if src.endswith(".gz") else open)
    out = {}
    block = {}
    def flush():
        if "Package" in block and "Filename" in block:
            out.setdefault(block["Package"], []).append(block["Filename"])
    with opener(src, "rt", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                flush(); block = {}; continue
            if ":" in line[:20]:
                k, v = line.split(":", 1)
                block[k.strip()] = v.strip()
    flush()
    return out

def main():
    os.makedirs(CACHE, exist_ok=True)
    os.makedirs(PREFIX, exist_ok=True)
    idx = {}
    for rel, comps in RELEASES.items():
        for comp in comps:
            print(f"index {rel}/{comp}", flush=True)
            idx[(rel, comp)] = load_index(rel, comp)
    missing = []
    for rel, pkg in WANT:
        fn = None
        for comp in RELEASES[rel]:
            cands = idx[(rel, comp)].get(pkg)
            if cands:
                fn = cands[-1]
                break
        if not fn:
            missing.append(f"{rel}:{pkg}")
            continue
        dest = os.path.join(CACHE, os.path.basename(fn))
        if not os.path.exists(dest):
            url = "http://archive.ubuntu.com/ubuntu/" + fn
            print("get", fn, flush=True)
            subprocess.run(["curl", "-sfL", "-o", dest, url], check=True)
        print("unpack", pkg, flush=True)
        subprocess.run(["dpkg", "-x", dest, PREFIX], check=True)
    print("MISSING:", " ".join(missing) if missing else "none", flush=True)
    # sanity
    for p in ["usr/lib/x86_64-linux-gnu/libSDL-1.2.so.0",
              "usr/lib/x86_64-linux-gnu/libGL.so.1",
              "usr/bin/Xvfb", "usr/include/SDL/SDL.h", "usr/bin/sdl-config"]:
        print("check", p, os.path.exists(os.path.join(PREFIX, p)), flush=True)

main()
