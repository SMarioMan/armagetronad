#!/bin/sh
# Build and run the 360 view lens map check.
#
# LD_LIBRARY_PATH matters: the test binary links against the prefix copies of
# libSDL_image and libpulsecommon, and without them it dies in the loader.
# "make check" then reports FAIL even though the model agrees everywhere, so
# run the binary directly here (or export the same path before make check).
A=/home/agent/agtk
P=$A/nativeprefix/usr/lib/x86_64-linux-gnu
sh "$(dirname "$0")/make.sh" wvLensMapTest
LD_LIBRARY_PATH=$P:$P/pulseaudio $A/repo360/src/wvLensMapTest
