#!/usr/bin/env python3
"""Generate the SDL 1.2 <-> runtime keysym table used by the wasm recording bridge.

The .aarec input section stores SDL_Event.key.keysym.sym as a raw number, and
the format is defined by the SDL 1.2 values the native build writes (SDLK_DOWN
== 274, SDLK_F1 == 282, ...).  The wasm build links emscripten's port of SDL 1.2,
which numbers the non-ASCII keys as ``0x400 | SDL2 scancode`` so they stay
inside the 16-bit keysym field: SDLK_DOWN == 1105 (0x400|0x51), SDLK_UP == 1106,
SDLK_LEFT == 1104, SDLK_RIGHT == 1103.  Only the ASCII range agrees between the
two, which is why Enter (13) kept working in a replay while every arrow,
function and keypad key was silently dropped.

Confirm the runtime convention before regenerating, on the toolchain in use:

    printf '#include <SDL/SDL.h>\\nint main(){return SDLK_DOWN;}' > /tmp/k.c
    em++ -s USE_SDL=1 /tmp/k.c -o /tmp/k.js && node /tmp/k.js   # -> 1105

or print the constants from a probe inside the game build (the harness does
this in uMenu.cpp while a port bring-up is being debugged).

Generate:

  python3 webgl/gen_sdl12_keymap.py \
      /path/to/SDL-1.2.15/include/SDL/SDL_keysym.h \
      /path/to/emscripten/system/include/SDL/SDL_scancode.h

Without a flag it prints a C++ array of {file, runtime} pairs for the keys whose
numbers differ, sorted by file value.  Keys that already agree are left out,
because the runtime lookup falls through to identity.

With --defines it prints src/render/rSDL12KeySyms.h instead: every SDL 1.2
special keysym (everything from 256 up, whether or not a scancode pairing was
found for it) redefined to the SDL 1.2 number, and the closing SDLK_LAST.  The
last one matters as much as the keys themselves: the game derives its mouse
pseudo-keys (SDLK_MOUSE_*, uInput.h) and the size of keymap[]/pressed[] from
SDLK_LAST, and the shipped configuration files bind those pseudo-keys by
number (config/default.cfg has "KEYBOARD 332 PLAYER_BIND ZOOM_IN"), so a port
whose SDLK_LAST differs puts the mouse bindings on slots the desktop build
never uses and leaves the configured numbers pointing at nothing.
"""

import re
import sys

# SDL 2 renamed a few physical keys; without these the pair simply goes
# missing and that key stays broken in one direction only.
ALIASES = {
    "NUMLOCK": "NUMLOCKCLEAR",
    "SCROLLOCK": "SCROLLLOCK",
    "PRINT": "PRINTSCREEN",
    "LMETA": "LGUI",
    "RMETA": "RGUI",
    "LSUPER": "LGUI",
    "RSUPER": "RGUI",
    "KP_EQUALS": "KP_EQUAL",
    "COMPOSE": "MENU",
    "MENU": "MENU",
    "MODE": "MODE",
}

RUNTIME_BASE = 0x400  # emscripten's SDL 1.2 keysym base for scancode keys


def parse(path, prefix):
    """name -> integer value, for enum members with the given prefix."""
    out = {}
    for name, raw in re.findall(r"(%s[A-Za-z0-9_]+)\s*=\s*([^,}]+)" % prefix,
                                open(path, errors="replace").read()):
        v = literal(raw.strip())
        if v is not None:
            out[name] = v
    return out


def literal(text):
    text = text.strip()
    m = re.match(r"^0[xX][0-9a-fA-F]+$", text)
    if m:
        return int(text, 16)
    m = re.match(r"^\d+$", text)
    if m:
        return int(text)
    m = re.match(r"^'(\\.|[^'\\])'$", text)
    if m:
        ch = m.group(1)
        if ch.startswith("\\"):
            return {
                "\\r": 13, "\\n": 10, "\\t": 9, "\\0": 0, "\\\\": 92,
                "\\'": 39, '\\"': 34, "\\a": 7, "\\b": 8, "\\v": 11, "\\f": 12,
            }.get(ch)
        return ord(ch)
    return None


