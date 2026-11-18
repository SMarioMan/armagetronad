#!/bin/sh
# Full build of the wide-view-360 branch tree: bootstrap, configure, compile.
set -e
A=/home/agent/agtk
cd $A/repo360

sh bootstrap.sh

./configure \
  --prefix=$A/prefixbr \
  --with-sdl-prefix=$A/nativeprefix/usr \
  "CXXFLAGS=-O2 -std=gnu++03 -w -fpermissive -include cstring -include cstdlib -include cstddef -include cstdio -include stdint.h" \
  "PKG_CONFIG_PATH=$A/nativeprefix/usr/lib/x86_64-linux-gnu/pkgconfig" \
  "LDFLAGS=-L$A/nativeprefix/usr/lib/x86_64-linux-gnu -L$A/nativeprefix/usr/lib/x86_64-linux-gnu/pulseaudio -Wl,--allow-shlib-undefined -Wl,-Bdynamic" \
  "CFLAGS=-O2 -w" \
  "XML2_CFLAGS=-I$A/nativeprefix/usr/include/libxml2" \
  "XML2_LIBS=-L$A/nativeprefix/usr/lib/x86_64-linux-gnu -lxml2" \
  "LIBPNG_LIBS=-lpng16 -lm -lz"

make -C src armagetronad_main -j6
echo "BRANCH BUILD OK"
