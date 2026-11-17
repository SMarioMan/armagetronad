# Sheet 49 - the perimeter replay, desktop and port at the same pose

Answers the two named divergences - "the grid lines fade at a distance on
the WebGL port while being wider and better preserved in the native build"
and "the light cycle shading is completely different" - with both builds
replaying the same recording and screenshots taken at the same world state.

## How the frames are matched

One recording, `perp2.aarec`: the local cycle runs the arena perimeter, 10
AI opponents, `MAX_FPS 60` so the same wall-clock run costs the port half
as many frames. The desktop build plays it under Xvfb with llvmpipe
(`webgl/harness/natpb.sh`); this build plays it in headless Chrome
(`webgl/harness/cdpd.py`), which freezes the wasm main loop and advances it
one frame at a time until the stamped camera pose reaches a waypoint, so
the screenshot and the stamp describe one frame on both sides. Every frame
carries a stamp on the desktop's stderr and in the page console:
`PF FRAME <n> state 6 gt <game time> pos <x> <y> h <h> dir <dx> <dy>`.

| pair | desktop gt / frame | port gt / frame | camera pose delta (world units) |
|---|---|---|---|
| 1 | 2.4792 / 342 | 2.4637 / 341 | 0.000 / 0.009 |
| 2 | 4.3674 / 453 | 4.3519 / 452 | 0.003 / 0.033 |
| 3 | 5.4920 / 519 | 5.5099 / 520 | 0.034 / 0.173 |

The early pairs are gated on the game clock; the last one is gated on the
camera position, because this port's playback of the same recording drifts
away from the desktop's: the stamped camera pose agrees to 0.01 world
units at gt 2.5 and gt 4.4, to 0.18 at gt 5.5, and is 7.7 units off by
gt 7.8 - past that the two runs are no longer in the same arena, so a
game-clock gate there compares different worlds. That is why this sheet
stops at gt 5.5, and it is also why nothing made of per-frame sparks lines
up between the two runs.

Side by side: `49-pair1.png` `49-pair2.png` `49-pair3.png` (full size), `49-all.png` (all stacked). The originals are
`49-Na-native-gt*.png` and `49-Nb-port-gt*.png`, listed with their stamps in
`49-g-native-shots.txt` and `49-g-port-shots.txt`.

## 1. The floor grid

`webgl/harness/gridprofile.py`: per depth band (0 = nearest floor, 3 = far,
toward the horizon) how many grid lines a scanline crosses, how wide a line
is, how far its brightness sits above the floor, and how bright the floor
is. Threshold +24 over a 15 px median baseline, floor rows 0.55-0.98 of
image height.

### pair 1 - gt 2.4792 desktop, gt 2.4637 port

| band | lines (desk/port) | line px % (desk/port) | width px (desk/port) | peak+ (desk/port) | floor (desk/port) |
|---|---|---|---|---|---|
| 0 | 8.0 / 6.0 | 1.1 / 0.8 | 1.52 / 1.49 | 99.0 / 107.4 | 0.0 / 0.0 |
| 1 | 11.2 / 7.8 | 1.7 / 1.3 | 1.99 / 2.08 | 137.2 / 138.0 | 0.0 / 0.0 |
| 2 | 19.8 / 16.8 | 5.3 / 4.7 | 2.98 / 2.87 | 182.4 / 182.2 | 1.7 / 2.3 |
| 3 | 22.5 / 22.2 | 4.2 / 4.2 | 2.55 / 2.40 | 175.7 / 185.7 | 15.7 / 15.7 |

### pair 2 - gt 4.3674 desktop, gt 4.3519 port

| band | lines (desk/port) | line px % (desk/port) | width px (desk/port) | peak+ (desk/port) | floor (desk/port) |
|---|---|---|---|---|---|
| 0 | 3.8 / 3.8 | 0.9 / 0.9 | 3.06 / 3.13 | 102.5 / 100.8 | 0.0 / 0.0 |
| 1 | 4.7 / 4.7 | 1.8 / 1.8 | 3.79 / 3.95 | 125.8 / 125.7 | 0.0 / 0.0 |
| 2 | 9.2 / 8.7 | 3.5 / 3.5 | 3.87 / 4.06 | 144.6 / 142.8 | 0.3 / 0.4 |
| 3 | 4.3 / 4.8 | 0.7 / 0.7 | 1.99 / 2.02 | 126.5 / 127.5 | 2.5 / 2.8 |

### pair 3 - gt 5.4920 desktop, gt 5.5099 port

| band | lines (desk/port) | line px % (desk/port) | width px (desk/port) | peak+ (desk/port) | floor (desk/port) |
|---|---|---|---|---|---|
| 0 | 2.7 / 3.3 | 0.6 / 0.6 | 3.31 / 2.07 | 92.6 / 108.0 | 0.0 / 0.0 |
| 1 | 5.3 / 8.0 | 1.9 / 2.2 | 3.63 / 2.72 | 78.3 / 69.7 | 5.9 / 5.7 |
| 2 | 9.3 / 11.2 | 3.4 / 3.6 | 3.36 / 3.06 | 101.6 / 104.2 | 16.0 / 16.9 |
| 3 | 5.3 / 6.8 | 1.5 / 1.5 | 3.58 / 2.63 | 86.6 / 88.5 | 6.6 / 7.7 |

### How much of that is the camera, not the renderer

