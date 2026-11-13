/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2000  Manuel Moos (manuel@moosnet.de)

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
***************************************************************************

*/

#ifndef ArmageTron_RENDER_H
#define ArmageTron_RENDER_H

#include "defs.h"
#include "rGL.h"
#include <vector>

#ifndef DONTDOIT
#define glBegin        #error glBegin disabled
#define glEnd          #error glEnd   disabled
#define glMatrixMode   #error glEnd   disabled
#endif

class rRenderer{
public:
    rRenderer();
    virtual ~rRenderer();

    virtual void Vertex(REAL x, REAL y)                 = 0;
    virtual void Vertex(REAL x, REAL y, REAL z)         = 0;
    virtual void Vertex3(REAL *x)                       = 0;
    virtual void Vertex(REAL x, REAL y, REAL z, REAL w) = 0;

    virtual void TexCoord(REAL u, REAL v)                 = 0;
    virtual void TexCoord(REAL u, REAL v, REAL w)         = 0;
    virtual void TexCoord(REAL u, REAL v, REAL w, REAL t) = 0;

    virtual void TexVertex(REAL x, REAL y, REAL z,
                           REAL u, REAL v);

    virtual void Color(REAL r, REAL g, REAL b)        = 0;
    virtual void Color(REAL r, REAL g, REAL b,REAL a) = 0;

    virtual void End(bool force=true)   = 0;

    virtual void BeginLines()      = 0;
    virtual void BeginTriangles()  = 0;
    virtual void BeginQuads()      = 0;

    virtual void BeginLineStrip()      = 0;
    virtual void BeginTriangleStrip()  = 0;
    virtual void BeginQuadStrip()      = 0;

    virtual void BeginTriangleFan()    = 0;
    virtual void BeginLineLoop()      = 0;

    virtual void Line(REAL x1, REAL y1, REAL z1,
                      REAL x2, REAL y2, REAL z2);


    virtual void ProjMatrix()     = 0;
    virtual void ModelMatrix()    = 0;
    virtual void TexMatrix()      = 0;

    virtual void PushMatrix()     = 0;
    virtual void PopMatrix()      = 0;
    virtual void MultMatrix(REAL mdata[4][4]) = 0;

    virtual void IdentityMatrix()                       = 0;
    virtual void ScaleMatrix(REAL f)                    = 0;
    virtual void ScaleMatrix(REAL f1, REAL f2, REAL f3) = 0;

    virtual void TranslateMatrix(REAL x1, REAL x2, REAL x3) = 0;


    typedef enum {BACKFACE_CULL=0, ALPHA_BLEND, ALPHA_TEST, DEPTH_TEST,
                  SMOOTH_SHADE, Z_OFFSET,
                  FLAG_END} flag;

    void PushFlags();
    void PopFlags();
    void SetFlag(flag f, bool c);

protected:
    virtual void ReallySetFlag(flag f, bool c) = 0;
    void         ChangeFlags(int before, int after) const;

#define STACK_DEPTH 100

    int flagstack[STACK_DEPTH];
    int stackpos;
};

extern rRenderer *renderer;

inline void Vertex(REAL x, REAL y){
    renderer->Vertex(x,y);
}

inline void Vertex(REAL x, REAL y, REAL z){
    renderer->Vertex(x,y,z);
}

inline void Vertex3(REAL *x){
    renderer->Vertex3(x);
}

inline void Vertex(REAL x, REAL y, REAL z, REAL w){
    renderer->Vertex(x,y,z,w);
}

inline void TexCoord(REAL u, REAL v){
    renderer->TexCoord(u,v);
}

inline void TexCoord(REAL u, REAL v, REAL w){
    renderer->TexCoord(u,v,w);
}

inline void TexCoord(REAL u, REAL v, REAL w, REAL t){
    renderer->TexCoord(u,v,w,t);
}

inline void TexVertex(REAL x, REAL y, REAL z,
                      REAL u, REAL v){
    renderer->TexVertex(x,y,z,u,v);
}

inline void Color(REAL r, REAL g, REAL b){
    renderer->Color(r,g,b);
}

inline void Color(REAL r, REAL g, REAL b,REAL a){
    renderer->Color(r,g,b,a);
}

