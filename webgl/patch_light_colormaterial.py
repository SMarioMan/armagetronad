#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Feeds the vertex colour into the ambient, diffuse, emission and material-alpha
terms of the generated per-vertex lighting pass.

What this is *not* is the GL colour-material rule.  GL_COLOR_MATERIAL is
disabled by default, nothing in this game calls glColorMaterial or enables the
token, and the desktop state probe settles it: with the current colour at
(1,1,1) the material ambient on the reference driver still reads the GL default
0.2, so no material term there tracks the vertex colour.  The emulation does
implement glMaterialfv -- the generated glue initializes materialDiffuse to 0.8
and uploads it through a uniform -- so this is not missing material state
either.  It is an empirical stand-in for a lighting-state difference that has
not been pinned down: lighting the material the game actually sets (src/tron/
gCycle.cpp sets GL_DIFFUSE and GL_SPECULAR to plain white once per cycle draw,
gCycle.cpp:4338-4339, and the wall polygons in src/tron/gWall.cpp carry nothing
but a vertex colour) comes out brighter than the desktop build.

Measured, on the frame-identical n814 pair at gt 10.06: the wall strip read mean
bright luminance 162 against the desktop's 88.  Measured again after the model
geometry was fixed (see webgl/README.md on the 16-bit element-index limit), on
the frame-exact myrec2 pair -- port n726 gt 2.5523 pos 69.6229 86.6206, desktop
n731 gt 2.5929 pos 69.6130 86.6206, heading identical on both -- inside the
120x110 box over the two opponent cycles:

    crop                 lit px   mean L   max L   px with mean L >= 120
    desktop               2021       34       107                        0
    this patch applied    1402       32       107                        0
    patch reversed        1985       50       202                      183

Whole-frame agreement with the desktop is a tie either way (full-frame mean
luminance 14.24 desktop, 14.29 with the patch, 14.30 without), so the patch
buys the desktop's luminance ceiling and costs 30 % of the lit area the desktop
has in that box.  Neither state is at parity there, and which term of the
emulation's lighting sum is over-bright is still open.  The earlier numbers
quoted in this file (cycle body 10/33/10 against the desktop's 41/49/12, lit
fraction 0.06 against 0.34) were taken while the cycle model was being drawn
with mangled indices and no longer describe the port.

Emission is the term with the strongest evidence: on the frame-exact n363 pair
the desktop shows 3865 saturated green pixels (g>200, r<80, b<80) in the near
band below row 520 and a port that tracks only ambient and diffuse shows none.
Colour material tracks the alpha too, so v_color.w follows the vertex colour and
a fading trail can fade.  SPECULAR stays on u_materialSpecular, and
webgl/patch_light_specular.py already handles shininess zero.
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
