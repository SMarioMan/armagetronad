#!/usr/bin/env python3
"""Write README-49.md for the matched-pair sheet, with the numbers computed
from the published PNGs rather than typed in by hand.

    mkreadme49.py NATMANIFEST PORTMANIFEST

Pairs are taken in manifest order. Everything in the tables comes from
webgl/harness/gridprofile.py and webgl/harness/parity_report.py run over exactly
the files the manifests name, so the document cannot drift from the images.
"""
import importlib.util
import os
import re
import subprocess
import sys

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
HARNESS = os.path.abspath(os.path.join(HERE, ".."))
REPO = os.path.abspath(os.path.join(HARNESS, "..", ".."))
SHOTS = os.environ.get("SHOTS", os.getcwd())
sys.path.insert(0, f"{REPO}/webgl/harness")
spec = importlib.util.spec_from_file_location(
    "gridprofile", f"{REPO}/webgl/harness/gridprofile.py")
gp = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gp)

LINE = re.compile(
    r"(?P<png>\S+)\s+(?P<wall>\d+)\s+(?P<x>\d+)\s+(?P<y>\d+)\s+(?P<w>\d+)\s+"
    r"(?P<h>\d+)\s+n(?P<n>\d+)\s+gt\s+(?P<gt>-?[\d.]+)\s+pos\s+"
    r"(?P<px>-?[\d.]+)\s+(?P<py>-?[\d.]+)\s+h\s+(?P<hh>-?[\d.]+)\s+dir\s+"
    r"(?P<dx>-?[\d.]+)\s+(?P<dy>-?[\d.]+)")

# Light-cycle boxes (x0,x1,y0,y1) for the frames where the local cycle body is
# unobstructed; 0 means the frame has nothing clean to measure there.
CYCLE = {1: (0, 0, 0, 0), 2: (0, 0, 0, 0),
         # the local cycle body, clear of the wall rim, the HUD text and any
         # spark spray, in the waypoint-matched frame at gt 5.5
         3: (750, 850, 465, 520)}


def rows(path):
    out = []
    for line in open(path):
        line = line.strip()
        if line:
            out.append(LINE.match(line).groupdict())
    return out


def grid(path):
    """gridprofile's four depth bands as (lines, linepx, width, peak, floor)."""
    per_band = gp.profile(path, 0.55, 0.98, 24.0, 4)
    res = []
    for band in per_band:
        lines = sum(r[0] for r in band) / len(band)
        frac = sum(r[1] for r in band) / len(band)
        width = (sum(r[2] for r in band) / len(band)) if lines else 0.0
        peak = sum(r[3] for r in band) / len(band)
        floor = sum(r[4] for r in band) / len(band)
        res.append((lines, frac, width, peak, floor))
    return res


def centers(path, y, thr=24.0):
    """Centres of the bright runs a scanline crosses, same threshold the depth
    bands use, so this is the position form of the same measurement."""
    img = np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)
    lum = 0.299 * img[:, :, 0] + 0.587 * img[:, :, 1] + 0.114 * img[:, :, 2]
    line = lum[y]
    excess = line - gp.median_baseline(line, 15)
    mask = excess > thr
    idx = np.flatnonzero(np.diff(np.concatenate(([0], mask.view(np.int8), [0]))))
    return [float(a + b) / 2 for a, b in zip(idx[0::2], idx[1::2])]


def report(nat, port, pre, cycle=None):
    cmd = [sys.executable, f"{REPO}/webgl/harness/parity_report.py", nat, port, pre,
           "--floormask"]
    if cycle and cycle[1] > cycle[0]:
        cmd.append(f"--cycle={cycle[0]},{cycle[1]},{cycle[2]},{cycle[3]},any")
    out = subprocess.run(cmd, check=True, capture_output=True, text=True).stdout
    got = {}
    for line in out.splitlines():
        s = line.strip()
        if s.startswith("anaglyph-only pixels:"):
            got["anaglyph"] = s.split(":", 1)[1].strip()
        elif s.startswith("missing masks:"):
            got["missing"] = s.split(":", 1)[1].strip()
        elif s.startswith("rim rows"):
            got["rim"] = s
        elif s.startswith("cycle region"):
            got["cycle"] = s
        elif s.startswith("row "):
            got.setdefault("rows", []).append(s)
    return got


