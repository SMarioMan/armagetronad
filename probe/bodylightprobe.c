/*
 * Reference-driver probe: what the desktop build's GL really does to a
 * light-cycle body, so the wasm port's generated lighting shader can be
 * compared against the reference instead of against a reading of the spec.
 *
 * The state is copied from src/tron/gCycle.cpp:4335-4346:
 *   glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, (1,1,1))
 *   glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  (1,1,1))
 *   GL_LIGHT0 diffuse/specular (1,.7,.7), position (320,240,200,0)
 *   GL_LIGHT1 diffuse/specular (.7,.7,1), position (-240,-100,200,0)
 * GL_AMBIENT, GL_EMISSION and GL_SHININESS are never touched, so their
 * defaults apply, and no light model parameter is ever set either.
 *
 * With the modelview left at the identity and a front-facing quad whose normal
 * is +z, the two light directions give
 *   N.L0 = 200/|(320,240,200)| = 0.447   N.L1 = 200/|(-240,-100,200)| = 0.610
 * so a diffuse-only result is
 *   0.447*(1,.7,.7) + 0.610*(.7,.7,1) = (0.874, 0.740, 0.923) -> about 223,189,235
 * while a specular term that is simply switched on wherever the half vector is
 * front-facing (which is what a shininess of 0 degenerates to) adds
 *   (1,.7,.7)+(.7,.7,1) -> saturates to white.
 * Those two are trivially distinguishable in the readback.  The reference keeps
 * the wash: with the game's own state the centre pixel reads 255,255,255, while
 * the same draw with the material specular forced to black reads 233,199,246,
 * exactly the hand-computed diffuse+ambient sum.  A shininess of 0 therefore
 * carries a full light-coloured specular wash, and the port stopped suppressing
 * the specular term in that case.
 *
 * The last case draws with a colour *array* (a bright green vertex colour) while
 * the material stays (1,1,1): if that comes back green, the reference build
 * shades from the vertex colour and the port's u_colorFromArray = 0 is wrong;
 * if it stays neutral, the material-only path is the right one.  It stayed
 * neutral - 233,199,246 with a green colour array, and again with a green
 * current colour - so an explicit material wins over the vertex colour.
 *
 * build: gcc -O2 -o bodylightprobe probe/bodylightprobe.c -lGL -lGLU -lX11
 * run:   DISPLAY=:97 LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe ./bodylightprobe
 */
#include <GL/glx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define W 64
#define H 64

static void px_centre(const char *what, const char *extra)
{
    static unsigned char px[W * H * 4];
    glFinish();
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
#define P(x, y) (&px[(((y) * W + (x)) * 4)])
    printf("%-34s centre=%3u,%3u,%3u,%3u  q(%d,%d)=%3u,%3u,%3u  err=0x%x  %s\n", what,
           P(W / 2, H / 2)[0], P(W / 2, H / 2)[1], P(W / 2, H / 2)[2], P(W / 2, H / 2)[3],
           4, 4, P(4, 4)[0], P(4, 4)[1], P(4, 4)[2], glGetError(), extra ? extra : "");
#undef P
}

static void quad(void)
{
    glBegin(GL_QUADS);
    glNormal3f(0.0f, 0.0f, 1.0f);
    glVertex3f(-2.0f, -2.0f, -3.0f);
    glVertex3f(2.0f, -2.0f, -3.0f);
    glVertex3f(2.0f, 2.0f, -3.0f);
    glVertex3f(-2.0f, 2.0f, -3.0f);
    glEnd();
}

static void game_light_state(void)
{
    GLfloat lposa[4] = { 320, 240, 200, 0 };
    GLfloat lposb[4] = { -240, -100, 200, 0 };
    GLfloat lighta[4] = { 1, .7, .7, 1 };
    GLfloat lightb[4] = { .7, .7, 1, 1 };
    GLfloat white[4] = { 1, 1, 1, 1 };

    glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
    glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, white);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lighta);
    glLightfv(GL_LIGHT0, GL_SPECULAR, lighta);
    glLightfv(GL_LIGHT0, GL_POSITION, lposa);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, lightb);
    glLightfv(GL_LIGHT1, GL_SPECULAR, lightb);
    glLightfv(GL_LIGHT1, GL_POSITION, lposb);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);
}