def norm(name):
    """SDLK_KP1 and SDL_SCANCODE_KP_1 both normalize to KP1."""
    for prefix in ("SDLK_", "SDL_SCANCODE_"):
        if name.startswith(prefix):
            name = name[len(prefix):]
            break
    return name.replace("_", "").upper()


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    defines = "--defines" in sys.argv
    if len(args) != 2:
        sys.exit(__doc__)
    sdl12 = parse(args[0], "SDLK_")
    scan = {norm(k): v for k, v in parse(args[1], "SDL_SCANCODE_").items()}
    pairs, unmapped = [], []
    for name, file_val in sorted(sdl12.items(), key=lambda kv: kv[1]):
        # SDL 1.2 numbered its special keys from 256 on; below that both APIs
        # use the same character values (RETURN = 13, ESCAPE = 27, ...), so
        # there is nothing to map -- and mapping them would be wrong, since
        # the runtime keysym for those is the character, not a scancode.
        if file_val < 256:
            continue
        short = name[len("SDLK_"):]
        sc = scan.get(norm(name))
        if sc is None:
            sc = scan.get(norm(ALIASES.get(short, "")))
        if sc is None:
            if file_val >= 256:  # the ASCII range agrees by definition
                unmapped.append(name)
            continue
        runtime = RUNTIME_BASE | sc
        if runtime != file_val:
            pairs.append((file_val, runtime, name))

    if not defines:
        print("// generated by webgl/gen_sdl12_keymap.py -- do not edit by hand")
        print("// SDL 1.2 keysym (the .aarec on-disk number) -> runtime keysym")
        print("static const struct atKeySymPair")
        print("{")
        print("    Uint32 file;")
        print("    Uint32 runtime;")
        print("}")
        print("atKeySymPairs[] =")
        print("{")
        for f, r, name in pairs:
            print(f"    {{ {f}u, 0x{r:04X}u }},  /* {name} */")
        print("};")

    if defines:
        print("#ifndef AT_SDL12_KEYSYMS_H")
        print("#define AT_SDL12_KEYSYMS_H")
        print("// Redefine the keysym constants this tree compiles against to the")
        print("// SDL 1.2 numbers.  Everything key related outside the SDL layer is")
        print("// built around those: the PLAYER_BIND configuration, the .aarec input")
        print("// format, the keymap[]/pressed[] bounds in uInput.cpp, the range tests")
        print("// in ePlayer.cpp and the key names in the HUD.  Without this a key")
        print("// press arrives under one number and its binding sits under another,")
        print("// so steering a replayed recording does nothing and the two builds")
        print("// simulate different games.  su_GetSDLInput() maps the incoming events")
        print("// to the same numbers; the include sits after <SDL.h> on purpose, so")
        print("// the enum declaration above is untouched and only later uses change.")
        print("// SDL 1.2 closed the enum one above its last special key, and the game")
        print("// numbers its mouse pseudo-keys from there (uInput.h: SDLK_MOUSE_X_PLUS")
        print("// == SDLK_LAST+1, SDLK_NEWLAST == SDLK_LAST+14); the configuration files")
        print("// bind those pseudo-keys by number, so the enum has to close at the same")
        print("// place here as it does on the desktop.")
        for file_val, name in sorted((v, k) for k, v in sdl12.items() if v >= 256):
            print(f"#define {name} {file_val}")
        # SDL 1.2's SDLKey enum ends in an unnamed sentinel one past the last
        # named special key, which is what SDLK_LAST means there.
        print(f"#define SDLK_LAST {max(v for v in sdl12.values() if v >= 256) + 1}")
        print("#endif")
    print(f"# sdl12={len(sdl12)} scancodes={len(scan)} "
          f"mapped={len(pairs)} unmapped={len(unmapped)}", file=sys.stderr)
    if unmapped:
        print("# no scancode pairing: " + " ".join(unmapped), file=sys.stderr)


if __name__ == "__main__":
    main()
