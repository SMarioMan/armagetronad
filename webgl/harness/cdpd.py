#!/usr/bin/env python3
"""CDP driver for the wasm/WebGL render checks (python websocket-client).

Usage: cdpd.py URL TAG STEPS [BOOT]
STEPS: comma separated  q/e/d/u/l/r/s = key, w:N = wait ms, s = screenshot
"""
import base64
import json
import os
import sys
import time
import urllib.parse
import urllib.request

import websocket

PORT = int(os.environ.get("CDPPORT", "9223"))
SHOTDIR = os.environ.get("SHOTDIR", "/home/agent/agtk/shots")

KEYS = {
    "q": ("Escape", "Escape", 27),
    "e": ("Enter", "Enter", 13),
    "d": ("ArrowDown", "ArrowDown", 40),
    "u": ("ArrowUp", "ArrowUp", 38),
    "l": ("ArrowLeft", "ArrowLeft", 37),
    "r": ("ArrowRight", "ArrowRight", 39),
    "b": ("Backspace", "Backspace", 8),
    "t": ("Tab", "Tab", 9),
    "g": ("Backquote", "Backquote", 192),
    # camera keys: v cycles the view, m/o are the rotate camera keys
    "v": ("v", "KeyV", 86),
    "m": (",", "Comma", 188),
    "o": (".", "Period", 190),
}


def http(path, method=None):
    req = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", method=method or "GET")
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.load(r)


def close_stale_pages():
    """Close every page target left over from earlier runs.

    A page that survived a cancelled capture keeps its IndexedDB connection
    open.  The harness page wipes that database at load so the seeded config is
    authoritative, and a delete issued while another tab still holds a
    connection never completes: the IDBFS mount hangs, the run dependency is
    never released, main() never runs and the capture silently waits on a
    300x150 canvas.  Closing the old tabs first makes every run independent.
    """
    try:
        targets = http("/json/list")
    except Exception as e:
        print("stale target scan failed:", e, flush=True)
        return
    for t in targets:
        if t.get("type") != "page":
            continue
        try:
            urllib.request.urlopen(
                f"http://127.0.0.1:{PORT}/json/close/{t['id']}", timeout=10).read()
            print("closed stale page", t["id"], flush=True)
        except Exception as e:
            print("close failed", t["id"], e, flush=True)

