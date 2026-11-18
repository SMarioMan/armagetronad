"""Nested Xephyr display on top of the Xvfb from runxvfb.py (:97 -> :98).

Must be invoked inside a private user+mount namespace (unshare -rm).
"""
import ctypes, os, sys

A = os.environ.get("AGTK") or os.path.expanduser("~/agtk")
libc = ctypes.CDLL(None, use_errno=True)

def bind(src, dst):
    for flag in (4096 | 16384, 4096):
        if libc.mount(src.encode(), dst.encode(), None, flag, None) == 0:
            return
    print("bind failed", src, dst, ctypes.get_errno())
    sys.exit(1)

bind(A + "/nativeprefix/usr/bin", "/usr/bin")
bind(A + "/nativeprefix/usr/share", "/usr/share")
bind(A + "/nativeprefix/usr/lib", "/usr/lib")
bind(A + "/etx11", "/etc/X11")

env = dict(os.environ)
P = A + "/nativeprefix/usr/lib/x86_64-linux-gnu"
env["LD_LIBRARY_PATH"] = P
env["LIBGL_ALWAYS_SOFTWARE"] = "1"
env["GALLIUM_DRIVER"] = "llvmpipe"
env["LIBGL_DRIVERS_PATH"] = P + "/dri"
env["DISPLAY"] = ":97"
env["PATH"] = A + "/nativeprefix/usr/bin:/usr/bin:/bin"
os.execve(A + "/nativeprefix/usr/bin/Xephyr",
          ["Xephyr", ":98", "+extension", "GLX",
           "-screen", "1280x800x24", "-nolisten", "tcp", "-no-host-grab"], env)
