# Replay determinism: does the port play a recording at the same frame and the
# same pose as the desktop build?

Both builds stamp every 11th rendered frame with the render frame number, the
game time, the camera position, the camera height and the heading.  The stamp
lives in `gGame::Render` in the capture working tree only (desktop: stderr;
port: `printf`, which the browser console carries into the capture log).
Pose error below is the plan distance between the two camera positions, in
world units; an arena is 200 units across.

## `myrec2.aarec` - player plus 2 CPUs: frame identical

| gate | desktop frame / game time | port frame / game time | pose error |
|---|---|---|---|
| waypoint 69.62, 86.62 | 726 / 2.5510 | 726 / 2.5523 | 0.0001 |
| waypoint 80.81, 94.00 | 781 / 3.0097 | 781 / 3.0110 | 0.0009 |

Same frame number on both sides, game times 1.3 ms apart, camera positions
within a thousandth of a world unit.  Those are the frame-identical pairs to
diff: `sparks-myrec2-n781-stack.png`, `sparks-myrec2-n726-stack.png`.

## `perp2.aarec` - player plus 10 CPUs: frame for frame identical

Pairing the current builds' stamps - desktop `shots/hlin4-stderr.txt`, port
`shots/spP9-console.txt` - needs no shift at all.  Both sides start stamping on
frame 11, at game time -3.2755 (desktop) and -3.2747 (port), and the 55 stamps of
the first round pair index for index, 11/11 through 605/605.

| measure | value |
|---|---|
| pose error | mean 0.022 units, max 0.084 |
| heading | agrees at all 55 pairs to the 4-decimal print, about 0.003 deg |
| camera height | largest difference 0.016 units |
| game-time skew | mean -0.0009 s |

An older pairing of the same two recordings (desktop run `hpb1` against this port
run) needed a whole-frame shift and left mean 0.318 units, max 0.985, heading up
to 3.5 deg; the current pair shows neither.

The second loop restarts on frame 616 at game time -3.4839 on both sides - the
same frame, the same instant - and 42 of the 46 paired stamps agree to the
printed precision, pose error 0.000.  Four do not, at game time 4.56 to 5.13: the
port's camera azimuth swings 110, 180, -141, -116 degrees while the desktop's
holds 90 and walks straight on.  That is the port's player being down with the
camera cut to another cycle while the desktop's is still running: a simulation
split about 4 seconds into the second pass, absent from the first.  The likely
mechanism is a dropped or duplicated recorded input event at the loop restart.
Proving it means re-applying the input tap and running both builds through the
second pass, which I have not done.  Every visual comparison in these sheets uses
first-round frames, and those are frame for frame identical.

For scale: before the SDL 1.2 keysym cutover the same recording left the port's
cycle up to 75 world units behind the desktop's with a flat camera height, because
the recorded arrow presses never reached the game.

## What the eye-plane line clipper contributes to the sparks

A/B on the frame-exact `n726` triple, crop `x 300-930, y 470-800`:

| build state | crop red px | crop chroma px | crop mean luminance |
|---|---|---|---|
| desktop | 2264 | 4598 | 9.66 |
| port, clipper on | 2240 | 4677 | 9.73 |
| port, `&nolineclip=1` | 2239 | 4644 | 9.54 |

The clipper moves the spark count by 0.04 %, so it is not the mechanism that
brought the grinding sparks back - the 16-bit index fix and dropping the
invented line-mode enum swap are the changes in that interval, and I have not
isolated which of them carries it.  The clipper is still load-bearing for
near-field wall and trail geometry, measured separately at frame 363: rows
520-600 went 0 to 1116 green-dominant pixels and rows 600-680 went 0 to 959,
against the desktop's 2412 and 1453.

Sheets: `sparks-myrec2-n726-clipper-on.png`, `sparks-myrec2-n726-clipper-off.png`,
`sparks-myrec2-n770-clipper-off.png`.

## Where the numbers came from

- desktop perp2 stamps: `shots/hlin4-stderr.txt` (127 stamps, two replay loops, game time -3.2755 to 10.0826)
- port perp2 stamps: `shots/spP9-console.txt` (101 stamps, two replay loops, game time -3.2747 to 5.1307 where the capture window closed); `shots/hpb1-stderr.txt` is the older desktop run kept for the pre-fix comparison
- myrec2 poses: `shots/spN6-shots.txt` (desktop) against `shots/spP5-console.txt` and `shots/spP6-console.txt` (port)
- spark pixel counts: `sparkpair.py` (crop census) and `chrompx.py` (row bands), sources under the harness directory
- alignment arithmetic: `posecmp.py`, which parses either stamp format
