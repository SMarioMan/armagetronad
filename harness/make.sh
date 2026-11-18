#!/bin/sh
# Incremental build inside the already configured tree.
#
# The include paths have to be passed explicitly: configure recorded
# -I/usr/include/SDL from the prefix's own sdl-config, but the SDL 1.2 headers
# actually live in the local prefix and /usr/include/SDL does not exist on the
# host.  Without -I<nativeprefix>/usr/include the GL headers are missing too.
A=/home/agent/agtk
CPP="-iquote . -iquote .. -iquote ./tools -iquote ./render -iquote ./ui -iquote ./network -iquote ./engine -iquote ./thirdparty/particles -iquote ./tron -I$A/nativeprefix/usr/include -I$A/nativeprefix/usr/include/libxml2 -I$A/nativeprefix/usr/include/SDL -D_GNU_SOURCE=1 -D_REENTRANT"
make -C $A/repo360/src "$@" CPPFLAGS="$CPP"
