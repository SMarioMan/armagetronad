#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Tracks GL_COLOR_MATERIAL in the generated per-vertex lighting pass.

Desktop GL initialises colour material ENABLED, tracking
GL_FRONT_AND_BACK / GL_AMBIENT_AND_DIFFUSE: every Vertex call pushes the
current raster colour into the material's ambient and diffuse components, and
only specular and shininess stay under explicit glMaterialfv control.
Armagetron relies on that default -- src/tron/gCycle.cpp sets
GL_DIFFUSE/GL_SPECULAR to plain white once per cycle draw and lets the
per-vertex body colours carry the material, so the two lights in that
function (GL_LIGHT0 diffuse (1,.7,.7), GL_LIGHT1 (.7,.7,1)) light a
*coloured* material and the body comes out dark olive with a visible shading
gradient.

The emulation instead multiplies the light terms by the raw material state
(u_materialAmbient / u_materialDiffuse), which for the cycle is that white.
On the same frame-identical replay the body strip measured R/G/B = 10/33/10
against the desktop driver's 41/49/12 with a lit fraction of 0.06 against
0.34: the port lights a white material, so the light terms dominate, the
vertex colours are lost, and the shading gradient flattens.  Replacing the
two components colour material tracks (ambient, diffuse) with the incoming
vertex colour reproduces the desktop behaviour; specular stays on
u_materialSpecular because colour material does not track it and gCycle.cpp
sets it explicitly.
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
]


def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    if 'u_lightModelAmbient.xyz * a_color.xyz' in data:
        print('patch_light_colormaterial: already patched')
        return
    missing = [old for old, _ in PATCHES if old not in data]
    if missing:
        sys.exit('ERROR: lighting pass emitter text not found: ' + repr(missing))
    for old, new in PATCHES:
        data = data.replace(old, new, 1)
    open(path, 'w').write(data)
    print('patch_light_colormaterial: applied GL_COLOR_MATERIAL tracking '
          '(ambient+diffuse from the vertex colour)')


if __name__ == '__main__':
    main()
