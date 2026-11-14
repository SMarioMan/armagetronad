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

This game's only lights are the two set in src/tron/gCycle.cpp:4341-4346.  Their
positions are `lposa = {320, 240, 200, 0}` and `lposb = {-240, -100, 200, 0}`
(gCycle.cpp:4333-4334), so w == 0 for both: they are *directional*.  The new
branch therefore never fires for shipped content, and this patch is
spec-correctness kept on the shelf rather than the explanation of any current
divergence.  The frame-exact A/B at n363 (gt 2.8494, shots/plA-00.png with the
patch against shots/plB-00.png without it) differs in only a fraction of a
percent of pixels in every floor band and leaves the near field untouched.  An
earlier version of this note claimed it explained the cycle body's shading and
the flattened far-grid contrast; that claim is retracted.

The lighting pass runs in eye space (the generated shader already declares
`vec4 ecPosition = u_modelView * a_position;` and lights with ecNormal), so the
positional form is u_lightPosition.xyz - ecPosition.xyz.  Both lights'
positions are already transformed by the modelview at glLightfv time, as GL
requires.

Measured on the reference driver here (llvmpipe, Mesa 25.2.8, the same library the
desktop build renders with) with the standalone probe in lightprobe.c at the top of
the work directory: a white front-facing quad at z = -3 lit by one positional light
on the view axis reads 208/255 at its centre.  That is the per-vertex value
4/sqrt(2^2 + 2^2 + 4^2) = 0.816; the constant-direction rule would give 255, and
Mesa returns 208 with GL_LIGHT_MODEL_LOCAL_VIEWER set either way.  So the form
below is the one the desktop build is actually lit with, and turning the viewer
flag off is not a lever available to this game: it never calls glLightModel*.

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
