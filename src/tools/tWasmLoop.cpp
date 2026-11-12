#include "tWasmLoop.h"

#ifdef __EMSCRIPTEN__

#include <emscripten.h>
#include "wdbg.h"

#include "tException.h"
#include "tConsole.h"
#include "tLocale.h"
#include "tRecorder.h"
#include "tSysTime.h"

namespace tWasmLoop
{

namespace
{
    enum { kMaxDepth = 32 };

    Frame s_frames[kMaxDepth] = { 0 };
    Exit  s_exits[kMaxDepth]  = { 0 };
    void * s_args[kMaxDepth] = { 0 };
    int   s_depth = 0;
    bool  s_registered = false;
    bool  s_paused = false;
    bool  s_stepPending = false;
    double s_pauseAt = -1.0; // (wasm) auto-pause when playback recSec reaches this
    Continuation s_continuation = 0;
    // animation frame time measurement + pacing state (wasm build)
    double s_tickLast = -1;
    double s_frameMs  = 2.0;

    // one animation frame: run the innermost loop's frame function.
    // (wasm build) while playing back, the recording was made at the native
    // frame rate, which can be far above the browser's rAF rate (e.g. 345fps
    // vs 60fps). Run several frames per animation tick so T sections are
    // consumed at the recorded rate and the playback stays in real time,
    // exactly like the native loop (which runs without a sleep in playback).
    // The count is capped by a CPU budget (a fraction of the tick interval)
    // so the tab does not saturate the main thread and stays responsive
    // to browser UI, JS and screenshots.
    bool Trampoline()
    {
        if ( s_depth <= 0 )
            return false;

        // (wasm build) A recording made by a native session stores the config
        // and file-read prefix of that session (user.cfg, autoexec.cfg, the
        // first-run wizard, ...).  Desynced playback used to be switched on
        // here unconditionally as a workaround for that prefix never
        // matching.  It is not switched on any more: desynced mode relaxes
        // the cycle-movement replay match (gCycleMovement.cpp) and changes
        // the time-archive path (tSysTime.cpp), so a port run that turns it
        // on is no longer running the same logic as the native run it is
        // being compared with.  If a strict section check fails, the recorder
        // switches the mode on by itself and the digest line says so.
        bool stepping = false;
        if ( s_paused )
        {
            if ( !s_stepPending )
                return s_depth > 0;
            s_stepPending = false;
            stepping = true; // run exactly one frame below
        }

        // (wasm build) auto-pause: when the playback reaches the target
        // recording time (set via SetPauseAt), freeze the loop so the tab
        // goes idle and the harness can inspect the frozen state.
        if ( !s_paused && !stepping && tRecorder::IsPlayingBack() && s_pauseAt > 0.0 )
        {
            if ( tPlaybackRecSec() >= s_pauseAt )
            {
                s_paused = true;
                wdbg("tWasmLoop: auto-paused at target recSec");
            }
        }

        // how many frames this tick runs (1 in normal mode)
        int reps = 1;
        if ( !stepping && tRecorder::IsPlayingBack() )
        {
            double now = emscripten_get_now();
            double dtms = ( s_tickLast >= 0 ) ? now - s_tickLast : 0;
            if ( dtms <= 0 || dtms > 250 )
                dtms = 16.7;
            s_tickLast = now;

            // desired count: consume T sections at the recorded frame rate
            reps = (int)( tPlaybackTargetFPS() * dtms * 1E-3 + 0.5 );
            if ( reps < 1 ) reps = 1;
            if ( reps > 64 ) reps = 64;

            // budget: spend at most ~70% of the tick on game frames, leaving
            // headroom for the rest of the page
            int budget = (int)( 0.7 * dtms / s_frameMs + 0.5 );
            if ( budget < 1 ) budget = 1;
            if ( reps > budget ) reps = budget;
        }

        while ( reps-- > 0 && s_depth > 0 )
        {
            bool cont = false;
            double f0 = emscripten_get_now();
            try
            {
                cont = s_frames[s_depth - 1]( s_args[s_depth - 1 ] );
            }
            catch ( tException const & e )
            {
                wdbgi("Trampoline: tException caught", s_depth);
                wdbg(e.GetName());
                // fatal: stop all loops and show an error dialog
                while ( s_depth > 0 )
                    Stop();
                tConsole::Message( e.GetName(), e.GetDescription(), 10 );
                return true; // keep the main loop alive for the dialog
            }
            catch ( ... )
            {
                wdbgi("Trampoline: generic exception caught", s_depth);
                while ( s_depth > 0 )
                    Stop();
                return false;
            }
            {
                // track the frame cost for the pacing budget
                double fms = emscripten_get_now() - f0;
                if ( fms > 0.05 )
                    s_frameMs = fms;
            }


            if ( cont )
                continue;

            // loop terminated: run its exit action, resume the outer loop
            Exit  exit = s_exits[s_depth - 1];
            void * arg  = s_args[s_depth - 1];
            --s_depth;
            if ( exit )
                exit( arg );

            // the outermost loop ended: run the registered continuation (the
            // next step of the startup flow; the C++ stack that "followed"
            // the first loop was unwound, see the header comment)
            if ( s_depth <= 0 && s_continuation )
            {
                Continuation c = s_continuation;
                s_continuation = 0;
                wdbg("tWasmLoop: running continuation");
                try
                {
                    c();
                }
                catch ( tException const & e )
                {
                    wdbgi("tWasmLoop: continuation threw", e.GetName().Len());
                    wdbg(e.GetName());
                    tConsole::Message( e.GetName(), e.GetDescription(), 10 );
                }
                catch ( ... )
                {
                }
            }

            // loop ended mid-tick: stop consuming reps, the outer loop
            // (if any) is served on the next animation tick
            return s_depth > 0;
        }

        return s_depth > 0;
    }
}

void Start( Frame frameFn, Exit exitFn, void * arg )
{
    // emscripten_set_main_loop wants a void(void) callback
    struct MainLoopAdapter
    {
        static void Frame()
        {
            Trampoline();
        }
    };
    if ( !frameFn || s_depth >= kMaxDepth )
        return;
    s_frames[s_depth] = frameFn;
    s_exits[s_depth]  = exitFn;
    s_args[s_depth]   = arg;
    ++s_depth;
    wdbgi("tWasmLoop::Start: depth now", s_depth);
    if ( !s_registered )
    {
        s_registered = true;
        wdbg("tWasmLoop::Start: calling emscripten_set_main_loop");
        emscripten_set_main_loop( &MainLoopAdapter::Frame, 0, 1 );
        wdbg("tWasmLoop::Start: emscripten_set_main_loop returned");
    }
    // note: the pump is never unregistered.  The main loop callback is
    // void, so once started it runs forever; calling
    // emscripten_set_main_loop again while the requestAnimationFrame
    // chain is pending would schedule a second pump (double frames).
}

void SetContinuation( Continuation fn )
{
    s_continuation = fn;
}

void Stop()
{
    if ( s_depth <= 0 )
        return;
    --s_depth;
    if ( s_exits[s_depth] )
        s_exits[s_depth]( s_args[s_depth] );
}

bool DropArg( void * arg )
{
    if ( !arg )
        return false;

    bool dropped = false;
    for ( int i = 0; i < s_depth; )
    {
        if ( s_args[i] != arg )
        {
            ++i;
            continue;
        }
        // remove the entry; the loops nested inside it move down one step
        for ( int j = i; j < s_depth - 1; ++j )
        {
            s_frames[j] = s_frames[j + 1];
            s_exits[j]  = s_exits[j + 1];
            s_args[j]   = s_args[j + 1];
        }
        --s_depth;
        s_frames[s_depth] = 0;
        s_exits[s_depth]  = 0;
        s_args[s_depth]   = 0;
        dropped = true;
        wdbgi("DropArg: dropped a loop entry, depth now", s_depth);
    }
    return dropped;
}


bool Active()
{
    return s_depth > 0;
}

int Depth()
{
    return s_depth;
}

void SetPaused( bool paused )
{
    s_paused = paused;
}

bool IsPaused()
{
    return s_paused;
}

void StepOne()
{
    // pause first (if running), then run exactly one frame on the next tick
    if ( !s_paused )
        s_paused = true;
    s_stepPending = true;
}

void SetPauseAt( double recSec )
{
    // < 0 disables auto-pause
    s_pauseAt = recSec;
}

} // namespace tWasmLoop

