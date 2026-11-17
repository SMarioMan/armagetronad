# 42 - port video settings trimmed to what a browser can act on

All sheets are 1280x800, same window.  The port columns were taken with
`webgl/harness/autoexec_menu.cfg` (language pinned, HUD and computer players
off); the desktop column used its own harness profile in `nativerun/user`.  No
round is started in any of these shots, so only the menu layer is on screen.

| file | build | menu |
|---|---|---|
| `42-native-menu02.png` | desktop, final binary | Display Settings (the four submenus) |
| `42-native-menu03.png` | desktop, final binary | Screen Mode |
| `42-native-menu04.png` | desktop, final binary | Detail Settings |
| `42-native-menu05.png` | desktop, final binary | Performance Tweaks |
| `42-native-menu06.png` | desktop, final binary | Preferences |
| `42-port-before-*.png` | port at HEAD, before the change | systemsetup, displaymenu, screenmode, detail, tweaks, preferences |
| `42-port-after-*.png` | port with the change | same six menus |

Compare `42-port-before-screenmode.png` with `42-port-after-screenmode.png`:
the seven entries Window Size, Screen Resolution, Fullscreen, Color Depth, Z
Buffer Depth, Keep Window Active and VSync are gone; Grab Mouse, FPS Limit,
Check Errors and Apply Changes remain.  The port's saved `user.cfg` had
`Fullscreen On` and `Dithering On` in it - both inert here, and now neither
offered nor variable.

`42-port-before-detail.png` vs `42-port-after-detail.png`: the four texture
filter groups, TrueColor Textures and Dithering are gone; Alpha Blending, Floor
Detail, Simple Trails and Floor Mirror remain.  `42-port-before-tweaks.png` vs
`42-port-after-tweaks.png`: Display Lists is gone.  Display Settings and
Preferences are unchanged.

The native column is the control: with the change in place the desktop build
still lists every entry, because each removal is a `#ifndef __EMSCRIPTEN__`
guard around the menu item.  A text-block count over each shot (rows with more
than 200 near-white pixels inside x 120..1160, y 80..700, consecutive rows
merged) gives the fingerprint:

| | Display Settings | Screen Mode | Detail | Tweaks | Preferences |
|---|---|---|---|---|---|
| desktop, before | 2 | 9 | 14 | 1 | 4 |
| desktop, after | 2 | 9 | 14 | 1 | 4 |
| port, before | 2 | 9 | 14 | 1 | 4 |
| port, after | 2 | 2 | 5 | 1 | 4 |

Desktop identical before/after; the port moves only in Screen Mode and Detail.
Tweaks is too short for that metric to separate 3 entries from 2, which is why
the pair of sheets is there.

Why each removed entry is inert:

| entry | consumer | why nothing changes |
|---|---|---|
| Window Size, Screen Resolution | `lowlevel_sr_InitDisplay` | rewrites both `.res` fields to "desktop" before use (rScreen.cpp:496-499) |
| Fullscreen | same block (rScreen.cpp:501) | the page owns the real fullscreen state (its button, the canvas double-click) |
| Color Depth, Z Buffer Depth | `SDL_GL_SetAttribute(RED/GREEN/BLUE/DEPTH_SIZE)`, rScreen.cpp:456-461 | the page created the WebGL context first, and a canvas keeps the attributes of the context created first (webgl/site/index.html) |
| VSync | `SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, ...)` | both calls are inside `#ifndef __EMSCRIPTEN__` (rScreen.cpp:468,474); the browser paces swaps |
| Dither | `glEnable/glDisable(GL_DITHER)`, rScreen.cpp:1169-1172 | WebGL has no dither state; emscripten's `libglemu.js` records cap 0xBD0 and never reads it |
| Smooth Shading | `glShadeModel` | a stub in the emulation ("TODO: glShadeModel"); vertex colours always interpolate |
| TrueColor Textures | texture upload | WebGL has no paletted or colour-index upload; the flag was already forced on |
| Texture filters x4 | `rTextureGroups::TextureMode[]` | works, but only trilinear mip-mapped world / bilinear font measures against the desktop; pinned |
| Display Lists | `rDisplayList.cpp` | compiles no list at all under `__EMSCRIPTEN__` |
| Keep Window Active | `sr_Activate()` | forces the app active on this platform, so the selected branch is never taken |

Kept because they demonstrably do something: Apply Changes (`sr_ReinitDisplay`),
Check Errors (`glGetError` is implemented in the emulation), Grab Mouse, FPS
Limit (it is what paces the port - the harness pins it to 60 for replay parity),
Alpha Blending, Floor Detail, Simple Trails, Floor Mirror, Infinity, Swap Mode,
and the whole Preferences menu.
