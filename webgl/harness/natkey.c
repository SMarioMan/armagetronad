/* harness: synthesize real keyboard events on the native X display (the
   replacement for the xdotool that is not available here).  Drives menus and
   the in-game SCREENSHOT action during record/playback parity runs.
   Usage: natkey <keysym-name|keycode> [count]   (press+release, XTEST)
   Build: cc -o natkey webgl/harness/natkey.c -lX11 -lXtst                  */
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    Display *dpy = XOpenDisplay(NULL);
    KeySym ks;
    KeyCode code;
    int i, n = 1;
    if (!dpy) {
        fprintf(stderr, "natkey: no display\n");
        return 1;
    }
    if (argc < 2) {
        fprintf(stderr, "usage: natkey <keysym|keycode> [count]\n");
        return 2;
    }
    if (argc > 2)
        n = atoi(argv[2]);
    if (sscanf(argv[1], "%d", (int *)&code) == 1 && !strchr(argv[1], '_'))
        ;
    else {
        ks = XStringToKeysym(argv[1]);
        if (ks == 0) {
            fprintf(stderr, "natkey: unknown keysym %s\n", argv[1]);
            return 2;
        }
        code = XKeysymToKeycode(dpy, ks);
        if (!code) {
            fprintf(stderr, "natkey: no keycode for %s\n", argv[1]);
            return 2;
        }
    }
    for (i = 0; i < n; i++) {
        XTestFakeKeyEvent(dpy, code, True, CurrentTime);
        XTestFakeKeyEvent(dpy, code, False, CurrentTime);
        XFlush(dpy);
    }
    XCloseDisplay(dpy);
    return 0;
}
