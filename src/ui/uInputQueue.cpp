/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2000  Manuel Moos (manuel@moosnet.de)

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

***************************************************************************

*/

#include "uInputQueue.h"
#include "rScreen.h"
#include "tConfiguration.h"
#include <iostream>

#ifndef DEDICATED
#include "rSDL.h"
#endif

#include  "tRecorder.h"

#include  "uMenu.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/em_asm.h>
#include <cstddef>
#include <cstdio>
#include "uInput.h"
#include "wdbg.h"
#endif

static su_TimerCallback *timer=NULL;

su_TimerCallback::su_TimerCallback(){
    timer = this;
}

su_TimerCallback::~su_TimerCallback(){
    if (timer == this)
        timer = NULL;
}

static inline REAL Time(){
    if (timer)
        return timer->GetTime();
    else
        return 0;
}

bool su_prefetchInput=false;
bool su_contInput=true;

#define MAX_PENDING_INPUT 100

static REAL times[MAX_PENDING_INPUT];
static SDL_Event tEvents[MAX_PENDING_INPUT];

static int   currentIn=0,current_out=0,next_in=1;


static inline void increase(int &i){
    i++;
    if (i>=MAX_PENDING_INPUT)
        i=0;
}


static bool input_get=false;

void su_FetchAndStoreSDLInput()
{
#ifndef DEDICATED
#ifndef WIN32
#ifndef MACOSX
    if (!tRecorder::IsRunning() )
        SDL_PumpEvents();
#endif
#endif
#endif
}


bool su_StoreSDLEvent(const SDL_Event &tEvent){
    if (next_in!=current_out && !input_get){
        //con << "Extra input!\n";
        tEvents[currentIn]=tEvent;
        times[currentIn]=Time();
        increase(currentIn);
        next_in=currentIn;
        increase(next_in);
        return false;
    }
    return true;
}

#ifndef DEDICATED
// read and write operators for keysyms
tRECORDING_ENUM( SDLKey );
tRECORDING_ENUM( SDLMod );
#ifdef __EMSCRIPTEN__
tRECORDING_ENUM( SDL_Scancode );
#endif
#endif

static char const * recordingSection = "INPUT";

// (wasm build) the emscripten SDL port exposes SDL2-style event type values
// (SDL_KEYDOWN=0x300, SDL_ACTIVEEVENT=0x7000, ...), while the .adr recording
// format stores the original SDL 1.2 values (SDL_KEYDOWN=2, SDL_ACTIVEEVENT=1,
// ...). Translate between the two at the archive boundary so recordings made
// on any platform interoperate.
#ifdef __EMSCRIPTEN__
enum
{
    kSdl12ActiveEvent     = 1,
    kSdl12KeyDown         = 2,
    kSdl12KeyUp           = 3,
    kSdl12MouseMotion     = 4,
    kSdl12MouseButtonDown = 5,
    kSdl12MouseButtonUp   = 6,
    kSdl12Quit            = 11
};

static Uint32 atFileToRuntimeEventType( Uint32 t )
{
    switch ( t )
    {
    case kSdl12ActiveEvent:     return SDL_ACTIVEEVENT;
    case kSdl12KeyDown:         return SDL_KEYDOWN;
    case kSdl12KeyUp:           return SDL_KEYUP;
    case kSdl12MouseMotion:     return SDL_MOUSEMOTION;
    case kSdl12MouseButtonDown: return SDL_MOUSEBUTTONDOWN;
    case kSdl12MouseButtonUp:   return SDL_MOUSEBUTTONUP;
    case kSdl12Quit:            return SDL_QUIT;
    default:                    return t;
    }
}

static Uint32 atRuntimeToFileEventType( Uint32 t )
{
    switch ( t )
    {
    case SDL_ACTIVEEVENT:     return kSdl12ActiveEvent;
    case SDL_KEYDOWN:         return kSdl12KeyDown;
    case SDL_KEYUP:           return kSdl12KeyUp;
    case SDL_MOUSEMOTION:     return kSdl12MouseMotion;
    case SDL_MOUSEBUTTONDOWN: return kSdl12MouseButtonDown;
    case SDL_MOUSEBUTTONUP:   return kSdl12MouseButtonUp;
    case SDL_QUIT:            return kSdl12Quit;
    default:                  return t;
    }
}

