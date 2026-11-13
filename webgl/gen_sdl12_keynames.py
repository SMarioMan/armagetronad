#!/usr/bin/env python3
"""Generate the SDL 1.2 key-name table the wasm build needs for key labels.

The game labels a bound key with SDL_GetKeyName() (ui/uInput.cpp keyname()),
which drives the "Press <left> or <o> to turn left" HUD line and the keyboard
configuration menu.  Desktop SDL 1.2 fills its name table inside
SDL_EnableKeyMap(), reached from the video/keyboard init, and returns the
string "unknown key" for anything the table does not hold.  Emscripten's SDL
1.2 layer implements SDL_GetKeyName out of library_sdl.js with a name for a-z
and 0-9 only: every other key -- arrows, return, space, function keys, every
modifier -- comes back as the empty string, so the port printed
"Press <> or <o> to turn right." where the desktop prints the key name.
Translating the keysym through the SDL 1.2 <-> runtime map cannot help,
because there is no name at the other end of that map either.

So the port carries the desktop table itself.  Regenerate it from the SDL 1.2
source the desktop library is built from:

  python3 webgl/gen_sdl12_keynames.py \
      /path/to/SDL-1.2.15/include/SDL/SDL_keysym.h \
      /path/to/SDL-1.2.15/src/events/SDL_keyboard.c

It prints the C++ rows (sorted by keysym, one per key SDL 1.2 has a name for)
that make up src/ui/uSDL12KeyNames.h, the table the wasm build labels keys
with.  Names are taken verbatim from the keynames[] assignments in
SDL_keyboard.c, so a label the player sees on the desktop is the label the
player sees in the browser.
"""
import re
import sys


def c_value(text):
    """Numeric value of an enum initializer: decimal, hex or a char literal."""
    if text.startswith("'"):
        return ord(text[1:-1].encode().decode("unicode_escape"))
    return int(text, 0)


def parse_keysyms(path):
    """SDLK_* -> numeric value, from the SDL 1.2 keysym header.

    The header spells the ASCII keys SDLK_a = 'a' and names the special keys
    SDLK_DOWN = 274, so both spellings have to be understood; SDL 1.2 also
    writes the single-letter keys in lower case (SDLK_a), which an upper-case
    only pattern silently drops.
    """
    out = {}
    pattern = re.compile(
        r"^\s*(SDLK_[A-Za-z0-9_]+)\s*=\s*"
        r"('(?:[^'\\]|\\.)*'|0[xX][0-9a-fA-F]+|\d+)\s*,?\s*(?:/\*.*?\*/)?\s*$")
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            match = pattern.match(line)
            if match:
                out[match.group(1)] = c_value(match.group(2))
    return out


def parse_names(path, keysyms):
    """keysym value -> display name, from the keynames[] assignments."""
    pattern = re.compile(
        r'^\s*keynames\[(SDLK_[A-Za-z0-9_]+)\]\s*=\s*"((?:[^"\\]|\\.)*)"\s*;')
    out = {}
    with open(path, encoding="utf-8", errors="replace") as handle:
        for line in handle:
            match = pattern.match(line)
            if not match:
                continue
            sym, name = match.group(1), match.group(2)
            if sym not in keysyms:
                raise SystemExit("unknown keysym in name table: " + sym)
            out[keysyms[sym]] = name.encode().decode("unicode_escape")
    return out


def c_escape(text):
    """Make the name safe inside a C++ string literal."""
    return text.replace("\\", "\\\\").replace('"', '\\"')


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: gen_sdl12_keynames.py SDL_keysym.h SDL_keyboard.c")
    keysyms = parse_keysyms(sys.argv[1])
    names = parse_names(sys.argv[2], keysyms)
    if not names:
        raise SystemExit("no keynames[] assignments found in " + sys.argv[2])
    for sym in sorted(names):
        print("    { %du, \"%s\" }," % (sym, c_escape(names[sym])))
    print("# %d named keysyms" % len(names), file=sys.stderr)


if __name__ == "__main__":
    main()
