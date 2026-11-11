#ifndef R_EMSCRIPTEN_GLU_H
#define R_EMSCRIPTEN_GLU_H

#ifdef __EMSCRIPTEN__
#include <SDL_opengl.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

inline void gluPerspective(GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar) {
    GLfloat f = 1.0f / tan(fovy * M_PI / 360.0);
    GLfloat m[16];
    m[0] = f / aspect; m[1] = 0; m[2] = 0; m[3] = 0;
    m[4] = 0; m[5] = f; m[6] = 0; m[7] = 0;
    m[8] = 0; m[9] = 0; m[10] = (zFar + zNear) / (zNear - zFar); m[11] = -1;
    m[12] = 0; m[13] = 0; m[14] = (2 * zFar * zNear) / (zNear - zFar); m[15] = 0;
    glMultMatrixf(m);
}

inline void gluLookAt(GLdouble eyex, GLdouble eyey, GLdouble eyez,
                      GLdouble centerx, GLdouble centery, GLdouble centerz,
                      GLdouble upx, GLdouble upy, GLdouble upz) {
    GLfloat forward[3], side[3], up[3];
    GLfloat m[16];

    forward[0] = centerx - eyex;
    forward[1] = centery - eyey;
    forward[2] = centerz - eyez;

    GLfloat mag = sqrt(forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]);
    if (mag) { forward[0] /= mag; forward[1] /= mag; forward[2] /= mag; }

    side[0] = forward[0+1]*up[0+2] - forward[0+2]*up[0+1]; // Mistake in indices, fixing below
}

// Better implementation of gluLookAt
inline void gluLookAt_Correct(GLdouble eyex, GLdouble eyey, GLdouble eyez,
                      GLdouble centerx, GLdouble centery, GLdouble centerz,
                      GLdouble upx, GLdouble upy, GLdouble upz) {
    GLfloat forward[3], side[3], up[3];
    GLfloat m[16];

    forward[0] = centerx - eyex;
    forward[1] = centery - eyey;
    forward[2] = centerz - eyez;

    GLfloat mag = sqrt(forward[0]*forward[0] + forward[1]*forward[1] + forward[2]*forward[2]);
    if (mag) { forward[0] /= mag; forward[1] /= mag; forward[2] /= mag; }

    side[0] = forward[1]*upz - forward[2]*upy;
    side[1] = forward[2]*upx - forward[0]*upz;
    side[2] = forward[0]*upy - forward[1]*upx;

    mag = sqrt(side[0]*side[0] + side[1]*side[1] + side[2]*side[2]);
    if (mag) { side[0] /= mag; side[1] /= mag; side[2] /= mag; }

    up[0] = side[1]*forward[2] - side[2]*forward[1];
    up[1] = side[2]*forward[0] - side[0]*forward[2];
    up[2] = side[0]*forward[1] - side[1]*forward[0];

    m[0] = side[0]; m[4] = side[1]; m[8] = side[2]; m[12] = 0.0;
    m[1] = up[0];   m[5] = up[1];   m[9] = up[2];   m[13] = 0.0;
    m[2] = -forward[0]; m[6] = -forward[1]; m[10] = -forward[2]; m[14] = 0.0;
    m[3] = 0.0;     m[7] = 0.0;     m[11] = 0.0;    m[15] = 1.0;

    glMultMatrixf(m);
    glTranslatef(-eyex, -eyey, -eyez);
}

#undef gluLookAt
#define gluLookAt gluLookAt_Correct

inline int gluBuild2DMipmaps(GLenum target, GLint internalFormat, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *data) {
    glTexImage2D(target, 0, internalFormat, width, height, 0, format, type, data);
    // In GLES2/3 we would call glGenerateMipmap(target);
    // For now we just skip mipmaps to simplify
    return 0;
}

typedef void* GLUquadricObj;
inline GLUquadricObj gluNewQuadric() { return (GLUquadricObj)1; }
inline void gluDeleteQuadric(GLUquadricObj obj) {}
inline void gluSphere(GLUquadricObj obj, GLdouble radius, GLint slices, GLint stacks) {
    // Stub: draw nothing or a simple cube/point if needed. 
    // Armagetron explosions are usually spheres.
}

inline const GLubyte* gluErrorString(GLenum error) {
    return (const GLubyte*)"GLU Error Helper";
}

// GL fallbacks for fixed function
extern "C" {
    inline void glTexCoord2d(GLdouble s, GLdouble t) { glTexCoord2f((GLfloat)s, (GLfloat)t); }
    inline void glTexCoord3fv(const GLfloat *v) { glTexCoord3f(v[0], v[1], v[2]); }
    // glHint/glShadeModel/glPolygonMode/glDrawBuffer are provided by the
    // emscripten GL emulation (glShadeModel/glPolygonMode are no-ops there;
    // the emscripten screen settings force sr_smoothShading, so only
    // GL_SMOOTH is ever requested). glRectf is implemented in rGL.cpp.

    void glRectf(GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
    inline void glRecti(GLint x1, GLint y1, GLint x2, GLint y2) { glRectf((GLfloat)x1, (GLfloat)y1, (GLfloat)x2, (GLfloat)y2); }
    // Display lists are never used on emscripten (rDisplayList.cpp forces
    // useList = false); these stubs satisfy the linker.
    inline GLuint glGenLists(GLsizei n) { (void)n; return 0; }
    inline void glNewList(GLuint list, GLenum mode) { (void)list; (void)mode; }
    inline void glEndList() {}
    inline void glCallList(GLuint list) { (void)list; }
    inline void glDeleteLists(GLuint first, GLsizei range) { (void)first; (void)range; }
}

// SDL shims for functions the emscripten SDL port does not provide
#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <stdlib.h>
extern "C" {
    // The emscripten SDL port has no event filter hook; the game's filter
    // (quick-exit keys, mouse warp) simply does not run.
    inline void SDL_SetEventFilter(SDL_EventFilter filter, void* userdata) {}
    // eSound's emscripten WAV reader always returns AUDIO_S16SYS data, so
    // the conversion path is never taken; the stubs satisfy the linker.
    inline int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 src_format, Uint8 src_channels, int src_rate, Uint16 dst_format, Uint8 dst_channels, int dst_rate) { return -1; }
    inline int SDL_ConvertAudio(SDL_AudioCVT *cvt) { return -1; }
    // The emscripten WAV reader allocates with malloc.
    inline void SDL_FreeWAV(Uint8 *audio_buf) { free((void*)audio_buf); }
#ifndef SDL_SaveBMP
    inline int SDL_SaveBMP(SDL_Surface *surface, const char *filename) { (void)surface; (void)filename; return -1; }
#endif
    // WebGL1 has no ARB extension points; the game checks the result.
    inline void * SDL_GL_GetProcAddress(const char *procName) { (void)procName; return 0; }
}
#endif


#endif // __EMSCRIPTEN__

#endif // R_EMSCRIPTEN_GLU_H
