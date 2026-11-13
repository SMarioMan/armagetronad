#!/usr/bin/env python3
"""Post-build patch for the emscripten GL legacy emulation in the generated JS.

Cut line primitives at the eye plane in the immediate-mode path, the way
desktop GL does.

Armagetron folds the camera into the projection matrix (the PF PROJ probe
reports the projection carrying the camera translation on both builds), so in
clip space the eye plane sits at z = -w, and a segment has clip-space w of
opposite signs at its two ends whenever it runs back past the eye.  This port's
renderer does not hand those segments to a driver that clips them: measured in
a bare WebGL1 page with the very matrices of replay frame 363, a line primitive
whose segment straddles that plane paints 0 of 4017 probed pixels with no GL
error, while a triangle under the same matrix clips correctly at 4004/4017.
The game's own draw of such a segment therefore vanishes, and the desktop's
does not.

src/render/rRender.h (atglDrawArrays) already cuts the segments that reach it
through client arrays, which is what the wall quads, the wall top edges and the
floor plane go through.  It cannot help the draws that stay on the immediate
path, and that is most of the line geometry: the game calls
glBegin/glVertex2f/glVertex3f/glEnd directly in engine/eDisplay.cpp,
tron/gSparks.cpp, tron/gExplosion.cpp, tron/gWinZone.cpp, engine/ePath.cpp,
render/rFont.cpp, ui/uMenu.cpp and the debug-line helpers.  On the frame-exact
n363 pair of the perimeter replay (desktop pos 107.8338 69.5185 h 16.4540, port
107.8338 69.5183 h 16.4542) that is what the missing near-field green looks
like: the desktop has saturated green pixels (g>200, r<80, b<80) reaching row
775 across all 1280 columns while the port has none below row 520, and the C++
census of the primitives the game itself issues matches between the two builds
in that window (LINES 88 against 88, TRIANGLE_FAN 22 against 22), so the
segments are submitted here and lost below.

Counting the straddling segments themselves at the n363 gate, with the port's
real projection and modelview captured from the running game: 171 line segments
have clip-space w of opposite sign at their two ends, and the desktop's GL
rasterizer draws 99 of them (58 %, 1492 pixels of coverage). Per primitive: 63
of 271 LINE_LOOP edges and 26 of 104 LINES pairs split across the plane, plus 91
whole loops dropped. The port draws none of them.

This patch puts the cut where it covers every immediate-mode draw: GLImmediate
has the finished vertex block at glEnd, with the attributes already interleaved,
so the cut runs there before the draw is issued.  Line-family primitives come
out as clipped GL_LINES segment pairs, because a strip with a piece removed
would otherwise connect across the gap; every other primitive is left exactly
as it was.  Triangles need nothing, they clip correctly in this renderer.

Two diagnostics, both read from the page rather than the build, so a measurement
can toggle the patch without rebuilding it (same convention as the projection
switch in patch_texture_projective_coords):
  window.arma_lineclip_debug  print the vertex layout found at the first draws
                              plus a keep/drop/split tally per draw
  window.arma_lineclip_off    run without the cut, for the A/B; a live page
  can ask for the same thing with &nolineclip=1 in its URL
"""
import sys

GL_END_HEAD = "var _emscripten_glEnd=()=>{"
GL_END_ANCHOR = (GL_END_HEAD + "GLImmediate.prepareClientAttributes("
                 "GLImmediate.rendererComponents[GLImmediate.VERTEX],true);"
                 "GLImmediate.firstVertex=0;"
                 "GLImmediate.lastVertex=GLImmediate.vertexCounter/(GLImmediate.stride>>2);"
                 "GLImmediate.flush();")

