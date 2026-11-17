# Measurement notes

These are the write-ups that went with the parity work: what was measured, the
numbers on both builds, and what fixed what. They were written in a scratch
directory next to the PNGs they describe; the images are **not** in the repo (a
sheet is tens of megabytes of derived pixels, and every one of them is
re-derivable), the numbers in the text are the part that has to survive.

## Reading them

- Each note names its capture: which replay, which game time or camera pose,
  which build. `webgl/harness/perp2.aarec`, `perpA.aarec` and `myrec2.aarec` are
  committed, so a note's frames can be taken again.
- A note refers to images by file name (`49-pair1.png`,
  `49-3-report-missing.png`). Regenerate them with the commands below instead of
  looking for them here.
- Older notes (before commit `1c181e2e`) quote the frame stamp as
  `HARNESS FRAME n gt ...`; the probe committed as
  `webgl/harness/parity-stamp.patch` prints the same fields under `PF FRAME`.
  Same data, one renamed token.
- `README.md` is the oldest sheet index and refers to builds from before the
  lighting and near-plane work; `README-lighting-parity.md`,
  `README-coloursource.md` and `README-49.md` supersede its conclusions.

## Index

| note | what it settles |
|---|---|
| `README.md` | the first frame-matched native/wasm comparison sheet, and how those frames were paired |
| `README-parity.md` | how a matched pair is produced and what to look at in one |
| `README-p3.md` | a frame-identical desktop/port comparison at a matched pose (2026-11-13) |
| `README-replay.md` | does the port play a recording at the same frame and the same place |
| `README-keysym.md` | the SDL 1.2 key numbers a replay carries vs the SDL 2 numbers the page runs |
| `README-floor-wall-cycle.md` | floor grid, wall and cycle on one identical frame, before/after |
| `README-grid-cycle-live.md` | the same two divergences measured on the live page |
| `README-coloursource.md` | the dark light-cycle body: colour material leaking into the lighting terms |
| `README-lighting-parity.md` | cycle shading closed, grid confirmed at parity |
| `README-nearplane.md` | line segments at the near plane: what the eye-plane clip costs and buys |
| `README-rimface.md` | the void beside a close wall: the arena rim face, fixed and deployed |
| `README-sparks.md` | grinding sparks in `myrec2.aarec`, desktop against port |
| `43-parity-census.md`, `parity-census.md` | census of what each build draws at the same instant |
| `44-spark-eyeclip-AB.md` | spark geometry is at parity; the eye-plane clip keeps the specks |
| `42-video-settings.md` | which video settings the port can actually honour in a browser |
| `48-rebuild-vs-deployed.md` | the live artifact against a from-source rebuild of the same commit |
| `README-49.md` | the perimeter replay with 10 CPUs: the two named divergences, measured at matched poses |
| `offset-AB-note.md`, `wallpairs-gt2.46-note.md` | polygon-offset A/B, and a wall/cycle pair at gt 2.46 |
| `server-browser.md` | the server browser over the multiplexed bridge |

## Re-measuring one of them

The three commands in `webgl/ENVIRONMENT.md` §2 are the whole pipeline: stamp
patch into both builds, `webgl/harness/natpb.sh` for the desktop run,
`webgl/harness/cdpd.py` for the port run at the same gates, then
`webgl/harness/sheets/{mkpair49,sheet49,mkreadme49}.py` to compose the pair
images, the reports and the note itself. `mkreadme49.py` writes the numbers into
markdown rather than restating them by hand, so a regenerated note and a
regenerated measurement cannot drift apart.
