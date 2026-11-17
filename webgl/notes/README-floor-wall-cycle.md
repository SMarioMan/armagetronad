# Floor grid, walls and light cycle — before / after on one identical frame

All images here come from the **same recorded game** (`perp2`/`perp3` archives)
played back on three builds, so a pair like `n770-native.png` and
`n770-port-after.png` shows the *same rendered world state*, not a similar one:

| capture | build | frame stamp |
|---|---|---|
| `n770-native.png` | desktop client, X11/GLX software (llvmpipe) | `n770 gt 9.3292 pos 610.0000 457.2097 h 16.7495 dir 0,1` |
| `n770-port-after.png` | wasm/WebGL, commit `055491d9` (live now) | `n770 gt 9.3298 pos 610.0000 457.2276 h 16.7495 dir 0,1` |
| `n770-port-before.png` | wasm/WebGL, previous commit `84aa6c88` (what was live before) | `n770 gt 9.3298 pos 610.0000 457.2276 h 16.7495 dir 0,1` |

Camera pose agrees to 0.018 world units; the frame counter is the same, so
anything that differs in the pixels is a rendering difference and nothing else.

## Sheets

* `threeway-n770.png` — full frames plus four zoomed crops (far wall band,
  far floor grid, near floor grid, light cycle), native | port-after |
  port-before, in that left-to-right order in every strip.
* `wall-after_bandpair.png` — box x820-1280 y150-300, zoom 4: native (left)
  vs port-after (right).
* `wall-before2_bandpair.png` — same box: native (left) vs port-before (right).

## Measured, floor grid (`harness/gridmetric.py`, rows are pixel rows of the
1280x800 frame; "lines" = bright crossings along the row, "width" = mean lit
run width in px, "contrast" = peak above the row median in units of the row's
own sigma, so exposure cannot inflate it)

| row | native lines / width / contrast | port after | port before |
|---|---|---|---|
| 410 | 60 / 2.71 / 3.29 | 60 / 2.69 / 3.26 | 67 / 2.63 / 3.00 |
| 440 | 55 / 2.66 / 3.53 | 55 / 2.70 / 3.50 | 66 / 2.70 / 2.76 |
| 470 | 0 / – (occluded) | 49 / 2.65 / 3.74 | 64 / 2.96 / 2.78 |
| 510 | 46 / 2.24 / 3.58 | 5 / 3.96 / 3.55 | 4 / 12.29 / 3.94 |
| 560 | 43 / 2.22 / 3.61 | 6 / 2.79 / 3.08 | 2 / 35.50 / 6.19 |
| 620 | 4 / 1.74 / 7.81 | 5 / 14.75 / 6.79 | 4 / 29.50 / 8.13 |
| 700 | 19 / 1.83 / 2.21 | 21 / 1.83 / 2.08 | 1 / 1.00 / 24.45 |
| 760 | 17 / 2.81 / 4.93 | 17 / 3.00 / 4.88 | 16 / 4.69 / 5.30 |

What that says:

* The mid floor (rows 410-440) is now line-for-line identical to the desktop:
  60 against 60 crossings, 2.69 against 2.71 px line width. Before the change
  the port counted 67 and 66 there — a different number of lines at all.
* Far floor line **thickness** is fixed: at row 560 the desktop measures
  2.22 px, the port now 2.79 px, while the old build smeared the same line to
  35.5 px (row 510: 2.24 against 3.96, old build 12.3). That is the mip-chain
  repair: the old software mip builder wrote one correct texel per row and read
  past the end of one-texel-tall levels, so `floor_b.png` (256x1) reached the
  sampler as noise below the first level.
* Far floor line **density is still wrong**: rows 510-560 count 43-46 crossings
  on the desktop against 5-6 on the port. The autocorrelation period in that
  band is 4 px on the desktop and 37 px on the port, i.e. the far grid is drawn
  roughly 9x too large rather than merely blurred. Open item; it is a texture
  coordinate/scale problem on the infinite floor plane, not a filter problem.

## Measured, far wall band (box x820-1280 y150-300, yellow-body rule: r>=80,
g>=80, b<=0.75*min(r,g))

| | native | port after | port before |
|---|---|---|---|
| columns with wall | 460 / 460 | 451 / 460 (9 empty at the left edge) | 460 / 460 |
| mean thickness | 11.99 px | 10.89 px | **1.69 px** |
| top edge y (min/max/mean) | 255/256/255.0 | 239/242/239.1 | 249/250/249.0 |

The old build drew that wall as a 1.7 px thread where the desktop paints an
11-12 px band — that is the "lines are 1 px thin and hot" symptom, and routing
the wall quads through client arrays took it to 10.9 px. The band now sits
16 px too high and 9 columns at the left of the box carry no wall at all, and
the hatch on it is far coarser than the desktop's (visible in both bandpair
PNGs), which is the same 9x scale error as the far floor grid.

## Measured, light cycle body (yellow pixel share and mean RGB)

| | native | port after | port before |
|---|---|---|---|
| yellow px | 0.22 % | 0.39 % | 0.31 % |
| mean RGB | 168.1, 168.1, 32.5 | 163.0, 163.1, 15.5 | 177.9, 178.3, 13.1 |
| lum>128 share | 1.22 % | 1.43 % | 1.01 % |

The body is still too large a yellow area (0.39 % against 0.22 %) and its blue
channel is still roughly half the desktop's. The specular gate took mean red
from 178 down to 163 (desktop 168) — the blow-out is gone, the remaining
difference is geometry plus the additive wedge around the body that is tracked
separately.

## Not fixed, and where it goes next

1. Far floor grid spacing 37 px against the desktop's 4 px (factor ~9) and the
   matching coarse hatch on far walls: one cause, the texture-coordinate scale
   of the infinite floor plane and of the wall quads under GL emulation.
2. Nine wall columns with no wall at the left edge of the measured box, and the
   band 16 px too high.
3. The page's console reports 58 `GLFIX … error 0x500` lines on `drawArrays
   (mode 0x6, count 6)` and `drawElements (mode 0x4, count 6)` in both builds —
   a handful of six-vertex draws are rejected by WebGL; the same count in the
   before build means it predates the client-array change.
