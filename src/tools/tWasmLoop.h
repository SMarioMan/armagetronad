#ifndef T_WASM_LOOP_H
#define T_WASM_LOOP_H

/*
 * tWasmLoop - drives the game's blocking frame loops from the JS event loop.
 *
 * In the browser build every loop iteration is a fresh top-level wasm call,
 * scheduled by the JS runtime (requestAnimationFrame, via
 * emscripten_set_main_loop).  This replaces asyncify/emscripten_sleep: with
 * asyncify each sleep re-executes the *entire* wasm stack from main(), which
 * re-runs game initialization, desyncs the input state and eventually traps -
 * fundamentally unusable for long-lived game loops.
 *
 * Nested loops are supported: frame functions are pushed onto a stack and
 * only the innermost one runs.  When it terminates (by returning false) its
 * exit action runs, then the outer loop resumes on the next animation frame.
 * Start() takes an optional exit action: exitFn(arg) is called exactly once
 * when the loop terminates.  This is how loops release their state (the menu
 * objects are heap-allocated in the browser build and freed by their exit
 * action).
 *
 * Continuations: the browser build cannot run the code that "follows" a
 * loop - emscripten_set_main_loop(fn, 0, 1) simulates an infinite loop and
 * unwinds the whole C++ call stack (the JS runtime throws 'unwind' through
 * it), so in the browser build main() hands over to the event loop at the
 * first loop and never comes back.  The startup flow (language menu ->
 * first setup -> first game -> main menu) is therefore re-run as a chain
 * of steps (see gArmagetron.cpp): each step ends by starting a loop and
 * registering the next step with SetContinuation(), which runs once when
 * the outermost active loop terminates.
 */

namespace tWasmLoop
{
    typedef bool (*Frame)( void *arg );
    typedef void (*Exit)( void *arg );
    // runs once when the outermost active loop terminates (after that
    // loop's exit action)
    typedef void (*Continuation)( void );
    // Run frameFn on every animation frame (interval 0 = as fast as the
    // display allows).  frameFn returns true to keep the loop running,
    // false to stop it; when it stops, exitFn(arg) (if given) runs once.
    void Start( Frame frameFn, Exit exitFn = 0, void * arg = 0 );
    // Stop the innermost loop (running its exit action).  The frame function
    // returning false is the normal way out; this is for aborting a loop
    // externally.
    void Stop();
    // Register the continuation that runs when the outermost loop ends
    // (after that loop's exit action).  Pass 0 to clear it.  (no-op in
    // the native build)
    void SetContinuation( Continuation fn );
    // True if at least one loop is registered.
    bool Active();
    // Number of nested loops.
    int  Depth();
    // (wasm build only) Pause the pump: no frames run until resumed.
    // StepOne() runs exactly one frame while paused (pauses first if
    // running).  Used by the playback harness / debugger; no-ops in the
    // native build (the native loop runs inline and blocking).
    void SetPaused( bool paused );
    bool IsPaused();
    // (wasm build only) Auto-pause when a playing back recording reaches
    // the given recording time (seconds).  Pass < 0 to disable.  No-op in
    // the native build.
    void SetPauseAt( double recSec );
    void StepOne();
}

#endif // T_WASM_LOOP_H
