#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Restores the projective divide on texture coordinates, which the emulation
drops entirely, in the one form that also keeps the sampler's level-of-detail
rule intact.

ArmageTron draws its floor (and the sky) as an "infinite" plane:
src/engine/eDisplay.cpp infinity_xy_plane() emits the near fan vertex as
glTexCoord4f(x, y, h, 1) and every rim vertex as glTexCoord4f(dx, dy, 0, 0),
and then scales the *texture* matrix by 1/GridSize (eDisplay.cpp
rFLOOR_TEXTURE branch).  Desktop GL turns a 4-component texture coordinate
into a sample position by dividing by its own q component, so the texture
lands with a smooth projective falloff: one repeat per GridSize world units
across the whole plane, from directly under the camera to the horizon.

The generated FFP shader ignores the coordinate's fourth component:

  texture2D(u_texUnit0, (u_textureMatrix0 * v_texCoord0).xy)

so the mapping degenerates to an affine one over the raw (s,t): the grid period
came out 34 px on the port against 6 px on a desktop GL driver, line peak 32
against 86, and the grid dissolved past mid distance.

Dividing by q *in the expression handed to texture2D* fixes the mapping but
breaks the mip selection.  WebGL derives the sampler LOD from the screen-space
partial derivatives of that expression, and a divisor clamped away from zero
makes the expression locally constant wherever the clamp is active -- right
along the horizon, where the rim vertices carry q = 0.  Derivative 0 means LOD
0, so the far floor kept sampling full base-level detail.  Measured on the
frame-identical pair hpbw5-02 / hppw6-02 (pose delta 0.02 world units, four
depth bands, mean grid-line run length in px / 95th-percentile line peak):

  band(near..far)   desktop GL        this port, clamped divide
  0                 1.30 /  82        1.42 / 114
  1                 1.00 /  96        3.50 / 112
  2                 2.31 / 152        4.13 / 157
  3                 0.77 /  24        3.65 /  94

i.e. the far grid stayed 4.7 px wide and nearly as bright as the near grid
instead of dropping to the box average of the texture.  The same saturation
also explains the wall body: at grazing angles the bolt pattern sampled mip 0
and aliased into dark smears instead of fading.

texture2DProj is the form that keeps both halves of the contract: it divides by
w inside the sampler, and GLSL ES gives it a derivative rule that never
differentiates through the divide -- the partials are formed from the numerator
and denominator separately, so q -> 0 drives the LOD to the top mip level
exactly as the hardware LOD clamp does on desktop GL.  At that level a 256x256
texture has collapsed to a single texel, so the sample position no longer
matters and q = 0 is harmless.  Only 1D/2D samplers are touched; cube maps keep
the upstream expression, and ordinary 2-component coordinates arrive as w = 1
where texture2DProj is identical to texture2D.
"""
import os
import sys

# Which form the projective divide takes.  Experiment switch, not a feature:
#   proj (default) - texture2DProj: divide and LOD rule inside the sampler.
#   div            - plain texture2D with the divide written out, so the
#                    sampler differentiates the normalized coordinate
#                    (correct LOD on steep far surfaces) but the q = 0 rim
#                    vertices produce NaN samples.
MODE = os.environ.get('AGTK_PROJMODE', 'proj').strip().lower()

MARKER = 'var sampler=(func=="texture2D")?"texture2DProj":func;'

ANCHOR = (
    'if(TEX_MATRIX_UNIFORM_PREFIX!=null){'
    'texCoordExpr=`(${TEX_MATRIX_UNIFORM_PREFIX}${texUnitID} * ${texCoordExpr})`}'
    'return`${func}(${TEX_UNIT_UNIFORM_PREFIX}${texUnitID}, ${texCoordExpr}.xy)`'
)

# Note on level-of-detail: the optional bias argument of texture2D/texture2DProj
# is not part of GLSL ES 1.00 (it arrived in ES 3.00).  Emitting it makes every
# textured program fail to link and the whole scene render black; measured on a
# replay frame, mean frame luminance dropped from 10.06 to 0.14.  Bias is not
# the lever here anyway: the engine never asks for anisotropic filtering (no
# anisotropy code exists in src/) and never asks for multisampling, so neither
# build has either, and the port's remaining coarser floor sampling is mip
# selection inside the WebGL sampler, not a missing filter request.

REPLACEMENT = (
    'if(TEX_MATRIX_UNIFORM_PREFIX!=null){'
    'texCoordExpr=`(${TEX_MATRIX_UNIFORM_PREFIX}${texUnitID} * ${texCoordExpr})`}'
    'var sampler=(func=="texture2D")?"texture2DProj":func;'
    'return`${sampler}(${TEX_UNIT_UNIFORM_PREFIX}${texUnitID}, ${texCoordExpr})`'
)

DIV_REPLACEMENT = (
    'if(TEX_MATRIX_UNIFORM_PREFIX!=null){'
    'texCoordExpr=`(${TEX_MATRIX_UNIFORM_PREFIX}${texUnitID} * ${texCoordExpr})`}'
    'return`${func}(${TEX_UNIT_UNIFORM_PREFIX}${texUnitID}, '
    '(${texCoordExpr}.xy)/${texCoordExpr}.w)`'
)


def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    if MODE not in ('proj', 'div'):
        sys.exit('ERROR: AGTK_PROJMODE must be proj or div (got %r)' % MODE)
    div_marker = '/*PROJECTIVE_DIV_MARKER*/return`'
    if MARKER in data or div_marker in data:
        print('patch_texture_projective_coords: already patched')
        return
    if ANCHOR not in data:
        sys.exit('ERROR: TexEnvJIT sampler coordinate expression not found '
                 '(generated shader emitter changed?)')
    repl = REPLACEMENT
    if MODE == 'div':
        repl = DIV_REPLACEMENT.replace('return`', div_marker, 1)
    data = data.replace(ANCHOR, repl, 1)
    open(path, 'w').write(data)
    print('patch_texture_projective_coords: projective divide form = %s' % MODE)


if __name__ == '__main__':
    main()