HELPER = r"""
GLImmediate.atglClipLinePrimitives=function(){
  var mode=GLImmediate.mode;
  // GL_LINES 1, GL_LINE_LOOP 2, GL_LINE_STRIP 3.  Every other primitive keeps
  // the emulated path exactly as it was: triangles clip correctly here.
  if(mode!==1&&mode!==2&&mode!==3)return;
  var stride=GLImmediate.stride,fl=stride>>2;
  if(fl<2)return;
  var n=(GLImmediate.vertexCounter/fl)|0;
  if(n<2)return;
  var src=GLImmediate.vertexData;
  if(!src||src.length<n*fl)return;
  var pos=GLImmediate.clientAttributes[GLImmediate.VERTEX];
  if(!pos||pos.size!==4)return;              // begin/end always stores x,y,z,w
  var po=pos.offset>>2;
  if(po<0||po+4>fl)return;
  // Float attributes to carry along the cut.
  var attrs=[],posAttr=null,s,a;
  for(s=0;s<GLImmediate.NUM_ATTRIBUTES;s++){
    a=GLImmediate.clientAttributes[s];
    if(a&&a.size&&a.type===GLctx.FLOAT){
      var o=a.offset>>2,c=a.size;
      if(o>=0&&o+c<=fl){attrs.push({o:o,c:c});if(o===po)posAttr=attrs[attrs.length-1];}
    }
  }
  if(!attrs.length||!posAttr)return;
  // Clip coordinates: the emulation owns the modelview and the projection
  // itself (WebGL has no fixed-function matrices) and feeds both to the shader,
  // so clip space here is projection * modelview * position, column major.
  var mv=GLImmediate.matrix[0],pj=GLImmediate.matrix[1],m=new Float32Array(16);
  for(var col=0;col<4;col++)for(var row=0;row<4;row++)
    m[col*4+row]=pj[row]*mv[4*col]+pj[4+row]*mv[4*col+1]+
                 pj[8+row]*mv[4*col+2]+pj[12+row]*mv[4*col+3];
  var zw=[0,0];
  function clipOf(i){
    var b=i*fl+po,x=src[b],y=src[b+1],z=src[b+2],w=src[b+3];
    zw[0]=m[2]*x+m[6]*y+m[10]*z+m[14]*w;
    zw[1]=m[3]*x+m[7]*y+m[11]*z+m[15]*w;
    if(!(zw[0]===zw[0]&&zw[1]===zw[1])){zw[0]=0;zw[1]=-1;}   // odd input: keep
  }
  // In front of the near plane means z + w >= 0; that test also implies w > 0,
  // which is precisely the case this renderer's line clipper fails on.
  var out=GLImmediate.atglClipScratch;
  if(!out||out.length<2*n*fl)
    out=GLImmediate.atglClipScratch=new Float32Array(Math.max(2*n*fl,512));
  var on=0,drop=0,split=0;
  function copyVert(i){
    var b=i*fl;
    for(var k=0;k<fl;k++)out[on*fl+k]=src[b+k];
    on++;
  }
  function copyCut(i,j,t){                   // j pulled up to the cut point
    var bi=i*fl,bj=j*fl,k,q;
    for(k=0;k<fl;k++)out[on*fl+k]=src[bj+k];
    for(q=0;q<attrs.length;q++){
      var o=attrs[q].o,c=attrs[q].c;
      for(k=0;k<c;k++)out[on*fl+o+k]=src[bi+o+k]+(src[bj+o+k]-src[bi+o+k])*t;
    }
    on++;
  }
  var segs=(mode===1)?(n/2)|0:(n-1+(mode===2?1:0));
  for(var e=0;e<segs;e++){
    var u,v;
    if(mode===1){u=2*e;v=2*e+1;}else{u=e;v=(e+1)%n;}
    clipOf(u);var zu=zw[0],wu=zw[1];
    clipOf(v);var zv=zw[0],wv=zw[1];
    var fu=zu+wu>=0,fv=zv+wv>=0;
    if(fu&&fv){copyVert(u);copyVert(v);continue;}
    if(!fu&&!fv){drop++;continue;}
    split++;
    var ff,rr,zo,wo;
    if(fu){ff=u;rr=v;zo=wu+zu;wo=wv+zv;}else{ff=v;rr=u;zo=wv+zv;wo=wu+zu;}
    copyVert(ff);
    var t=zo/(zo-wo);                        // a clipper's own interpolation
    if(!(t>0&&t<1))t=0;
    copyCut(ff,rr,t);
  }
  // Nothing was cut: leave the block and its mode exactly as the game issued
  // them.  A polyline drawn as GL_LINE_STRIP has its joins merged by the
  // rasterizer, and re-emitting the same edges as separate GL_LINES pairs
  // paints extra blended pixels at every join, so the conversion to pairs
  // happens only where a segment really was cut or dropped.
  if(!split&&!drop)return;
  if(on<2){                                  // nothing drawable survived
    GLImmediate.vertexCounter=0;GLImmediate.lastVertex=0;return;
  }
  for(var w2=0;w2<on*fl;w2++)src[w2]=out[w2];
  GLImmediate.vertexCounter=on*fl;
  GLImmediate.lastVertex=on;
  GLImmediate.mode=1;                        // pairs: GL_LINES
  if(window.arma_lineclip_debug){
    GLImmediate.atglClipLogged=(GLImmediate.atglClipLogged|0)+1;
    if(GLImmediate.atglClipLogged<=400)
      console.log('LINECLIP mode '+mode+' n '+n+' -> '+on+' drop '+drop+
                  ' split '+split+' stride '+stride+' posoff '+po+
                  ' attrs '+attrs.map(function(q){return q.o+':'+q.c;}).join(','));
  }
};
"""

CALL = ("if(!(window.arma_lineclip_off||(window.location&&"
        "/[?&]nolineclip/.test(window.location.href)))){"
        "GLImmediate.atglClipLinePrimitives();}")


def main():
    path = sys.argv[1]
    data = open(path, 'r').read()
    if "atglClipLinePrimitives" in data:
        print('patch_line_eyeclip: already patched')
        return
    if GL_END_ANCHOR not in data:
        sys.exit('ERROR: glEnd emitter not found: ' + repr(GL_END_ANCHOR))
    # The helper goes in immediately before glEnd, in the same scope, so it
    # reaches GLImmediate and GLctx exactly like the code around it.
    data = data.replace(GL_END_HEAD, HELPER + "\n" + GL_END_HEAD, 1)
    data = data.replace(GL_END_ANCHOR,
                        GL_END_ANCHOR.replace("GLImmediate.flush();",
                                              CALL + "GLImmediate.flush();"), 1)
    open(path, 'w').write(data)
    print('patch_line_eyeclip: immediate-mode line primitives cut at the eye plane')


if __name__ == '__main__':
    main()
