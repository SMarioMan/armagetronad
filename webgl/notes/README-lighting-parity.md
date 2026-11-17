# Light-cycle shading fixed, floor grid confirmed at parity

Commit `0212d2a8` "Match the desktop specular rule for a zero shininess", pushed
to `webgl`. CI rebuilt and deployed: `armagetronad_main.wasm` 2209685 bytes and
`armagetronad_main.js` 313583 bytes, both `last-modified: Sat, 14 Nov 2026
19:18:06 GMT`. The live JS now carries the ungated specular term
(`vec3 specular = specularI * u_lightSpecular..`). The page HTML is served with
`cache-control: max-age=600`, so hard-reload to pick it up.

## Cause

`webgl/patch_light_specular.py` had been suppressing the specular term whenever
`GL_SHININESS` is 0. `probe/bodylightprobe.c`, run against the reference driver
(llvmpipe, Mesa 25.2.8, direct rendering) with the game's own state
(`src/tron/gCycle.cpp:4331-4346`: material specular `(1,1,1)`, shininess left at
the default 0, two directional lights with `w = 0`, a front-facing quad), reads
`255,255,255` at the centre; with material specular forced to black it reads
`233,199,246`, exactly the diffuse+ambient sum `0.04 + 0.447*(1,.7,.7) +
0.610*(.7,.7,1)`. So the desktop keeps a full light-coloured specular wash at
shininess 0, and the emulation's `exp(shininess*log(N.H)) = 1` already reproduces
it. The guard is gone rather than retuned, and `patch_light_positional.py` was
dropped with it: both lights pass `w = 0`, so they are directional and a
positional branch cannot change shipped content.

## Cycle body, same replay, same archive position

`harness/bodywhite.py`, box around the followed light cycle (the two builds sit
about one world unit apart in pose because the render clock lags the archive
clock slightly):

| pose (archive time) | build | near-neutral px | green px | mean green | max luminance | body bbox |
|---|---|---|---|---|---|---|
| gt 4.20 | desktop | 14 | 199 | 192 | 189 | 10x77 |
| gt 4.20 | port before | 1 | 131 | 152 | 120 | 8x73 |
| gt 4.20 | port after | 10 | 187 | 191 | 192 | 10x75 |
| gt 9.52 | desktop | 14 | 176 | 191 | 189 | 8x77 |
| gt 9.52 | port before | 1 | 127 | 152 | 121 | 8x77 |
| gt 9.52 | port after | 14 | 181 | 188 | 192 | 10x77 |

`cyclebody-parity.png` is the gt 9.52 triple (desktop | port before | port after,
left to right), `cyclebody-parityB.png` the gt 4.20 triple.

## Floor grid, matched poses

`harness/gridprofile.py` on `perp3p-04.png` (desktop) and `lit4-02.png` (port,
after) - identical stamp: `gt 9.5182 pos 610.0000 462.8890 h 16.7496 dir 0.0000
1.0000`. `y` is the scanline, `lines` counts grid crossings, `ampmean`/`ampmax`
are the rise above the local background, `fwhm` the line width.

| scanline | lines desk/port | ampmean desk/port | ampmax desk/port | fwhm desk/port |
|---|---|---|---|---|
| 300 (far) | 7 / 7 | 12.0 / 12.0 | 12 / 12 | 2.00 / 2.00 |
| 380 | 50 / 49 | 13.6 / 13.5 | 15 / 15 | 1.86 / 1.84 |
| 460 | 64 / 64 | 16.2 / 16.1 | 20 / 20 | 1.94 / 1.94 |
| 580 | 47 / 46 | 35.2 / 35.2 | 253 / 253 | 2.04 / 2.09 |
| 700 | 41 / 42 | 18.1 / 18.1 | 129 / 128 | 1.80 / 1.81 |
| 760 | 37 / 37 | 16.5 / 16.9 | 74 / 89 | 1.59 / 1.78 |

The far grid is present at the same count, amplitude and width on both sides, so
the lighting change left the floor untouched and the far rows now read the same
in both builds.
`cmp-far-poseA.png` / `cmp-far-poseB.png` are the far-floor crops, desktop above,
port below.

## The yellow arrow in the desktop shots is debug-only

`markernative.png` (five desktop poses) and `markerport.png` (three port poses)
use the same crop. The yellow shape above the desktop cycle is
`ePath::RenderLast()`, compiled only under `#ifdef DEBUG`
(`src/engine/ePath.cpp:363`, called from `src/engine/eDisplay.cpp:592`): the
debug AI navigation line, a two-vertex strip from `z = 0` to `z = 50` at the
path's current position, which is why it tilts as the interpolation runs and is
missing entirely at gt 6.8 where no path is current. It is an artefact of the
debug build used for these captures, not something the release desktop build
draws; no port change is warranted. If your own desktop build defines `DEBUG`
you will see it there too, and that is the case where it would become a real
difference to close.
