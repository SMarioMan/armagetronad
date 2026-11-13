#!/bin/bash
# Build the WebAssembly (WebGL) port of Armagetron Advanced.
#
# Design decisions (see webgl/README.md for the full write-up):
#  - Pure JS SDL1 port (-s USE_SDL=1). No C SDL2 ports are linked: the game
#    uses SDL1 headers and the SDL2 ports have a different ABI.
#  - Image loading: -s STB_IMAGE=1 provides IMG_* with the standard alpha
#    convention (255 = opaque), matching native SDL_image, where
#    IMG_InvertAlpha() has been a no-op since SDL_image 1.0.10.
#  - Audio: no mixer (background music is dropped). eSound uses plain
#    SDL_OpenAudio; WAV files are read by the RIFF parser in eSound.cpp
#    (the emscripten SDL1 port has no SDL_LoadWAV).
#  - GL: fixed-function pipeline via -s LEGACY_GL_EMULATION=1, plus the
#    post-build patches below (webgl/patch_*.py).
#  - Data files are preloaded into the virtual FS at DATA_DIR.
#
# Environment:
#  EMSDK_DIR       emsdk install prefix           (default /opt/emsdk)
#  LIBXML2_PREFIX  wasm libxml2 install prefix    (default /opt/libxml2-wasm)
#  CLEAN=1         run "make clean" first if a Makefile exists (default 1)
#  JOBS            parallel make jobs             (default nproc)
set -e

cd "$(dirname "$0")/.."
EMSDK_DIR="${EMSDK_DIR:-/opt/emsdk}"
LIBXML2_PREFIX="${LIBXML2_PREFIX:-/opt/libxml2-wasm}"
JOBS="${JOBS:-$(nproc)}"
export PATH="$EMSDK_DIR:$EMSDK_DIR/upstream/emscripten:$EMSDK_DIR/upstream/bin:$PATH"

export PKG_CONFIG_PATH="$LIBXML2_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH"
export XML2_CFLAGS="-I$LIBXML2_PREFIX/include/libxml2"
export XML2_LIBS="-L$LIBXML2_PREFIX/lib -lxml2"

DATA_DIR='/usr/local/share/games/armagetronad'
# wasm-only install paths. The generated src/tUniversalVariables.h keeps the
# native defaults; these -D overrides steer the wasm build to its preload root
# and disable binreloc (unavailable in the browser), so the native build is
# unaffected.
# Single-quoted so the \" stay literal in CXXFLAGS: the Makefile must carry
# escaped quotes (-DBINDIR=\"/usr/local/bin\") because make runs the compile
# through the shell, which would otherwise strip the quotes and turn the -D
# value into a bare path (parsed as C code).
WASM_DEFS='-DPREFIX=\"/usr/local\" -DEXEC_PREFIX=\"/usr/local\" -DBINDIR=\"/usr/local/bin\" -DSBINDIR=\"/usr/local/sbin\" -DDATADIR=\"/usr/local/share\" -DAA_DATADIR=\"/usr/local/share/games/armagetronad\" -DSYSCONFDIR=\"/usr/local/etc\" -DAA_SYSCONFDIR=\"/usr/local/etc/games/armagetronad\" -DLOCALSTATEDIR=\"/usr/local/var\" -DAA_LOCALSTATEDIR=\"/usr/local/var/games/armagetronad\" -DSCRIPTDIR=\"/usr/local/share/games/armagetronad/scripts\" -DDOCDIR=\"/usr/local/share/doc\" -DAA_DOCDIR=\"/usr/local/share/doc/games/armagetronad\" -DRUNDIR=\"/usr/local/var/run\" -DAA_RUNDIR=\"/usr/local/var/run\" -DLOGDIR=\"/usr/local/var/log\" -DAA_LOGDIR=\"/usr/local/var/log\" -DOLDVARDIR=\"/usr/local/var/games/armagetronad/var\" -DAA_OLDVARDIR=\"/usr/local/var/games/armagetronad/var\" -DRELOCATABLE=\"no\"'
export COMMON_FLAGS="-s USE_SDL=1 -s STB_IMAGE=1 -s USE_ZLIB=1 -s USE_LIBPNG=1 -s FILESYSTEM=1"
# EXTRA_CXXFLAGS/EXTRA_LDFLAGS are appended for diagnostic builds
# (e.g. EXTRA_CXXFLAGS="-g4" EXTRA_LDFLAGS="-s ASSERTIONS=2 -g4"); they are
# empty in normal use and keep the release flags untouched.
export CXXFLAGS="-O2 -fexceptions $COMMON_FLAGS $WASM_DEFS -DHAVE_SINF -DHAVE_COSF -DHAVE_TANF -DHAVE_ATAN2F -DHAVE_SQRTF -DHAVE_LOGF -DHAVE_EXPF -DHAVE_FABSF -DHAVE_FLOORF -DHAVE_ISBLANK -DHAVE_WMEMSET $EXTRA_CXXFLAGS"
export CFLAGS="$CXXFLAGS"

