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

static Uint32 atFileToRuntimeKeySym( Uint32 s )
{
    for ( size_t i = 0; i < sizeof( atKeySymPairs ) / sizeof( atKeySymPairs[0] ); ++i )
    {
        if ( atKeySymPairs[i].file == s )
            return atKeySymPairs[i].runtime;
    }
    return s;
}

static Uint32 atRuntimeToFileKeySym( Uint32 s )
{
    for ( size_t i = 0; i < sizeof( atKeySymPairs ) / sizeof( atKeySymPairs[0] ); ++i )
    {
        if ( atKeySymPairs[i].runtime == s )
            return atKeySymPairs[i].file;
    }
    return s;
}
#endif

//! Read or write event data
template< class Archiver > class EventArchiver
{
public:
#ifndef DEDICATED
    static void ArchiveKey( Archiver & archive, SDL_KeyboardEvent & key )
    {
        archive.Archive(key.state).Archive(key.keysym.scancode).Archive(key.keysym.sym).Archive(key.keysym.mod).Archive(key.keysym.unicode);
#ifdef __EMSCRIPTEN__
        // reading back: the stored SDL 1.2 keysym becomes this build's key
        if ( tRecorder::IsPlayingBack() )
            key.keysym.sym = atFileToRuntimeKeySym( key.keysym.sym );
#endif
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

#ifdef __EMSCRIPTEN__
    // storing: write the SDL 1.2 number so the file stays readable on native
    key.keysym.sym = atRuntimeToFileKeySym( key.keysym.sym );
#endif
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


