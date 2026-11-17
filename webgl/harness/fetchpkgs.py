#!/usr/bin/env python3
"""Resolve and download a Debian package closure, the base of the desktop
prefix that the native capture runs use (webgl/ENVIRONMENT.md).

Used to bring in the bullseye X11/Xorg/mesa stack: noble's mesa 24 removed the
legacy drisw entry points, so Xvfb's software GLX has no visuals there.

    fetchpkgs.py INDEX.gz SEED-PACKAGE...      # INDEX = Packages.gz / Packages-bullseye.gz

Downloads go to the current directory as .deb files; webgl/harness/debclosure.txt
is the set the prefix on the reference machine ended up with.
"""
import gzip
import os
import re
import subprocess
import sys

BASE = "http://deb.debian.org/debian"
IDX = sys.argv[1]
SEEDS = sys.argv[2:]

SKIP = {
    "libc6", "libc6-dev", "libgcc-s1", "libstdc++6", "gcc-10-base", "gcc-12-base",
    "locales", "multiarch-support", "sysvinit-utils", "util-linux", "base-files",
    "bsdutils", "coreutils", "dpkg", "tar", "sed", "grep", "findutils",
    "libaudit-common", "libdrm-common", "x11-common", "cpp-10", "libatomic1",
    "libgomp1", "libgdbm6", "libgdbm-compat4", "perl", "perl-base", "libpcre3",
    "libsepol1", "libsepol2", "libselinux1", "libtinfo6", "libncurses6",
    "libncursesw6", "libudev1", "libsystemd0", "libapparmor1", "libwrap0",
}


def parse(path):
    db = {}
    rec = {}
    with gzip.open(path, "rt", errors="replace") as fh:
        for line in fh:
            line = line.rstrip("\n")
            if not line:
                if rec.get("Package"):
                    db[rec["Package"]] = rec
                rec = {}
                continue
            m = re.match(r"^(\w[\w-]*): (.*)$", line)
            if m:
                rec[m.group(1)] = m.group(2)
    if rec.get("Package"):
        db[rec["Package"]] = rec
    return db


def deps(rec):
    out = []
    for field in ("Depends", "Pre-Depends"):
        for alt in (rec.get(field) or "").split(","):
            alt = alt.strip()
            if not alt:
                continue
            name = re.split(r"[(\[]", alt)[0].strip()
            if "|" in alt:
                name = alt.split("|")[0].strip()
                name = re.split(r"[(\[]", name)[0].strip()
            if name:
                out.append(name)
    return out


db = parse(IDX)
seen = set()
queue = list(SEEDS)
picked = []
while queue:
    name = queue.pop(0)
    if name in seen or name in SKIP:
        continue
    seen.add(name)
    rec = db.get(name)
    if not rec:
        print("no such package (skipped):", name)
        continue
    fn = rec.get("Filename")
    if not fn:
        continue
    picked.append((name, fn))
    queue.extend(deps(rec))

here = os.path.dirname(os.path.abspath(__file__))
debs = os.path.join(here, "debs")
os.makedirs(debs, exist_ok=True)
for name, fn in picked:
    dst = os.path.join(debs, os.path.basename(fn))
    if not os.path.exists(dst):
        url = BASE + "/" + fn
        r = subprocess.run(["curl", "-fsS", "-m", "300", "-o", dst, url])
        if r.returncode != 0:
            print("FAILED", name, url)
            os.path.exists(dst) and os.unlink(dst)
            continue
    r = subprocess.run(["dpkg-deb", "-x", dst, os.path.join(here, "nativeprefix")])
    print(("ok " if r.returncode == 0 else "extract-failed ") + name)
print("total", len(picked))
