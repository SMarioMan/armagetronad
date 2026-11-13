#!/usr/bin/env python3
"""Cycle-body shading and mid-distance floor grid: desktop vs wasm numbers.

Neither measurement needs the two builds to share AI state: the player's light
cycle is driven by the recording and the stamp manifest shows it at the same
position, height and heading in both runs, so its screen crop is directly
comparable; the floor statistics are taken over a fixed pixel band, so they are
properties of the renderer rather than of the round's geometry.

usage: cyclegrid.py DESKTOP.png WASM.png OUTPREFIX
"""
import sys

import numpy as np
from PIL import Image

# Screen box holding the player's cycle: the wall top edges in the replay all
# converge on image (640,530), which is where the bike itself is drawn.
CYCLE = (560, 450, 720, 610)
# Mid distance floor band: above the near-field cycle, below the horizon haze.
BAND = (200, 380, 1100, 470)


def load(path):
    rgb = np.asarray(Image.open(path).convert("RGB"), dtype=np.int16)
    return rgb, rgb.sum(axis=2)


def crop(lum, box):
    x0, y0, x1, y1 = box
    return lum[y0:y1, x0:x1]


def cycle_stats(l, rgb, tag):
    print(f"  {tag}: crop {crop(l, CYCLE).shape}  "
          f"mean {crop(l, CYCLE).mean():6.1f}  max {crop(l, CYCLE).max():4d}  "
          f"px>150 {int((crop(l, CYCLE) > 150).sum()):5d}  "
          f"px>400 {int((crop(l, CYCLE) > 400).sum()):5d}  "
          f"px>600 {int((crop(l, CYCLE) > 600).sum()):5d}")
    # Which channels carry the lit body: the desktop lights the bike with a
    # warm key light and a cool fill (gCycle.cpp lighta/lightb).
    x0, y0, x1, y1 = CYCLE
    for ch, name in enumerate("RGB"):
        c = rgb[y0:y1, x0:x1, ch]
        print(f"      {name}: mean {c.mean():6.1f}  px>150 {int((c > 150).sum()):5d}")


def grid_stats(l, tag):
    x0, y0, x1, y1 = BAND
    band = l[y0:y1, x0:x1]
    rows = band.mean(axis=1)
    cols = band.mean(axis=0)
    for name, prof in (("rows", rows), ("cols", cols)):
        med = np.median(prof)
        # local maxima that stand out from their immediate neighbours
        peaks = []
        for i in range(1, len(prof) - 1):
            if prof[i] > med and prof[i] >= prof[i - 1] and prof[i] > prof[i + 1] \
               and prof[i] - med > 12:
                if peaks and i - peaks[-1] < 3:
                    continue
                peaks.append(i)
        amp = np.percentile(prof, 95) - np.percentile(prof, 5)
        top = sorted((prof[i] - med for i in peaks), reverse=True)[:6]
        print(f"  {tag} {name}: median {med:6.1f}  p95-p5 {amp:6.1f}  "
              f"peaks {len(peaks):3d}  top peak heights "
              + " ".join(f"{t:5.1f}" for t in top))


def pair(desktop, wasm, box, out):
    a = Image.open(desktop).convert("RGB").crop(box)
    b = Image.open(wasm).convert("RGB").crop(box)
    w, h = a.size
    canvas = Image.new("RGB", (w * 2 + 16, h), (255, 0, 255))
    canvas.paste(a, (0, 0))
    canvas.paste(b, (w + 16, 0))
    canvas.save(out)


def main():
    dpath, wpath, outp = sys.argv[1:4]
    drgb, dl = load(dpath)
    wrgb, wl = load(wpath)
    print("cycle body crop (desktop | wasm):")
    cycle_stats(dl, drgb, "desktop")
    cycle_stats(wl, wrgb, "wasm   ")
    print("floor band statistics (desktop | wasm):")
    grid_stats(dl, "desktop")
    grid_stats(wl, "wasm   ")
    pair(dpath, wpath, CYCLE, outp + "-cycle.png")
    pair(dpath, wpath, BAND, outp + "-grid.png")
    print(f"wrote {outp}-cycle.png and {outp}-grid.png (desktop left, wasm right)")


if __name__ == "__main__":
    main()
