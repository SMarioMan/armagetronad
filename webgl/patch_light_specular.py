#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Suppresses the specular term when the shininess is zero, the way desktop GL
defines it.

GL fixes the specular exponent: a shininess of 0 means no highlight at all, and
the drivers used as the reference here implement that by zeroing the
specular coefficient.  ArmageTron relies on it -- src/tron/gCycle.cpp sets
GL_SHININESS to 0 and leaves GL_SPECULAR at (1,1,1), so the desktop build
renders the light cycle body with no highlight, only the two-light diffuse
gradient.

The emulation computes the exponent as ``exp(shininess * log(N.H))``, which is
exactly 1 when the shininess is 0, so the guard `(diffuseI > 0 && specularI > 0)`
lets a full-intensity, light-coloured highlight land on every vertex that faces
the light at all.  Measured on the frame-identical pair hpbw5-02 / hppwf-00
(same replay, camera pose within 0.9 world units) inside the cycle body box:

  native   lit 971 px of 65536 (1.5%)   mean green  70
  port     lit 4665 px          (7.1%)  mean green 254   (saturated)

i.e. the body blows out to white instead of shading, which is the "light cycle
shading is completely different" symptom.  The far floor is affected the same
way: depth band 3 measured 13.7 grid crossings at peak 152 against the desktop
build's 2.5 at peak 24.

Adding the exponent guard in the vertex lighting pass restores the desktop
result without touching the diffuse or ambient terms.
"""
import sys

ANCHOR = ('vsLightingPass+="    vec3 specular = specularI * u_lightSpecular"'
          '+lightId+".xyz * u_materialSpecular.xyz;";')

REPLACEMENT = ('vsLightingPass+="    vec3 specular = (u_materialShininess > 0.0 ? '
               'specularI : 0.0) * u_lightSpecular"+lightId+".xyz * '
               'u_materialSpecular.xyz;";')

MARKER = 'vec3 specular = (u_materialShininess > 0.0 ? specularI : 0.0)'


def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    if MARKER in data:
        print('patch_light_specular: already patched')
        return
    n = data.count(ANCHOR)
    if n == 0:
        sys.exit('ERROR: lighting pass specular emitter text not found '
                 '(generated shader emitter changed?)')
    data = data.replace(ANCHOR, REPLACEMENT)
    open(path, 'w').write(data)
    print('patch_light_specular: gated the specular term on a non-zero '
          'shininess (%d site%s)' % (n, '' if n == 1 else 's'))


if __name__ == '__main__':
    main()
