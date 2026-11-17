# Wall and cycle pair at game time 2.46 (2026-11-13, commit 2f37858b)

Both builds played the same recording (`webgl/harness/perp2.aarec`), captured at
the same stamped game time: native gt 2.4555, webgl gt 2.4637, camera 107.83,
69.60 against 107.83, 69.69.  The webgl side is the build that contains the
wall texcoord fix, the LINE_STRIP/LINE_LOOP enum mapping and the positional
light patch that previously never applied.

Files:

- `wallpairs-gt2.46-full.png` - whole frames, native left, webgl right.
- `wallpairs-gt2.46-far.png` - the wall band y 420-490, native above/below
  webgl side by side.  This is the band the wall fix was supposed to repair.
- `wallpairs-gt2.46-near.png` - the floor and trail band y 490-690, same layout.
  Still diverges: 8.9 % of the native's pixels sit above luminance 120 against
  6.9 % on the port, and over the pixels where the native is bright the port
  reads RGB (31,33,32) with a median of 0, i.e. about half are black.
- `wallpairs-gt2.46-nearest.png` - y 660-790, the nearest floor: native p99 89,
  port p99 36.

Numbers behind the pictures:

| region | native | webgl | verdict |
|---|---|---|---|
| far wall band, pixels > lum 40 | 24.1 % | 23.8 % | parity |
| far wall band, mean green | 37.9 | 36.7 | parity |
| far wall band, pixels > lum 120 | 9.4 % | 8.8 % | parity |
| near band, pixels > lum 120 | 8.9 % | 6.9 % | port low |
| nearest band, 99th pct green | 89 | 36 | port low |
| cycle specular core, px count | 6944 | 6989 | parity |
| cycle specular core, mean RGB | 215,215,215 | 213,213,213 | parity |
| cycle saturated green, mean RGB | 0,253,0 | 0,254,0 | parity |
| saturated green coverage, lower half | 8120 | 2356 | port low |

Read: the wall top edge and the far grid are fixed at matched poses, the cycle
body shading matches, and what is left is the geometry near the camera - the
trail segments lying flat on the floor close to the viewpoint.  That is the open
question recorded in `webgl/README.md`, "Visual parity against the native
build".
