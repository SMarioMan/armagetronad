# Web graphics parity census — 2026-11-15

Everything below was measured, not inferred. Sources are the capture manifests
(`*.aarec` replay on both builds, frozen at the same game time) and pixel
measurement scripts run over the PNGs in this folder.

## How the two sides were made comparable

One recording (`perp2.aarec`, 60 fps cap, 10 AI opponents) is replayed by both
builds. Both builds stamp every rendered frame
`HARNESS FRAME n gt <game time> pos x y h <height> dir dx dy`; the capture
drivers freeze the process (desktop `SIGSTOP`, browser a frame-step pause) at a
chosen game time and shoot, so image and stamp belong to the same frame. The
freezing removes the biggest source of noise: the browser needs 0.2–0.6 game
seconds to take a screenshot, during which the world moves.

Residual mismatch at a frozen gate is under one simulation tick (pose within
~0.5 world units), which is the floor of what replay matching can reach: the
rendered camera interpolates between simulation ticks, so the two builds can sit
at different sub-tick phases. Frame counts are *not* comparable across builds
(the counter starts at process start and the two render menus at very different
rates); only game time is.

## 1. Floor grid at the default settings — parity

`FLOOR_DETAIL 3` (two-texture floor) is the built-in default of both builds
(`src/render/rScreen.cpp:1062`, and the desktop first-run default set in
`sr_LoadDefaultConfig`).

Pair: desktop `f3-00.png` (game time 1.0223, camera 107.83/46.07 h 14.98) vs
port `pf3-00.png` (0.9896, 107.83/44.71 h 14.89). Measurement window x 700..1060
(pure floor), rows sampled every 15 px, per-row detection of local maxima over
the floor background.

| row | desktop lines | width px | contrast | port lines | width px | contrast |
|----:|--------------:|---------:|---------:|-----------:|---------:|---------:|
| 330 | 22 | 1.86 | 11.1 | 22 | 1.82 | 11.1 |
| 420 | 17 | 1.65 | 15.9 | 17 | 1.47 | 15.9 |
| 540 | 13 | 1.69 | 20.6 | 13 | 1.77 | 20.6 |
| 660 | 10 | 1.50 | 25.4 | 10 | 1.50 | 25.4 |

Over the whole floor band (rows 300–790): summed line contrast 570 (desktop)
vs 571 (port); mean lines per row 14.09 vs 14.03; mean line width 1.66 px vs
1.67 px. **Ratio 1.00 on every measure.**

## 2. Floor detail modes — this is where "wider" comes from

Same replay, three desktop runs with `FLOOR_DETAIL` set to 1, 2, 3
(`native_floor_modes.png`), plus matched port frames for modes 2 and 3
(`floor_mode_matrix.png`).

| mode | what it draws | lines/row | line width px | summed contrast |
|-----:|---------------|----------:|--------------:|----------------:|
| 1 Grid | geometry lines, one cell per arena grid square | 3.30 | 0.98 | 1546 |
| 2 Texture | one baked-grid texture | 12.91 | 2.68 | 1228 |
| 3 TwoTexture | two baked-grid textures, additive | 14.09 | 1.66 | 570 |

Mode 2's grid lines are **1.6× wider** than mode 3's (2.68 px vs 1.66 px) and
spread over fewer, brighter passes. If the desktop install is set to mode 2 and
the browser build runs the default 3, "the desktop lines are wider and better
preserved" is exactly what that difference looks like — no renderer divergence
needed. Mode 1 is the opposite case: sparse, thin, and *absent* beyond mid
distance.

Note for the browser side: the game reads settings in the order
`user.cfg` → `autoexec.cfg` → the recording's own config prefix
(`src/tools/tConfiguration.cpp:978-999`), and the browser profile persists its
`user.cfg` in IndexedDB. Attempts to change `FLOOR_DETAIL` by editing the file
served next to the page did **not** take effect on the port; the reliable way to
change it in the browser build is the in-game graphics menu (Floor detail:
grid / texture / 2-texture), which writes the persisted config.

## 3. Light-cycle body — no systematic dimming left

Own cycle, camera-locked boxes on three frozen pairs (browser frozen at the
desktop's game time; the body lands within 1 px of the desktop's box in every
pair). Sheets: `cyclebody_g77.86.png`, `cyclebody_g83.99.png`,
`cyclebody_g86.24.png` (8× zoom, desktop left / browser right).

