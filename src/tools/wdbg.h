// debug logging for the wasm port (routes to the browser console).
// Enabled only when WASM_DBG is defined at compile time; otherwise all
// calls compile to empty no-ops with zero runtime cost.
#pragma once
#if defined(__EMSCRIPTEN__) && defined(WASM_DBG)
#include <emscripten/em_asm.h>

#include <cstdio>

static inline void wdbg(const char * s)
{
    EM_ASM( console.log('WDBG: ' + UTF8ToString($0) );, s );
}

static inline void wdbgi(const char * what, int v)
{
    char buf[160];
    snprintf(buf, sizeof(buf), "%s = %d", what, v);
    EM_ASM( console.log('WDBG: ' + UTF8ToString($0) );, buf );
}

static inline void wdbg2(const char * a, const char * b)
{
    EM_ASM( console.log('WDBG: ' + UTF8ToString($0) + ' ' + UTF8ToString($1) );, a, b );
}

#else
static inline void wdbg(const char *)
{
}
static inline void wdbgi(const char *, int)
{
}
static inline void wdbg2(const char *, const char *)
{
}
#endif
