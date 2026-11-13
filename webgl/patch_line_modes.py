#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Fixes the primitive-mode mapping of the two line loops.  Desktop GL numbers
primitives  GL_LINE_STRIP 2, GL_LINE_LOOP 3;  WebGL numbers them the other way
round, GL_LINE_LOOP 0x0002, GL_LINE_STRIP 0x0003.  The emulation stores the
mode the game asked for verbatim (GLImmediate.mode = mode) and passes it to
gl.drawArrays/gl.drawElements unchanged, so on the port:

  - a wall-top GL_LINE_STRIP is drawn as a closed loop: the last vertex is
    joined back to the first, painting a stray edge across the piece, and
  - a GL_LINE_LOOP is drawn as an open strip: its closing edge is missing.

Both are used by the display-list line pass of the player walls (gWall.cpp,
BeginLines) and by immediate LINE_LOOP draws, so the wall outlines the port
draws are not the wall outlines the desktop draws.

One textual change, in src/armagetronad_main.js: every assignment
GLImmediate.mode = mode (three sites: glBegin, glDrawArrays and the
drawElements path) routes the two swapped enums through each other.  Points,
lines and the triangle family share numbering in both APIs and are left alone;
GL_QUADS and GL_QUAD_STRIP are handled by patch_gl_emulation.py.

LINE_PATCH=0 in the environment reverses the change instead of applying it, so
the same build tree can be measured with and without it.
"""
import os
import sys

SWAPPED = 'GLImmediate.mode=(mode===2?3:mode===3?2:mode);'
PLAIN = 'GLImmediate.mode=mode;'


def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    off = os.environ.get('LINE_PATCH', '1') == '0'

    if off:
        if SWAPPED in data:
            open(path, 'w').write(data.replace(SWAPPED, PLAIN))
            print('patch_line_modes: LINE_PATCH=0, reverted %d sites'
                  % data.count(SWAPPED))
        else:
            print('patch_line_modes: LINE_PATCH=0, nothing to revert')
        return

    if SWAPPED in data:
        print('patch_line_modes: already patched')
        return
    n = data.count(PLAIN)
    if n != 3:
        sys.exit('ERROR: expected 3 GLImmediate.mode=mode; sites, found %d' % n)
    open(path, 'w').write(data.replace(PLAIN, SWAPPED))
    print('patch_line_modes: swapped LINE_STRIP/LINE_LOOP enums at 3 sites')


if __name__ == '__main__':
    main()
