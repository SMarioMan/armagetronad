/* Report GLX presence, version and an FBConfig count, plus the extension list
   the server advertises - the readiness check for webgl/harness/native_x.sh.
   Build it against the desktop prefix:
     cc -I"$P/usr/include" -L"$P/usr/lib/x86_64-linux-gnu" \
        -o glxprobe webgl/harness/glxprobe.c -lGL -lX11                                  */
#include <stdio.h>
#include <GL/gl.h>
#include <GL/glx.h>

int main(void)
{
    Display *dpy = XOpenDisplay(NULL);
    int vmaj = 0, vmin = 0, fbconfigs = 0;
    GLXFBConfig *cfg;
    if (!dpy) {
        printf("no display\n");
        return 1;
    }
    {
        int n = 0;
        char **ext = XListExtensions(dpy, &n);
        int i;
        printf("extensions(%d):", n);
        for (i = 0; ext && i < n; i++)
            printf(" %s", ext[i]);
        printf("\n");
        if (ext)
            XFreeExtensionList(ext);
    }
    if (!dpy) {
        printf("GLX absent=1 (no display)\n");
        return 1;
    }
    if (!glXQueryVersion(dpy, &vmaj, &vmin)) {
        printf("GLX absent=1\n");
        return 1;
    }
    cfg = glXChooseFBConfig(dpy, DefaultScreen(dpy), NULL, &fbconfigs);
    printf("GLX present=1 version %d.%d FBConfigs=%d\n", vmaj, vmin, fbconfigs);
    XCloseDisplay(dpy);
    return 0;
}
