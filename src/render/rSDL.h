#ifndef AT_SDL_H
#define AT_SDL_H

#include "config.h"

#ifndef DEDICATED
#include <SDL.h>
#ifdef __EMSCRIPTEN__
// The SDL headers this build compiles against number the non-ASCII keys as
// 0x400 | SDL2 scancode (SDLK_DOWN == 0x451), but everything else in the tree
// works with the SDL 1.2 numbers the desktop build uses: the PLAYER_BIND
// configuration, the .aarec input format, keymap[]/pressed[] indexing in
// uInput.cpp, the range tests in ePlayer.cpp and the key names in the HUD.
// Redefining the constants here -- after the enum declarations, so those are
// left intact -- puts the whole game on one numbering; su_GetSDLInput()
// converts the events the SDL layer delivers so that they agree.
#include "rSDL12KeySyms.h"
#endif
#else
#define SDLK_LAST 1024
typedef int SDL_keysym;
typedef int SDL_Event;
typedef unsigned char Uint8;
typedef unsigned int Uint32;
typedef int SDL_AudioSpec;
#endif

#endif