// The .aarec input section stores SDL_Event.key.keysym.sym as a raw number and
// the format is defined by the SDL 1.2 values that the native build writes.
// This build links emscripten's port of SDL 1.2, which numbers the non-ASCII
// keys as 0x400 | SDL2 scancode: SDLK_DOWN is 1105 here, neither SDL 1.2's 274
// nor SDL2's 0x40000051.  The special keys therefore have to be translated at
// the archive boundary, exactly like the event type above.  Without it a
// recording arrives with SDLK_DOWN == 274 and every arrow, function and keypad
// key is dropped silently -- a replayed recording can then neither navigate a
// menu nor steer a light cycle.  ASCII keys (letters, digits, RETURN = 13,
// ESCAPE = 27, DELETE = 127) agree in both APIs: they are absent from the
// table and pass through unchanged.
// Generated by webgl/gen_sdl12_keymap.py, whose docstring explains how to
// re-check this convention against the SDL headers the build uses.
static const struct atKeySymPair
{
    Uint32 file;
    Uint32 runtime;
}
atKeySymPairs[] =
{
    { 256u, 0x0462u },  /* SDLK_KP0 */
    { 257u, 0x0459u },  /* SDLK_KP1 */
    { 258u, 0x045Au },  /* SDLK_KP2 */
    { 259u, 0x045Bu },  /* SDLK_KP3 */
    { 260u, 0x045Cu },  /* SDLK_KP4 */
    { 261u, 0x045Du },  /* SDLK_KP5 */
    { 262u, 0x045Eu },  /* SDLK_KP6 */
    { 263u, 0x045Fu },  /* SDLK_KP7 */
    { 264u, 0x0460u },  /* SDLK_KP8 */
    { 265u, 0x0461u },  /* SDLK_KP9 */
    { 266u, 0x0463u },  /* SDLK_KP_PERIOD */
    { 267u, 0x0454u },  /* SDLK_KP_DIVIDE */
    { 268u, 0x0455u },  /* SDLK_KP_MULTIPLY */
    { 269u, 0x0456u },  /* SDLK_KP_MINUS */
    { 270u, 0x0457u },  /* SDLK_KP_PLUS */
    { 271u, 0x0458u },  /* SDLK_KP_ENTER */
    { 272u, 0x0467u },  /* SDLK_KP_EQUALS */
    { 273u, 0x0452u },  /* SDLK_UP */
    { 274u, 0x0451u },  /* SDLK_DOWN */
    { 275u, 0x044Fu },  /* SDLK_RIGHT */
    { 276u, 0x0450u },  /* SDLK_LEFT */
    { 277u, 0x0449u },  /* SDLK_INSERT */
    { 278u, 0x044Au },  /* SDLK_HOME */
    { 279u, 0x044Du },  /* SDLK_END */
    { 280u, 0x044Bu },  /* SDLK_PAGEUP */
    { 281u, 0x044Eu },  /* SDLK_PAGEDOWN */
    { 282u, 0x043Au },  /* SDLK_F1 */
    { 283u, 0x043Bu },  /* SDLK_F2 */
    { 284u, 0x043Cu },  /* SDLK_F3 */
    { 285u, 0x043Du },  /* SDLK_F4 */
    { 286u, 0x043Eu },  /* SDLK_F5 */
    { 287u, 0x043Fu },  /* SDLK_F6 */
    { 288u, 0x0440u },  /* SDLK_F7 */
    { 289u, 0x0441u },  /* SDLK_F8 */
    { 290u, 0x0442u },  /* SDLK_F9 */
    { 291u, 0x0443u },  /* SDLK_F10 */
    { 292u, 0x0444u },  /* SDLK_F11 */
    { 293u, 0x0445u },  /* SDLK_F12 */
    { 294u, 0x0468u },  /* SDLK_F13 */
    { 295u, 0x0469u },  /* SDLK_F14 */
    { 296u, 0x046Au },  /* SDLK_F15 */
    { 300u, 0x0453u },  /* SDLK_NUMLOCK */
    { 301u, 0x0439u },  /* SDLK_CAPSLOCK */
    { 302u, 0x0447u },  /* SDLK_SCROLLOCK */
    { 303u, 0x04E5u },  /* SDLK_RSHIFT */
    { 304u, 0x04E1u },  /* SDLK_LSHIFT */
    { 305u, 0x04E4u },  /* SDLK_RCTRL */
    { 306u, 0x04E0u },  /* SDLK_LCTRL */
    { 307u, 0x04E6u },  /* SDLK_RALT */
    { 308u, 0x04E2u },  /* SDLK_LALT */
    { 309u, 0x04E7u },  /* SDLK_RMETA */
    { 310u, 0x04E3u },  /* SDLK_LMETA */
    { 311u, 0x04E3u },  /* SDLK_LSUPER */
    { 312u, 0x04E7u },  /* SDLK_RSUPER */
    { 313u, 0x0501u },  /* SDLK_MODE */
    { 314u, 0x0476u },  /* SDLK_COMPOSE */
    { 315u, 0x0475u },  /* SDLK_HELP */
    { 316u, 0x0446u },  /* SDLK_PRINT */
    { 317u, 0x049Au },  /* SDLK_SYSREQ */
    { 319u, 0x0476u },  /* SDLK_MENU */
    { 320u, 0x0466u },  /* SDLK_POWER */
    { 322u, 0x047Au },  /* SDLK_UNDO */
};

