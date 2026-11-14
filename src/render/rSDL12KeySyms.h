#ifndef AT_SDL12_KEYSYMS_H
#define AT_SDL12_KEYSYMS_H
// Redefine the keysym constants this tree compiles against to the
// SDL 1.2 numbers.  Everything key related outside the SDL layer is
// built around those: the PLAYER_BIND configuration, the .aarec input
// format, the keymap[]/pressed[] bounds in uInput.cpp, the range tests
// in ePlayer.cpp and the key names in the HUD.  Without this a key
// press arrives under one number and its binding sits under another,
// so steering a replayed recording does nothing and the two builds
// simulate different games.  su_GetSDLInput() maps the incoming events
// to the same numbers; the include sits after <SDL.h> on purpose, so
// the enum declaration above is untouched and only later uses change.
// SDL 1.2 closed the enum one above its last special key, and the game
// numbers its mouse pseudo-keys from there (uInput.h: SDLK_MOUSE_X_PLUS
// == SDLK_LAST+1, SDLK_NEWLAST == SDLK_LAST+14); the configuration files
// bind those pseudo-keys by number, so the enum has to close at the same
// place here as it does on the desktop.
#define SDLK_KP0 256
#define SDLK_KP1 257
#define SDLK_KP2 258
#define SDLK_KP3 259
#define SDLK_KP4 260
#define SDLK_KP5 261
#define SDLK_KP6 262
#define SDLK_KP7 263
#define SDLK_KP8 264
#define SDLK_KP9 265
#define SDLK_KP_PERIOD 266
#define SDLK_KP_DIVIDE 267
#define SDLK_KP_MULTIPLY 268
#define SDLK_KP_MINUS 269
#define SDLK_KP_PLUS 270
#define SDLK_KP_ENTER 271
#define SDLK_KP_EQUALS 272
#define SDLK_UP 273
#define SDLK_DOWN 274
#define SDLK_RIGHT 275
#define SDLK_LEFT 276
#define SDLK_INSERT 277
#define SDLK_HOME 278
#define SDLK_END 279
#define SDLK_PAGEUP 280
#define SDLK_PAGEDOWN 281
#define SDLK_F1 282
#define SDLK_F2 283
#define SDLK_F3 284
#define SDLK_F4 285
#define SDLK_F5 286
#define SDLK_F6 287
#define SDLK_F7 288
#define SDLK_F8 289
#define SDLK_F9 290
#define SDLK_F10 291
#define SDLK_F11 292
#define SDLK_F12 293
#define SDLK_F13 294
#define SDLK_F14 295
#define SDLK_F15 296
#define SDLK_NUMLOCK 300
#define SDLK_CAPSLOCK 301
#define SDLK_SCROLLOCK 302
#define SDLK_RSHIFT 303
#define SDLK_LSHIFT 304
#define SDLK_RCTRL 305
#define SDLK_LCTRL 306
#define SDLK_RALT 307
#define SDLK_LALT 308
#define SDLK_RMETA 309
#define SDLK_LMETA 310
#define SDLK_LSUPER 311
#define SDLK_RSUPER 312
#define SDLK_MODE 313
#define SDLK_COMPOSE 314
#define SDLK_HELP 315
#define SDLK_PRINT 316
#define SDLK_SYSREQ 317
#define SDLK_BREAK 318
#define SDLK_MENU 319
#define SDLK_POWER 320
#define SDLK_EURO 321
#define SDLK_UNDO 322
#define SDLK_LAST 323
#endif
