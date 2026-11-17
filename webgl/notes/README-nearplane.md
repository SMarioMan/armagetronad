# Near-plane line clipping: before / after on the perimeter replay

Both port captures are the *same build* of commit 55264844 playing the same
recording (`perp2.aarec`, 10 CPUs, 60 fps cap), gated to the identical
rendered frame `n363` (game time 2.849). The only difference is the page URL:

- `nearplane_off_*` — `http://127.0.0.1:8080/index.html?args=--playback,perp2.aarec&nolineclip=1`
- `nearplane_on_*`  — same URL without `&nolineclip=1`

The native reference is `shots/hlin4-00.png` (llvmpipe, same recording, frame
`n363`, game time 2.8486). Pose agreement at that frame: desktop
`107.8338 69.5185`, camera height `16.4540`; port `107.8338 69.5183`, `16.4542`.

## Sheets

| file | panels | what to look at |
|---|---|---|
| `nearplane_off_full.png` | desktop, port (cut off), diff ×8 | whole frame; the diff lights up over the lower half |
| `nearplane_on_full.png` | desktop, port (cut on), diff ×8 | same frame, cut on |
| `nearplane_off_band440.png` | same, cropped to rows 440-800 | the near floor and the cycle |
| `nearplane_on_band440.png` | same, cut on | the near floor and the cycle |
| `nearplane_off_bike.png` | rows 470-790 × columns 430-850, zoom 2 | the cycle body and its light rays, cut off: a few sparse rays |
| `nearplane_on_bike.png` | same, cut on | the ray star is back and matches the desktop's structure |
| `nearplane_cut_effect.png` | port cut off, port cut on, diff ×12 | only what the clipper changed, isolated from other differences |

## Numbers at that frame

| measure | desktop | port, cut off | port, cut on |
|---|---|---|---|
| green-dominant pixels, rows 520-560 | 1539 | 24 | 608 |
| green-dominant pixels, rows 560-600 | 965 | 37 | 589 |
| green-dominant pixels, rows 600-640 | 763 | 37 | 593 |
| green-dominant pixels, rows 640-680 | 801 | 40 | 467 |
| green-dominant pixels, rows 680-720 | 630 | 40 | 322 |
| green-dominant pixels, rows 720-760 | 355 | 40 | 288 |
| green-dominant pixels, rows 760-800 | 254 | 40 | 141 |
| total below row 520 | 5307 | 258 | 3008 |
| saturated green (g>200, r<80, b<80), lower half | 10684 | 3046 | 10178 |
| mean abs channel difference vs desktop, whole frame | - | 3.45 | 3.47 |
| pixels differing by more than 32, whole frame | - | 3.65 % | 3.48 % |

So the cut restores the near-field geometry without disturbing the rest of the
frame: whole-frame agreement with the desktop stays flat (it was slightly
*worse* before the last refinement, 4.09, when untouched polylines were also
re-emitted as separate line pairs and every join picked up extra blended
coverage).

## What is still open here

The remaining near-band gap is coverage and intensity, not missing geometry:
57 % of the desktop's saturated-green pixel count in the lower half is now
matched in extent but the rays fade on a different curve, and the cycle body
keeps a mid-tone deficit (that one is a fixed-function lighting question - see
`patch_light_positional.py` and the lighting entries in `webgl/README.md`).
