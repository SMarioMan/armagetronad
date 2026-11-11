#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Adds immediate-mode GL_QUAD_STRIP (mode 8) support, which upstream
libglemu.js does not implement (it aborts with
"unsupported immediate mode 8").  Armagetron draws player walls with
glBegin(GL_QUAD_STRIP) (gWall.cpp, BeginQuadStrip), so without this patch
the first wall drawn aborts the runtime.

The strip's vertex layout is (bottom, top, bottom, top, ...), i.e. exactly
the vertex order of a GL_TRIANGLE_STRIP running along the wall: quad k
corners (v, v+1, v+3, v+2) triangulate to (v, v+1, v+3) and (v, v+3, v+2),
which is what a triangle strip over v, v+1, v+2, ... produces.  So the
implementation only has to retarget the primitive: WebGL draws triangle
strips natively, no index pattern or extra buffer is needed.

One change, in src/armagetronad_main.js: GLImmediate.flush, the mode > 6
branch.  Mode 8 becomes GL_TRIANGLE_STRIP and keeps numIndexes == 0, so
flush() takes its drawArrays(mode, startIndex, numVertices) path; mode 7
(GL_QUADS) keeps upstream's pre-generated quad index pattern.
"""
import sys

def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    if 'GLImmediate.mode==8){GLImmediate.mode=GLctx.TRIANGLE_STRIP}' in data:
        print('patch_gl_emulation: already patched')
        return
    orig = data

    anchor2 = 'else if(GLImmediate.mode>6){'
    i = data.find(anchor2)
    if i < 0:
        sys.exit('ERROR: mode>6 flush branch not found')
    # branch runs until the next 'renderer.prepare()' call
    end = data.find('renderer.prepare()', i)
    if end < 0:
        sys.exit('ERROR: renderer.prepare() not found after mode>6 branch')
    old_branch = data[i:end]
    expected = (
        'else if(GLImmediate.mode>6){if(GLImmediate.mode!=7)'
        'abort("unsupported immediate mode "+GLImmediate.mode);'
        'ptr=GLImmediate.firstVertex*3;var numQuads=numVertices/4;'
        'numIndexes=numQuads*6;GLctx.bindBuffer(GLctx.ELEMENT_ARRAY_BUFFER,'
        'GL.currentContext.tempQuadIndexBuffer);emulatedElementArrayBuffer=true;'
        'GLImmediate.mode=GLctx.TRIANGLES}'
    )
    if old_branch != expected:
        sys.exit('ERROR: unexpected mode>6 branch text: ' + repr(old_branch))
    new_branch = (
        'else if(GLImmediate.mode>6){'
        'if(GLImmediate.mode==8){'
        'GLImmediate.mode=GLctx.TRIANGLE_STRIP}'
        'else if(GLImmediate.mode!=7){abort("unsupported immediate mode "+GLImmediate.mode)}'
        'else{ptr=GLImmediate.firstVertex*3;var numQuads=numVertices/4;'
        'numIndexes=numQuads*6;GLctx.bindBuffer(GLctx.ELEMENT_ARRAY_BUFFER,'
        'GL.currentContext.tempQuadIndexBuffer);emulatedElementArrayBuffer=true;'
        'GLImmediate.mode=GLctx.TRIANGLES}}'
    )
    data = data[:i] + new_branch + data[end:]

    if data == orig:
        sys.exit('ERROR: no changes made')
    open(path, 'w').write(data)
    print('patch_gl_emulation: applied GL_QUAD_STRIP support')

if __name__ == '__main__':
    main()