| pair | lit px > 60 | lit px > 120 | mean luma | peak luma |
|-----:|------------:|-------------:|----------:|----------:|
| gt ≈ 3.3 | +16 % | +21 % | +6.9 % | 255 vs 254 |
| gt ≈ 3.6 | −14 % | −22 % | −12.1 % | 253 vs 253 |
| gt ≈ 4.2 | +6 % | +3 % | +4.4 % | 253 vs 253 |

The sign flips between pairs, so this is the cycle yaw differing by a fraction of
a simulation tick (different facets lit), not a shading bias. Before the
colour-source fix that went in earlier (`webgl/patch_light_colormaterial.py`),
the same measurement read 237 lit pixels against the desktop's 5 623 — that
regression is gone.

GL state at the cycle draw, read from the live browser context: light positions
`(320,240,200,0)` and `(−240,−100,200,0)` (w = 0, directional), light diffuse
`(1,.7,.7)` and `(.7,.7,1)`, material ambient `(0.2,0.2,0.2)`, material diffuse
and specular `(1,1,1,1)`, shininess 0 — identical to the values the desktop
build reports for the same draw.

Wheel-trail band, same frame pair: desktop 6 719 selected px at RGB
(23.6, 71.3, 118.9) mean luma 62.5; browser 7 992 px at (24.1, 72.5, 121.0)
63.6 — colour within 2 %, the area difference is the pose offset (`trail_and_walls_gt5.07.png`).

## 4. Sampler level-of-detail — ruled out here, ready to test on your GPU

The floor is textured through the fixed-function texture environment, so every
textured draw in the browser build is emitted as
`texture2DProj(uTexUnit, (uTexMatrix * vTexCoord))`. Which mip level a
projective sampler resolves is exactly the kind of thing two GPUs can disagree
about, so the repo now carries `webgl/site/lodtest.html`: one grazing floor plane
drawn four times, each quarter with a different level rule, measuring its own
output and printing the numbers.

On the capture stack used for everything above (Chrome/SwiftShader):

| strip | sampler level rule | far band contrast | far lit px | near contrast |
|-------|--------------------|------------------:|-----------:|--------------:|
| A | `texture2DProj` — what the game emits | 6.5 | 16.7 | 10.4 |
| B | explicit level, divided-coordinate gradients | 6.5 | 16.7 | 10.4 |
| C | explicit level, un-divided gradients | 6.5 | 16.7 | 10.4 |
| D | plain `texture2D`, reference | 6.5 | 16.7 | 10.4 |

All four agree, so the projective call is not what is thinning the distant grid
here, and an explicit-level patch (the obvious-looking fix) changes nothing on
this hardware — which is why it is not shipped. If on your machine A sits below
D, the page names the mechanism in one line and the patch becomes worth
finishing; `lodtest-swiftshader-reference.png` is the reference render.

## What to check on your machine

1. Hard-reload the browser build (the page HTML is cached for 600 s): Ctrl-Shift-R.
2. Open <https://smarioman.github.io/armagetronad/lodtest.html> and report the
   four numbers (or a screenshot of that block).
3. Report `FLOOR_DETAIL` in the desktop build's `user.cfg`/`autoexec.cfg`
   (0 off, 1 grid, 2 texture, 3 two-texture) and the same value the browser
   build shows in its graphics menu. Section 2 predicts that a mismatch there is
   the whole "wider grid" effect.
4. For the cycle: the published 8× zoom crops are the same frames on both sides.
   If your desktop cycle looks different from the crop on the right, tell me the
   setting you have for alpha blending and floor detail — the cycle shadow is
   gated on `sr_floorDetail > rFLOOR_GRID && TextureMode[TEX_FLOOR] > 0 &&
   sr_alphaBlend` (`src/tron/gCycle.cpp:4671`), so a floor-detail mismatch also
   turns the ground shadow on one side and off the other.

## Caveats

* Browser captures here run against Chrome/SwiftShader; your machine runs a real
  GPU. Section 4 is the instrument for that gap, and anisotropic filtering is
  left at 1.0 on both sides because the game never requests it (the desktop log
  reports the same).
* The browser frames here come from a local build carrying the harness stamp
  probe; the probe only prints, and the rendering code is what is on the `webgl`
  branch.
* Replays match to under one simulation tick; anything measured at a smaller
  tolerance than that is noise, which is why the tables above are ratios over
  whole bands rather than per-pixel diffs.