export ac_cv_exeext=.js
export ac_cv_c_cross=yes
export ac_cv_prog_cxx_cross=yes
export cross_compiling=yes

# The cross build cannot run its compiler tests, so the cache file pre-seeds
# them.  It is generated rather than committed: autoconf records the absolute
# compiler paths of whoever ran configure last and refuses to reuse the cache
# when they differ.
cat > webgl/emscripten.cache << 'EOF'
cross_compiling=yes
ac_cv_c_cross=yes
ac_cv_prog_cxx_cross=yes
ac_cv_prog_cc_cross=yes
ac_cv_exeext=.js
ac_cv_lib_GL___glVertex3f=yes
ac_cv_lib_GL___glBegin=yes
ac_cv_lib_GLU___gluBuild3DMipmaps=yes
# autoconf spells these keys differently across versions (the release
# configure of 2.69 uses the extra underscores, a freshly bootstrapped 2.71
# does not) because the library argument is a shell variable.  Emscripten has
# no libGLU to link against at all - the port implements the GLU entry points
# in render/rEmscriptenGLU.h - so the check has to be answered, and both
# spellings are seeded.
ac_cv_lib_GL_glVertex3f=yes
ac_cv_lib_GL_glBegin=yes
ac_cv_lib_GLU_gluBuild3DMipmaps=yes
ac_cv_lib_m___exp=yes
ac_cv_lib_SDL_image_IMG_Load=yes
ac_cv_func_png_sig_cmp=yes
ac_cv_lib_png_png_sig_cmp=yes
ac_cv_lib_m_exp=yes
ac_cv_lib_SDL_SDL_Init=yes
ac_cv_func_select=yes
ac_cv_func_isblank=yes
ac_cv_func_atan2f=yes
ac_cv_func_sinf=yes
ac_cv_func_cosf=yes
ac_cv_func_tanf=yes
ac_cv_func_sqrtf=yes
ac_cv_func_logf=yes
ac_cv_func_expf=yes
ac_cv_func_fabsf=yes
ac_cv_func_floorf=yes
ac_cv_func_wmemset=yes
ac_cv_func_clearenv=no
ac_cv_file__proc_self_maps=no
EOF

emconfigure ./configure \
    --cache-file=webgl/emscripten.cache \
    --host=wasm32-unknown-emscripten \
    --build=$(gcc -dumpmachine) \
    LDFLAGS="-s USE_SDL=1 $COMMON_FLAGS" \
    LIBPNG_CFLAGS="-I$EMSDK_DIR/upstream/emscripten/cache/sysroot/include" \
    LIBPNG_LIBS="-lpng" \
    EXEEXT=.js \
    --disable-sysinstall \
    --disable-curl \
    --disable-useradd \
    --disable-master \
    --disable-uninstall \
    --disable-desktop \
    --disable-armathentication \
    --disable-authentication

if [ -f Makefile ] && [ "${CLEAN:-1}" = 1 ]; then
    emmake make clean
fi
# The playable maps are not in the repository: resource/Makefile.am generates
# resource/included from resource/proto, and the top level Makefile visits src
# before resource.  On a clean tree that means the link would pack an empty
# resource tree ("Map load failure" at the first Local Game), so the resources
# are built first.
make -C resource

# The packed data depends on the preloaded trees, which make knows nothing
# about; drop the link outputs so the packer always sees fresh resources.
rm -f src/armagetronad_main.js src/armagetronad_main.wasm src/armagetronad_main.data

emmake make -j"$JOBS" \
LDFLAGS="-s LEGACY_GL_EMULATION=1 -s GL_UNSAFE_OPTS=0 -s ALLOW_MEMORY_GROWTH=1 \
$EXTRA_LDFLAGS \
-s USE_SDL=1 -s STB_IMAGE=1 -s USE_ZLIB=1 -s USE_LIBPNG=1 -s FILESYSTEM=1 \
--preload-file ../textures@${DATA_DIR}/textures \
--preload-file ../models@${DATA_DIR}/models \
--preload-file ../language@${DATA_DIR}/language \
--preload-file ../resource@${DATA_DIR}/resource \
--preload-file ../sound@${DATA_DIR}/sound \
--preload-file ../config@${DATA_DIR}/config" \
LIBS="-L$LIBXML2_PREFIX/lib -lxml2 -lm -lpng -lidbfs.js -lwebsocket.js"

