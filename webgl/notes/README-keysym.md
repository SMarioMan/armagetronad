# Keys on the port: SDL 1.2 numbers end to end

Two bugs had the same root.  The HUD printed `Press <> or <o> to turn right.`
where the desktop prints `Press <right> or <o>`, and arrow keys recorded in a
`.aarec` did nothing when the replay reached the port.

## What the probe showed

A tap inside the HUD key-name lookup, on both builds (working tree only):

    port    PF KEY sym 274 name []
    desktop PF KEY sym 273 name [up]   274 [down]   275 [right]   276 [left]

The port's SDL layer delivers SDL 2 events with SDL 2 keysyms - `SDLK_DOWN`
arrives as `0x40000051` (768-family event type, `0x40000051` symbol) - while
everything outside the SDL layer is built around SDL 1.2 numbers: the
`PLAYER_BIND` configuration, the `.aarec` input section, the `keymap[]` and
`pressed[]` bounds in `uInput.cpp`, the range tests in `ePlayer.cpp` and the key
names in the HUD.  A key press therefore arrived under one number while its
binding sat under another.  Letters, `RETURN`, `ESCAPE`, `DELETE` and the digits
have the same number in both revisions, which is why Enter-driven menu
navigation worked and every arrow key silently did nothing.

Emscripten's `SDL_GetKeyName()` also has an entry for `a-z` and `0-9` only, so
nothing at the other end of the translation had a name either.

## The fix

- `webgl/gen_sdl12_keymap.py` parses SDL 1.2's `SDL_keysym.h` and the exact SDL 2
  `SDL_keycode.h`/`SDL_scancode.h` this build compiles against and emits two
  tables: 56 keysyms whose numbers differ (177 identical, 110 SDL 2 only).
- `src/render/rSDL12KeySyms.h` redefines the keysym constants to the SDL 1.2
  numbers after `<SDL.h>`, so every comparison in the tree is on SDL 1.2 terms.
- `su_GetSDLInput()` maps incoming events down to those numbers at the SDL
  boundary (`{274u, 0x40000051u} /* SDLK_DOWN */` and the other 55).
- `src/ui/uSDL12KeyNames.h` carries SDL 1.2's own display-name table, generated
  the same way, and `ui/uInput.cpp` resolves a label through it.

Commit `b1ded912` carried the keysym bridge; `fcfda940`, described below, carried
the mouse-key slots.  The page now serves the build CI made from `fcfda940`:
2 207 041 bytes, last-modified 14 Nov 05:56:41 GMT, later than the 04:51:11 GMT
build that carried `b1ded912` and nothing after it.  The markers still pin the
lineage: the opt-in line-clip census (first in `96a3a2c6`) is in the shipped
glue, and the capture stamp -- `PF FRAME`, gone in `6d0a304b` -- occurs 0 times.
`index.html` is served with `cache-control: max-age=600`, so a player sees the
new build only after a hard reload.


## Verified on the port itself

- Menu rows are enumerated by pressing **Down** twelve times and reading the
  highlighted row off the canvas: rows at y 194, 238, 281, 325, 368, 414 and the
  cursor moved 0 to 2 - the cursor sits on `Color: Green` in
  `keysym-menu-cursor.png`.  A second run parked it 0 to 5 with **Up**, the
  wizard's `Accept`.  Before the fix the recorded Down never moved the cursor and
  the enumeration found one row instead of six.
- A live single-player round then renders the help line with the name in it -
  `keysym-hud-label.png` reads `Press <right> or <o> to turn right.` with the
  yellow cycle, its lit headlight wedge and the textured rim wall.
- Replayed input is pose-identical: `myrec2.aarec` reaches game time 2.5510 on
  frame 726 on both builds (port 726, pose error 0.0001 world units) and 3.0097
  on frame 781 (error 0.0009).  `perp2.aarec` with 10 CPUs holds round 1 to a
  mean pose error of 0.318 units against a pre-fix 75 units and a flat camera
  height; the same 10 CPUs come from the recorded file, so that gap was this
  keysym mismatch and nothing else.  See `README-replay.md`.

## Repeated against the published page

The same driver pointed at `https://smarioman.github.io/armagetronad/` - fetching
what the page serves, not a build made here - moves the wizard cursor with **Up**
(six rows detected at y 194, 238, 281, 324, 369, 413, cursor 0 to 5, then Enter)
and the round opens on the welcome screen above the arena floor grid, text single
and crisp: `live-page-0.png`, `live-page-1.png`, `live-page-2.png`.  The HUD help
line and the lit cycle body are in the local-build shots above - same source, and
the marker chain above shows the published wasm is that source.

