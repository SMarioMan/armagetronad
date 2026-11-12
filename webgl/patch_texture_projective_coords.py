#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Restores the projective divide on texture coordinates, which the emulation
drops entirely.

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

so the mapping degenerates to an affine one over the raw (s,t).  Measured on
a frame-identical pair (same replay, same settings, camera pose d=0.0004):
the floor grid period 34 px on the port against 6 px on a desktop GL driver,
line peak 32 against 86, and the grid dissolves entirely past mid distance.
That is the "grid lines are thin and fade at a distance" symptom.

Emscripten never selects texture2DProj here (genTexUnitSampleExpr only ever
picks texture2D or textureCube), so the divide is simply missing rather than
delegated.  Clamping the divisor instead of letting it reach zero keeps the
rim vertices finite: the sample coordinate saturates at ~1e6, the sampler
picks the top mip level, and the horizon shows the box average of the
texture -- which is what the hardware LOD clamp produces on desktop GL too.
Only 1D/2D samplers are touched; cube maps keep the upstream expression.
"""
import sys

MARKER = 'max(abs(${texCoordExpr}.w),0.000001)'

ANCHOR = (
    'if(TEX_MATRIX_UNIFORM_PREFIX!=null){'
    'texCoordExpr=`(${TEX_MATRIX_UNIFORM_PREFIX}${texUnitID} * ${texCoordExpr})`}'
    'return`${func}(${TEX_UNIT_UNIFORM_PREFIX}${texUnitID}, ${texCoordExpr}.xy)`'
)

REPLACEMENT = (
    'if(TEX_MATRIX_UNIFORM_PREFIX!=null){'
    'texCoordExpr=`(${TEX_MATRIX_UNIFORM_PREFIX}${texUnitID} * ${texCoordExpr})`}'
    'var texCoordArg=(func=="texture2D")?'
    '`(${texCoordExpr}.xy)/max(abs(${texCoordExpr}.w),0.000001)`:'
    '`${texCoordExpr}.xy`;'
    'return`${func}(${TEX_UNIT_UNIFORM_PREFIX}${texUnitID}, ${texCoordArg})`'
)


def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    if MARKER in data:
        print('patch_texture_projective_coords: already patched')
        return
    if ANCHOR not in data:
        sys.exit('ERROR: TexEnvJIT sampler coordinate expression not found '
                 '(generated shader emitter changed?)')
    data = data.replace(ANCHOR, REPLACEMENT, 1)
    open(path, 'w').write(data)
    print('patch_texture_projective_coords: applied projective texture '
          'coordinate divide')


if __name__ == '__main__':
    main()
