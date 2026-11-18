#!/bin/sh
# Native build of the client inside a rootless container: bootstrap,
# configure, compile, then run the test suite.  Run it through
# runbootstrap.py so autoheader finds the prefix's autom4te.
set -e
A=${AGTK:-$HOME/agtk}
cd "${AGTK_SRC:-$A/src}"

# automake wants these two in the tree; the packaged copies are symlinks
# into /usr/share/misc, which the prefix does not carry.
for f in config.guess config.sub; do
    if test ! -f $f; then
        cp $A/nativeprefix/usr/share/autoconf/build-aux/$f .
    fi
done

sh bootstrap.sh

./configure \
  --prefix=$A/prefixnative \
  "CXXFLAGS=-O2 -std=gnu++03 -w -fpermissive -include cstring -include cstdlib -include cstddef -include cstdio -include stdint.h" \
  "PKG_CONFIG_PATH=$A/nativeprefix/usr/lib/x86_64-linux-gnu/pkgconfig" \
  "LDFLAGS=-L$A/nativeprefix/usr/lib/x86_64-linux-gnu -L$A/nativeprefix/usr/lib/x86_64-linux-gnu/pulseaudio -Wl,--allow-shlib-undefined -Wl,-Bdynamic" \
  "CFLAGS=-O2 -w" \
  "XML2_CFLAGS=-I$A/nativeprefix/usr/include/libxml2" \
  "XML2_LIBS=-L$A/nativeprefix/usr/lib/x86_64-linux-gnu -lxml2" \
  "LIBPNG_LIBS=-lpng16 -lm -lz"

make -C src armagetronad_main -j${JOBS:-6}
make -C src check
echo "NATIVE BUILD OK"
