#!/bin/bash
# One-shot toolchain setup for the WebAssembly port: pinned emsdk + libxml2
# cross-compiled for wasm (there is no emscripten port of libxml2, and the
# game's resource manager needs it to parse the .aamap/.xml resources).
#
# Environment:
#  EMSDK_DIR       install prefix for emsdk        (default /opt/emsdk)
#  LIBXML2_PREFIX  install prefix for libxml2-wasm (default /opt/libxml2-wasm)
#  SDK_VERSION     emsdk version to install        (default: webgl/SDK_VERSION)
#  DOWNLOAD_DIR    where the libxml2 tarball goes  (default /tmp)
set -e
cd "$(dirname "$0")/.."
EMSDK_DIR="${EMSDK_DIR:-/opt/emsdk}"
LIBXML2_PREFIX="${LIBXML2_PREFIX:-/opt/libxml2-wasm}"
SDK_VERSION="${SDK_VERSION:-$(cat webgl/SDK_VERSION)}"
LIBXML2_VERSION="${LIBXML2_VERSION:-2.9.14}"
DOWNLOAD_DIR="${DOWNLOAD_DIR:-/tmp}"
mkdir -p "$DOWNLOAD_DIR"
step(){ echo "==== $(date -u +%H:%M:%S) $* ===="; }
step "emsdk $SDK_VERSION into $EMSDK_DIR"

if [ ! -d "$EMSDK_DIR" ]; then
    git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
fi
cd "$EMSDK_DIR"
[ -f ./emsdk ] || { echo "no emsdk checkout in $EMSDK_DIR" >&2; exit 1; }
./emsdk install "$SDK_VERSION"
./emsdk activate "$SDK_VERSION"
cd - >/dev/null
export PATH="$EMSDK_DIR:$EMSDK_DIR/upstream/emscripten:$EMSDK_DIR/upstream/bin:$PATH"
emcc --version | head -2

TARBALL="$DOWNLOAD_DIR/libxml2-$LIBXML2_VERSION.tar.xz"
# -f matters: without it an HTTP error page becomes the "tarball" and the
# failure only shows up inside tar.  The validity check also throws away a
# broken archive left behind by an earlier interrupted run.
if ! tar tf "$TARBALL" >/dev/null 2>&1; then
    curl -fSL --retry 3 -o "$TARBALL" \
        "https://download.gnome.org/sources/libxml2/${LIBXML2_VERSION%.*}/libxml2-$LIBXML2_VERSION.tar.xz"
    tar tf "$TARBALL" >/dev/null \
        || { echo "downloaded $TARBALL is not an xz archive" >&2; exit 1; }
fi
rm -rf "$DOWNLOAD_DIR/libxml2-$LIBXML2_VERSION"
tar xf "$TARBALL" -C "$DOWNLOAD_DIR"
cd "$DOWNLOAD_DIR/libxml2-$LIBXML2_VERSION"
step "libxml2 $LIBXML2_VERSION for wasm into $LIBXML2_PREFIX"
emconfigure ./configure \
  --prefix="$LIBXML2_PREFIX" \
  --without-python \
  --without-zlib \
  --without-lzma \
  --without-threads \
  --enable-static \
  --disable-shared \
  --with-http \
  --without-ftp \
  --without-iconv \
  CFLAGS="-O2" CXXFLAGS="-O2"
emmake make -j"$(nproc)"
emmake make install
step "artifacts"
ls -l "$EMSDK_DIR/upstream/emscripten/em++" "$LIBXML2_PREFIX/lib/libxml2.a" \
    || { echo "the build produced no libxml2.a under $LIBXML2_PREFIX" >&2; exit 1; }
echo "=== TOOLCHAIN READY (emsdk $SDK_VERSION, libxml2 $LIBXML2_VERSION at $LIBXML2_PREFIX) ==="
