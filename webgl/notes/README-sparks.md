# Grinding sparks in `myrec2.aarec` - desktop against port, current builds

What is being compared: the spark objects the player's cycle throws while it
grinds along a wall.  Each `gSpark` draws ten `GL_LINES` segments with additive
blending in the cycle's own colour; the probe reads the colour as
`own 0.60 0.00 0.00` (red) with `white_sparks 0` on both builds.

## Geometry reaches both builds

PF SPARK probe (working tree, both builds) over a full `myrec2` playback, one
sample every fifth `gSpark::Render` call, 800 samples on either side:

| build | samples | with live segments | mean total segment length | max |
|---|---|---|---|---|
| desktop | 800 | 777 | 1.07 world units | 1.88 |
| port | 800 | 773 | 1.07 world units | 1.80 |

Same frame window, straight from the two logs - desktop at game time 3.002,
port at 3.003, three consecutive spark objects:

```
desktop  PF SPARK 2401 gt 3.002 alive 10 len 0.13 heat0 2.92 a0 1.00
desktop  PF SPARK 2406 gt 3.002 alive 10 len 0.26 heat0 2.10 a0 1.00
desktop  PF SPARK 2411 gt 3.002 alive 10 len 0.49 heat0 2.55 a0 1.00
port     PF SPARK 3141 gt 3.003 alive 10 len 0.14 heat0 2.04 a0 1.00
port     PF SPARK 3146 gt 3.003 alive 10 len 0.28 heat0 2.24 a0 1.00
port     PF SPARK 3151 gt 3.003 alive 10 len 0.47 heat0 2.18 a0 1.00
```

The port column is the current working tree.  It differs from the wasm the page
serves (2207797 bytes, last modified 14 Nov 02:55 GMT) only by the SDL 1.2
keysym compile-time cutover, which touches input and no draw call, and by the
never-commit PF probes.

## Pixels on gate-matched frames

`sparkpair.py`, box `x 300-930, y 470-800` over the spark spray; the near band
of `chrompx.py` is rows 520-800.  Red-dominant means
`r - max(g,b) >= 40 and r == max`.

| pair | frames | game time | pose delta | near-band red | near-band chroma | near meanL |
|---|---|---|---|---|---|---|
| `spN6-02` / `spP5-01` | 781 both | 3.0097 / 3.0110 | 0.0012 units | 3092 / 3842 | 3203 / 3978 | 4.59 / 4.10 |
| `spN6-00` / `spP5-00` | 726 / 737 | 2.5510 / 2.6440 | 0.031 units | 2264 / 2243 | 4598 / 4767 | 9.66 / 9.99 |

Frame 781 is the same rendered frame on both builds and the same pose to a
thousandth of a world unit, so the top row is a direct comparison: the port
draws at least as much spark coverage as the desktop there, and marginally less
in the lower-spray crop of the second pair.

Sheets (desktop above, port below, same crop in both halves):

- `sparks-myrec2-n781-stack.png`
- `sparks-myrec2-n726-stack.png`

## What is still open

- Attribution: the current dist build carries the immediate-mode eye-plane line
  clipper, the 16-bit index fix and the removal of the invented GL_LINE_STRIP /
  GL_LINE_LOOP enum swap.  The `&nolineclip=1` A/B at these same waypoints says
  how much of the recovery the clipper alone accounts for, and is queued.
- A further emscripten-only change under test in `gSpark::Render` (measuring a
  spark's age against the simulation clock when it runs ahead of the smoothed
  render clock) is being measured at the same waypoints; the numbers above were
  taken without it.
- The retracted "2607 against 1446 near-white pixels" figure in
  `myrec/README.md` compared gates that were not pose-matched; the rows above
  are.
