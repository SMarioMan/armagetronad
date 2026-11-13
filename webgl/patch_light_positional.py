#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Distinguishes positional lights from directional ones in the generated
per-vertex lighting pass.

The emulation always generated

    vec3 lightDirection = normalize(u_lightPositionN).xyz;

which is the fixed-function rule for an *infinitely far* light (w == 0).  For a
positional light (w == 1) the spec says the direction is the light position
minus the vertex position, both in eye coordinates.  Treating a positional
light as directional points it from the eye-space origin -- i.e. from the
camera -- so every surface that faces the camera is lit as if a lamp sat behind
the viewer, and the light's actual location is ignored entirely.

Armagetron uses positional lights: src/tron/gCycle.cpp places GL_LIGHT0 and
GL_LIGHT1 next to the bike (glLightfv(GL_POSITION) at the cycle's own
coordinates, w = 1) and never touches the attenuation factors, so the default
constant-only attenuation applies and the position itself is the whole
distance falloff.  The lighting pass runs in eye space (the generated shader
already declares `vec4 ecPosition = u_modelView * a_position;` and lights with
ecNormal), so the positional form is u_lightPosition.xyz - ecPosition.xyz.
Both lights' positions are already transformed by the modelview at glLightfv
time, as GL requires.

That is the mechanism behind the two differences reported against the desktop
build: the cycle body, which should be lit from beside the bike, is lit from
the camera instead, and a lit textured floor plane picks up a broad wash from
the same misplaced lights, flattening the grid's contrast.  The measured
before-symptom on a replayed pair at the same game time, pose and canvas size
(mid-distance band): the grid line peaked 52 levels above the floor against 177
on the desktop, and the dark gap between lines measured 41 against 1.  This
patch restores the specified behaviour; how much of the reported picture that
on its own accounts for is measured separately.

The half-vector stays `lightDirection + vec3(0,0,1)`: GL's default half-vector
uses the headlight eye vector for a vertex at the eye point, which is what the
emulation already emits, and the two lights are close enough to the geometry
that the approximation is the same one the desktop driver makes.
"""
import sys

PATCHES = [
    ('vsLightingPass+="    vec3 lightDirection = normalize(u_lightPosition"+lightId+").xyz;";',
     'vsLightingPass+="    vec3 lightDirection = (u_lightPosition"+lightId+".w < 0.5) '
     '? normalize(u_lightPosition"+lightId+".xyz) '
     ': normalize(u_lightPosition"+lightId+".xyz - ecPosition.xyz);";'),
]


def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    if '- ecPosition.xyz);' in data:
        print('patch_light_positional: already patched')
        return
    missing = [old for old, _ in PATCHES if old not in data]
    if missing:
        sys.exit('ERROR: light direction emitter text not found: ' + repr(missing))
    for old, new in PATCHES:
        data = data.replace(old, new, 1)
    open(path, 'w').write(data)
    print('patch_light_positional: applied positional vs directional light '
          'distinction (w == 0 directional, otherwise light - vertex)')


if __name__ == '__main__':
    main()
