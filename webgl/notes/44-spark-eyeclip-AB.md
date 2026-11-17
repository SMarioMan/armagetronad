# 44 - grinding sparks: geometry is at parity, and the eye-plane clip is what keeps the specks

Sheet: `44-spark-eyeclip-AB.png`.

Everything here is measured with the in-tree probes (uncommitted harness only): the
per-frame spark census (`src/tron/gSparks.cpp` accumulators, printed by
`gGame::Render` as `PF SPK` under `ARMA_PARITY_STAMP=1` / `window.arma_parity_stamp`),
the shower log `PF SPKNEW`, and the frame stamp `PF FRAME`.  Both builds replay the
same archive, `shots/perp2.aarec`, so the spark *content* is driven by the recording
rather than by divergent live play.

## 1. Spark construction and drawn geometry: matches

Census at matched game time in the same replay (port at gt 2.88, desktop at
gt 2.87, both mid-shower):

| build | alive spark objects | drawn segments | total segment length | mean segment length |
|---|---|---|---|---|
| desktop (native, llvmpipe) | 12 | 120 | 21.90 | 0.183 world units |
| port, eye-clip patch ON | 14 | 140 | 24.84 | 0.177 world units |
| port, eye-clip patch OFF | 14 | 140 | 22.94 | 0.164 world units |

Mean drawn segment length agrees to 3 % (0.177 against 0.183).  The object count
differs by 17 % because the two replays' AI worlds split at gt 1.88 (documented in
note 43): the recorded player's own grinding shower is the same, the other cycles'
are not.

Consequence worth stating plainly: on **both** builds a spark segment is ~0.18 world
units, which at a chase-camera height of 16.5 is under one device pixel.  Grinding
sparks are sub-pixel additive specks on the desktop as well, so there is no "the
port draws shorter sparks" divergence to fix - the geometry handed to the rasteriser
is already at parity.  That closes the construction/geometry branch of the spark
investigation; what is left is the raster path, section 2.

## 2. Raster stage: removing the eye-plane clip turns specks into streaks

Two port runs of the same replay, one with the line-clip patch (`webgl/`'s eye-plane
cut for immediate-mode lines) and one with `&nolineclip=1`.  Each with/without pair
is captured inside the frozen wasm loop, so the two frames are **one rendered frame
apart** (spkP1: `n364 gt 2.8660` -> `n365 gt 2.8822`, i.e. 0.0162 s and 0.006 world
units of camera travel).  Difference layer over the whole 1280x800 frame, bucketed by
luminance of the per-pixel delta and by component extent:

| variant | spark specks (dL>60, extent<=12 px) | speck px | long components (extent>12 px) | their px | all changed px |
|---|---|---|---|---|---|
| eye-clip patch ON (ships) | 210 | 803 | 15 (mean extent 20.1 px) | 1123 | 4116 |
| eye-clip patch OFF | 147 | 596 | 19 (mean extent 19.8 px) | 1257 | 4728 |
| patch ON, stricter dL>140 | 136 | 625 | 5 | 181 | - |
| patch OFF, stricter dL>140 | 73 | 395 | 9 | 340 | - |

Removing the clip costs 30 % of the spark specks (210 -> 147 at dL>60, and 136 -> 73,
i.e. 46 %, at dL>140) while the long-component bucket *grows* (15 -> 19 components,
1123 -> 1257 px): segments that should have been cut at the eye plane are submitted
whole and rasterised as streaks across the scene, soaking spark pixels into geometry
that belongs to no spark.

The long-component bucket is also where the one frame of world change between the two
captures shows up (a wall top advancing between frames; mean extent ~20 px, present
at 15 components in the patched variant and 19 in the unpatched one, so it does not
account for the speck loss).

## 3. Cost side: the clip is not a cost, it is a win

Counting `PF FRAME` stamps up to the same game time in the same replay (read before
any pause, so the capture gating cannot inflate it):

| variant | rendered frames by gt 11.01 | page's own FPS readout |
|---|---|---|
| eye-clip patch ON | 848 | 48 |
| eye-clip patch OFF | 283 | 15 |

Ratio 3.0x from the frame counter, 3.2x from the on-page readout - the same effect
measured two ways.  So the unpatched path spends three times the frame budget and,
on top of that, loses close to half of the bright spark pixels.  This is the
measurement behind keeping the patch shipped; `&nolineclip=1` stays available as a
diagnostic switch.

## 4. Frame cadence, and what could not be measured

The simulation runs on a fixed 60 steps-per-game-second clock, so the regime that
matters for visual parity is when *rendered* frames exceed that rate.  The port sits
well inside it: in playback its game clock advances at roughly 0.1-0.5 s per wall
second, so even with the harness profile's `MAX_FPS 60` cap it delivers on the order
of 100-400 rendered frames per game-second, and the patched variant renders 3x more
frames per game-second than the unpatched one (848 against 283).  The desktop client
in this environment does not: under llvmpipe it manages about 30 frames/s
(374 stamps over a 12 s playback run) while game time advances at nearly real speed,
so it renders roughly at the simulation cadence, not above it.  The desktop numbers
in section 1 are geometry numbers, which do not depend on frame cadence; the
desktop-versus-port comparison in a >60-rendered-frames regime has no desktop
counterpart here, and that is a limit of this machine, not of the port.

Also not achievable: a pixel-exact desktop-versus-port spark pair.  The desktop
camera follows a different cycle than the port's replay does (worlds split at
gt 1.88), and the desktop camera travels 0.4 world units per rendered frame, so even
two desktop runs gated to the same game time land ~25 px apart.  The desktop row of
the sheet is therefore labelled appearance-only, and the desktop/port comparison is
carried by the census table rather than by a difference image.

## Files behind these numbers

- port frames: `shots/spkP1-00.png` (sparks) / `shots/spkP1-01.png` (sparks hidden one frame later), `shots/spkP2-00/01.png` for the `&nolineclip=1` variant; amplified differences `shots/spkPair-diff00/01.png`; stroke lists `shots/spkPair-strokes00/01.txt`.
- desktop frames: `shots/nspkA-00.png` (gt 3.02) / `shots/nspkB-00.png` (sparks hidden through `ARMA_NO_SPARKS=1`, gt 3.00); logs `shots/nspkA-stderr.txt`, `shots/nspkB-stderr.txt`.
- driver logs: `shots/spkP1-console.txt`, `shots/spkP2-console.txt` (the same probes, emitted into the page console).
- tooling: `harness/sparklayer.py` (difference-layer census: changed px, percentiles, connected components, speck/long-edge split, stroke dumps), `harness/pcaplive.sh` (port capture with `I:` game-time gates and the in-loop spark switch), `natpb.sh` + `natdrive.sh` (desktop capture with `G:`/`P:` gates and SIGSTOP-frozen frames).
