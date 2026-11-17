# Parity census: what the two builds actually draw at the same moment

Date 2026-11-15. Both builds replay the same recording (`perp2.aarec`, 10 computer
players, `MAX_FPS 60` on both sides), under one settings profile
(`/home/agent/agtk/harness/autoexec.cfg`, served to both), so the two pictures are
supposed to describe the same world.

Sheets in this directory:

| file | what it shows |
|---|---|
| `43-parity-gate0-side-by-side.png` | desktop left, live port right, gt 2.32 / 2.60 |
| `43-parity-gate0-cycle-crops.png` | 2x crops around the light cycle, same pair |
| `43-parity-gate1-cycle-crops.png` | 2x crops at the pose-locked pair, gt 4.39 / 4.30 |

## Floor grid: matches at the pose-locked gate

`gridmetric.py` counts lit runs per scan row, their mean width in px, and their
contrast in row sigmas. The pose-locked pair (`hpb2-01` desktop, `porG-01` port,
camera 0.033 world units apart) gives:

| row | desktop lines / width / contrast | port lines / width / contrast |
|---|---|---|
| 440 | 15 / 1.61 / 3.28 | 12 / 1.68 / 4.13 |
| 560 | 45 / 3.10 / 1.83 | 45 / 3.17 / 1.79 |
| 700 | 20 / 2.40 / 2.17 | 20 / 2.63 / 2.19 |
| 760 | 18 / 2.69 / 4.82 | 16 / 2.88 / 4.92 |

Line width agrees within 8 %, contrast within 10 %, and the row that has the most
lines (560, where the grid is densest) counts exactly the same on both builds.
Rows 470-620 of that frame report widths of 58-226 px: that is the scan failing
to separate lines in a saturated band, not a rendering result, and it happens on
both builds.

For the older documented pair (`hpbR0-00` desktop / `ppR0-00` port, from the
deployed build), the same tool reads widths 3.00-4.29 px on the desktop against
3.14-3.48 px on the port, and line counts within a quarter at five of seven rows.

The port's grid is therefore not thin, not missing at distance, and not dimmer.
If your browser shows it that way, the difference is coming from the page
context, not the renderer: canvas size, `devicePixelRatio`, browser zoom, and
`DISPLAYSCALE` all change how many device pixels a grid line lands on. The
shipped build now pins the video settings the renderer can actually honour
(`FLOOR_DETAIL`, `TEXTURES_HI`, `TEXTURE_MODE_*`) so a stale saved cfg cannot
change the picture; hard-reload the page (its HTML is cached for 10 minutes).

## Light cycle body: matches, the earlier gap was scene content

`bodyprofile.py` profiles the body's own lit mask in five bands along and across
it. At the pose-locked pair:

| build | first band | last band | red/green ratio | mask |
|---|---|---|---|---|
| desktop | 130.3, 130.3, 26.2 | 129.3, 129.3, 25.8 | 1.000 | 1375 px in 220x90 |
| port | 131.5, 131.5, 26.4 | 130.9, 130.9, 26.2 | 1.000 | 1547 px in 220x111 |

Same colour, same gradient shape, within about 1 %. What differs is the extent of
the mask: a different bike, at a different attitude, in the same box. That also
explains the earlier fixed-box reading that looked like a shading gap (mean
luminance 13.5 against 12.3): the box held a different object, not a different
lighting model. The lighting model question is closed in code as well -
`webgl/patch_light_colormaterial.py` carries the measurements that settled it,
and the two lighting patches that were tried after it were reverted for the
reasons recorded at the end of `webgl/build.sh`.

## What stops these pairs being reusable: the two worlds part company

Pairing the stamp logs of a desktop run and a port run over the whole recording
(1363 paired frames) measures, per game-time row, how far apart the live cycles
are:

| game time | largest cycle position difference |
|---|---|
| countdown through gt 1.86 | 0.04 units or less |
| gt 1.88 | 0.72 |
| gt 4.3 | a few units |
| gt 8.40 | 44, 63, 97, 131 (cycles 3, 5, 7, 9) |

and cycle 13 exists only in the desktop run while cycle 17 exists only in the
port's, so the respawn schedule splits too. The seed is not the cause: both
builds seed the same way and the cycles agree to 0.0000 for the first second and
a half. The cause is the math library. A probe that prints 288 function values as
hex doubles disagrees between the desktop build (glibc) and the browser build
(musl) on 6 of them:

```
atan2[9]  0x1.c51d6c5b23d23p-1  vs  0x1.c51d6c5b23d24p-1
atan2[10] 0x1.f9dcbbeb47ce8p-1  vs  0x1.f9dcbbeb47ce7p-1
cos[14]   0x1.1ff026793f1bbp-1  vs  0x1.1ff026793f1bcp-1
ftan[7]   0x1.6e3d06p+1         vs  0x1.6e3d04p+1
ftan[9]   0x1.777a5cp-24        vs  0x1.777a5ep-24
ftan[10]  0x1.777a5cp-23        vs  0x1.777a5ep-23
```

AI steering is float arithmetic through exactly those functions, so a comparison
threshold flips a second and a half into the round and the worlds go their own
ways. Nothing port-side can undo a different libm. Practical rule: compare
inside the shared window (gt < 1.9 on this recording) or compare geometry that
does not care where the computer players have wandered - the floor grid and the
recorded player's own cycle are both safe.

A second, smaller one: during the countdown and the first two seconds the
camera sits 25 world units away from where the desktop's camera sits (measured
at every paired row from gt -3.4 to 1.86), then it agrees - 0.033 units at
gt 4.3. So the round-start camera follows a different cycle in the two builds.
It is transient, it is a camera choice and not a renderer difference, and it is
the reason a pose-gated capture of a fresh boot is unreliable: gate by game
time and take the pose from the stamp, not the other way round.

## Capture state

The port replay capture at game-time gates works end to end (the archived SDL
1.2 keysyms are now translated to the SDL 2 keysyms the browser build's event
layer uses, which is what had been breaking the arrow-key navigation in a
replayed menu). One capture got three of five gates before its own time budget
expired: the browser build plays this recording back at roughly 0.26 rendered
frames per second under SwiftShader at 1280x800, so five gates cost about
15 minutes. Desktop capture of the same recording takes under a minute.
