#!/usr/bin/env python3
"""Measure how the floor grid reads in screenshots of the same world state.

The complaint this settles: the web port's floor grid lines look about one pixel
thin and burnt out, and the grid at a distance disappears, while the native
build draws wider lines that stay readable far away.  A straight pixel diff
cannot answer that: the geometry is identical, so the diff mostly shows
sub-pixel edge disagreements.  Instead measure, per depth band, how the lines
actually read -- how many depth lines a scanline crosses, how thick each line
is, how far its brightness sits above the surrounding floor, and how bright the
floor itself is.

A scanline crossing the floor runs near -> far as y decreases (the horizon sits
above the floor region), so band 0 is the near floor and the last band is far.

usage: gridprofile.py [--y0=0.55] [--y1=0.98] [--thr=24] [--bands=4]
                      [--rect=x,y,w,h] IMG [IMG ...]
"""
import sys

import numpy as np
from PIL import Image


def median_baseline(line, win):
    """Baseline brightness of the floor itself: the median of a wide window, so
    the thin bright lines (a few pixels wide) do not move it."""
    half = win // 2
    padded = np.concatenate((np.full(half, line[0]), line, np.full(half, line[-1])))
    stacks = np.lib.stride_tricks.sliding_window_view(padded, win)
    return np.median(stacks, axis=1)


def runs(mask):
    out, start = [], None
    for i, v in enumerate(mask):
        if v and start is None:
            start = i
        elif not v and start is not None:
            out.append(i - start)
            start = None
    if start is not None:
        out.append(len(mask) - start)
    return out


def profile(path, y0, y1, thr, bands, rect=None):
    img = np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)
    if rect:
        x, y, w, h = rect
        x, y = max(0, x), max(0, y)
        img = img[y:min(img.shape[0], y + h), x:min(img.shape[1], x + w)]
    h, w, _ = img.shape
    lum = 0.299 * img[:, :, 0] + 0.587 * img[:, :, 1] + 0.114 * img[:, :, 2]
    top, bot = int(h * y0), int(h * y1)
    rows = np.linspace(bot - 1, top, bands * 6).astype(int)  # bottom = near
    per_band = [[] for _ in range(bands)]
    for i, y in enumerate(rows):
        line = lum[y]
        base = median_baseline(line, 15)
        excess = line - base
        mask = excess > thr
        r = runs(mask)
        band = min(bands - 1, (i * bands) // len(rows))
        if not r:
            per_band[band].append((0, 0.0, 0.0, 0.0, float(np.median(line))))
            continue
        per_band[band].append((
            len(r),
            100.0 * sum(r) / len(mask),
            float(np.mean(r)),
            float(np.percentile(excess[mask], 95)),
            float(np.median(line)),
        ))
    return per_band


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    opt = {}
    for a in sys.argv[1:]:
        if a.startswith("--"):
            k, v = a[2:].split("=")
            if k in ("y0", "y1", "thr"):
                opt[k] = float(v)
            elif k == "bands":
                opt[k] = int(v)
            else:
                opt[k] = v
    y0 = opt.get("y0", 0.55)
    y1 = opt.get("y1", 0.98)
    thr = opt.get("thr", 24.0)
    bands = opt.get("bands", 4)
    rect = [int(v) for v in opt["rect"].split(",")] if "rect" in opt else None

    data = {}
    for p in args:
        data[p.rsplit("/", 1)[-1]] = profile(p, y0, y1, thr, bands, rect)

    print(f"floor region y {y0:.2f}-{y1:.2f} of image height, line threshold "
          f"+{thr:.0f} over a 15 px median baseline, {bands} depth bands "
          f"(0 = near, {bands-1} = far)"
          + (f", crop {opt['rect']}\n" if rect else "\n"))
    hdr = f"{'image':<26} {'band':>4} {'lines':>6} {'line px':>8} {'width':>6} {'peak+':>6} {'floor':>6}"
    print(hdr)
    print("-" * len(hdr))
    for name, per_band in data.items():
        for b, rows in enumerate(per_band):
            lines = float(np.mean([r[0] for r in rows]))
            frac = float(np.mean([r[1] for r in rows]))
            width = float(np.mean([r[2] for r in rows])) if lines else 0.0
            peak = float(np.mean([r[3] for r in rows]))
            floor = float(np.mean([r[4] for r in rows]))
            print(f"{name:<26} {b:>4} {lines:>6.1f} {frac:>7.1f}% {width:>6.2f} "
                  f"{peak:>6.1f} {floor:>6.1f}")
        print()
    print("lines   = depth lines a scanline crosses (a vanishing grid lowers this)")
    print("line px = share of the scanline covered by grid line")
    print("width   = mean run length of a line, in pixels")
    print("peak+   = 95th percentile brightness above the floor baseline (burnt-out lines)")
    print("floor   = median floor brightness")


if __name__ == "__main__":
    main()
