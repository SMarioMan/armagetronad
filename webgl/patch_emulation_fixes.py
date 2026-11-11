#!/usr/bin/env python3
"""Post-build fixes for the emscripten GL legacy emulation in the generated JS.

The legacy GL emulation (libglemu.js, embedded in src/armagetronad_main.js)
compiles a fixed-function "FFP" shader for fog / per-vertex lighting /
texture-environment.  Several bugs in that shader produce visual artifacts:

1. Fragment-precision banding (the big one).
   createRenderer() builds the FFP fragment shader with a HARDCODED
   `precision mediump float;`.  On desktop WebGL this forces 16-bit
   (half-float) color / fog interpolation, which quantizes smooth
   gradients into visible bands: the light-bike trail shows bars instead
   of a smooth fade, the floor lighting bands, and per-vertex alpha snaps
   (walls vanish at odd moments).  The default (non-FFP) shader already
   uses the correct `#ifdef GL_FRAGMENT_PRECISION_HIGH` guard; the FFP
   shader does not.  Promote it to highp.

   Symptom -> cause mapping:
   - "visible bars within the light bike trails"  -> mediump fog/color banding
   - "improper shading"                            -> mediump color quantization
   - "odd light artifact on the floor"             -> mediump fog banding
   - "walls disappear at odd or improper times"    -> per-vertex alpha snapping
"""
import sys


def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    orig = data
    changes = []

    # ---- fix 1: FFP fragment shader precision mediump -> highp ----
    # The FFP fragment shader is declared as:
    #   var fsSource=["precision mediump float;",texUnitVaryingList,...
    # The default (non-FFP) shader uses an #ifdef guard and is left untouched.
    needle = 'fsSource=["precision mediump float'
    if needle in data:
        data = data.replace(needle, 'fsSource=["precision highp float', 1)
        changes.append('FFP fragment precision mediump -> highp')
    elif 'fsSource=["precision highp float' in data:
        pass  # already fixed
    else:
        sys.exit('ERROR: FFP fsSource precision needle not found')

    if data == orig:
        print('patch_emulation_fixes: no changes needed (already applied)')
        return
    open(path, 'w').write(data)
    print('patch_emulation_fixes: applied ' + '; '.join(changes))


if __name__ == '__main__':
    main()