// (wasm build) JS-facing bridge for the playback harness / debugger:
// pause the game, resume it, step exactly one frame, and read the
// position in the recording.
extern "C"
{
    EMSCRIPTEN_KEEPALIVE void adSetPaused( int on )
    {
        tWasmLoop::SetPaused( on != 0 );
    }

    EMSCRIPTEN_KEEPALIVE int adIsPaused()
    {
        return tWasmLoop::IsPaused() ? 1 : 0;
    }

    EMSCRIPTEN_KEEPALIVE void adStepOne()
    {
        tWasmLoop::StepOne();
    }
    EMSCRIPTEN_KEEPALIVE void adSetPauseAt( double recSec )
    {
        tWasmLoop::SetPauseAt( recSec );
    }

}

#else

// Native: run the loop inline (blocking), exactly like the classic
// "while (cond) { body }" construction the loops were written with.
namespace tWasmLoop
{

void Start( Frame frameFn, Exit exitFn, void * arg )
{
    while ( frameFn( arg ) )
    {
    }
    if ( exitFn )
        exitFn( arg );
}

void Stop()
{
}

bool DropArg( void * )
{
    // native menu loops block, so a loop can never outlive its menu
    return false;
}

void SetContinuation( Continuation fn )
{
}

bool Active()
{
    return false;
}

int Depth()
{
    return 0;
}

void SetPaused( bool paused )
{
    // native loop runs inline/blocking; pause is not applicable
}

bool IsPaused()
{
    return false;
}

void StepOne()
{
}

void SetPauseAt( double recSec )
{
}

} // namespace tWasmLoop

#endif // __EMSCRIPTEN__
