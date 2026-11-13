/*
 * Dev tool for render parity, not built by any makefile: interpose the GL entry
 * points the desktop build uses, print a per-mode draw-call census at exit and
 * the projection matrix of the last frame.  The wasm port's side of the same
 * census comes from a page-side getContext hook, so the two builds can be
 * compared on what they actually hand the rasterizer and not only on pixels.
 *
 * build:  gcc -O2 -shared -fPIC -o glinstrument.so glinstrument.c -ldl
 * use:    LD_PRELOAD=./glinstrument.so ./src/armagetronad_main \
 *             --playback /path/to/perp2.aarec
 *
 * glBegin modes (desktop GL): 0 POINTS, 1 LINES, 2 LINE_STRIP, 3 LINE_LOOP,
 * 4 TRIANGLES, 5 TRIANGLE_STRIP, 6 TRIANGLE_FAN, 7 QUADS, 8 QUAD_STRIP,
 * 9 POLYGON.
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>

static long begins[10];
static long arrays[10];
static long arrayverts[10];
static long elements[10];
static long everts[10];

static void *real(const char *name)
{
    void *p = dlsym(RTLD_NEXT, name);
    if (!p) {
        fprintf(stderr, "SHIMERR missing %s\n", name);
    }
    return p;
}
#define GL_VIEWPORT 0x0BA2
#define GL_PROJECTION_MATRIX 0x0BA6
#define GL_ALPHA_TEST_FUNC 0x0BC1
#define GL_ALPHA_TEST_REF 0x0BC2
#define GL_DEPTH_TEST 0x0B71

/* Every few hundred primitives, report the matrix and per-fragment state the
   desktop is really using, so the same numbers can be read off the port with a
   page hook and compared for one and the same recorded frame. */
static void state_probe(void);
/* The last few dozen line-family vertex arguments, kept so that a run stopped
   at a frame gate can print exactly the geometry that frame submitted.  The
   wasm build prints the same numbers from its own line path. */
static int cur_mode = -1;
static int vrun = 0;
static int vseq[64];
static float vx[64], vy[64], vz[64];
static int vslot;


void glBegin(unsigned mode)
{
    static void (*fn)(unsigned);
    if (!fn) {
        fn = (void (*)(unsigned))real("glBegin");
    }
    if (mode < 10) {
        begins[mode]++;
    }
    cur_mode = (int)mode;
    state_probe();

    fn(mode);
}

void glDrawArrays(unsigned mode, int first, int count)
{
    static void (*fn)(unsigned, int, int);
    if (!fn) {
        fn = (void (*)(unsigned, int, int))real("glDrawArrays");
    }
    if (mode < 10) {
        arrays[mode]++;
        arrayverts[mode] += count;
    }
    fn(mode, first, count);
}

void glDrawElements(unsigned mode, int count, unsigned type, const void *indices)
{
    static void (*fn)(unsigned, int, unsigned, const void *);
    if (!fn) {
        fn = (void (*)(unsigned, int, unsigned, const void *))real("glDrawElements");
    }
    if (mode < 10) {
        elements[mode]++;
        everts[mode] += count;
    }
    fn(mode, count, type, indices);
}

static void dump(void);
static void vert_dump(void);

static void state_probe(void)
{
    static void (*getfv)(unsigned, float *);
    static void (*getiv)(unsigned, int *);
    static void (*getbv)(unsigned, unsigned char *);
    static long n;
    float pj[16], vp[4];
    int atest = -1, aref = -1;
    unsigned char depth = 0;

    ++n;
    if (n % 400) {
        if (n % 1600 == 0) {
            // Print without waiting for exit: the harness stops the client with
            // a signal, and a signalled process never runs its destructors.
            dump();
            vert_dump();
        }
        return;
    }
    if (!getfv) {
        getfv = (void (*)(unsigned, float *))real("glGetFloatv");
        getiv = (void (*)(unsigned, int *))real("glGetIntegerv");
        getbv = (void (*)(unsigned, unsigned char *))real("glGetBooleanv");
    }
    if (!getfv || !getiv || !getbv) {
        return;
    }
    getfv(GL_PROJECTION_MATRIX, pj);
    getfv(GL_VIEWPORT, vp);
    getiv(GL_ALPHA_TEST_FUNC, &atest);
    getiv(GL_ALPHA_TEST_REF, &aref);
    getbv(GL_DEPTH_TEST, &depth);
    fprintf(stderr, "SHIMGSTATE vp %g %g %g %g depth %d atest %d aref %d pj",
            vp[0], vp[1], vp[2], vp[3], depth, atest, aref);
    for (int i = 0; i < 16; i++) {
        fprintf(stderr, " %.6g", pj[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}

static void dump(void) __attribute__((destructor));
static void dump(void)
{
    int i;
    fprintf(stderr, "SHIMCENSUS glBegin");
    for (i = 0; i < 10; i++) {
        fprintf(stderr, " m%d=%ld", i, begins[i]);
    }
    fprintf(stderr, " | drawArrays");
    for (i = 0; i < 10; i++) {
        fprintf(stderr, " m%d=%ld/%ldv", i, arrays[i], arrayverts[i]);
    }
    fprintf(stderr, " | drawElements");
    for (i = 0; i < 10; i++) {
        fprintf(stderr, " m%d=%ld/%ldv", i, elements[i], everts[i]);
    }
    fprintf(stderr, "\n");
    fflush(stderr);
}

void glVertex3f(float x, float y, float z)
{
    static void (*fn)(float, float, float);
    if (!fn) {
        fn = (void (*)(float, float, float))real("glVertex3f");
    }
    if (cur_mode >= 1 && cur_mode <= 3) {
        vseq[vslot] = vrun;
        vx[vslot] = x;
        vy[vslot] = y;
        vz[vslot] = z;
        vslot = (vslot + 1) % 64;
    }
    fn(x, y, z);
}

void glVertex2f(float x, float y)
{
    glVertex3f(x, y, 0.0f);
}

void glEnd(void)
{
    static void (*fn)(void);
    if (!fn) {
        fn = (void (*)(void))real("glEnd");
    }
    if (cur_mode >= 1 && cur_mode <= 3) {
        vrun++;
    }
    cur_mode = -1;
    fn();
}

static void vert_dump(void) __attribute__((destructor));
static void vert_dump(void)
{
    int i, k;
    fprintf(stderr, "SHIMVERT last %d line-family vertices of %d draws\n", 64, vrun);
    for (k = 0; k < 64; k++) {
        i = (vslot + k) % 64;
        if (vseq[i] >= 0) {
            fprintf(stderr, "SHIMVERT draw %d vtx %g %g %g\n", vseq[i],
                    (double)vx[i], (double)vy[i], (double)vz[i]);
        }
    }
    fflush(stderr);
}