class Page:
    def __init__(self, url):
        target = http("/json/new?" + urllib.parse.quote(url, safe=""), "PUT")
        self.id = target["id"]
        self.ws = websocket.create_connection(target["webSocketDebuggerUrl"], timeout=120)
        self._id = 0
        self.console = []
        self.cfile = None
        for d in ("Page.enable", "Runtime.enable", "Log.enable", "Network.enable"):
            self.cmd(d)
        # The page reserves 56 px of chrome below the canvas, so the port
        # renders at innerHeight - 56: an 856 tall viewport lands the buffer on
        # exactly the 1280x800 the native capture uses.  Anything smaller makes
        # the game render shorter and the pinned css box stretches it, which
        # silently changes the vertical field of view and invalidates every
        # pixel comparison against the desktop build.
        w, h = (int(v) for v in os.environ.get("VIEW", "1280x856").split("x"))
        self.cmd(
            "Emulation.setDeviceMetricsOverride",
            {"width": w, "height": h, "deviceScaleFactor": 1, "mobile": False},
        )

    def _log(self, line):
        if self.cfile:
            self.cfile.write(line + "\n")
            self.cfile.flush()

    def cmd(self, method, params=None):
        self._id += 1
        mid = self._id
        self.ws.send(json.dumps({"id": mid, "method": method, "params": params or {}}))
        deadline = time.time() + 60
        while time.time() < deadline:
            msg = json.loads(self.ws.recv())
            if msg.get("method") == "Runtime.consoleAPICalled":
                txt = " ".join(a.get("value", a.get("description", "")) for a in msg["params"].get("args", []))
                self.console.append(f"{msg['params']['type']}: {txt}")
                self._log(self.console[-1])
            elif msg.get("method") == "Log.entryAdded":
                self.console.append(f"log: {msg['params']['entry'].get('text', '')}")
                self._log(self.console[-1])
            elif msg.get("method") == "Runtime.exceptionThrown":
                # An uncaught page error is how a broken post-build JS patch
                # shows up: no console line at all, the module never reaches its
                # main loop, and the canvas stays at the 300x150 default.
                exc = msg["params"]["exception"]
                txt = exc.get("description") or exc.get("value") or json.dumps(exc)[:300]
                self.console.append(f"exception: {txt}")
                self._log(self.console[-1])
            if msg.get("id") == mid:
                if "error" in msg:
                    raise RuntimeError(f"{method}: {msg['error']}")
                return msg.get("result", {})
        raise RuntimeError(f"{method}: no reply")

    def eval(self, expr):
        r = self.cmd(
            "Runtime.evaluate", {"expression": expr, "returnByValue": True, "awaitPromise": True}
        )
        return r.get("result", {}).get("value")

    def key(self, key, code, keycode, text=None):
        base = {"key": key, "code": code, "windowsVirtualKeyCode": keycode, "nativeVirtualKeyCode": keycode}
        self.cmd("Input.dispatchKeyEvent", {**base, "type": "keyDown", "text": text or ""})
        self.cmd("Input.dispatchKeyEvent", {**base, "type": "keyUp"})

    def type_char(self, ch):
        code, vk, mods = "Undefined", ord(ch), 0
        unmod = ch
        if ch == " ":
            code, vk = "Space", 32
        elif ch.isalpha():
            code, vk = "Key" + ch.upper(), ord(ch.upper())
        elif ch.isdigit():
            code, vk = "Digit" + ch, ord(ch)
        elif ch == ".":
            code, vk = "Period", 190
        elif ch == ":":
            code, vk = "Semicolon", 186
        elif ch == "/":
            code, vk = "Slash", 191
        elif ch == "-":
            code, vk = "Minus", 189
        elif ch == "_":
            code, vk, mods, unmod = "Minus", 189, 1, "-"
        base = {"key": ch, "code": code, "windowsVirtualKeyCode": vk,
                "nativeVirtualKeyCode": vk, "modifiers": mods}
        self.cmd("Input.dispatchKeyEvent", {**base, "type": "rawKeyDown", "text": ch})
        self.cmd("Input.dispatchKeyEvent",
                 {**base, "type": "char", "text": ch, "unmodifiedText": unmod})
        self.cmd("Input.dispatchKeyEvent", {**base, "type": "keyUp"})
        time.sleep(0.03)

    def type_text(self, s):
        for ch in s:
            self.type_char(ch)

    def click_canvas(self, x=640, y=360):
        for t in ("mousePressed", "mouseReleased"):
            self.cmd(
                "Input.dispatchMouseEvent",
                {"type": t, "x": x, "y": y, "button": "left", "clickCount": 1, "buttons": 1},
            )
    def press(self, ch, hold=0.0):
        code, vk = "Undefined", ord(ch)
        if ch == " ":
            code, vk = "Space", 32
        elif ch.isalpha():
            code, vk = "Key" + ch.upper(), ord(ch.upper())
        elif ch.isdigit():
            code, vk = "Digit" + ch, ord(ch)
        elif ch == ",":
            code, vk = "Comma", 188
        elif ch == ".":
            code, vk = "Period", 190
        elif ch == "/":
            code, vk = "Slash", 191
        elif ch == "-":
            code, vk = "Minus", 189
        base = {"key": ch, "code": code, "windowsVirtualKeyCode": vk, "nativeVirtualKeyCode": vk}
        self.cmd("Input.dispatchKeyEvent", {**base, "type": "keyDown"})
        if hold:
            time.sleep(hold)
        self.cmd("Input.dispatchKeyEvent", {**base, "type": "keyUp"})

    def shot(self, path, clip=None):
        params = {"format": "png", "fromSurface": True}
        if clip:
            # Capture just the canvas rectangle.  The page's status strip sits
            # under the canvas, so a viewport screenshot comes out 56 px
            # taller than the game image and every side-by-side against the
            # native 1280x800 frame lands offset by that much.  The clip is in
            # CSS px at scale 1, which is exactly the canvas backing store the
            # harness pins to 1280x800.
            params["clip"] = {"x": clip[0], "y": clip[1],
                              "width": clip[2], "height": clip[3], "scale": 1}
        r = self.cmd("Page.captureScreenshot", params)
        with open(path, "wb") as f:
            f.write(base64.b64decode(r["data"]))

    def close(self):
        try:
            self.ws.close()
        except Exception:
            pass
        try:
            http(f"/json/close/{self.id}")
        except Exception:
            pass