// Turn a key number the browser SDL layer reports into the SDL 1.2 number the
// game works with.  Below 256 both layers carry the character itself, so those
// pass straight through; everything else arrives as 0x400 | scancode and is
// looked up.  A key with no SDL 1.2 equivalent at all -- the function keys above
// F15, the internet and media keys SDL 2 invented -- becomes SDLK_UNKNOWN: it
// binds to nothing, exactly as it does on the desktop, and it can never index
// keymap[]/pressed[] past the end now that those arrays are sized by
// SDLK_NEWLAST -- the desktop's 323 keys plus the mouse and pad pseudo-keys --
// instead of the oversized 1550 the SDL layer's own SDLK_LAST would give them.
static Uint32 atRuntimeToFileKeySym( Uint32 s )
{
    if ( s < 256 )
        return s;
    // Only the browser layer's own 0x400|scancode range needs translating.
    // Everything else above 255 is already in the desktop numbering: key
    // numbers read back out of a recording, numbers written into configuration
    // files, and the mouse and pad pseudo-keys past SDLK_LAST.  Looking those up
    // anyway turned every replayed arrow, function and keypad key into
    // SDLK_UNKNOWN, so a replayed recording could navigate no menu and steer no
    // light cycle -- the desktop build reads the same file without a mapping
    // layer and works fine, which is how the divergence stayed hidden.
    if ( s < 0x400u || s > 0x4FFu )
        return s;
    for ( size_t i = 0; i < sizeof( atKeySymPairs ) / sizeof( atKeySymPairs[0] ); ++i )
    {
        if ( atKeySymPairs[i].runtime == s )
            return atKeySymPairs[i].file;
    }
    return SDLK_UNKNOWN;
}

// Display names for the key numbers this build stores and displays.  The
// desktop build gets them from SDL 1.2's keynames[] table, which SDL fills in
// SDL_EnableKeyMap(); emscripten's SDL_GetKeyName() (library_sdl.js) names a-z
// and 0-9 and nothing else, so the table has to travel with the port.  It is
// generated -- see webgl/gen_sdl12_keynames.py.
#include "uSDL12KeyNames.h"

static char const *atSDL12KeyNameLookup( Uint32 sym )
{
    for ( size_t i = 0; i < sizeof( atSDL12KeyNames ) / sizeof( atSDL12KeyNames[0] ); ++i )
    {
        if ( atSDL12KeyNames[i].sym == sym )
            return atSDL12KeyNames[i].name;
    }
    return NULL;
}

char const *su_SDL12KeyName( Uint32 sym )
{
    char const *name = atSDL12KeyNameLookup( sym );
    if ( !name )
    {
        // A key pressed in the browser is stored in this build's own numbering
        // while a configuration default or a .aarec holds the SDL 1.2 number;
        // the label the player reads has to be the same either way.
        Uint32 fileSym = atRuntimeToFileKeySym( sym );
        if ( fileSym != sym )
            name = atSDL12KeyNameLookup( fileSym );
    }
    return name;
}
#endif

//! Read or write event data
template< class Archiver > class EventArchiver
{
public:
#ifndef DEDICATED
    static void ArchiveKey( Archiver & archive, SDL_KeyboardEvent & key )
    {
        // Every platform stores SDL 1.2 key numbers in the file.  This build
        // converts the browser's key numbers to that numbering before the game
        // ever sees an event (see su_GetSDLInput), so the game, its bindings and
        // the archive all agree on one numbering and a recording made anywhere
        // plays back everywhere with every key press landing on the same action.
        archive.Archive(key.state).Archive(key.keysym.scancode).Archive(key.keysym.sym).Archive(key.keysym.mod).Archive(key.keysym.unicode);
    }
#endif

    static bool Archive( SDL_Event & event, REAL & time, bool & ret )
    {
        // start archive block if archiving is active
        Archiver archive;
        if ( archive.Initialize( recordingSection ) )
        {
#ifndef DEDICATED
            archive.Archive( ret );
            if ( !ret )
                return false;

            // write or read data
            archive.Archive( time );
#ifdef __EMSCRIPTEN__
            if ( tRecorder::IsPlayingBack() )
            {
                // file stores SDL 1.2 event types; convert to runtime values
                Uint32 wireType = 0;
                archive.Archive( wireType );
                event.type = atFileToRuntimeEventType( wireType );
            }
            else
            {
                // file must hold SDL 1.2 event types for native compatibility
                Uint32 wireType = atRuntimeToFileEventType( event.type );
                archive.Archive( wireType );
            }
#else
            archive.Archive( event.type );
#endif
            switch ( event.type )
            {
            case SDL_ACTIVEEVENT:
            {
                SDL_ActiveEvent & active = event.active;

                archive.Archive(active.gain).Archive(active.state);
            }
            break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                SDL_KeyboardEvent & key = event.key;
                ArchiveKey( archive, key );
            }
            break;
            case SDL_MOUSEMOTION:
            {
                SDL_MouseMotionEvent & motion = event.motion;

                archive.Archive(motion.state).Archive(motion.x).Archive(motion.y).Archive(motion.xrel).Archive(motion.yrel);
            }
            break;
            case SDL_MOUSEBUTTONUP:
            case SDL_MOUSEBUTTONDOWN:
            {
                SDL_MouseButtonEvent & button = event.button;

                archive.Archive(button.button).Archive(button.state).Archive(button.x).Archive(button.y);
            }
            break;
            default:
                // do nothing
                break;
            }

#endif  // DEDICATED

            return true;
        }

        return false;
    }
};

