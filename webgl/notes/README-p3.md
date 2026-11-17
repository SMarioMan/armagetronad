# Round p3 — frame-identical desktop vs port comparison (2026-11-13)

Files: `p3-n814-native.png`, `p3-n814-port.png`, `p3-n814-wallcrop.png`,
`p3-n814-cyclecrop.png`. The crops are the wall strip (box 300,300–1280,440,
3x, desktop left / port right) and the light-cycle body cluster.

## How these two frames were made identical

A new recording was made on the desktop build (Xvfb + llvmpipe, 1280x800,
`MAX_FPS 60`): `perp3.aarec`. Both builds replay that one file, driven only by
the inputs archived inside it, and each screenshot is taken with the game loop
frozen on the stamped frame whose game time is nearest the gate.

| | desktop | port |
|---|---|---|
| rendered frame counter | n814 | n814 |
| game time | 10.0631 | 10.0637 |
| camera position | 610.0000, 479.2253 | 610.0000, 479.2432 |
| camera height / direction | 16.7498, (0, 1) | 16.7498, (0, 1) |

Same frame number, same camera to within 0.02 world units: the two PNGs show
the same rendered frame, so every difference below is a rendering difference,
not a different moment of the game.

## Floor grid (per-depth bands, 0 = near, 3 = far)

`gridprofile.py`: line run width in px, and 95th-percentile line brightness
above the floor baseline.

| band | desktop width | port width | desktop peak+ | port peak+ | desktop crossings | port crossings |
|---|---|---|---|---|---|---|
| 0 near | 1.30 | 0.83 | 81.6 | 93.5 | 2.7 | 1.7 |
| 1 | 1.67 | 0.78 | 113.1 | 37.6 | 1.3 | 1.7 |
| 2 | 2.30 | 3.96 | 151.5 | 151.7 | 8.5 | 10.0 |
| 3 far | 1.67 | 2.20 | 73.0 | 57.1 | 0.7 | 16.0 |

Mid-distance line brightness now matches (151.5 vs 151.7). The far band went
the other way: the port resolves 16 line crossings where the desktop resolves
0.7, and the near lines are thinner than desktop (0.83 px vs 1.30 px). Band 1
is contaminated by the wall strip lying in that row range on both sides.

## Walls

Scanline along the wall strip, y = 380:

| x | 460 | 540 | 620 | 700 | 780 | 940 | 1180 |
|---|---|---|---|---|---|---|---|
| desktop | 83,83,43 | 100,100,22 | 102,102,20 | 103,103,21 | 102,102,20 | 102,102,20 | 102,102,20 |
| port | 30,60,30 | 3,7,3 | 250,250,50 | 220,220,44 | 0,0,0 | 0,0,0 | 0,0,0 |

Box y355–395, x460–1279: desktop mean RGB (46.9,46.8,10.9), 37.7% of pixels
above luminance 60 but only 3.6% above 120, bright-pixel mean 88.5. Port: mean
(46.2,48.7,11.2), 19.7% above 60, 18.8% above 120, bright-pixel mean 162.2 —
the same wall colour at roughly twice the brightness. Geometry also differs:
the top edge of the desktop's strip runs flat (368–378) across the box, the
port's runs from 407 up to 317, so the port's strip is a different quad set
(see `p3-n814-wallcrop.png`).

Far wall band at the top of the frame (y 20–60): desktop mean RGB
(33.8,38.0,73.2) with 23.7% of pixels above luminance 60; port mean
(2.7,3.5,8.3) with 0.0% — the distant wall is essentially not drawn on the
port.

## Light cycle body

Box y455–545, x575–761 (the camera is directly behind this cycle in both):

| | saturated yellow px | mean RGB of those px |
|---|---|---|
| desktop | 40 (0.2%) | 222,222,0 |
| port | 1977 (11.7%) | 202,206,2 |

The port draws a big additive wedge out of the bike that the desktop build does
not draw at all (`p3-n814-cyclecrop.png`).

## What is settled and what is open

Settled: the two builds replay the same archive under byte-identical config
(the same `autoexec.cfg` text is seeded into both) and with the same program
version string (`0.2.9.3_alpha_z3224_20261113`), so neither side flags
"probably desynced" playback; the desktop replay activates no desync at all.
Floor filtering is not a candidate: both builds report `GL_LINEAR_MIPMAP_LINEAR`
/ `GL_LINEAR`, `GL_CLAMP_TO_EDGE` and anisotropy 1.0 on the floor texture.

Open, and being chased with the numbers above:
1. Wall and cycle-body blow-out. The build under test had dropped the
   `patch_light_colormaterial.py` step (GL_COLOR_MATERIAL tracking: ambient and
   diffuse come from the vertex colour, as desktop GL does by default). It is
   re-enabled; the wall brightness and body-pixel counts above are the baseline
   it has to move.
2. The additive wedge around the cycle body, and the far wall being missing.
   Runtime gates (`AA_NO_BODY`, `AA_NO_WHEELS`, port-only, harness only) switch
   single meshes off so the wedge can be attributed to one draw without
   rebuilding.
3. Far-band grid density and near-band line width.