def red_rows(path):
    """Centres and weights of reddish text rows in a screenshot.  Armagetron
    draws the highlighted menu item in red and the others in white, so this is
    enough to locate the menu cursor without knowing the menu's layout."""
    from PIL import Image
    im = Image.open(path).convert("RGB")
    px = im.load()
    hits = []
    for yy in range(im.height):
        c = 0
        for xx in range(60, im.width - 60, 2):
            r, g, b = px[xx, yy]
            if r > 110 and r - g > 45 and r - b > 45:
                c += 1
        if c:
            hits.append((yy, c))
    if not hits:
        return []
    clusters, cur, prev = [], [], None
    for yy, c in hits:
        if prev is not None and yy - prev > 4:
            clusters.append(cur)
            cur = []
        cur.append((yy, c))
        prev = yy
    clusters.append(cur)
    return [(sum(a * b for a, b in cl) / sum(b for _, b in cl), sum(b for _, b in cl))
            for cl in clusters]


def main():
    url = sys.argv[1]
    tag = sys.argv[2]
    steps = sys.argv[3] if len(sys.argv) > 3 else ""
    boot = int(os.environ.get("BOOT", "18000"))
    close_stale_pages()
    time.sleep(1.0)
    p = Page(url)
    p.cfile = open(f"{SHOTDIR}/{tag}-console.txt", "w", buffering=1)
    time.sleep(boot / 1000.0)
    try:
        print("canvas:", p.eval("JSON.stringify([Module.canvas.width,Module.canvas.height,innerWidth,innerHeight])"), flush=True)
    except Exception as e:
        print("canvas query failed:", e, flush=True)
    try:
        p.click_canvas()
    except Exception as e:
        print("canvas click failed:", e, flush=True)
    open(f"{SHOTDIR}/{tag}-shots.txt", "w").close()
    n = 0
    for step in [s for s in steps.split(",") if s]:
        name = step.split(":")[0]
        if name in ("y", "c"):
            parts = [name, step.split(":", 1)[1]]
        else:
            parts = step.split(":")
        name = parts[0]
        ms = int(parts[1]) if len(parts) > 1 and parts[1].isdigit() else 500
        if name == "w":
            time.sleep(ms / 1000.0)
        elif name == "G":
            # G:<value> - wait until the stamped game clock reaches <value>.  The
            # browser pumps the simulation at whatever rate SwiftShader allows, so
            # wall-clock pacing would land the shots at different world states than
            # the native run: only the game time is comparable across builds.
            target = float(parts[1])
            limit = time.time() + int(os.environ.get("GATEWAIT", "600"))
            gts = []
            while time.time() < limit:
                p.eval("1")
                gts = []
                for l in p.console:
                    if " FRAME " in l and " gt " in l:
                        gts.append(float(l.split(" gt ")[1].split()[0]))
                if gts and gts[-1] >= target:
                    break
                time.sleep(0.1)
            print(f"gt wait {target}: at {gts[-1] if gts else None}", flush=True)
        elif name == "N":
            target = int(float(parts[1]))
            # N:<frame> - wait until the stamped rendered-frame counter reaches
            # <frame>, then pause the wasm main loop.  Only useful to compare two
            # runs of the SAME build: playback skips rendering whenever it is
            # behind schedule (rSysdep.cpp SwapGL), so the number of rendered
            # frames per unit of game time tracks the render speed, not the world
            # state.  Measured: perp2 playback on wasm at gt 2.27 sits 0.01 world
            # units from the native at gt 2.2876 although the frame counts differ
            # by a factor of three - the game clock (G:) is the cross-build key.
            limit = time.time() + int(os.environ.get("GATEWAIT", "600"))
            # A finished N: step leaves the wasm main loop paused for its
            # screenshot; the next gate has to release the loop again or the
            # frame counter never advances and every later shot captures the
            # same frozen frame.
            p.eval("Module._adSetPaused(0);")
            time.sleep(0.1)
            last = None
            while time.time() < limit:
                p.eval("1")
                last = None
                for l in p.console:
                    if " FRAME " in l and " gt " in l:
                        last = int(l.split(" FRAME ")[1].split()[0])
                if last is not None and last >= target:
                    break
                time.sleep(0.05)
            try:
                p.eval("Module._adSetPaused(1);")
            except Exception as e:
                print("pause failed:", e, flush=True)
            print(f"frame wait {target}: at {last}, paused", flush=True)
            time.sleep(0.25)
        elif name in ("P", "X"):
            # P:<y> / X:<x> - wait until the camera crosses that world
            # coordinate.  Game time cannot be matched across builds: the two
            # simulations drift apart within a second of play (at gt 5.68 the
            # recorded poses differed by 19 world units), so a time gate
            # compares different worlds.  A camera position can be matched,
            # and over identical floor geometry that is what makes two builds'
            # frames comparable at all.
            axis = 1 if name == "X" else 2
            target = float(parts[1])
            limit = time.time() + 900
            got = None
            while time.time() < limit:
                p.eval("1")
                last = None
                for l in p.console:
                    if " FRAME " in l and " pos " in l:
                        last = l
                if last:
                    try:
                        got = float(last.split(" pos ")[1].split()[axis - 1])
                    except (IndexError, ValueError):
                        got = None
                    if got is not None and got >= target:
                        break
                time.sleep(0.03)
            print(f"pos {'x' if axis == 1 else 'y'} wait {target}: at {got}", flush=True)
        elif name == "T":
            # T:<x>:<y>[:r] - wait until the camera comes within r (default 1
            # world unit) of a recorded waypoint.  Replaying the same recording
            # on both builds reproduces the same trajectory to well under a
            # unit, so a waypoint gate lands both cameras at the same place --
            # which is what makes the two builds' frames comparable at all.
            tx, ty = float(parts[1]), float(parts[2])
            r = float(parts[3]) if len(parts) > 3 else 1.0
            limit = time.time() + 900
            got = None
            while time.time() < limit:
                p.eval("1")
                last = None
                for l in p.console:
                    if " FRAME " in l and " pos " in l:
                        last = l
                if last:
                    try:
                        x, y = (float(v) for v in last.split(" pos ")[1].split()[:2])
                        got = ((x - tx) ** 2 + (y - ty) ** 2) ** 0.5
                    except (IndexError, ValueError):
                        got = None
                    if got is not None and got <= r:
                        break
                time.sleep(0.03)
            print(f"target {tx},{ty} r{r}: at d={got}", flush=True)
        elif name == "Q":
            # Q:<x>:<y>[:r] - land the camera on a recorded waypoint exactly.
            # A plain T: gate races at real-time frame rates: the pose is inside
            # the window for a couple of frames and the console stream lags, so
            # the newest stamp can already be past the point.  Here the game runs
            # free until it approaches, then the wasm main loop is frozen and
            # advanced one frame at a time through the same bridge the F: step
            # uses, until the stamped pose is inside r.  The following s: step
            # then captures a frozen canvas: pixels and stamp describe one frame.
            tx, ty = float(parts[1]), float(parts[2])
            r = float(parts[3]) if len(parts) > 3 else 1.0

            def lastpose():
                p.eval("1")
                last = None
                for l in p.console:
                    if " FRAME " in l and " pos " in l:
                        last = l
                if not last:
                    return None
                try:
                    return tuple(float(v) for v in last.split(" pos ")[1].split()[:2])
                except ValueError:
                    return None

            def dst(pt):
                return None if pt is None else ((pt[0] - tx) ** 2 + (pt[1] - ty) ** 2) ** 0.5

            # A finished Q: step leaves the wasm loop paused for its screenshot;
            # the next gate has to unpause it again or the pose never changes
            # and the approach loop would spin until its deadline.
            p.eval("Module._adSetPaused(0);")
            time.sleep(0.15)
            deadline = time.time() + 900
            while True:
                d = dst(lastpose())
                if d is not None and d <= r + 3.0:
                    break
                if time.time() > deadline:
                    break
                time.sleep(0.05)
            p.eval("Module._adSetPaused(1);")
            time.sleep(0.2)
            d, best = dst(lastpose()), None
            for _ in range(600):
                if d is not None and d <= r:
                    break
                if d is not None and best is not None and d > best + 0.8:
                    break  # already drove past the waypoint
                p.eval("Module._adSetPaused(0);")
                time.sleep(0.02)
                p.eval("Module._adSetPaused(1);")
                time.sleep(0.05)
                d = dst(lastpose())
                if d is not None and (best is None or d < best):
                    best = d
            print(f"waypoint {tx},{ty} r{r}: froze at d={d} best={best}", flush=True)
        elif name == "I":
            # I:<gt>[:eps] - stop the loop on the rendered frame whose stamped
            # game time is the closest one at or before <gt>.  A plain G: gate
            # leaves the game running while the screenshot is in flight, and at
            # SwiftShader frame rates that costs up to half a second of world
            # motion; a Q: waypoint gate cannot be used here because this port's
            # simulation drifts away from the desktop build's trajectory within
            # a second of play, so the desktop run's poses are never reached.
            # Game time is the only clock both builds share, so it is the gate.
            target = float(parts[1])
            eps = float(parts[2]) if len(parts) > 2 else 0.05

            def lastgt():
                p.eval("1")
                last = None
                for l in p.console:
                    if " FRAME " in l and " gt " in l:
                        last = l
                if not last:
                    return None
                try:
                    return float(last.split(" gt ")[1].split()[0])
                except ValueError:
                    return None

            p.eval("Module._adSetPaused(0);")
            deadline = time.time() + 900
            g = None
            # Free-run until the newest stamp is inside the console-lag margin of
            # the target, then take the rest one frame at a time, so the frozen
            # frame really is the one the stamp describes.
            while time.time() < deadline:
                g = lastgt()
                if g is not None and g + 0.35 + eps >= target:
                    break
                time.sleep(0.05)
            p.eval("Module._adSetPaused(1);")
            time.sleep(0.25)
            g = lastgt()
            for _ in range(600):
                if g is None or g + eps >= target:
                    break
                p.eval("Module._adSetPaused(0);")
                time.sleep(0.02)
                p.eval("Module._adSetPaused(1);")
                time.sleep(0.05)
                g = lastgt()
            print(f"gt {target} (eps {eps}): froze at {g}", flush=True)
        elif name == "M":
            # M:<index> - put the menu cursor on the index-th row of the menu that
            # is currently open.  A few Down presses enumerate the rows (the menu
            # wraps, so the walk stops when a row repeats), then the cursor is
            # moved the short way to the requested row.
            want = int(parts[1])
            probe = f"{SHOTDIR}/.menu-probe.png"

            def cursor_y():
                p.shot(probe)
                rows = red_rows(probe)
                if not rows:
                    return None
                return max(rows, key=lambda rc: rc[1])[0]

            y = cursor_y()
            if y is None:
                print("menu: no highlighted row on screen", flush=True)
                continue
            ys = [y]
            for _ in range(12):
                p.key(*KEYS["d"])
                time.sleep(0.12)
                y2 = cursor_y()
                if y2 is None or any(abs(y2 - v) < 5 for v in ys):
                    break
                ys.append(y2)
            ys.sort()
            if want >= len(ys):
                print(f"menu: found {len(ys)} rows, cannot reach index {want}", flush=True)
                continue
            ynow = cursor_y()
            cur = min(range(len(ys)), key=lambda i: abs(ys[i] - (ynow if ynow is not None else y)))
            fwd = (want - cur) % len(ys)
            back = (cur - want) % len(ys)
            key = "d" if fwd <= back else "u"
            for _ in range(min(fwd, back)):
                p.key(*KEYS[key])
                time.sleep(0.12)
            print(f"menu: {len(ys)} rows at {['%.0f' % v for v in ys]}, "
                  f"cursor {cur} -> {want} via {key}x{min(fwd, back)}", flush=True)
        elif name == "c":
            rest = ":".join(parts[1:])
            p.key("Backquote", "Backquote", 192)
            time.sleep(0.5)
            p.type_text(rest)
            time.sleep(0.4)
            p.key("Enter", "Enter", 13)
            time.sleep(1.0)
        elif name == "s":
            path = f"{SHOTDIR}/{tag}-{n:02d}.png"
            try:
                # The canvas carries a 1 px css border, so its bounding rect is
                # the border box (1282x802) while the game pixels live in the
                # client box (1280x800).  Clip to the client box: the captured
                # image is then exactly the size the native capture reports
                # instead of every comparison carrying a 1 px offset.
                raw = p.eval(
                    "JSON.stringify((function(){var r=Module.canvas.getBoundingClientRect();"
                    "return [Math.round(r.left)+Module.canvas.clientLeft,"
                    "Math.round(r.top)+Module.canvas.clientTop,"
                    "Module.canvas.clientWidth,Module.canvas.clientHeight];})())"
                )
                box = [int(v) for v in raw.strip().strip("[]").split(",")]
            except Exception:
                box = None
            t0 = time.time()
            p.shot(path, clip=box)
            t1 = time.time()
            stamp = ""
            try:
                p.eval("1")
                last = None
                for l in p.console:
                    if " FRAME " in l and " gt " in l:
                        last = l
                if last:
                    def num(field, count):
                        return " ".join(last.split(" " + field + " ")[1].split()[:count])
                    stamp = (f" n{last.split(' FRAME ')[1].split()[0]} gt {num('gt', 1)}"
                             f" pos {num('pos', 2)} h {num('h', 1)} dir {num('dir', 2)}")
            except Exception:
                pass
            rect = f"0 0 {box[2]} {box[3]}" if box else ""
            print("shot", path, "page box", box, rect, stamp, flush=True)
            with open(f"{SHOTDIR}/{tag}-shots.txt", "a") as f:
                f.write(f"{path} {(t0 + t1) / 2:.6f} {rect}{stamp}\n")
            n += 1
            time.sleep(ms / 1000.0)
        elif name == "F":
            # F:<0|1> - freeze/thaw the wasm main loop through the port's own
            # bridge.  Without this, the game keeps stepping while the CDP
            # screenshot is in flight (0.2-0.6 of game time at these frame
            # rates), so the captured pixels and the stamped pose describe
            # different frames.  Paused, the canvas holds exactly the frame the
            # last stamp describes.
            on = int(parts[1])
            try:
                print("freeze", on, "->", p.eval(
                    f"Module._adSetPaused({on}); String(Module._adIsPaused())"), flush=True)
            except Exception as e:
                print("freeze failed:", e, flush=True)
            time.sleep(0.25)
        elif name == "H":
            # H:<key>:<ms> - hold the way real hardware does: one keyDown, then
            # autoRepeat keyDowns while the key is held, then keyUp.  A single
            # bare keyDown does not drive the game's steering.
            k = KEYS[parts[1]]
            dur = float(parts[2]) / 1000.0
            base = {"key": k[0], "code": k[1], "windowsVirtualKeyCode": k[2], "nativeVirtualKeyCode": k[2]}
            p.cmd("Input.dispatchKeyEvent", dict(base, type="rawKeyDown"))
            end = time.time() + dur
            while time.time() < end:
                p.cmd("Input.dispatchKeyEvent", dict(base, type="rawKeyDown", autoRepeat=True))
                time.sleep(0.03)
            p.cmd("Input.dispatchKeyEvent", dict(base, type="keyUp"))
            time.sleep(0.2)
        elif name == "K":
            # K:<key>:<ms> - repeated complete presses every 30 ms.  This step
            # used to be spelled T:, which the waypoint branch above shadowed, so
            # it had been dead code.
            k = KEYS[parts[1]]
            end = time.time() + float(parts[2]) / 1000.0
            while time.time() < end:
                p.key(*k)
                time.sleep(0.03)
        elif name == "R":
            # R:<w>x<h> - resize the browser viewport.  With the game in
            # desktop screen mode the page debounces the event and the wasm
            # re-inits the display (sr_WasmWindowResized), recreating the GL
            # context, so this reproduces what a player does when they
            # maximize or drag the window mid-game.
            w, h = (int(v) for v in parts[1].split("x"))
            p.cmd("Emulation.setDeviceMetricsOverride", {"width": w, "height": h, "deviceScaleFactor": 1, "mobile": False})
            time.sleep(1.5)
        elif name.startswith("+"):
            k = KEYS[name[1:]]
            p.cmd("Input.dispatchKeyEvent", {"key": k[0], "code": k[1], "windowsVirtualKeyCode": k[2], "nativeVirtualKeyCode": k[2], "type": "keyDown"})
            time.sleep(ms / 1000.0)
        elif name.startswith("-"):
            k = KEYS[name[1:]]
            p.cmd("Input.dispatchKeyEvent", {"key": k[0], "code": k[1], "windowsVirtualKeyCode": k[2], "nativeVirtualKeyCode": k[2], "type": "keyUp"})
            time.sleep(ms / 1000.0)
        elif name == "y":
            p.key("Backquote", "Backquote", 192)
            time.sleep(0.6)
            p.type_text(parts[1])
            time.sleep(ms / 1000.0)
        elif name == "n":
            p.key("Enter", "Enter", 13)
            time.sleep(1.0)
        elif name == "x":
            print("eval:", p.eval("1+1"), flush=True)
        elif name == "J":
            src = open(parts[1]).read()
            print("js:", p.eval(src), flush=True)
            time.sleep(ms / 1000.0)
        elif name == "p":
            p.press(parts[1])
            time.sleep(ms / 1000.0)
        elif name == "h":
            hold = int(parts[2]) if len(parts) > 2 and parts[2].isdigit() else 500
            p.press(parts[1], hold / 1000.0)
            time.sleep(0.1)
        elif name in KEYS:
            p.key(*KEYS[name])
            time.sleep(ms / 1000.0)
        else:
            raise SystemExit("bad step " + step)
    with open(f"{SHOTDIR}/{tag}-console.txt", "w") as f:
        f.write("\n".join(p.console))
    errs = [l for l in p.console if any(k in l.lower() for k in ("exception", "abort", "error"))]
    print(f"done {tag}: {n} shots, {len(p.console)} console lines, {len(errs)} errors")
    for l in errs[:5]:
        print("  ", l[:160])
    p.close()


if __name__ == "__main__":
    main()