One build alone, the desktop, gated at five game times 0.05 s apart
(`49-sens-1.png` .. `49-sens-5.png`, the camera moving ~19 world units a
second along the wall):

| frame | band 0 width px | band 0 peak+ | band 1 lines | band 3 width px |
|---|---|---|---|---|
| 49-sens-1.png | 2.37 | 93.4 | 6.0 | 2.31 |
| 49-sens-2.png | 2.48 | 94.4 | 5.5 | 3.11 |
| 49-sens-3.png | 3.31 | 92.6 | 5.3 | 3.58 |
| 49-sens-4.png | 2.31 | 74.5 | 5.8 | 3.45 |
| 49-sens-5.png | 1.33 | 82.2 | 5.8 | 3.58 |

A 0.05 s difference in the gate is therefore worth more than a factor of
two in reported line width. That is why the band tables can only be read
as agreement, not as proof: the pairs at 0.01 world units of pose
difference agree closely, and the pair at 0.18 sits inside the spread a
single build produces by itself.

### Where the lines sit, pixel for pixel

Position is a much sharper test than any average: it asks whether the same
grid line lands on the same pixel in both builds.

| pair | scan rows | desktop runs | port runs | paired within 1 px | paired within 3 px |
|---|---|---|---|---|---|
| 1 | 560-720 | 90 | 74 | 49 | 50 |
| 2 | 560-720 | 34 | 34 | 32 | 34 |
| 3 | 560-720 | 35 | 44 | 24 | 24 |

Read that as a lower bound. The scanlines cross the thin magenta debris
streaks and the cycle sparks as well, and those are per-frame particles that
the two simulations do not place identically, so an unpaired run is normally
a streak rather than a lost grid line; `49-N-report-missing.png` says which,
and on all three pairs its marks sit on streaks, sparks, text and a sub-pixel
offset of the far wall - never on a grid line. Pair 2, cameras 0.003 world
units apart, pairs 32 of 34 runs on the same pixel. Pair 3 is gated 0.18
units apart, which at a grazing floor angle displaces every line by more
than the 1 px tolerance, so only the 3 px column means anything there.

What fixed the grid in the first place is recorded in
`webgl/patch_texture_projective_coords.py`: the floor plane carries 4-component
texture coordinates whose `q` component the generated shader ignored, so the
grid degenerated to an affine map - period 34 px against the desktop's 6 px,
line peak 32 against 86 - and the far floor either dissolved or, with a naive
divide, kept sampling full base-level detail all the way to the horizon.
`texture2DProj` restores the projective divide and the hardware level-of
detail rule together.

## 2. The light cycle

`webgl/harness/parity_report.py --cycle=x0,x1,y0,y1,any` counts the lit
pixels inside a box on the local cycle body and reports their mean colour;
`any` classifies by luminance because on this replay the cycle is blue, not
the green of the earlier published pairs. Only pairs with a clean, unobstructed
cycle body are measured.

- **pair 3**: `cycle region (750,465)-(850,520): native 818 lit px mean (140,140,140) peak 178 | port 1010 lit px mean (138,138,138) peak 178`

The body geometry, its team colour and the two lights the game hangs next to
it (`src/tron/gCycle.cpp:4333-4346`, both with `w = 0`, i.e. directional) are
reproduced with the same uniforms on both builds; the shading defect that made
cycles near-black on the port was the GL emulation substituting a stale
generic vertex colour into the material terms of model draws, documented and
fixed in `webgl/patch_light_colormaterial.py`.

## 3. Whole-frame difference

| pair | anaglyph-only pixels (native / port) | rim rows 60-340 |
|---|---|---|
| 1 | native 13721, port 13982 | mean diff 3.83, px>32 8899 |
| 2 | native 2471, port 3025 | mean diff 3.23, px>32 10358 |
| 3 | native 1459, port 1104 | mean diff 10.10, px>32 19982 |

The `missing` images (`49-N-report-missing.png`) and the anaglyphs
(`49-N-report-anaglyph.png`) are the visual form of the same counts: red is
lit on the desktop and dark on the port, green the other way round, yellow
agreement.

## Caveats that matter when you read these numbers

- Both sides are software rasterizers here: llvmpipe for the desktop run,
  Chrome's SwiftShader for the port. Neither is your GPU. The context
  attributes are deliberately matched (`webgl/site/index.html`: no MSAA,
  anisotropy left at 1.0, because the desktop build reports exactly that),
  so a difference that only shows up on hardware will not appear in this
  sheet. The page prints its own context line - `multisample ... anisotropy
  up to ...` - at startup; if your browser's floor looks different from both
  images, that line is the first thing worth comparing.
- The deployed build and the build measured here differ by the capture stamp
  probe (the published tree does not carry it); rendering code is identical.
- Spark and debris particles are simulated per frame and do not survive the
  pose drift between the two playback runs, so any metric that counts bright
  runs on a scanline is contaminated by them (see the `lines` column above).
- The page HTML is served with `cache-control: max-age=600`; check the
  deployed artifact with
  `curl -sSI https://smarioman.github.io/armagetronad/armagetronad_main.wasm | grep -iE 'last-modified|content-length'`
  and hard-reload before comparing.

_Generated by `tools/mkreadme49.py` from `49-g-native-shots.txt 49-g-port-shots.txt` on 2026-11-17T20:06Z._

