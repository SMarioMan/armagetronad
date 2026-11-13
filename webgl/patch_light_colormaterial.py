#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Tracks GL_COLOR_MATERIAL in the generated per-vertex lighting pass.

Desktop GL enables colour material by default, for face GL_FRONT_AND_BACK with
glColorMaterial parameters (R,G,B,A) all true.  Mesa applies that per vertex:
the vertex colour replaces the RGB of the material's AMBIENT, DIFFUSE and
EMISSION (and the alpha of all three), while SPECULAR keeps its RGB from
glMaterialfv and only its alpha, plus SHININESS, follow the colour's alpha.
Armagetron relies on that default everywhere it lights anything:
src/tron/gCycle.cpp sets GL_DIFFUSE and GL_SPECULAR to plain white once per
cycle draw (gCycle.cpp:4338-4339) and then colours the body per vertex, and the
wall polygons in src/tron/gWall.cpp carry nothing but a vertex colour.

The emulation instead feeds the raw glMaterialfv state into every lighting
term (u_materialAmbient / u_materialDiffuse / u_materialEmission) and never
implements colour material at all -- there is no GL_COLOR_MATERIAL token in the
generated file.  Measured on frame-identical replays:
  * cycle body strip, before any colour-material work: R/G/B = 10/33/10 against
    the desktop driver's 41/49/12, lit fraction 0.06 against 0.34;
  * wall strip on the n814 pair at gt 10.06 with the material terms lit white:
    mean bright luminance 162 against the desktop's 88.
An earlier revision of this file tracked only AMBIENT and DIFFUSE, which is the
common case but not the whole rule.  The remaining two terms show up as the
user's two complaints:
  * EMISSION never gets the vertex colour, so the port has no self-lit colour
    anywhere: in the frame-exact n363 pair the desktop shows 3865 saturated
    green pixels (g>200, r<80, b<80) in the near band below row 520, where the
    player's own cycle and its rays sit, and the port shows exactly zero there
    while matching in the mid band (chroma 234 against 229); the channel
    extremes lose blue the same way (max blue 107 against 253).
  * the material alpha keeps coming from glMaterialfv, so a per-vertex fading
    trail cannot fade: v_color.w is built from u_materialDiffuse.w.
Colour material tracks the alpha too, so v_color.w follows the vertex colour.
SPECULAR stays on u_materialSpecular because colour material leaves its colour
alone and webgl/patch_light_specular.py already handles shininess zero.
"""
import sys

PATCHES = [
    # global ambient * material ambient  ->  global ambient * vertex colour
    ('vsLightingPass+="  v_color.xyz += u_lightModelAmbient.xyz * u_materialAmbient.xyz;";',
     'vsLightingPass+="  v_color.xyz += u_lightModelAmbient.xyz * a_color.xyz;";'),
    # per-light ambient
    ('vsLightingPass+="    vec3 ambient = u_lightAmbient"+lightId+".xyz * u_materialAmbient.xyz;";',
     'vsLightingPass+="    vec3 ambient = u_lightAmbient"+lightId+".xyz * a_color.xyz;";'),
    # per-light diffuse
    ('vsLightingPass+="    vec3 diffuse = diffuseI * u_lightDiffuse"+lightId+".xyz * u_materialDiffuse.xyz;";',
     'vsLightingPass+="    vec3 diffuse = diffuseI * u_lightDiffuse"+lightId+".xyz * a_color.xyz;";'),
    # emission: colour material tracks the material emission as well, so a lit
    # primitive carries its own colour even where no light reaches it
    ('vsLightingPass+="  v_color.xyz = u_materialEmission.xyz;";',
     'vsLightingPass+="  v_color.xyz = a_color.xyz;";'),
    # material alpha of the tracked components follows the vertex colour, which
    # is what lets trails fade out along their length
    ('vsLightingPass+="  v_color.w = u_materialDiffuse.w;";',
     'vsLightingPass+="  v_color.w = a_color.w;";'),
]

# Marker of a fully applied patch, used to keep the script idempotent.
MARKER = 'v_color.xyz = a_color.xyz;'


def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    if MARKER in data and 'u_lightModelAmbient.xyz * a_color.xyz' in data:
        print('patch_light_colormaterial: already patched')
        return
    missing = [old for old, _ in PATCHES if old not in data]
    if missing:
        sys.exit('ERROR: lighting pass emitter text not found: ' + repr(missing))
    for old, new in PATCHES:
        data = data.replace(old, new, 1)
    open(path, 'w').write(data)
    print('patch_light_colormaterial: applied GL_COLOR_MATERIAL tracking '
          '(ambient, diffuse, emission and alpha from the vertex colour)')


if __name__ == '__main__':
    main()