#ifndef DEDICATED
//! Read or write event data
template<>
void EventArchiver< tRecordingBlock >::ArchiveKey( tRecordingBlock & archive, SDL_KeyboardEvent & orig )
{
    SDL_KeyboardEvent key = orig;
    if ( uInputScrambler::Scrambled() )
    {
        switch( key.keysym.sym )
        {
        case SDLK_ESCAPE:
        case SDLK_SPACE:
        case SDLK_KP_ENTER:
        case SDLK_RETURN:
        case SDLK_UP:
        case SDLK_DOWN:
        case SDLK_LEFT:
        case SDLK_RIGHT:
        case SDLK_BACKSPACE:
        case SDLK_DELETE:
            break;
        default:
            key.keysym.mod = KMOD_NONE;
            key.keysym.sym = SDLK_x;
#ifdef __EMSCRIPTEN__
            key.keysym.scancode = (SDL_Scancode)0;
#else
            key.keysym.scancode = 0;
#endif
            key.keysym.unicode = '*';
        }
    }

    archive.Archive(key.state).Archive(key.keysym.scancode).Archive(key.keysym.sym).Archive(key.keysym.mod).Archive(key.keysym.unicode);
}
#endif

static const char * su_end = "END";
static const char * su_endInput = "ENDINPUT";

// flag indicating input was made and an input start marker is needed for the next input loop
static bool su_markerRequired = false;

void su_EndGetSDLInput()
{
    if ( su_markerRequired )
    {
        // record end of input fetching
        tRecorder::Playback(su_endInput);
        tRecorder::Record(su_endInput);
        su_markerRequired = false;
    }
}

uInputProcessGuard::uInputProcessGuard()
{}
uInputProcessGuard::~uInputProcessGuard()
{
    su_EndGetSDLInput();
}

int uInputScrambler::scrambled_ = 0;

uInputScrambler::uInputScrambler()
{
    scrambled_ ++;
}

uInputScrambler::~uInputScrambler()
{
    --scrambled_;
}

bool uInputScrambler::Scrambled()
{
    return scrambled_ > 0;
}

