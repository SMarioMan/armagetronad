#!/usr/bin/env python3
"""Cross-build visual parity report: native frame vs port frame -> legible diffs.

    parity_report.py NATIVE.png PORT.png OUTPREFIX \
        [--port-crop X,Y,W,H] [--band-top N] [--cycle X0,X1,Y0,Y1]

Every image answers "what is missing, in which build" instead of showing a
noise field of absolute differences:

  OUT-anaglyph.png  R = native luminance, G = port luminance.
                    red  -> feature only in the native build
                    green-> feature only in the port build
                    yellow-> both agree
  OUT-missing.png   dimmed native base; red where native is lit and the port is
                    dark, green where the port is lit and native is dark.
  OUT-stretch.png   both builds stretched to their own min/max, stacked, so a
                    faint feature is readable in both instead of only brighter.
  OUT-crops.png     far / middle / near grid bands at 3x: native above port, then
                    the same bands with per-build stretch.
  OUT-rows.png      per scan-row line metrics as a printed table.
  OUT-stats.png     region measurements (light-cycle colour, rim band offset).

Requires numpy + PIL. Font: DejaVuSans from the local nativeprefix.
"""

import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

FONT = "/home/agent/agtk/nativeprefix/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"


def font(pt):
    try:
        return ImageFont.truetype(FONT, pt)
    except OSError:
        return ImageFont.load_default()


def lum(a):
    return (0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2])


def argb(a):
    return (int(a[0]), int(a[1]), int(a[2]))


def label(img, lines, size=17):
    """Prepend a black text band on top of an RGB array."""
    im = Image.fromarray(img.astype(np.uint8))
    f = font(size)
    pad = 8 + 22 * len(lines)
    out = Image.new("RGB", (im.width, im.height + pad), (0, 0, 0))
    d = ImageDraw.Draw(out)
    y = 6
    for ln in lines:
        d.text((10, y), ln, fill=(235, 235, 235), font=f)
        y += 22
    out.paste(im, (0, pad))
    return np.asarray(out)


def stack(titles, arrays, gap=10):
    w = max(a.shape[1] for a in arrays)
    h = sum(a.shape[0] for a in arrays) + gap * (len(arrays) - 1)
    out = np.zeros((h, w, 3), np.uint8)
    y = 0
    for a in arrays:
        out[y:y + a.shape[0], :a.shape[1]] = a
        y += a.shape[0] + gap
    rows = []
    for t, a in zip(titles, arrays):
        rows.append(label(a, [t]))
    return stack_raw(rows, gap)


def stack_raw(labeled, gap=10):
    w = max(a.shape[1] for a in labeled)
    h = sum(a.shape[0] for a in labeled) + gap * (len(labeled) - 1)
    out = np.zeros((h, w, 3), np.uint8)
    y = 0
    for a in labeled:
        out[y:y + a.shape[0], :a.shape[1]] = a
        y += a.shape[0] + gap
    return out


def stretch(a, lo=None, hi=None):
    l = lum(a)
    lo = np.percentile(l, 1) if lo is None else lo
    hi = np.percentile(l, 99.8) if hi is None else hi
    if hi <= lo:
        hi = lo + 1
    s = np.clip((l - lo) * 255.0 / (hi - lo), 0, 255).astype(np.uint8)
    return np.dstack([s, s, s])


def row_metrics(l, y0, y1, step):
    """Per scan row: number of line runs, their mean peak and mean half-max width."""
    rows = []
    for y in range(y0, y1 + 1, step):
        seg = l[y]
        base = np.median(seg)
        th = base + 8
        mask = seg > th
        # run boundaries
        d = np.diff(mask.astype(np.int8))
        starts = np.flatnonzero(d == 1) + 1
        ends = np.flatnonzero(d == -1) + 1
        if mask[0]:
            starts = np.r_[0, starts]
        if mask[-1]:
            ends = np.r_[ends, len(seg)]
        n = min(len(starts), len(ends))
        if n == 0:
            rows.append((y, 0, 0.0, 0.0))
            continue
        peaks, widths = [], []
        for s, e in zip(starts[:n], ends[:n]):
            run = seg[s:e]
            pk = run.max()
            half = base + 0.5 * (pk - base)
            widths.append(float(np.count_nonzero(run >= half)))
            peaks.append(float(pk - base))
        rows.append((y, n, float(np.mean(peaks)), float(np.mean(widths))))
    return rows