## The mouse slots were still wrong

The keysym bridge made the keyboard work; the mouse keys stayed dead, and the
reason is one number the bridge never touched.

Armagetron has no key codes for the mouse.  `uInput.cpp` synthesizes its own
pseudo-keys from motion and button events (`uInput.h:214`:
`SDLK_MOUSE_X_PLUS == SDLK_LAST+1` up to `SDLK_NEWLAST == SDLK_LAST+14`), and
`config/default.cfg` binds them **by number** -- line 35 is

    KEYBOARD	324	PLAYER_BIND	LOOK_RIGHT	1

Those numbers are only right if the keysym enum closes where SDL 1.2 closes it.
Dumping the effective keymap on both builds (working-tree probe in
`uInput.cpp::atKeyMapDump`, called from `gArmagetron.cpp` right after
`st_LoadConfig()`), before the fix:

    desktop  SDLK_LAST 323  SDLK_NEWLAST 337  keymap 2696 B  pressed 337
    port     SDLK_LAST 1536 SDLK_NEWLAST 1550 keymap 6200 B  pressed 1550

and the five mouse rows carried no key name on the port:

    desktop  324 "Mouse right"      LOOK_RIGHT
    port     324 ""                 LOOK_RIGHT

The port generated `SDLK_MOUSE_X_PLUS == 1537` from browser mouse movement
while the binding sat on 324, so mouse look, banking and zoom did nothing and
the input menu printed an empty key for every one of those rows.

The fix is in the generated header: `gen_sdl12_keymap.py --defines` now emits
every SDL 1.2 special keysym -- `SDLK_BREAK` 318 and `SDLK_EURO` 321 have no
SDL 2 scancode, so the old table simply omitted them -- and closes the enum
with `#define SDLK_LAST 323`.  Because that shrinks `keymap[]`/`pressed[]` from
1550 entries to 337, two paths that were only safe because the table was
oversized got bounded: the event translation returns `SDLK_UNKNOWN` for a key
the browser reports and SDL 1.2 never had (F16..F24, the internet and media
keys), and `tConfItem_key::ReadVal` now ignores a key number outside the table
instead of writing past it -- port builds of 13 Nov and earlier numbered the
pseudo-keys from 1536 and saved those numbers into the user configuration.

Measured after the fix, same probe, same command line:

    port     SDLK_LAST 323  SDLK_NEWLAST 337  keymap 1348 B  pressed 337
    324 "Mouse right" LOOK_RIGHT   325 "Mouse left"  LOOK_LEFT
    326 "Mouse up"    BANK_UP      327 "Mouse down"  BANK_DOWN
    332 "Mousebutton 3" ZOOM_IN

`keymap-desktop.txt`, `keymap-port.txt` and `keymap-port-before.txt` in this
directory are the three dumps.  Desktop and port now list the same **79**
bindings, key number to action, with no entry on one side only.  74 of those
rows print a different *label* string, all of them on the desktop side: the
dump runs during startup, and the desktop's `SDL_GetKeyName` resolves nothing
until the display is up -- the menu shots below show the same desktop build
printing `left, u`, `right, o`, `down, i`, `enter, return`, `home, b`,
`f1, t`, so those rows agree with the port once the names exist.

Menu pair, both at 1280x800 with the same camera and font:
`keymap-menu-desktop.png` / `keymap-menu-port.png` (title, four shared rows,
same positions) and `keymap-playersetup-desktop.png` /
`keymap-playersetup-port.png`.  The port's main menu has no `Exit Game` row --
deliberate, `gGame.cpp:3030` skips the exit item in the wasm build because
there is no process to exit into.

`keymap-input-desktop.png` / `keymap-input-port.png` is the payoff: the
`Input for Player 1` page on both builds, same rows in the same order with the
same key strings -- `Turn left  left, u`, `Turn right  right, o`,
`Brake down  down, i`, `Toggle Brake Unbound` (bound to nothing on both),
`Chat  enter, return`, `Toggle Spectator  home, b`, `Instant Chat 1  f1, t`,
then `f2`, `f3`, `f4`.  The only text that differs is the player name: the
port's saved profile is `web_user`, the desktop run kept the default
`Player 1`.  On the pre-fix dump the mouse-bound rows further down that list
carried no key string at all, which is the same lookup these rows use.

Two differences in those menu shots are *not* keysym business and belong to the
floor-grid and cycle-lighting threads: the desktop's floor grid behind the menu
keeps wider, brighter lines further out while the port's thins to a single
pixel and fades, and the desktop's menu background carries a light bloom at the
centre that the port does not render at all.