#if defined(__EMSCRIPTEN__) && !defined(DEDICATED)
// *********************************************
//  Game pad, wasm build
// *********************************************
//
// A pad is not a keyboard, but every input binding in this tree is one key
// number mapped to one action, and the mouse already exploits that by owning
// pseudo keysyms past SDLK_LAST (uInput.h).  The pad does the same: its buttons
// and stick directions get numbers of their own, and the code below reads the
// browser's Gamepad API and posts KEYDOWN/KEYUP events carrying those numbers
// into the queue su_GetSDLInput() drains.  Binding and rebinding in the input
// configuration menu, the "press <> or <> to turn right" tooltips, the
// double-bind timeout, the key state the analog actions run on and the .aarec
// input section then all work on pad controls without any of them knowing a
// pad exists.
//
// The state is read from the Gamepad API rather than through SDL because the
// SDL layer this build links exposes no joystick support at all.  Button and
// axis order is the W3C standard gamepad layout, which is what Chrome reports
// for a wired or wireless Xbox 360/One/Series controller, and the default
// bindings below are written for that pad.
//
// Browsers hand out the pad object only after the player has pressed a button,
// so the poll runs every time input is fetched and copes with the pad
// appearing or disappearing; a pad that disappears releases everything it was
// holding, so an unplugged pad does not leave the cycle turning into a wall.
// The sticks are analog while the player actions are digital, so each stick
// axis turns into up to two digital directions behind a dead zone.
namespace
{
// W3C standard button order.  The guide button is deliberately unused: the
// browser keeps it.
enum atPadButton
{
    atPAD_A = 0, atPAD_B, atPAD_X, atPAD_Y,
    atPAD_LB, atPAD_RB, atPAD_LT, atPAD_RT,
    atPAD_BACK, atPAD_START, atPAD_LSTICK_CLICK, atPAD_RSTICK_CLICK,
    atPAD_DUP, atPAD_DDOWN, atPAD_DLEFT, atPAD_DRIGHT, atPAD_GUIDE,
    atPAD_BUTTON_COUNT = 17
};

// Standard layout axes: left stick x/y, then right stick x/y.  Some mappings
// mirror the triggers into these slots as well; the triggers are read as
// buttons, where the pad reports their pull directly.
enum { atPAD_AXIS_COUNT = 4 };

struct atPadSample
{
    unsigned char present;
    unsigned char button[atPAD_BUTTON_COUNT];
    unsigned char align[2];         // keeps the axis array 4-byte aligned
    float axis[atPAD_AXIS_COUNT];
};

atPadSample s_pad;

//! Copy the first connected pad into s_pad, one JS round trip per call.
//! Returns 0 when the sample was written, 1 when it could not be.  The body
//! catches everything it can throw: this runs inside the input pump, and an
//! exception crossing the wasm boundary takes the pump -- and with it the
//! keyboard -- down with it.
int atReadPad()
{
    return EM_ASM_ARGS({
        // No top-level commas anywhere inside the braces: the C++ preprocessor
        // splits macro arguments at them, and only the first piece becomes the
        // JavaScript body -- the rest is parsed as C++ arguments to the macro.
        try
        {
            // The heap views are reachable through Module only because
            // webgl/build.sh exports them.  They are read through Module and
            // not by bare name because an EM_ASM body is turned into a string
            // and evaluated outside the scope of the generated glue, where the
            // names the glue uses have been shortened.
            var b8 = Module.HEAPU8;
            var f32 = Module.HEAPF32;
            if (!b8 || !f32)
                return 1;
            var dst = $0;
            var bOff = $1;
            var aOff = $2;
            var pads = navigator.getGamepads ? navigator.getGamepads() : [];
            var pad = null;
            for (var i = 0; i < pads.length; ++i)
            {
                if (pads[i] && pads[i].connected) { pad = pads[i]; break; }
            }
            if (!pad)
            {
                b8[dst] = 0;
                return 0;
            }
            b8[dst] = 1;
            var nb = Math.min(pad.buttons.length, 17);
            for (var b = 0; b < 17; ++b)
            {
                var btn = (b < nb) ? pad.buttons[b] : null;
                // Triggers report their pull in .value and only set .pressed
                // past the halfway point; taking either keeps them digital.
                b8[dst + bOff + b] = (btn && (btn.pressed || btn.value > 0.5)) ? 1 : 0;
            }
            var na = Math.min(pad.axes.length, 4);
            var base = (dst + aOff) >> 2;
            for (var a = 0; a < 4; ++a)
            {
                var v = (a < na) ? pad.axes[a] : 0;
                f32[base + a] = (typeof v === 'number') ? v : 0;
            }
            return 0;
        }
        catch (e)
        {
            return 1;
        }
    }, &s_pad, (int)offsetof(atPadSample, button), (int)offsetof(atPadSample, axis));
}

//! One physical control mapped to one key number.
struct atPadControl
{
    int  padIndex;  // button index, or axis index when isAxis
    bool isAxis;
    int  side;      // axes only: -1 for the negative half, 1 for the positive
    int  keysym;
};

const atPadControl s_padControls[] =
{
    // The d-pad and the two face buttons used in menus post real key numbers:
    // menus navigate on key numbers, and the arrow keys carry navigation
    // bindings in every keyboard layout, so a player gets working menus and
    // in-game behaviour that matches the layout they chose without having to
    // configure anything.
    { atPAD_DUP,          false, 0, SDLK_UP },
    { atPAD_DDOWN,        false, 0, SDLK_DOWN },
    { atPAD_DLEFT,        false, 0, SDLK_LEFT },
    { atPAD_DRIGHT,       false, 0, SDLK_RIGHT },
    { atPAD_A,            false, 0, SDLK_SPACE },
    { atPAD_B,            false, 0, SDLK_ESCAPE },

    // Everything else posts a pad number and is bound to an action below.
    { atPAD_X,            false, 0, SDLK_PAD_X },
    { atPAD_Y,            false, 0, SDLK_PAD_Y },
    { atPAD_LB,           false, 0, SDLK_PAD_LB },
    { atPAD_RB,           false, 0, SDLK_PAD_RB },
    { atPAD_LT,           false, 0, SDLK_PAD_LT },
    { atPAD_RT,           false, 0, SDLK_PAD_RT },
    { atPAD_BACK,         false, 0, SDLK_PAD_BACK },
    { atPAD_START,        false, 0, SDLK_PAD_START },
    { atPAD_LSTICK_CLICK, false, 0, SDLK_PAD_LSTICK },
    { atPAD_RSTICK_CLICK, false, 0, SDLK_PAD_RSTICK },

    { 0, true, -1, SDLK_PAD_LLEFT },
    { 0, true,  1, SDLK_PAD_LRIGHT },
    { 1, true, -1, SDLK_PAD_LUP },
    { 1, true,  1, SDLK_PAD_LDOWN },
    { 2, true, -1, SDLK_PAD_RLEFT },
    { 2, true,  1, SDLK_PAD_RRIGHT },
    { 3, true, -1, SDLK_PAD_RUP },
    { 3, true,  1, SDLK_PAD_RDOWN },
};

bool s_held[sizeof(s_padControls) / sizeof(s_padControls[0])];

//! Post a press or a release for one pad control.  The event is pushed into
//! SDL's own queue so that every loop which reads input - the game's, a menu's,
//! the replay archiver - sees a pad control exactly as it sees a key press,
//! which is the point.
void atInjectKey(int keysym, bool down)
{
    SDL_Event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    ev.key.state = down ? 1 : 0;        // SDL 1.2 SDL_PRESSED / SDL_RELEASED
    ev.key.keysym.sym = (SDLKey)keysym;
    ev.key.keysym.mod = KMOD_NONE;
    ev.key.keysym.scancode = (SDL_Scancode)0;
    // su_GetSDLInput() throws away presses of key numbers below SDLK_z that
    // carry no character, a filter for keyboard events the browser mangles.
    // Handing the real key numbers their character gets them past that filter,
    // just as a keyboard press does; pad numbers are above the filtered range
    // anyway.
    ev.key.keysym.unicode = (down && keysym < 256) ? (Uint16)keysym : 0;
    // Push into SDL rather than into su_GetSDLInput()'s own queue.  A menu
    // pumps SDL directly - and a menu is open during most of what a pad player
    // does: the local game's settings menu stays open behind a running round,
    // as does the chat line.  A queued event is invisible to those loops while
    // a real browser key is not, so pushing is what makes a pad control reach
    // the game exactly where a keyboard control does.
    sr_LockSDL();
    SDL_PushEvent(&ev);
    sr_UnlockSDL();
}

struct atPadDefaultBind
{
    int          keysym;
    char const  *action;
    int          player;    // PLAYER_BIND numbering: 0 is global, 1 is player 1
};

// A standard Xbox pad laid out the way a pad player expects: the left stick and
// the bumpers steer, the triggers brake, the right stick moves the camera, a
// stick click changes the view, X looks back, Y toggles the brake, back shows
// the score and start opens the in-game menu.  The right stick's click and the
// left stick's up and down stay unbound on purpose: a game that accelerates by
// itself has nothing sensible for them to do.
const atPadDefaultBind s_padDefaults[] =
{
    { SDLK_PAD_LLEFT,  "CYCLE_TURN_LEFT",     1 },
    { SDLK_PAD_LRIGHT, "CYCLE_TURN_RIGHT",    1 },
    { SDLK_PAD_LB,     "CYCLE_TURN_LEFT",     1 },
    { SDLK_PAD_RB,     "CYCLE_TURN_RIGHT",    1 },
    { SDLK_PAD_LT,     "CYCLE_BRAKE",         1 },
    { SDLK_PAD_RT,     "CYCLE_BRAKE",         1 },
    { SDLK_PAD_X,      "GLANCE_BACK",         1 },
    { SDLK_PAD_Y,      "CYCLE_BRAKE_TOGGLE",  1 },
    { SDLK_PAD_LSTICK, "SWITCH_VIEW",         1 },
    { SDLK_PAD_RLEFT,  "LOOK_LEFT",           1 },
    { SDLK_PAD_RRIGHT, "LOOK_RIGHT",          1 },
    { SDLK_PAD_RUP,    "MOVE_UP",             1 },
    { SDLK_PAD_RDOWN,  "MOVE_DOWN",           1 },
    { SDLK_PAD_BACK,   "SCORE",               0 },
    { SDLK_PAD_START,  "INGAME_MENU",         0 },
};

bool s_gamepadSupport = true;
int  s_gamepadDefaultsAdded = 0;

// The marker is a version, not a flag.  Builds up to 2026-11-13 numbered the
// mouse and pad pseudo-keys from the SDL layer's own SDLK_LAST and wrote those
// numbers -- together with this marker -- into the user configuration.  A
// player carrying such a config has pad bindings pointing at key numbers that
// no longer exist, and the marker stopped the corrected ones from ever being
// made.  Re-running the pass is safe: a key number that is already bound is
// left alone, so a control the player rebound deliberately keeps that binding.
// Bump the version whenever the default map or the key numbering changes.
int const s_padBindVersion = 2;

tConfItem<bool> s_gamepadSupportConf("GAMEPAD_SUPPORT", s_gamepadSupport);
tConfItem<int>  s_gamepadDefaultsConf("GAMEPAD_DEFAULTS_ADDED", s_gamepadDefaultsAdded);

//! Add the default pad bindings, once.  The marker travels in the user
//! configuration, so a player who rebinds or clears a pad control in the input
//! menu keeps that decision from then on.
void atAddPadDefaults()
{
    if (s_gamepadDefaultsAdded >= s_padBindVersion)
        return;
    s_gamepadDefaultsAdded = s_padBindVersion;

    int added = 0, already = 0, conflict = 0, unknown = 0;
    for (size_t i = 0; i < sizeof(s_padDefaults) / sizeof(s_padDefaults[0]); ++i)
    {
        int sym = s_padDefaults[i].keysym;
        uAction *act = uAction::Find(s_padDefaults[i].action);
        if (!act)
        {
            ++unknown;
            continue;
        }
        if (sym < 0 || sym >= SDLK_NEWLAST)
            continue;
        uBind *old = keymap[sym];
        if (old)
        {
            // The binding is there: an earlier build made it and the user
            // configuration carried it back, or the player bound the key by
            // hand.  Report it, never overwrite it.
            if (old->act != act)
                ++conflict;
            ++already;
            continue;
        }
        keymap[sym] = uBindPlayer::NewBind(act, s_padDefaults[i].player);
        ++added;
    }
    // Only say something when the defaults moved or something failed to bind:
    // the ordinary "everything was already there" case is not worth a line.
    if (added || conflict || unknown)
    {
        fprintf(stderr, "Armagetron: game pad defaults: %d added, %d already bound, %d on other actions, %d unknown.\n",
                added, already, conflict, unknown);
        fflush(stderr);
    }
}

// A pad whose state cannot be sampled is worse than no pad: the read throws
// across the wasm boundary and takes the whole input pump with it.  Give up on
// the pad after a few failed reads.  This is deliberately not tied to the
// GAMEPAD_SUPPORT setting -- a player's own choice must not be overwritten in
// the user configuration by a failure that may be specific to one session.
bool s_padDisabled = false;
int  s_padReadFailures = 0;

//! Sample the pad and turn every change of state into a key press or release.
void atPollPad()
{
    if (!s_gamepadSupport || s_padDisabled)
        return;

    if (atReadPad() != 0)
    {
        if (++s_padReadFailures == 4)
        {
            s_padDisabled = true;
            fprintf(stderr, "Armagetron: the game pad could not be read; pad support is off.\n");
            fflush(stderr);
        }
        return;
    }

    if (s_pad.present)
        atAddPadDefaults();

    // Wall-clock scheduling for the key-repeat emulation below: when a control
    // started being held, and when it last fired a repeat.
    static Uint32 s_holdStart[sizeof(s_held) / sizeof(s_held[0])];
    static Uint32 s_lastRepeat[sizeof(s_held) / sizeof(s_held[0])];

    for (size_t c = 0; c < sizeof(s_held) / sizeof(s_held[0]); ++c)
    {
        atPadControl const &ctl = s_padControls[c];

        bool down;
        if (!ctl.isAxis)
        {
            down = (s_pad.present && s_pad.button[ctl.padIndex] != 0);
        }
        else
        {
            float v = s_pad.present ? s_pad.axis[ctl.padIndex] : 0;
            v *= ctl.side;
            // Digital edges on an analog axis: press past 0.6, release back
            // inside 0.4, so a stick resting off centre holds still instead of
            // chattering between the two directions.
            down = s_held[c] ? (v > 0.4f) : (v > 0.6f);
        }

        bool edge = (down != s_held[c]);
        if (edge)
            s_held[c] = down;

        // A release is a single event, on its edge.
        if (!down)
        {
            if (edge)
                atInjectKey(ctl.keysym, false);
            continue;
        }

        if (edge)
        {
            s_holdStart[c] = s_lastRepeat[c] = SDL_GetTicks();
            atInjectKey(ctl.keysym, true);
            continue;
        }

        // Still held.  Every action in this game is written for a keyboard,
        // where a held key keeps arriving because the window system repeats it;
        // a light cycle turns because it is told about the held key again and
        // again.  A pad control has no repeat of its own, so a single press
        // would nudge the cycle once and then stop.  Emulate the SDL default -
        // half a second of delay, then about thirty repeats a second - which is
        // what holding the same key on the keyboard does.
        Uint32 const now = SDL_GetTicks();
        if ((Sint32)(now - s_holdStart[c]) > 500 && (Sint32)(now - s_lastRepeat[c]) >= 33)
        {
            s_lastRepeat[c] = now;
            atInjectKey(ctl.keysym, true);
        }
    }
}
}   // namespace
#endif