inline void RenderEnd(bool force=true){
    renderer->End(force);
}

inline void BeginLines(){
    renderer->BeginLines();
}

inline void BeginTriangles(){
    renderer->BeginTriangles();
}

inline void BeginQuads(){
    renderer->BeginQuads();
}

inline void BeginLineStrip(){
    renderer->BeginLineStrip();
}

inline void BeginLineLoop(){
    renderer->BeginLineLoop();
}

inline void BeginTriangleStrip(){
    renderer->BeginTriangleStrip();
}

inline void BeginQuadStrip(){
    renderer->BeginQuadStrip();
}

inline void BeginTriangleFan(){
    renderer->BeginTriangleFan();
}

#ifdef __EMSCRIPTEN__
// Draw a vertex list through client arrays instead of the immediate-mode
// Begin/Vertex/End path.  The port's legacy GL emulation binds a texcoord (or
// colour) array only when the matching client state is enabled; a primitive
// that never enabled one gets a single constant generic attribute -- the
// texcoord one is set to (0,0,0,1) -- so the whole primitive samples one
// texel of its texture and carries one colour.  The desktop pipeline
// interpolates the per-vertex values the code supplies, so every textured or
// per-vertex-coloured primitive whose look matters has to take this path on
// the port.  rModel.cpp and gCycle.cpp already use client arrays for exactly
// this reason.
inline void atglDrawArrays(GLenum mode, GLsizei n, const GLfloat *v,
                           const GLfloat *t, const GLfloat *c)
{
    RenderEnd(true);        // flush anything the renderer is still holding open

    // A line primitive whose two ends straddle the eye plane is dropped whole
    // instead of clipped: the game folds the camera into the projection matrix,
    // so a wall top edge running back past the eye has clip-space w of opposite
    // signs at its two ends, and Chrome's ANGLE/SwiftShader line clipper then
    // emits nothing at all for it (measured in a bare WebGL1 page with the very
    // matrices of replay frame 363: 0 of 4017 probed pixels, no GL error, while
    // a triangle under the same matrix clips correctly at 4004/4017).  Cutting
    // the segment at the near plane here, where the per-vertex attributes are
    // still at hand, leaves the driver segments it handles and puts the same
    // pixels on screen as the desktop build, whose GL does this clip for us.
    // Triangles need nothing: they are clipped correctly already.
    static std::vector<GLfloat> ov, ot, oc;
    if ((mode == GL_LINES || mode == GL_LINE_STRIP || mode == GL_LINE_LOOP) && n >= 2)
    {
        GLfloat mv[16], pj[16], m[16];
        glGetFloatv(GL_MODELVIEW_MATRIX, mv);
        glGetFloatv(GL_PROJECTION_MATRIX, pj);
        for (int col = 0; col < 4; col++)
            for (int row = 0; row < 4; row++)
                m[col * 4 + row] =
                    pj[row] * mv[col * 4] + pj[4 + row] * mv[col * 4 + 1] +
                    pj[8 + row] * mv[col * 4 + 2] + pj[12 + row] * mv[col * 4 + 3];

        // Clip space: the near plane is z = -w, so a point is in front of it
        // exactly where z + w >= 0; that same test implies w > 0, which is what
        // the driver's line clipper trips over.
        GLfloat gs[2][4];
        GLsizei segs;
        if (mode == GL_LINES)
            segs = n / 2;
        else
            segs = n - 1 + (mode == GL_LINE_LOOP ? 1 : 0);

        ov.clear(); ot.clear(); oc.clear();
        for (GLsizei k = 0; k < segs; k++)
        {
            GLsizei a, b;
            if (mode == GL_LINES) { a = 2 * k; b = a + 1; }
            else { a = k; b = (k + 1) % n; }

            for (int i = 0; i < 2; i++)
            {
                const GLfloat *p = v + 3 * (i ? b : a);
                GLfloat *cc = gs[i];
                for (int r = 0; r < 4; r++)
                    cc[r] = m[r] * p[0] + m[4 + r] * p[1] + m[8 + r] * p[2] + m[12 + r];
            }
            GLfloat da = gs[0][2] + gs[0][3], db = gs[1][2] + gs[1][3];
            bool fa = da >= 0, fb = db >= 0;
            if (fa == fb && fa)
            {
                // Both ends in front: hand the pair over untouched.
                for (int i = 0; i < 2; i++)
                {
                    const GLfloat *p = v + 3 * (i ? b : a);
                    ov.insert(ov.end(), p, p + 3);
                    if (t) { const GLfloat *q = t + 2 * (i ? b : a); ot.insert(ot.end(), q, q + 2); }
                    if (c) { const GLfloat *q = c + 4 * (i ? b : a); oc.insert(oc.end(), q, q + 4); }
                }
                continue;
            }
            if (!fa && !fb)
                continue;                       // entirely behind the eye

            // One end in front, one behind: keep the front end and the point
            // where the segment meets the near plane.  Interpolating along the
            // segment is the same interpolation a hardware clipper applies to
            // the clip coordinates, so position, texture coordinate and colour
            // all land where the desktop's clipper puts them.
            GLsizei f = fa ? a : b;
            GLsizei r2 = fa ? b : a;
            GLfloat t0 = fa ? da : db, t1 = fa ? db : da;
            GLfloat mix = t0 / (t0 - t1);
            const GLfloat *pf = v + 3 * f, *pr = v + 3 * r2;
            for (int j = 0; j < 3; j++)
                ov.push_back(pf[j] + (pr[j] - pf[j]) * mix);
            if (t)
            {
                const GLfloat *qf = t + 2 * f, *qr = t + 2 * r2;
                for (int j = 0; j < 2; j++)
                    ot.push_back(qf[j] + (qr[j] - qf[j]) * mix);
            }
            if (c)
            {
                const GLfloat *qf = c + 4 * f, *qr = c + 4 * r2;
                for (int j = 0; j < 4; j++)
                    oc.push_back(qf[j] + (qr[j] - qf[j]) * mix);
            }
            for (int i = 0; i < 2; i++)
            {
                const GLfloat *p = v + 3 * (i ? b : a);
                if ((i ? fb : fa))
                {
                    ov.insert(ov.end(), p, p + 3);
                    if (t) { const GLfloat *q = t + 2 * (i ? b : a); ot.insert(ot.end(), q, q + 2); }
                    if (c) { const GLfloat *q = c + 4 * (i ? b : a); oc.insert(oc.end(), q, q + 4); }
                }
            }
        }
        if (ov.size() < 6)
            return;                             // nothing of it is in front
        mode = GL_LINES;                        // the pieces are independent
        n = ov.size() / 3;
        v = ov.data();
        t = t ? ot.data() : nullptr;
        c = c ? oc.data() : nullptr;
    }

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, v);
    if (t)
    {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, t);
    }
    if (c)
    {
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_FLOAT, 0, c);
    }
    glDrawArrays(mode, 0, n);
    if (c)
        glDisableClientState(GL_COLOR_ARRAY);
    if (t)
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}
#endif


inline void Line(REAL x1, REAL y1, REAL z1,
                 REAL x2, REAL y2, REAL z2){
    renderer->Line(x1,y1,z1,x2,y2,z2);
}



inline void ProjMatrix(){
    renderer->ProjMatrix();
}

inline void ModelMatrix(){
    renderer->ModelMatrix();
}

inline void TexMatrix(){
    renderer->TexMatrix();
}


inline void PushMatrix(){
    renderer->PushMatrix();
}

inline void PopMatrix(){
    renderer->PopMatrix();
}


inline void MultMatrix(REAL mdata[4][4]){
    renderer->MultMatrix(mdata);
}


inline void IdentityMatrix(){
    renderer->IdentityMatrix();
}

inline void ScaleMatrix(REAL f){
    renderer->ScaleMatrix(f);
}

inline void ScaleMatrix(REAL f1, REAL f2, REAL f3){
    renderer->ScaleMatrix(f1,f2,f3);
}


inline void TranslateMatrix(REAL x1, REAL x2, REAL x3){
    renderer->TranslateMatrix(x1,x2,x3);
}

void sr_RendererCleanup();
void sr_glRendererInit();

#endif
