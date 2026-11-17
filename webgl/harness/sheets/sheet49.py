#!/usr/bin/env python3
"""Compose the published sheet-49 comparisons.

    sheet49.py NATMANIFEST PORTMANIFEST

Reads the two capture manifests (same format as *-shots.txt: png, wall, box,
frame, gt, pose, heading, direction) and writes

    49-pairN.png   native left, port right, full size, pose delta in the label
    49-all.png     every pair stacked, halved, with the grid numbers underneath

Labels are drawn with the PIL default font scaled up, because these files are
meant to be opened by a person, not parsed.
"""
import os
import re
import sys

from PIL import Image, ImageDraw, ImageFont

SHOTS = os.environ.get("SHOTS", os.getcwd())
LINE = re.compile(
    r"(?P<png>\S+)\s+(?P<wall>\d+)\s+(?P<x>\d+)\s+(?P<y>\d+)\s+(?P<w>\d+)\s+"
    r"(?P<h>\d+)\s+n(?P<n>\d+)\s+gt\s+(?P<gt>-?[\d.]+)\s+pos\s+"
    r"(?P<px>-?[\d.]+)\s+(?P<py>-?[\d.]+)\s+h\s+(?P<hh>-?[\d.]+)\s+dir\s+"
    r"(?P<dx>-?[\d.]+)\s+(?P<dy>-?[\d.]+)")


def rows(path):
    out = []
    for line in open(path):
        line = line.strip()
        if line:
            out.append(LINE.match(line).groupdict())
    return out


def font(px=22):
    try:
        return ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", px)
    except OSError:
        return ImageFont.load_default()


def banner(text, w, f, px=22):
    im = Image.new("RGB", (w, px + 12), (12, 12, 12))
    d = ImageDraw.Draw(im)
    d.text((10, 5), text, fill=(240, 240, 240), font=f)
    return im


def pair(nat, port, i, dx, dy):
    a, b = Image.open(nat["png"]).convert("RGB"), Image.open(port["png"]).convert("RGB")
    w = min(a.width, b.width)
    h = min(a.height, b.height)
    a, b = a.crop((0, 0, w, h)), b.crop((0, 0, w, h))
    f = font(22)
    head = banner(
        f"pair {i}   desktop gt {nat['gt']} frame {nat['n']}   |   "
        f"port gt {port['gt']} frame {port['n']}   |   "
        f"camera pose delta {dx:.3f} / {dy:.3f} world units", 2 * w + 12, f)
    la = banner("desktop (native Linux build, llvmpipe)", w, font(19))
    lb = banner("port (this build, browser software GL)", w, font(19))
    out = Image.new("RGB", (2 * w + 12, h + 2 * la.height + head.height), (12, 12, 12))
    y = 0
    out.paste(head, (0, y)); y += head.height + 4
    out.paste(la, (0, y)); out.paste(lb, (w + 12, y)); y += la.height + 4
    out.paste(a, (0, y)); out.paste(b, (w + 12, y))
    out.save(f"{SHOTS}/49-pair{i}.png")
    return out


def main():
    nat, port = rows(sys.argv[1]), rows(sys.argv[2])
    assert len(nat) == len(port), f"{len(nat)} desktop rows vs {len(port)} port rows"
    halves, notes = [], []
    for i, (n, p) in enumerate(zip(nat, port), 1):
        dx = abs(float(n["px"]) - float(p["px"]))
        dy = abs(float(n["py"]) - float(p["py"]))
        full = pair(n, p, i, dx, dy)
        half = full.resize((full.width // 2, full.height // 2), Image.LANCZOS)
        halves.append(half)
        notes.append(f"pair {i}: desktop gt {n['gt']} (frame {n['n']}) vs port gt "
                     f"{p['gt']} (frame {p['n']}), pose delta {dx:.3f} / {dy:.3f}")
    w = max(im.width for im in halves)
    hh = sum(im.height for im in halves) + 8 * len(halves)
    canvas = Image.new("RGB", (w, hh), (12, 12, 12))
    y = 0
    for im in halves:
        canvas.paste(im, (0, y))
        y += im.height + 8
    canvas.save(f"{SHOTS}/49-all.png")
    print("\n".join(notes))


main()
