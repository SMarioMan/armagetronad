# Native vs wasm port: how these frames were made, and what to look at

> **Update, 2026-11-13.** This file records the first frame-identical pair and
> the numbers that were current when it was written. Two of its "Still open"
> items are now closed and one of its browser-hook numbers was wrong, so read
> it with the corrections below. The current record lives in `webgl/README.md`
> in the repository; the later comparison sets here are
> `wallpairs-gt2.46-*.png` (see `wallpairs-gt2.46-note.md`) and
> `offset-AAB-gt2.46.png` (see `offset-AB-note.md`).
>
> - Distance grid: **at parity** at matched poses — far/mid/left window peaks
>   255 / 253 / 211 against the native's 255 / 255 / 255, bright-row coverage
>   within 1.5 points, matching sampling period. The line below ("not fixed")
>   predates the wall top-edge and projective-texture fixes.
> - Light-cycle body: **at parity** — specular core 6989 px against the native's
>   6944, mean 213 against 215, body colour 0,254,0 against 0,253,0.
> - The port never asks for anisotropic filtering or multisampling: the
>   in-browser context measures `SAMPLES 0`, no sample buffers and
>   `TEXTURE_MAX_ANISOTROPY_EXT = 1.0`. The "4 sample buffers, 8x anisotropy in
>   use" paragraph further down is contradicted by direct measurement.
> - Still open: the floor and the trail nearest the camera (native 8.9 % of the
>   band above luminance 120, port 6.9 %), plus a few artefact reports that do
>   not reproduce on the current build — HUD name tags render single and crisp
>   at 2x zoom (`hud-text-zoom.png`).
>
> The per-frame stamp mentioned below is now a committed switch rather than a
> working-tree patch: set `ARMA_PARITY_STAMP=1` in the environment of a native
> build, or `window.arma_parity_stamp = true` before the page starts for the
> port (the capture driver does the latter automatically). The replay is also
> published now: `https://smarioman.github.io/armagetronad/?args=--playback,perp2.aarec`
> replays exactly the game state these frames come from.

Everything here comes from **one recorded game**, replayed on both builds, so
the two sides are not "similar screenshots" but the same world state.

## How it was produced

1. A single-player round (10 AI cycles, walls left standing, 60 fps cap) was
   recorded on a native Linux build: `--record perp2.aarec`.
2. The same file is replayed on both builds with `--playback perp2.aarec` —
   natively, and in the port at `?args=--playback,perp2.aarec`.
3. Captures are taken at fixed **game-time** gates (2.29 s, 4.35 s, 6.21 s,
   8.35 s, 10.10 s), not at wall-clock moments.
4. Both builds print a stamp per captured frame: frame number, game time,
   camera position, camera direction. Those stamps are the
   `manifest-*.txt` files here, and they are what makes a pair checkable.

`perp2.aarec` (265 KB) is in this directory, so you can replay it in your own
desktop build with `src/armagetronad --playback perp2.aarec` and compare
against these frames directly.

## The pair that matters

Gate 1 is **frame-identical**: native frame `n331`, game time 2.2876; port
frame `n331`, game time 2.2885; camera position identical to four decimals.
That is the comparison to trust. Gates 2-4 are not pose-comparable — a few
seconds into a playback the spectator camera follows a *different* cycle on the
two builds (a 178-degree view-direction difference at the 4.35 s gate), so later
captures show different scenes.

| file | what it is |
|---|---|
| `parity-00-horizon-stack.png` | three strips, full width, rows 55-70 % of the frame (nearest the horizon), top to bottom: native / port as published / port with the floor-mode change that was reverted |
| `parity-00-mid-stack.png` | same, rows 70-85 % (mid-distance floor) |
| `parity-00-near-stack.png` | same, rows 85-100 % (floor nearest the camera) |
| `parity-00-native.png`, `parity-00-port-before.png`, `parity-00-port-after.png` | the three full 1280x800 frames |
| `parity-00-{horizon,mid,near}-{native,port-before,port-after}.png` | the individual 1280x120 strips, unscaled: pixel widths are directly comparable |
| `parity-01-mid-stack-camera-diverged.png`, `parity-02-mid-stack-camera-diverged.png` | later gates, kept only to show the camera divergence — not a parity judgement |
| `native-0X.png`, `port-before-0X.png`, `manifest-*.txt` | the same native frames (pixel-identical to `parity-00-native.png`); the `port-before-0X.png` shots are 1280x856 **page** captures of the same port frames, not canvas-cropped — compare the `parity-*` set, which is cropped to the 1280x800 canvas on both sides |
| `parity-1..6-*`, `compare-*`, `cmpold-*`, `grid-before-*` | earlier attempts from before the replay path worked: superseded, do not use for judgement |

## What the strips show

In the horizon band the native frame carries a dense, readable grid all the way
to the horizon. Both port frames lose almost all of it, and the wall strip
across the middle of the frame is flat saturated yellow on the port where the
native still shows its segment detail. The floor-mode change (third strip) did
not bring the grid back — which is why that change was reverted.

Measured, from those strips (line ink and peak brightness above the row
median; "horizon" is the band nearest the horizon):

| band | native | port as published | port + floor 3 (reverted) |
|---|---|---|---|
| horizon ink / peak | 9744 / 152 | 6135 / 103 | 4841 / 73 |
| mid ink / peak | 17439 / 195 | 14805 / 194 | 15559 / 172 |
| near ink / peak | 2293 / 86 | 1478 / 32 | 1330 / 22 |
| crossings per scan line | 15.6 / 17.7 / 7.4 | 16.2 / 10.3 / 7.2 | 23.4 / 12.0 / 21.4 |

Shading, over the frame's brightest pixels (mean colour — this is where a
lighting error shows up directly):

| threshold | native | port as published | port after the lighting patches |
|---|---|---|---|
| luminance > 128 | 3.89 % of pixels, R109 G224 B101 | 3.17 %, R171 G208 B132 | 3.13 %, R173 G208 B134 |
| luminance > 64 | 11.75 %, R91 G130 B80 | 12.37 %, R94 G124 B79 | 12.12 %, R95 G124 B80 |

So: the floor's overall colour matches the native closely (a few units per
channel), the light-cycle body does not — green is roughly held while red and
blue are lifted by about 60 and 30.

## Measured on the port side, in the browser

Hook on the page's own GL calls: renderer "WebKit WebGL", 4 sample buffers
(4x MSAA on), max texture 8192, `EXT_texture_filter_anisotropic` present with
maximum 16 and 8x in use on the floor textures, `ALIASED_LINE_WIDTH_RANGE`
1..1, and the floor's line textures uploaded as `1x256` and `256x1` with a full
nine-level mip chain under `GL_LINEAR_MIPMAP_LINEAR`.

## Still open

- Distance grid contrast: 103 vs 152 peak in the horizon band, 32 vs 86 in the
  nearest band. Not fixed. Unproven mechanism worth testing: mipping a `1x256`
  line texture toward `1x1` averages the line into a flat colour, so the grid
  disappears at high lod.
- Light cycle body colour, as above: not moved by either lighting patch.
- The native side's own GL context numbers (line width range, the anisotropy
  the driver really applies, the filter the game ends up with) are not measured:
  the probe needs mesa headers this machine does not have.
- The reference native run uses **llvmpipe** software GL, not a desktop GPU —
  closer to the browser's SwiftShader than to your machine. Frames from your
  own build against the same `perp2.aarec` are the missing third column.