static void report_defaults(void)
{
    GLfloat f[4] = {0};
    glGetMaterialfv(GL_FRONT, GL_AMBIENT, f);
    printf("material AMBIENT   = %g,%g,%g,%g\n", f[0], f[1], f[2], f[3]);
    glGetMaterialfv(GL_FRONT, GL_DIFFUSE, f);
    printf("material DIFFUSE   = %g,%g,%g,%g\n", f[0], f[1], f[2], f[3]);
    glGetMaterialfv(GL_FRONT, GL_SPECULAR, f);
    printf("material SPECULAR  = %g,%g,%g,%g\n", f[0], f[1], f[2], f[3]);
    glGetMaterialfv(GL_FRONT, GL_EMISSION, f);
    printf("material EMISSION  = %g,%g,%g,%g\n", f[0], f[1], f[2], f[3]);
    glGetMaterialfv(GL_FRONT, GL_SHININESS, f);
    printf("material SHININESS = %g\n", f[0]);
    glGetFloatv(GL_LIGHT_MODEL_AMBIENT, f);
    printf("light model AMBIENT= %g,%g,%g,%g\n", f[0], f[1], f[2], f[3]);
    glGetFloatv(GL_LIGHT0 + GL_DIFFUSE, f);
    printf("LIGHT0 default diffuse = %g,%g,%g,%g\n", f[0], f[1], f[2], f[3]);
    glGetFloatv(GL_LIGHT0 + GL_SPOT_CUTOFF, f);
    printf("LIGHT0 default cutoff  = %g\n", f[0]);
}

int main(void)
{
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "no display\n"); return 1; }
    int attr[] = { GLX_RGBA, GLX_DOUBLEBUFFER, GLX_RED_SIZE, 1, GLX_GREEN_SIZE, 1,
                   GLX_BLUE_SIZE, 1, GLX_ALPHA_SIZE, 1, GLX_DEPTH_SIZE, 16, None };
    XVisualInfo *vi = glXChooseVisual(dpy, DefaultScreen(dpy), attr);
    if (!vi) { fprintf(stderr, "no visual\n"); return 1; }
    Colormap cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
    XSetWindowAttributes swa;
    memset(&swa, 0, sizeof swa);
    swa.colormap = cmap;
    swa.event_mask = ExposureMask;
    Window win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, W, H, 0,
                               vi->depth, InputOutput, vi->visual,
                               CWColormap | CWEventMask, &swa);
    XMapWindow(dpy, win);
    XSync(dpy, False);
    GLXContext ctx = glXCreateContext(dpy, vi, NULL, GL_TRUE);
    if (!ctx) { fprintf(stderr, "no context\n"); return 1; }
    if (!glXMakeCurrent(dpy, win, ctx)) { fprintf(stderr, "make current failed\n"); return 1; }
    printf("direct: %d  renderer: %s  version: %s\n", glXIsDirect(dpy, ctx),
           glGetString(GL_RENDERER), glGetString(GL_VERSION));
    report_defaults();

    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClearDepth(1.0);
    glEnable(GL_DEPTH_TEST);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glFrustum(-1.5, 1.5, -1.5, 1.5, 1.0, 20.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    /* control: unlit, white raster colour */
    glColor3f(1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    quad();
    px_centre("control: lighting OFF", NULL);

    game_light_state();
    glEnable(GL_LIGHTING);

    /* 1: the game's exact state, specular material (1,1,1), shininess default */
    glColor3f(1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    quad();
    px_centre("lit, game state, spec mat (1,1,1)", "expect 223,189,235 if no specular");

    /* 2: same but the material specular is black: the diffuse-only reference */
    {
        GLfloat black[4] = { 0, 0, 0, 1 };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        quad();
        px_centre("lit, material spec black", "diffuse-only reference");
        GLfloat white[4] = { 1, 1, 1, 1 };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
    }

    /* 3: shininess set explicitly to 0, to check the default really behaves as 0 */
    glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    /* 4: a colour ARRAY feeding the vertex, material still (1,1,1).  Material
       specular is forced black first so the specular wash cannot saturate the
       result and hide which colour source the lighting used. */
    {
        GLfloat black[4] = { 0, 0, 0, 1 };
        static const GLfloat colour[4][4] = { {0,1,0,1}, {0,1,0,1}, {0,1,0,1}, {0,1,0,1} };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, black);
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_FLOAT, 0, &colour[0][0]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        quad();
        px_centre("lit spec-black, colour ARRAY green", "green => vertex colour is the material");
        glDisableClientState(GL_COLOR_ARRAY);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glColor3f(0, 1, 0);
        quad();
        px_centre("lit spec-black, glColor green", "reference for the raster colour path");
        glColor3f(1, 1, 1);
        GLfloat white[4] = { 1, 1, 1, 1 };
        glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, white);
    }
    /* 4: a colour ARRAY feeding the vertex, material still (1,1,1) */
    {
        static const GLfloat colour[4][4] = { {0,1,0,1}, {0,1,0,1}, {0,1,0,1}, {0,1,0,1} };
        glEnableClientState(GL_COLOR_ARRAY);
        glColorPointer(4, GL_FLOAT, 0, &colour[0][0]);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        quad();
        px_centre("lit, colour ARRAY green, mat (1,1,1)", "green => vertex colour is the material");
        glDisableClientState(GL_COLOR_ARRAY);
    }

    /* 5: lighting on, both lights but only LIGHT0 enabled: is the port's two-light
       sum the same shape as the reference's? */
    glDisable(GL_LIGHT1);
    glColor3f(1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    quad();
    px_centre("lit, LIGHT0 only, spec mat black-ish case", NULL);

    return 0;
}