def main():
    args, opts = [], {}
    it = iter(sys.argv[1:])
    for a in it:
        if a.startswith("--"):
            k, sep, v = a[2:].partition("=")
            opts[k] = v if sep else next(it, "")
        else:
            args.append(a)
    nat_p, port_p, pre = args[0], args[1], args[2]

    nat = np.asarray(Image.open(nat_p).convert("RGB"))
    port = np.asarray(Image.open(port_p).convert("RGB"))
    if "port-crop" in opts:
        x, y, w, h = (int(v) for v in opts["port-crop"].split(","))
        port = port[y:y + h, x:x + w]
    if "crop" in opts:
        x, y, w, h = (int(v) for v in opts["crop"].split(","))
        nat = nat[y:y + h, x:x + w]

    h = min(nat.shape[0], port.shape[0])
    w = min(nat.shape[1], port.shape[1])
    nat, port = nat[:h, :w], port[:h, :w]
    top = int(opts.get("band-top", 0))          # skip page-header overlap band
    nat, port = nat[top:], port[top:]

    ln, lp = lum(nat), lum(port)

    # 1) anaglyph: what exists in which build
    an = np.dstack([ln.astype(np.uint8), lp.astype(np.uint8),
                    np.zeros_like(ln, dtype=np.uint8)])
    n_only = int(np.count_nonzero((ln >= 90) & (lp < 40)))
    p_only = int(np.count_nonzero((lp >= 90) & (ln < 40)))
    Image.fromarray(label(an, [
        f"ANAGLYPH  red = native only ({n_only} px)   green = port only ({p_only} px)"
        "   yellow = agree"]).astype(np.uint8)).save(pre + "-anaglyph.png")

    # 2) where is something missing, over a dimmed base
    base = (0.45 * np.dstack([ln, ln, ln])).astype(np.uint8)
    miss_n = (ln >= 90) & (lp < 0.45 * 90)
    miss_p = (lp >= 90) & (ln < 0.45 * 90)
    m = base.copy()
    m[miss_n] = (235, 40, 40)
    m[miss_p] = (40, 215, 40)
    Image.fromarray(label(m, [
        f"MISSING WHERE  red = lit in native, dark in port ({int(miss_n.sum())} px)"
        f"   green = lit in port, dark in native ({int(miss_p.sum())} px)"]).astype(np.uint8)
    ).save(pre + "-missing.png")

    # 3) independent contrast stretch: faint features in both
    sn, sp = stretch(nat), stretch(port)
    Image.fromarray(stack(["native stretched to its own min/max",
                           "port stretched to its own min/max"], [sn, sp])).save(
        pre + "-stretch.png")

    # 4) grid bands: far / middle / near, 3x, native above port
    def band(key, default):
        a, b = opts.get(key, default).split(",")
        return int(a), int(b)
    bands = [band("far", "380,420"), band("mid", "500,540"),
             band("near", "660,700")]
    x0 = int(opts.get("bandx", "300"))
    x1 = int(opts.get("bandx2", "980"))
    groups = []
    for (y0, y1) in bands:
        cn = nat[y0:y1, x0:x1]
        cp = port[y0:y1, x0:x1]
        f = 3
        cn3 = np.repeat(np.repeat(cn, f, 0), f, 1)
        cp3 = np.repeat(np.repeat(cp, f, 0), f, 1)
        sn3 = np.repeat(np.repeat(stretch(cn), f, 0), f, 1)
        sp3 = np.repeat(np.repeat(stretch(cp), f, 0), f, 1)
        groups.append(stack_raw([
            label(cn3, [f"rows {y0}-{y1}  native", "  plain"]),
            label(cp3, ["  port", "  plain"]),
            label(sn3, ["  native contrast-stretched"]),
            label(sp3, ["  port contrast-stretched"]),
        ], gap=6))
    Image.fromarray(stack_raw(groups, gap=24)).save(pre + "-crops.png")

    # 5) per-row line metrics table
    r0 = int(opts.get("rows", "380"))
    r1 = int(opts.get("rows2", "720"))
    step = int(opts.get("rowstep", "40"))
    mn = row_metrics(ln, r0, r1, step)
    mp = row_metrics(lp, r0, r1, step)
    txt = [f"row   native lines/peak/width      port lines/peak/width"]
    for (y, n1, p1, w1), (_, n2, p2, w2) in zip(mn, mp):
        txt.append(f"{y:5d}   {n1:4d} / {p1:5.1f} / {w1:4.2f}"
                   f"            {n2:4d} / {p2:5.1f} / {w2:4.2f}")
    canvas = np.zeros((28 + 26 * len(txt), 760, 3), np.uint8)
    im = Image.fromarray(canvas)
    d = ImageDraw.Draw(im)
    y = 6
    for t in txt:
        d.text((12, y), t, fill=(235, 235, 235), font=font(17))
        y += 26
    Image.fromarray(np.asarray(im)).save(pre + "-rows.png")

    # 6) region measurements: light cycle body + rim band difference
    if "cycle" in opts:
        cx0, cx1, cy0, cy1 = (int(v) for v in opts["cycle"].split(","))
        def cyc(a):
            r = a[cy0:cy1, cx0:cx1]
            g = r[..., 1].astype(int)
            lit = (g > 90) & (g > r[..., 0].astype(int) + 30) & (g > r[..., 2].astype(int) + 30)
            n = int(lit.sum())
            mean = r[lit].mean(axis=0) if n else (0, 0, 0)
            return n, mean, int(r[..., 1].max())
        n1, m1, mx1 = cyc(nat)
        n2, m2, mx2 = cyc(port)
        cn = np.repeat(np.repeat(nat[cy0:cy1, cx0:cx1], 2, 0), 2, 1)
        cp = np.repeat(np.repeat(port[cy0:cy1, cx0:cx1], 2, 0), 2, 1)
        an = np.dstack([lum(nat[cy0:cy1, cx0:cx1]).astype(np.uint8),
                        lum(port[cy0:cy1, cx0:cx1]).astype(np.uint8),
                        np.zeros((cy1 - cy0, cx1 - cx0), np.uint8)])
        rows = [label(cn, [f"light cycle  native: {n1} lit px, mean RGB "
                           f"({m1[0]:.0f},{m1[1]:.0f},{m1[2]:.0f}), max G {mx1}"]),
                label(cp, [f"light cycle  port:   {n2} lit px, mean RGB "
                           f"({m2[0]:.0f},{m2[1]:.0f},{m2[2]:.0f}), max G {mx2}"]),
                label(an, ["anaglyph of the same region"])]
        Image.fromarray(stack_raw(rows, gap=12)).save(pre + "-cycle.png")
        print(f"  cycle region ({cx0},{cy0})-({cx1},{cy1}): native {n1} lit px mean "
              f"({m1[0]:.0f},{m1[1]:.0f},{m1[2]:.0f}) max G {mx1} | port {n2} lit px mean "
              f"({m2[0]:.0f},{m2[1]:.0f},{m2[2]:.0f}) max G {mx2}")

    rim = slice(int(opts.get("rim0", 60)), int(opts.get("rim1", 340)))
    dmean = float(np.abs(ln[rim] - lp[rim]).mean())
    dbig = int(np.count_nonzero(np.abs(ln[rim] - lp[rim]) > 32))
    print(f"{pre}: {w}x{h} (top band {top} skipped)")
    print(f"  anaglyph-only pixels: native {n_only}, port {p_only}")
    print(f"  missing masks: native-lit/port-dark {int(miss_n.sum())}, "
          f"port-lit/native-dark {int(miss_p.sum())}")
    print(f"  rim rows {rim.start}-{rim.stop}: mean |diff| {dmean:.2f}, px>32 {dbig}")
    for (y, a1, b1, c1), (_, a2, b2, c2) in zip(mn, mp):
        print(f"  row {y}: native {a1}/{b1:.1f}/{c1:.2f}  port {a2}/{b2:.1f}/{c2:.2f}")


if __name__ == "__main__":
    main()
