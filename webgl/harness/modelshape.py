#!/usr/bin/env python3
"""Harness (never commit): shape signature of the followed cycle in a frame.

The report is that the browser build draws the lightbike model shredded.  The
model is three .mod files (cycle_body/front/rear) drawn with
glVertexPointer/glNormalPointer/glTexCoordPointer plus glDrawElements, and the
port's own log reports INVALID_ENUM at a drawElements with count 243, which is
one of those models.  A broken element index reads faces between unrelated
vertices, so the silhouette spreads wider, breaks into more disconnected pieces
and its column-height profile becomes jagged.  This script turns that into
numbers per frame so a native/port pair can be compared without eyeballing.

usage: modelshape.py IMAGE [IMAGE...] [--box X0 Y0 X1 Y1]
"""
import sys
import numpy as np
from PIL import Image


def components(mask):
    """Connected-component count of a boolean mask (4-neighbour flood fill)."""
    seen = np.zeros(mask.shape, bool)
    n = 0
    h, w = mask.shape
    for y in range(h):
        for x in range(w):
            if mask[y, x] and not seen[y, x]:
                n += 1
                stack = [(y, x)]
                seen[y, x] = True
                while stack:
                    cy, cx = stack.pop()
                    for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                        ny, nx = cy + dy, cx + dx
                        if 0 <= ny < h and 0 <= nx < w and mask[ny, nx] and not seen[ny, nx]:
                            seen[ny, nx] = True
                            stack.append((ny, nx))
    return n


def shape(path, box):
    a = np.asarray(Image.open(path).convert("RGB")).astype(float)
    r, g, b = a[..., 0], a[..., 1], a[..., 2]
    x0, y0, x1, y1 = box
    # The followed cycle is drawn in the local player's colour, and the harness
    # config pins that to pure green (COLOR_*_1 = 0,15,0), so the model is found
    # by colour dominance instead of brightness: with the camera parked 20 units
    # above the floor, a brightness mask picked up lit floor and wall band instead
    # of the cycle.  The player's own wall trail is green too, but it is a 1-2 px
    # dashed line behind the bike, which the neighbourhood filter below removes.
    solid = ((g > r + 30) & (g > b + 30) & (g > 40))[y0:y1, x0:x1]
    # The HUD draws one-pixel crosshair/marker lines through the cycle: a pixel
    # only counts when a 3x3 neighbourhood also has neighbours, which removes
    # lines but keeps the body.
    acc = np.zeros_like(solid, int)
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            acc += np.roll(np.roll(solid.astype(int), dy, axis=0), dx, axis=1)
    body = solid & (acc >= 4)
    if not body.any():
        return dict(path=path, n=0)
    ys, xs = np.nonzero(body)
    h = ys.max() - ys.min() + 1
    w = xs.max() - xs.min() + 1
    col = np.array([body[:, x].sum() for x in range(xs.min(), xs.max() + 1)])
    rough = float(np.abs(np.diff(col)).sum())
    return dict(path=path, n=int(body.sum()), box=(w, h), area_pct=100.0 * body.sum() / (w * h),
                comps=components(body), rough=rough, colmean=float(col.mean()),
                bbox=(xs.min() + x0, ys.min() + y0, w, h))


def main():
    args = [x for x in sys.argv[1:] if not x.startswith("--box=")]
    box = (520, 380, 780, 620)
    for x in sys.argv[1:]:
        if x.startswith("--box="):
            box = tuple(int(v) for v in x[6:].split(","))
    print("box", box, "(green-dominant pixels with a filled 3x3 neighbourhood)")
    for p in args:
        d = shape(p, box)
        if d["n"] == 0:
            print("%-24s nothing bright enough in the box" % p.split("/")[-1])
            continue
        print("%-24s px %4d  bbox %dx%-3d fill %5.1f%%  pieces %2d  profile_roughness %6.1f  "
              "mean col %4.1f  bbox at (%d,%d)"
              % (p.split("/")[-1], d["n"], d["box"][0], d["box"][1], d["area_pct"],
                 d["comps"], d["rough"], d["colmean"], d["bbox"][0], d["bbox"][1]))


if __name__ == "__main__":
    main()
