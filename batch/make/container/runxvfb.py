#!/usr/bin/env python3
"""Run Xvfb with the local prefix shadowing /usr/bin, /usr/share, /usr/lib
and /etc/X11 ($AGTK/etx11, see x11/xorg.conf in this directory).

Must be invoked inside a private user+mount namespace (unshare -rm).  Whether
the server then offers GLX depends on the packaged xserver building the glx
module: check with xdpyinfo | grep -i glx.
"""
import os

A = os.environ.get("AGTK") or os.path.expanduser("~/agtk")
P = f"{A}/nativeprefix/usr/lib/x86_64-linux-gnu"
import ctypes
import ctypes.util

libc = ctypes.CDLL(None, use_errno=True)
MS_BIND, MS_REC = 4096, 16384

def bind(src, dst):
    ret = libc.mount(src.encode(), dst.encode(), None,
                     ctypes.c_ulong(MS_BIND | MS_REC), None)
    if ret != 0:
        raise OSError(f"mount {src} -> {dst} failed: errno {ctypes.get_errno()}")

bind(f"{A}/nativeprefix/usr/bin", "/usr/bin")
bind(f"{A}/nativeprefix/usr/share", "/usr/share")
bind(f"{A}/nativeprefix/usr/lib", "/usr/lib")
bind(f"{A}/etx11", "/etc/X11")

env = dict(os.environ)
env["LD_LIBRARY_PATH"] = P
env["LIBGL_ALWAYS_SOFTWARE"] = "1"
env["GALLIUM_DRIVER"] = "llvmpipe"

os.execve(f"{A}/nativeprefix/usr/bin/Xvfb",
          ["Xvfb", ":97", "-screen", "0", "1280x800x24",
           "+extension", "GLX", "+iglx", "-nolisten", "tcp"], env)