# Patch the SDL1 JS port: fix the SDL_ConvertSurface crash on surfaces whose
# 2D canvas was discarded by IMG_Load_RW in SDL.GL mode (buffer-copy
# fallback). See webgl/patch_sdl_convert.py.
python3 webgl/patch_sdl_convert.py src/armagetronad_main.js || exit 1
# Patch the generated legacy GL emulation: add immediate-mode GL_QUAD_STRIP
# (mode 8), which Armagetron uses for player walls. See webgl/patch_gl_emulation.py.
python3 webgl/patch_gl_emulation.py src/armagetronad_main.js || exit 1

# Fix the generated legacy GL emulation's FFP fragment shader precision
# (mediump -> highp) to stop 16-bit fog/color banding in trails, floor
# lighting and wall alpha. See webgl/patch_emulation_fixes.py.
python3 webgl/patch_emulation_fixes.py src/armagetronad_main.js || exit 1
# Restore the projective divide on texture coordinates.  The emulation sampled
# with (textureMatrix * texCoord).xy and threw the coordinate's fourth
# component away, but a plane can encode its perspective falloff in exactly
# that component: the infinite floor and sky planes in src/engine/eDisplay.cpp
# pass glTexCoord4f with w=1 at the near vertex and w=0 along the rim.  The
# divide goes to texture2DProj, whose sampler LOD rule survives q = 0; the
# earlier plain divide with a clamped divisor went locally flat at the horizon
# and left the far floor grid on full base-level detail.
# See webgl/patch_texture_projective_coords.py.
python3 webgl/patch_texture_projective_coords.py src/armagetronad_main.js || exit 1
# Track GL_COLOR_MATERIAL, which desktop GL enables by default: ambient and
# diffuse then come from the vertex colour, not from the material state.  The
# cycle body in src/tron/gCycle.cpp sets its material to plain white and colours
# the geometry per vertex, and the wall polygons in src/tron/gWall.cpp carry
# only a vertex colour, so lighting a white material instead blows both out: on
# the frame-identical n814 pair at gt 10.06 the wall strip measured mean bright
# luminance 162 against the desktop's 88 (250,250,50 against 102,102,20 down a
# scanline) and the cycle body box 1977 bright px against 40.  An earlier
# revision of this file dropped this patch; the measurement says it belongs.
# See webgl/patch_light_colormaterial.py.
python3 webgl/patch_light_colormaterial.py src/armagetronad_main.js || exit 1
# Tell positional lights from directional ones, the way desktop GL does: w = 0
# is a direction (normalize(u_lightPositionN)) while w != 0 is a point that has
# to be measured against the vertex (u_lightPositionN - ecPosition), and the
# lighting pass runs in eye space.  The emulation always used the directional
# rule, so any light the game places as a point was lit as if it were at the
# eye-space origin.  The two lights gCycle.cpp sets for the cycle body are
# directional (measured w = 0), so this does not change the body; it matters for
# the point lights elsewhere.  See webgl/patch_light_positional.py.
python3 webgl/patch_light_positional.py src/armagetronad_main.js || exit 1
# Suppress the specular term when the shininess is 0, as desktop GL defines it.
# The emulation approximates the exponent with exp(shininess*log(N.H)), which is
# 1 at shininess 0, so every vertex facing a light got a full-intensity
# highlight: gCycle.cpp sets GL_SHININESS 0 with GL_SPECULAR (1,1,1), and the
# cycle body blew out to white (mean green 254 against the desktop 70 in the
# body box).  See webgl/patch_light_specular.py.
python3 webgl/patch_light_specular.py src/armagetronad_main.js || exit 1
# Every patch above edits generated JavaScript in place, so a bad anchor or a
# mistyped replacement only shows up as a SyntaxError in the browser.  Catch it
# here instead, where the build fails loudly.
node --check src/armagetronad_main.js

# ---- assemble the deployable site ------------------------------------------
rm -rf webgl/dist
mkdir -p webgl/dist
cp webgl/site/index.html webgl/dist/index.html
cp src/armagetronad_main.js src/armagetronad_main.wasm src/armagetronad_main.data webgl/dist/
# The browser always asks for a favicon; without one every page load logs a
# 404.  Ship the game's own 32x32 icon.
cp textures/icon.png webgl/dist/favicon.png
# The reference replay every parity measurement in webgl/README.md is taken
# from.  It ships so the published page can be aimed at it
# (index.html?args=--playback,perp2.aarec): the same recording the native build
# recorded and both builds are measured against.  It is a harness artefact, not
# part of the game.
cp webgl/harness/perp2.aarec webgl/dist/perp2.aarec

echo "=== Build complete: webgl/dist ==="
ls -la webgl/dist
