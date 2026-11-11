#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Adds immediate-mode GL_QUAD_STRIP (mode 8) support, which upstream
libglemu.js does not implement (it aborts with
"unsupported immediate mode 8").  Armagetron draws player walls with
glBegin(GL_QUAD_STRIP) (gWall.cpp, BeginQuadStrip), so without this patch
the first wall drawn aborts the runtime.

Two changes, both in src/armagetronad_main.js:

1. GL.generateTempBuffers: in addition to the pre-generated GL_QUADS index
   pattern (0,1,2, 0,2,3, 4,5,6, ...), pre-generate the GL_QUAD_STRIP
   pattern.  Strip quad k over vertices (v, v+1, v+2, v+3) has corner
   order (v, v+1, v+3, v+2) -> triangles (v, v+1, v+3) and
   (v, v+3, v+2); the next quad starts 2 vertices later:
   (0,1,3, 0,3,2, 2,3,5, 2,5,4, 4,5,7, 4,7,6, ...)

2. GLImmediate.flush: the mode > 6 branch.  Mode 7 (GL_QUADS) keeps the
   existing path (quad k at index-buffer byte offset 12k = 3 * 4k).
   Mode 8 (GL_QUAD_STRIP): N vertices form floor(N/2)-1 quads, strip quad
   k at byte offset 12 * (k) = 6 * 2k, so ptr = firstVertex * 6.
"""
import sys

def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    if 'tempQuadStripIndexBuffer' in data:
        print('patch_gl_emulation: already patched')
        return
    orig = data
    # ---- change 1: pre-generate the quad-strip index pattern ----
    anchor = 'context.tempQuadIndexBuffer=GLctx.createBuffer()'
    i = data.find(anchor)
    if i < 0:
        sys.exit('ERROR: tempQuadIndexBuffer creation not found')
    # end of the if(quads){...} block: the bufferData/bindBuffer(null) tail
    tail = 'context.GLctx.bufferData(34963,quadIndexes,35044);context.GLctx.bindBuffer(34963,null)}'
    j = data.find(tail, i)
    if j < 0:
        sys.exit('ERROR: tempQuadIndexBuffer tail not found')
    j += len(tail)
    strip_block = (
        'context.tempQuadStripIndexBuffer=GLctx.createBuffer();'
        'context.GLctx.bindBuffer(34963,context.tempQuadStripIndexBuffer);'
        'var numStripIndexes=GL.MAX_TEMP_BUFFER_SIZE>>1;'
        'var stripIndexes=new Uint16Array(numStripIndexes);'
        'var i=0,v=0;while(1){stripIndexes[i++]=v;if(i>=numStripIndexes)break;'
        'stripIndexes[i++]=v+1;if(i>=numStripIndexes)break;'
        'stripIndexes[i++]=v+3;if(i>=numStripIndexes)break;'
        'stripIndexes[i++]=v;if(i>=numStripIndexes)break;'
        'stripIndexes[i++]=v+3;if(i>=numStripIndexes)break;'
        'stripIndexes[i++]=v+2;if(i>=numStripIndexes)break;v+=2}'
        'context.GLctx.bufferData(34963,stripIndexes,35044);'
        'context.GLctx.bindBuffer(34963,null);'
    )
    data = data[:j] + strip_block + data[j:]

    # ---- change 2: mode-8 branch in flush ----
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
        'var numQuads=(numVertices>>1)-1;if(numQuads<1)numQuads=0;'
        'numIndexes=numQuads*6;ptr=GLImmediate.firstVertex*6;'
        'GLctx.bindBuffer(GLctx.ELEMENT_ARRAY_BUFFER,'
        'GL.currentContext.tempQuadStripIndexBuffer);emulatedElementArrayBuffer=true;'
        'GLImmediate.mode=GLctx.TRIANGLES}'
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