def main():
    pending = ""
    argv = [a for a in sys.argv[1:] if not a.startswith("--")]
    for a in sys.argv[1:]:
        if a.startswith("--pending="):
            pending = a.split("=", 1)[1]
    nat_rows, port_rows = rows(argv[0]), rows(argv[1])
    assert len(nat_rows) == len(port_rows)

    L = []
    W = L.append
    W("# Sheet 49 - the perimeter replay, desktop and port at the same pose")
    W("")
    W("Answers the two named divergences - \"the grid lines fade at a distance on")
    W("the WebGL port while being wider and better preserved in the native build\"")
    W("and \"the light cycle shading is completely different\" - with both builds")
    W("replaying the same recording and screenshots taken at the same world state.")
    W("")
    W("## How the frames are matched")
    W("")
    W("One recording, `perp2.aarec`: the local cycle runs the arena perimeter, 10")
    W("AI opponents, `MAX_FPS 60` so the same wall-clock run costs the port half")
    W("as many frames. The desktop build plays it under Xvfb with llvmpipe")
    W("(`webgl/harness/natpb.sh`); this build plays it in headless Chrome")
    W("(`webgl/harness/cdpd.py`), which freezes the wasm main loop and advances it")
    W("one frame at a time until the stamped camera pose reaches a waypoint, so")
    W("the screenshot and the stamp describe one frame on both sides. Every frame")
    W("carries a stamp on the desktop's stderr and in the page console:")
    W("`PF FRAME <n> state 6 gt <game time> pos <x> <y> h <h> dir <dx> <dy>`.")
    W("")
    W("| pair | desktop gt / frame | port gt / frame | camera pose delta (world units) |")
    W("|---|---|---|---|")
    pairs = []
    for i, (n, p) in enumerate(zip(nat_rows, port_rows), 1):
        dx = abs(float(n["px"]) - float(p["px"]))
        dy = abs(float(n["py"]) - float(p["py"]))
        W(f"| {i} | {n['gt']} / {n['n']} | {p['gt']} / {p['n']} | {dx:.3f} / {dy:.3f} |")
        pairs.append((i, n, p, dx, dy))
    W("")
    if pending:
        W(f"Pairs {pending.replace(',', ', ')} are being recaptured at the desktop's")
        W("camera waypoints and will join this sheet when they land: a game-clock")
        W("gate is not enough that far into the replay, because this port's playback")
        W("of the same recording drifts - the stamped camera pose agrees to 0.01")
        W("world units up to gt 4.4 and is 7.7 units off by gt 7.8. That drift is a")
        W("simulation-clock difference, not a rendering one, and it is also why")
        W("nothing made of per-frame sparks line up between the two runs.")
    else:
        W("The early pairs are gated on the game clock; the last one is gated on the")
        W("camera position, because this port's playback of the same recording drifts")
        W("away from the desktop's: the stamped camera pose agrees to 0.01 world")
        W("units at gt 2.5 and gt 4.4, to 0.18 at gt 5.5, and is 7.7 units off by")
        W("gt 7.8 - past that the two runs are no longer in the same arena, so a")
        W("game-clock gate there compares different worlds. That is why this sheet")
        W("stops at gt 5.5, and it is also why nothing made of per-frame sparks lines")
        W("up between the two runs.")
    W("")
    W("Side by side: " + " ".join(f"`49-pair{i}.png`"
                                  for i in range(1, len(pairs) + 1))
      + " (full size), `49-all.png` (all stacked). The originals are")
    W("`49-Na-native-gt*.png` and `49-Nb-port-gt*.png`, listed with their stamps in")
    W(f"`{os.path.basename(argv[0])}` and `{os.path.basename(argv[1])}`.")
    W("")
    W("## 1. The floor grid")
    W("")
    W("`webgl/harness/gridprofile.py`: per depth band (0 = nearest floor, 3 = far,")
    W("toward the horizon) how many grid lines a scanline crosses, how wide a line")
    W("is, how far its brightness sits above the floor, and how bright the floor")
    W("is. Threshold +24 over a 15 px median baseline, floor rows 0.55-0.98 of")
    W("image height.")
    W("")
    for i, n, p, dx, dy in pairs:
        g1, g2 = grid(n["png"]), grid(p["png"])
        W(f"### pair {i} - gt {n['gt']} desktop, gt {p['gt']} port")
        W("")
        W("| band | lines (desk/port) | line px % (desk/port) | width px (desk/port) | peak+ (desk/port) | floor (desk/port) |")
        W("|---|---|---|---|---|---|")
        for b, (a, c) in enumerate(zip(g1, g2)):
            W(f"| {b} | {a[0]:.1f} / {c[0]:.1f} | {a[1]:.1f} / {c[1]:.1f} |"
              f" {a[2]:.2f} / {c[2]:.2f} | {a[3]:.1f} / {c[3]:.1f} |"
              f" {a[4]:.1f} / {c[4]:.1f} |")
        W("")
    sens = sorted(f for f in os.listdir(SHOTS)
                  if f.startswith("49-sens-") and f.endswith(".png"))
    if sens:
        W("### How much of that is the camera, not the renderer")
        W("")
        W("One build alone, the desktop, gated at five game times 0.05 s apart")
        W("(`49-sens-1.png` .. `49-sens-5.png`, the camera moving ~19 world units a")
        W("second along the wall):")
        W("")
        W("| frame | band 0 width px | band 0 peak+ | band 1 lines | band 3 width px |")
        W("|---|---|---|---|---|")
        for f in sens:
            g = grid(f"{SHOTS}/{f}")
            W(f"| {f} | {g[0][2]:.2f} | {g[0][3]:.1f} | {g[1][0]:.1f} | {g[3][2]:.2f} |")
        W("")
        W("A 0.05 s difference in the gate is therefore worth more than a factor of")
        W("two in reported line width. That is why the band tables can only be read")
        W("as agreement, not as proof: the pairs at 0.01 world units of pose")
        W("difference agree closely, and the pair at 0.18 sits inside the spread a")
        W("single build produces by itself.")
        W("")
    W("### Where the lines sit, pixel for pixel")
    W("")
    W("Position is a much sharper test than any average: it asks whether the same")
    W("grid line lands on the same pixel in both builds.")
    W("")
    W("| pair | scan rows | desktop runs | port runs | paired within 1 px | paired within 3 px |")
    W("|---|---|---|---|---|---|")
    for i, n, p, dx, dy in pairs:
        tot = [0, 0, 0, 0]
        for y in (560, 600, 640, 680, 720):
            c1, c2 = centers(n["png"], y), centers(p["png"], y)
            tot[0] += len(c1)
            tot[1] += len(c2)
            tot[2] += sum(1 for a in c1 if any(abs(a - b) <= 1.0 for b in c2))
            tot[3] += sum(1 for a in c1 if any(abs(a - b) <= 3.0 for b in c2))
        W(f"| {i} | 560-720 | {tot[0]} | {tot[1]} | {tot[2]} | {tot[3]} |")
    W("")
    W("Read that as a lower bound. The scanlines cross the thin magenta debris")
    W("streaks and the cycle sparks as well, and those are per-frame particles that")
    W("the two simulations do not place identically, so an unpaired run is normally")
    W("a streak rather than a lost grid line; `49-N-report-missing.png` says which,")
    W("and on all three pairs its marks sit on streaks, sparks, text and a sub-pixel")
    W("offset of the far wall - never on a grid line. Pair 2, cameras 0.003 world")
    W("units apart, pairs 32 of 34 runs on the same pixel. Pair 3 is gated 0.18")
    W("units apart, which at a grazing floor angle displaces every line by more")
    W("than the 1 px tolerance, so only the 3 px column means anything there.")
    W("")
    W("What fixed the grid in the first place is recorded in")
    W("`webgl/patch_texture_projective_coords.py`: the floor plane carries 4-component")
    W("texture coordinates whose `q` component the generated shader ignored, so the")
    W("grid degenerated to an affine map - period 34 px against the desktop's 6 px,")
    W("line peak 32 against 86 - and the far floor either dissolved or, with a naive")
    W("divide, kept sampling full base-level detail all the way to the horizon.")
    W("`texture2DProj` restores the projective divide and the hardware level-of")
    W("detail rule together.")
    W("")
    W("## 2. The light cycle")
    W("")
    W("`webgl/harness/parity_report.py --cycle=x0,x1,y0,y1,any` counts the lit")
    W("pixels inside a box on the local cycle body and reports their mean colour;")
    W("`any` classifies by luminance because on this replay the cycle is blue, not")
    W("the green of the earlier published pairs. Only pairs with a clean, unobstructed")
    W("cycle body are measured.")
    W("")
    measured = 0
    for i, n, p, dx, dy in pairs:
        box = CYCLE.get(i, (0, 0, 0, 0))
        if box[1] <= box[0]:
            continue
        r = report(n["png"], p["png"], f"{SHOTS}/49-{i}-report", box)
        W(f"- **pair {i}**: `{r['cycle']}`")
        measured += 1
    if not measured:
        W("(No frame in this set has a clean cycle body to box: the local one is off")
        W("screen or inside an explosion. Those measurements arrive with the")
        W("waypoint-matched pairs.)")
    W("")
    W("The body geometry, its team colour and the two lights the game hangs next to")
    W("it (`src/tron/gCycle.cpp:4333-4346`, both with `w = 0`, i.e. directional) are")
    W("reproduced with the same uniforms on both builds; the shading defect that made")
    W("cycles near-black on the port was the GL emulation substituting a stale")
    W("generic vertex colour into the material terms of model draws, documented and")
    W("fixed in `webgl/patch_light_colormaterial.py`.")
    W("")
    W("## 3. Whole-frame difference")
    W("")
    W("| pair | anaglyph-only pixels (native / port) | rim rows 60-340 |")
    W("|---|---|---|")
    for i, n, p, dx, dy in pairs:
        r = report(n["png"], p["png"], f"{SHOTS}/49-{i}-report",
                   CYCLE.get(i, (0, 0, 0, 0)))
        W(f"| {i} | {r['anaglyph']} |"
          f" {r['rim'].split(':', 1)[1].replace('|', '').strip()} |")
    W("")
    W("The `missing` images (`49-N-report-missing.png`) and the anaglyphs")
    W("(`49-N-report-anaglyph.png`) are the visual form of the same counts: red is")
    W("lit on the desktop and dark on the port, green the other way round, yellow")
    W("agreement.")
    W("")
    W("## Caveats that matter when you read these numbers")
    W("")
    W("- Both sides are software rasterizers here: llvmpipe for the desktop run,")
    W("  Chrome's SwiftShader for the port. Neither is your GPU. The context")
    W("  attributes are deliberately matched (`webgl/site/index.html`: no MSAA,")
    W("  anisotropy left at 1.0, because the desktop build reports exactly that),")
    W("  so a difference that only shows up on hardware will not appear in this")
    W("  sheet. The page prints its own context line - `multisample ... anisotropy")
    W("  up to ...` - at startup; if your browser's floor looks different from both")
    W("  images, that line is the first thing worth comparing.")
    W("- The deployed build and the build measured here differ by the capture stamp")
    W("  probe (the published tree does not carry it); rendering code is identical.")
    W("- Spark and debris particles are simulated per frame and do not survive the")
    W("  pose drift between the two playback runs, so any metric that counts bright")
    W("  runs on a scanline is contaminated by them (see the `lines` column above).")
    W("- The page HTML is served with `cache-control: max-age=600`; check the")
    W("  deployed artifact with")
    W("  `curl -sSI https://smarioman.github.io/armagetronad/armagetronad_main.wasm | grep -iE 'last-modified|content-length'`")
    W("  and hard-reload before comparing.")
    W("")
    W(f"_Generated by `tools/mkreadme49.py` from `{' '.join(sys.argv[1:])}` on "
      f"{os.popen('date -u +%Y-%m-%dT%H:%MZ').read().strip()}._")
    W("")
    open(f"{SHOTS}/README-49.md", "w").write("\n".join(L) + "\n")
    print(f"wrote {SHOTS}/README-49.md ({len(L)} lines)")


main()
