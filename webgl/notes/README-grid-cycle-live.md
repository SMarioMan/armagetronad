# Floor grid and light-cycle shading, measured on the live build

Build under test: `webgl` HEAD `dfbf997f`, deployed wasm `2207126` bytes,
`last-modified: Fri, 13 Nov 2026 22:34:43 GMT` (verified with
`curl -sSI https://smarioman.github.io/armagetronad/armagetronad_main.wasm`).
Desktop reference: the same source tree built natively, run under Xvfb with
llvmpipe (`LIBGL_ALWAYS_SOFTWARE=1`), 1280x800.

Both sides play back the **same `.aarec`** file recorded by the native build and
are screenshotted at a *frame-number* gate, using a per-frame stamp both builds
print (`n`, `gt`, camera `pos`, `h`, `dir`). Pose delta at the gates below is
below 0.001, so the pairs are the same moment, not merely the same game time.

## Files

| file | what it is |
|---|---|
| `m2-farfloor-crop.png` | myrec2 gate frame 715/716, rows 340-500 cols 520-760, 3x, desktop left / port right |
| `m2-fargrid-profile.png` | per-column contrast profile over rows 330-500 of the same pair (green = desktop, red = port) |
| `m2-farrows-crop.png` | same pair, rows 166-330 (the band that does diverge) |
| `m2-cycle-crop.png` | same pair, rows 480-590 cols 430-700: the blue light cycle body |
| `n363-farfloor-crop.png` | perp2 (perimeter) gate frame 363, rows 150-430, desktop left / port right |

Metric used for the grid: for every pixel column in a row band, take
`percentile(98) - median` of luminance (the height of the bright grid line
above the dark floor around it), then average over columns. This is insensitive
to small horizontal misalignments, so it survives the 1-pixel pose jitter.

## 1. Floor grid — the far field is where the port still loses

`myrec2.aarec` (the recording you supplied, 2 CPUs), frame 715 desktop / 716
port. Ratio = port mean / desktop mean.

| row band | desktop | port | ratio |
|---|---|---|---|
| 120-166 (horizon) | 0.92 | 0.39 | **0.42** |
| 166-222 | 2.20 | 1.86 | **0.85** |
| 222-330 | 4.40 | 4.38 | 1.00 |
| 330-500 | 8.42 | 8.25 | 0.98 |
| 500-600 | 10.99 | 10.48 | 0.95 |
| 600-780 | 14.40 | 13.65 | 0.95 |

So the grid *period and coverage* are already identical; what is missing is
contrast in the extreme far band, the rows right below the horizon, exactly the
"lines fade out at a distance" you see. Everything closer than row 222 is at
parity (within 5 %).

`perp2.aarec` (perimeter recording), frame 363 on both sides, same metric over
luminance:

| row band | desktop | port | ratio |
|---|---|---|---|
| 120-220 | 197.4 | 199.4 | 1.01 |
| 220-320 | 35.2 | 35.8 | 1.02 |
| 320-420 | 55.0 | 70.2 | 1.28 |
| 420-520 | 113.7 | 114.5 | 1.01 |
| 520-620 | 148.8 | 133.9 | 0.90 |
| 620-760 | 23.9 | 18.5 | 0.77 |

That frame is dominated by trail lines and the wall band, so the far grid there
already matches; the loss sits in the near half, and the last row also carries
the cycle-body effect below (green-channel 99th percentile 127 desktop against
15 port).

Line *thickness* is the other half of the "wider in the native build"
observation. `gridprofile.py` on the frame-363 pair: in rows 490-680 the
desktop's median horizontal run of grid-line pixels is 3 px (4-5 px at 480-500)
while the port's is 2 px, and the share of pixels carrying grid signal there is
3.4-3.8 % desktop against 1.1-1.2 % port. Above row 480 the port is equal or
slightly higher. So up close the port's lines are one pixel thinner *and* cover
a third as much area.

Status: the mechanism is texture level-of-detail selection on the floor
texture (the grid is baked into `floor.png`/`floor_a.png`/`floor_b.png`, drawn
with `FLOOR_DETAIL 3`). GL state (mip chain, min filter, anisotropy) has been
measured identical on both sides, so the difference is *which* level the sampler
picks. An explicit-LOD rewrite of the emulation's generated sampling call
(`texture2DLodEXT` with a negative bias, `OES_standard_derivatives` +
`EXT_shader_texture_lod`) is being A/B'ed now; the numbers above are the
before-side.

