# The void beside a close wall (arena rim face) — fixed and deployed

Commit `b35e83db` "Draw the arena rim face through client arrays on the port",
pushed 22:15 UTC. The live build is already running it:
`armagetronad_main.wasm` 2 207 126 bytes, `last-modified: Fri, 13 Nov 2026
22:18:55 GMT`, embedded version `0.2.9.3_alpha_z3245`. The page HTML is served
with `cache-control: max-age=600`, so a hard reload (Ctrl-Shift-R) is needed to
pick up the new wasm.

## What was wrong

Beside a close arena wall the WebGL port showed the black void behind the arena,
while the desktop build showed the grey-blue rim texture (`rim_wall.png`) that
continues the wall body down to the floor. That face — `gWallRim_helper`, the
wall body's only textured face — was submitted on both builds and reached the
port's WebGL layer, but nothing was rasterized there.

## Measurement

Pair: your own `myrec.aarec`, played back on both builds, shot at game time
8.06/8.08 with the camera 3.2 world units off the west rim, looking along it
(port pose 3.2089 59.1453, height 16.7466, heading (0,-1); desktop 3.2161
58.4567, height 16.7467). "Right half" is columns 640-1280, rows 40-640 — the
right half of the 3D area with the HUD excluded. "Dark" counts pixels whose mean
of the three channels is under 20. "Difference" is the mean absolute difference
between two frames averaged over the three channels; "px > 12" is the share of
pixels whose averaged difference exceeds 12.

| | desktop | port before | port after |
|---|---|---|---|
| right half mean RGB | 27, 32, 46 | 2, 2, 2 | 27, 31, 46 |
| right half dark pixels | 18.1 % | 97.6 % | 18.4 % |
| right-half difference vs desktop | – | 33.8 | 7.9 |
| whole-frame difference vs desktop | – | 19.1 | 6.4 |
| whole-frame px differing > 12 | – | 47.3 % | 14.1 % |

`rimface.png` shows the three states stacked with crops of the wall/rim region
and the signed difference maps; the numbers above are printed inside the sheet.

## What was ruled out first

A GL-state probe compiled into both builds and fired at that frame showed the
submitted data was never at fault: quad endpoints and texture coordinates, the
10 000-unit wall height and how the sky clamp leaves it untouched, fog off,
alpha test at reference 0, blending on, depth test on, culling off, and the
clip-space corner signs of all four corners (+62, −4955, −5022, −5) were
identical on the two builds, and `textures/rim_wall.png` carries no alpha
channel that could have been discarded. A WebGL-side draw census then found the
face arriving as `drawElements(TRIANGLES, 6)` with those coordinates and drawing
nothing, while the same two triangles submitted through client arrays rasterize
correctly. So the failure is inside emscripten's legacy GL emulation
`Begin/Vertex` path, not in the game's geometry.

## The change

Under `__EMSCRIPTEN__` only, `gWallRim_helper` now emits the two triangles
`GL_QUADS` would have produced, with explicit per-vertex position, texture
coordinate and inherited intensity, through `atglDrawArrays` — the route the
wall top edge, the turning-cap strip and the bulk wall quads already use. The
desktop path is untouched.
