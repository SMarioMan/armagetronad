#!/usr/bin/env python3
"""Turn one cdpd.py frame-exact port run into the published 49 pair set.

    mkpair49.py PORTMANIFEST GATECOUNT

Copies each port PNG to 49-<i>b-port-gt<gt>.png, writes the matching manifest,
then runs webgl/harness/parity_report.py on every matched pair and prints the
per-row line metrics so they can be quoted in the sheet notes.
"""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SHOTS = os.environ.get("SHOTS", os.getcwd())
REPORT = os.path.join(HERE, "..", "parity_report.py")
PY = sys.executable

# light-cycle boxes per gate index: (x0, x1, y0, y1)
BOX = {
    # Only the two frames where the local cycle body sits unobstructed and
    # without explosion sparks in it; the other gates have the cycle off screen
    # or a dying cycle's spark spray inside any box that would hold the body.
    1: (0, 0, 0, 0),
    2: (0, 0, 0, 0),
    3: (600, 720, 455, 545),
    4: (600, 700, 460, 520),
}

LINE = re.compile(r"(?P<png>\S+)\s+(?P<t>[\d.]+)\s+(?P<x>\d+)\s+(?P<y>\d+)\s+"
                  r"(?P<w>\d+)\s+(?P<h>\d+)\s+n(?P<n>\d+)\s+gt\s+(?P<gt>-?[\d.]+)\s+"
                  r"pos\s+(?P<px>-?[\d.]+)\s+(?P<py>-?[\d.]+)\s+h\s+(?P<ph>-?[\d.]+)\s+"
                  r"dir\s+(?P<dx>-?[\d.]+)\s+(?P<dy>-?[\d.]+)")


def main():
    man, ngates = sys.argv[1], int(sys.argv[2])
    rows = [LINE.match(l.strip()) for l in open(man) if l.strip()]
    rows = [r for r in rows if r]
    assert len(rows) == ngates, f"{len(rows)} port shots, expected {ngates}"
    portman = []
    for i, r in enumerate(rows, 1):
        d = r.groupdict()
        new = f"{SHOTS}/49-{i}b-port-gt{d['gt']}.png"
        shutil.copyfile(d["png"], new)
        portman.append(f"{new} 0 0 0 1280 800 n{d['n']} gt {d['gt']} pos {d['px']} "
                       f"{d['py']} h {d['ph']} dir {d['dx']} {d['dy']}")
        print(f"gate {i}: port frame {d['n']} gt {d['gt']} pos {d['px']} {d['py']}")
    with open(f"{SHOTS}/49-b-port-shots.txt", "w") as f:
        f.write("\n".join(portman) + "\n")

    nat = [LINE.match(l.strip()).groupdict()
           for l in open(f"{SHOTS}/49-a-native-shots.txt") if l.strip()]
    port = [LINE.match(l).groupdict() for l in portman]
    for i, (n, p) in enumerate(zip(nat, port), 1):
        dx = abs(float(n["px"]) - float(p["px"]))
        dy = abs(float(n["py"]) - float(p["py"]))
        print(f"pair {i}: gt {n['gt']} vs {p['gt']}  frame {n['n']} vs {p['n']}  "
              f"pose delta {dx:.3f} / {dy:.3f}")
    print()
    for i, (n, p) in enumerate(zip(nat, port), 1):
        pre = f"{SHOTS}/49-{i}-report"
        cmd = [PY, REPORT, n["png"], p["png"], pre]
        cmd.append("--floormask")
        x0, x1, y0, y1 = BOX[i]
        if x1 > x0:
            cmd.append(f"--cycle={x0},{x1},{y0},{y1},any")
        print(f"=== pair {i} {' '.join(cmd[3:])}")
        subprocess.run(cmd, check=True)


main()