## 2. Light cycle body — the port never reaches the desktop's highlight level

Same myrec2 pair, whole frame, "saturated blue" = pixel whose blue channel is
the maximum, chroma > 60 and red < 120 (the cycle body and its trail):

| | desktop | port |
|---|---|---|
| blue pixels | 3144 | 3824 |
| mean luminance | 74.9 | 71.1 |
| brightest blue pixel | **164.7** at (row 517, col 583) = rgb 92,165,237 | **114.0** at (row 533, col 622) = rgb 38,114,190 |
| blue pixels with luminance >= 120 | **236** | **0** |
| red-family pixels | 367, brightest 120.3 | 276, brightest 119.3 |

The port draws *more* blue area (its trail is longer in that frame) yet its
cycle body tops out at luminance 114 where the desktop's reaches 165, and not a
single pixel crosses 120. Comparing the two brightest body pixels channel by
channel, the port is at 0.41x red, 0.69x green, 0.80x blue of the desktop - the
red light is the one that is missing, not overall brightness.

In the desktop source the body is lit by two lights set in `src/tron/gCycle.cpp`
with colours `(1, .7, .7)` and `(.7, .7, 1)` and positions `lposa (320,240,200,0)`,
`lposb (-240,-100,200,0)` - `w = 0`, i.e. **directional** vectors. The Web
build's GL layer implements fixed-function lighting by generating a vertex
shader; its per-light block is only emitted when its own table thinks the light
is on:

```glsl
uniform vec4 u_lightDiffuse0; uniform vec4 u_lightPosition0;
vec3 lightDirection = normalize(u_lightPosition0).xyz;
float diffuseI = max(dot(ecNormal, lightDirection), 0.0);
v_color.xyz += ambient + diffuse + specular;
```

The channel pattern (red light contribution absent) points at that table: if
only one of the two light blocks lands in the generated program, the body is lit
by the blue light alone, which is exactly a 0.41/0.69/0.80 shortfall. The next
measurement reads the actual uniform values and the number of light blocks the
browser compiles, and will be appended here.

## Same measurement on the menu screen, 14 Nov

The main menu renders the arena floor behind it on both builds, with no HUD, no
cycle and no wall interference, so it is the cleanest grid surface available.
`keymap-menu-desktop.png` and `keymap-menu-port.png` (both 1280x800, same
camera, same font) measured over the band y 560..700, x < 1000, counting a
pixel as lit when any channel exceeds 40:

| | desktop | port |
|---|---|---|
| lit pixels in the band | **698** | **158** |
| mean intensity of those pixels | 72.4 | **133.8** |
| median horizontal run length | **2 px** | 1 px |
| 90th percentile run length | **7 px** | 2 px |
| number of separate runs | 250 | 103 |

The port draws a quarter of the lit pixels and every run is thinner.  How much
of that is "gone" depends on the threshold, so the same band at four thresholds:

| threshold (max channel) | desktop lit px / crossings | port lit px / crossings |
|---|---|---|
| 40 | 932 / 346 | 141 / 86 |
| 25 | 1259 / 345 | 1033 / 345 |
| 15 | 1553 / 345 | 1360 / 345 |
| 8  | 1706 / 344 | 1590 / 344 |

The crossing count is identical on both sides as soon as the threshold drops to
25: every grid line the desktop draws, the port draws too, at the same place.
What differs is the profile of each line.  Away from the cursor arrow at the
right edge and the status text along the bottom, the brightest floor pixel on a
sampled row is 62 on the desktop against 41 on the port -- the web build's
lines never get as bright -- and the desktop's lines are wide enough that 932
pixels clear 40 where the port only manages 141.  The mean intensity in the
first table is higher on the port simply because at that threshold the
desktop's wide, soft
lines contribute a lot of near-threshold pixels and the port contributes almost
nothing but its own peaks.

Same lines, same places: thinner and dimmer on the web build.  That points at
texture filtering and blending, not at missing geometry or at lighting.

The cycle-body numbers in section 2 were measured before the positional-light
and specular patches went into the build chain (`webgl/patch_light_positional.py`,
`patch_light_specular.py`); they need re-measuring against the current build
before they can be trusted.
