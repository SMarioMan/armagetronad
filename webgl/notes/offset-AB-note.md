# Polygon-offset A/B at game time 2.46

`offset-AAB-gt2.46.png` — three captures of the same replayed frame, cropped to
rows 400..700 at full width, top to bottom:

1. `native (desktop, offset on)` — the reference Linux build (llvmpipe), rows as
   captured in `frames/native-gt2.46.png`.
2. `port, offset on (shipped)` — the WebGL build as deployed, `glPolygonOffset
   (-2,-5)` with the three polygon-offset enables, exactly what the desktop does
   (`src/render/rScreen.cpp`, `sr_DepthOffset`).
3. `port, offset REMOVED (A/B test)` — a throwaway port build with
   `glPolygonOffset(0,0)` and all three enables switched off under
   `__EMSCRIPTEN__`. That build is not and will not be shipped.

All three are at the same game-time gate and the same camera pose: port
`(107.8338, 69.6826) h 16.3236`, native `(107.8338, 69.6817) h 16.3234`, 0.00
world units apart. So the pictures are of the same geometry from the same place.

| measurement | native | port, offset on | port, offset removed |
|---|---|---|---|
| far wall band (rows 420-490) above luminance 40 | 24.07 % | 23.82 % | 9.48 % |
| near band (rows 490-690) above luminance 120 | 4.92 % | 2.75 % | 2.73 % |
| saturated green over rows 450-800 | 8120 px | 2356 px | 0 px |
| pixels changed vs the shipped port capture | — | — | 433 641 |

What this settles: the depth offset is not hiding anything near the camera.
Switching it off collapses the far wall band and removes every saturated-green
pixel in the frame, so the desktop's `-2,-5` is doing necessary work on the
WebGL context, and the near-band deficit is untouched by it (2.75 % to 2.73 %).
It was the last mechanism on the list that could be tested with the tooling
here; what is left untested is the wall quad strip's own coverage and the
trail's per-vertex alpha.
