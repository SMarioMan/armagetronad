# 48 - the live build against a from-source rebuild of the same commit

Date 2026-11-17. The machine that held the desktop capture environment was reset
that morning, so this sheet answers a different question than 43-47: **is the
published build reproducible?** The same commit (`0c0a3a56`, the deployed one)
was rebuilt from scratch in a clean container — emsdk 6.0.8 plus wasm libxml2
from `webgl/setup_toolchain.sh`, then `webgl/build.sh` — and both builds replayed
`perp2.aarec` in the same Chromium, captured at the same recording positions.

| file | what it is |
|---|---|
| `48-a-rebuild-rec18.32.png` | the from-source rebuild, frozen at recSec 18.323399 |
| `48-b-deployed-rec18.32.png` | the deployed build (served from the site over http), same recSec |
| `48-c-side-by-side-rec18.32.png` | the two joined left/right |
| `48-d-minimap-diff-x4.png` | 4x crop: rebuild, deployed, then their difference x8 — the only pixels that ever differ, and only at the later gate |
| `48-f-rebuild-rec18.31.png`, `48-g-deployed-rec18.31.png` | the first gate, recSec 18.305126, both builds |
| `48-h-rebuild-rec18.52.png`, `48-i-deployed-rec18.52.png` | the last gate, recSec 18.520989, both builds — the only pair with any differing pixels |
| `48-e-delta-x8-rec18.52.png` | whole-frame difference x8 at recSec 18.520989 (black apart from the minimap markers) |

## Census

Both builds were driven by the in-game pause-at gate (`P:` in
`webgl/harness/cdpd.py` → `adSetPauseAt` in `src/tools/tWasmLoop.cpp`), which
stops the wasm main loop on the frame that crosses a recording position. Three
runs — deployed, from-source rebuild, and from-source rebuild with the harness
stamp probe applied — froze on exactly the same frame boundaries: recSec
18.305126, 18.323399, 18.520989.

| frame pair | pixels differing (of 1 024 000) | mean abs diff per channel |
|---|---|---|
| deployed vs rebuild, 18.305126 | 0 (a handful of pixels differ by 1) | 0.0000 |
| deployed vs rebuild, 18.323399 | **0 — bit-identical** | 0.0000 |
| deployed vs rebuild, 18.520989 | 11 px, all minimap player markers | 0.0008 |
| rebuild vs rebuild+stamp probe, 18.323399 | **0 — bit-identical** | 0.0000 |
| rebuild vs rebuild+stamp probe, 18.520989 | 11 px, the same markers | 0.0008 |

The 11 px are the minimap's own player markers and they also move between two
runs of a single binary, so they are run-to-run HUD jitter, not a build
difference. The floor-grid ruler (`webgl/harness/gridprofile.py`) reads the pair
identically, band by band: lines 6.0 / 8.0 / 20.5 / 22.3 per scanline, widths
1.50 / 2.04 / 2.74 / 2.40 px, peaks 96.9 / 137.9 / 199.3 / 154.7, floor 0 / 0 /
2.0 / 15.9.

So: the renderer pipeline (every `webgl/patch_*.py` in the chain) reproduces from
source down to the bit. What is *not* reproducible between the two builds is the
simulation's arithmetic — the desktop links glibc's `libm`, the browser build
musl's — which is documented in `parity-2026-11-15/README.md` as the reason the
two worlds part company about 1.9 game time into a round.

## Reproducing

    (cd webgl/dist && python3 -m http.server 8080) &      # the local build
    (cd <dir with the downloaded site files> && python3 -m http.server 8081) &
    google-chrome --headless=new --remote-debugging-port=9223 --no-sandbox \
        --user-data-dir=$HOME/chrome about:blank &
    for port in 8080 8081; do
      SHOTDIR=$PWD BOOT=20000 STAMP=0 python3 webgl/harness/cdpd.py \
        "http://127.0.0.1:$port/index.html?args=--playback,perp2.aarec" \
        rb$port "P:16.5,s,P:17.5,s,P:18.5,s"
    done

Two measured facts about those gates. Playback of `perp2.aarec` in the browser
advanced at about 0.4 recording-seconds per wall-clock second under SwiftShader
and stopped advancing altogether at recSec ≈ 18.65 in every run — deployed and
rebuilt alike — while still rendering at 58 fps, so the gates had to sit below
that. The published captures of the same archive on the previous capture machine
reach recSec ≈ 25, so that wall is a property of this machine's run and not of
the build. The HUD clock runs four seconds behind the recording position (at
recSec 18.323 the frame reads `T:14.323`). `N:<frame>` gates are useless across
builds: how many frames get rendered per unit of game time tracks render speed,
not the world.

The rebuild needed one environment workaround: `configure` is not checked in and
that container has no autoconf/automake, so the generated files (`configure`,
`config.h.in`, every `Makefile.in`, the aux scripts) were taken from the
`armagetronad-0.2.9.3.0` release tarball and their timestamps ordered behind
their prerequisites. That is safe because `src/**/Makefile.am` is identical
between that tarball and this branch (`diff -rq --include=Makefile.am src` is
empty), so the copied rules compile the same file list. `COPYING` also has to
exist for the doc subdirectory to build. CI does not need any of this: it
installs autoconf/automake.
