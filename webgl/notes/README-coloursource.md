# Light-cycle body shading: live build vs candidate fix

Pairing: the same `perp2.aarec` recording (recorded on the native build, capped at
60 fps, `MAX_FPS 60`) played back on both builds. The camera pose the page reached
is identical to the native run in the first digits (`n352`, game time 2.6519,
camera 107.8338 69.5988), so the two frames are directly comparable.

Images in this directory:

- `cycle_body_4way.png` - 4x crop (x300-470, y440-560) around the followed cycle.
  Desktop paints a bright neutral body; the live port paints **no body at all** in
  this box; the candidate fix paints the body again, but still too dark and tinted.
- `grid_cycle_wholeframe.png` - the three full frames stacked (desktop, live port,
  candidate fix).
- `floorgrid_crop3.png` - same three frames, floor crop x0-700 y520-780 at 2x, for
  the grid-line complaint.

## What changed

`webgl/patch_light_colormaterial.py` used to replace the generated lighting pass's
material terms with the vertex colour unconditionally:

    v_color.xyz = a_color.xyz * (ambient + diffuse + emission)

The emulator only feeds `a_color` when the draw actually binds a colour array. A
model draw (the cycle body, loaded through the model loader) has the colour
attribute **disabled**, so `a_color` there is not the cycle's colour at all: it is
whatever `glColor` was last set to in the immediate renderer. Measured on the page
side that stale value is `(1, 0, 1, 1)`, so every body pixel got its green channel
thrown away, and the alpha came from the same stale register - an alpha of 0 from a
fading trail makes the body disappear outright, which is the "light cycles shade
invisible" symptom.

The patch now uploads one extra int per lit draw -
`GLctx.getVertexAttrib(colorLocation, ATTRIB_ARRAY_ENABLED)` - and the shader picks
the vertex colour only when that is true, otherwise the material uniforms
(`u_materialAmbient/Diffuse/Emission/Specular`, `u_materialDiffuse.w`), which is
what the desktop build shades with:

    v_color.xyz = (u_colorFromArray != 0) ? a_color.xyz : u_materialEmission.xyz + ...

Desktop reference values at the body draw, read by the C++ probe in
`gCycle.cpp` (native stderr, `PF BODY` at gt 2.8486 / 2.8651 / 2.8813):
`lighting=1 light0=1 light1=1 texturing=1 alphatest=1`,
`mamb 0.2 mdif 1.0 mspec 1.0 mshine 0.0 memis 0.0`, current colour
`0.0 0.7 0.0` (the team colour - proof that the body draw has no colour array
bound and that its vertex colour is not what shading should use).

## Measured, pose-matched frames (1280x800)

| build | mean abs diff vs desktop | px > 12 | px > 40 | bright body-like px in x250-760 y430-640 | mean green of those px | magenta-signature px (whole frame) |
|---|---|---|---|---|---|---|
| desktop at 2.6162 (desktop-to-desktop reference pair) | 1.26 | 1.04% | 0.53% | 2826 | 227 | 109 |
| live site: unconditional colour source | 3.73 | 4.13% | 3.93% | 2286 | 231 | 161 |
| candidate: conditional colour source | 3.40 | 3.73% | 3.49% | 2714 | 226 | 118 |
| emulation without any colour-source patch | 4.63 | 5.89% | 4.20% | 2287 | 231 | 108 |

The desktop pair (two native frames 0.075 s apart) shows how much of the residual
is scene movement rather than rendering: 1.26 mean.

Still open, visible in `cycle_body_4way.png`: with the fix the body is present but
much darker than the desktop and carries a violet cast. That is a lighting-input
difference, not a colour-source difference, and it is being tracked separately.