bool su_GetSDLInput(SDL_Event &tEvent,REAL &time){
    bool ret=false;

    // clear out data
    memset( &tEvent, 0, sizeof( SDL_Event ) );

    // find end of recording in playback
    if ( tRecorder::Playback(su_end) )
    {
        tRecorder::Record(su_end);
        uMenu::quickexit=uMenu::QuickExit_Total;
    }

    // try to fetch event from playback
    if ( !EventArchiver< tPlaybackBlock >::Archive( tEvent, time, ret ) )
    {
        // get real event
        // The pad is sampled here rather than in a frame hook: this is the one
        // place every input consumer passes through -- game frames, menus, the
        // server browser -- and this branch is not reached while a recording is
        // playing back, so replayed input is left alone.
#if defined(__EMSCRIPTEN__) && !defined(DEDICATED)
        atPollPad();
#endif
        sr_LockSDL();
        input_get=true;
        if (current_out!=currentIn){
            time=times[current_out];
            tEvent=tEvents[current_out];
            increase(current_out);
            ret=true;
        }
        else{
            time=Time();
            ret=
#ifndef DEDICATED
                SDL_PollEvent(&tEvent);
#else
                false;
#endif
        }
        sr_UnlockSDL();
        input_get=false;
    }
#ifdef __EMSCRIPTEN__
        // A key number coming from the browser is not the number the game works
        // with.  Everything key related in this tree is built around the SDL 1.2
        // numbers the desktop build uses: the keymap[]/pressed[] tables
        // (uInput.cpp, sized SDLK_NEWLAST), the PLAYER_BIND
        // configuration, the .aarec input format and the key names shown in the
        // HUD.  The browser's SDL 2 layer numbers the same keys differently
        // (SDLK_DOWN is 0x451 here, 274 on the desktop), which leaves a browser
        // key press on a binding that does not exist -- and, worse, indexes
        // keymap[0x451] past the end of a 1038-entry array.  Convert once, at
        // the single place where a real event enters the game, so every consumer
        // downstream sees the desktop numbering.  Events that came out of a
        // playback archive are already in that numbering and pass through.
        if ( ret && ( tEvent.type == SDL_KEYDOWN || tEvent.type == SDL_KEYUP ) )
            tEvent.key.keysym.sym = atRuntimeToFileKeySym( tEvent.key.keysym.sym );
#endif

    su_markerRequired |= ret;

    // store event in recording
    if ( ret )
        EventArchiver< tRecordingBlock >::Archive( tEvent, time, ret );

#ifndef DEDICATED
    // filter bogus events. Some keys cause key events with wrong keysyms.
    static unsigned short blockedScancode = 0xffff;
    static SDLKey blockedKeysym = SDLK_LAST;

    if( tEvent.type == SDL_KEYDOWN )
    {
        // you can spot them by zero unicode; control keys are allowed to have that,
        // but not letter and number and sign keys
        if( tEvent.key.keysym.unicode == 0 )
        {
            if ( tEvent.key.keysym.sym >= SDLK_ESCAPE && 
                 tEvent.key.keysym.sym <= SDLK_z )
            {
                ret = false;

                blockedScancode = tEvent.key.keysym.scancode;
                blockedKeysym = tEvent.key.keysym.sym;
            }
        }
    }
    else if ( tEvent.type == SDL_KEYUP )
    {
        if( blockedScancode == tEvent.key.keysym.scancode && 
            blockedKeysym == tEvent.key.keysym.sym )
        {
            ret = false;

            blockedScancode = 0xffff;
            blockedKeysym = SDLK_LAST;
        }
    }
#endif

    return ret;
}

/*
int su_InputThread(void *){
    while (su_contInput){
        if (sr_screen && su_prefetchInput){
            sr_LockSDL();
#ifndef DEDICATED
            SDL_PumpEvents();
#endif
            sr_UnlockSDL();
        }
#ifndef WIN32
        usleep(100000);
#endif
    }
    return 0;
}
*/


