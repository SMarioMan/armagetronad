"""Run a command with the local prefix's autotools visible to autoheader.

Must be invoked inside a private user+mount namespace:

    unshare -rm --propagation private python3 batch/make/container/runbootstrap.py sh batch/make/container/native_build.sh

The prefix lives in $AGTK (default ~/agtk); the source tree is the current
directory unless $AGTK_SRC names it.  The automake/autoconf data directory
names are for automake 1.16 and autoconf 2.69/2.71.
"""
import ctypes, os, sys

libc = ctypes.CDLL(None, use_errno=True)
A = os.environ.get("AGTK") or os.path.expanduser("~/agtk")
MS = 4096 | 16384  # MS_RDONLY | MS_NOSUID

def bind(src, dst, flags=MS):
    r = libc.mount(src.encode(), dst.encode(), None, flags, None)
    if r:
        print("bind fail", src, dst, ctypes.get_errno())
    return r

# keep the host perl data reachable through /tmp
os.makedirs("/tmp/hs", exist_ok=True)
for d in ("perl", "perl5"):
    if os.path.isdir("/usr/share/" + d):
        os.makedirs("/tmp/hs/" + d, exist_ok=True)
        bind("/usr/share/" + d, "/tmp/hs/" + d)

# tmpfs over /usr/share so we can add the autotools data directories
if libc.mount(b"tmpfs", b"/usr/share", b"tmpfs", 0, None):
    print("tmpfs failed", ctypes.get_errno())
    sys.exit(1)
for d in ("perl", "perl5"):
    os.symlink("/tmp/hs/" + d, "/usr/share/" + d)
for d in ("aclocal", "aclocal-1.16", "autoconf", "automake-1.16"):
    src = A + "/nativeprefix/usr/share/" + d
    if os.path.isdir(src):
        os.symlink(src, "/usr/share/" + d)


os.chdir(os.environ.get("AGTK_SRC") or os.getcwd())
env = dict(os.environ)
env["PERL5LIB"] = (A + "/nativeprefix/usr/share/autoconf:" +
                  A + "/nativeprefix/usr/share/automake-1.16")
env["PATH"] = A + "/nativeprefix/usr/bin:/usr/bin:/bin"
env["M4"] = A + "/nativeprefix/usr/bin/m4"
env["AUTOM4TE"] = A + "/nativeprefix/usr/bin/autom4te"
os.execvpe(sys.argv[1], [sys.argv[1]] + sys.argv[2:], env)
