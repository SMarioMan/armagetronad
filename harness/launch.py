#!/usr/bin/env python3
"""Run the locally built armagetronad_main against Xvfb :97.

Must be invoked inside a private user+mount namespace (unshare -rm) so the
nativeprefix can shadow /usr/bin, /usr/share and /usr/lib.  Extra arguments
are appended to the game command line.
"""
import ctypes, os, sys

A = "/home/agent/agtk"
libc = ctypes.CDLL(None, use_errno=True)

def bind(src, dst):
    if libc.mount(src.encode(), dst.encode(), None, 4096 | 16384, None) != 0:
        raise OSError(f"bind {src} -> {dst}: {ctypes.get_errno()}")

bind(f"{A}/nativeprefix/usr/bin", "/usr/bin")
bind(f"{A}/nativeprefix/usr/share", "/usr/share")
bind(f"{A}/nativeprefix/usr/lib", "/usr/lib")
bind(f"{A}/etx11", "/etc/X11")

P = f"{A}/nativeprefix/usr/lib/x86_64-linux-gnu"
env = dict(os.environ)
env.update({
    "LD_LIBRARY_PATH": P,
    "LIBGL_ALWAYS_SOFTWARE": "1",
    "GALLIUM_DRIVER": "llvmpipe",
    "LIBGL_DRIVERS_PATH": P + "/dri",
    "DISPLAY": ":97",
    "HOME": f"{A}/cfg360",
})

data = "/workspace/armagetronad-linux64"
args = [f"{A}/repo360/src/armagetronad_main",
        "--datadir", data,
        "--configdir", f"{data}/config",
        "--userdatadir", f"{A}/cfg360",
        "--userconfigdir", f"{A}/cfg360",
        "--vardir", f"{A}/run360/var"]
args += sys.argv[1:]

os.execve(args[0], args, env)
