#!/usr/bin/env python3
"""Serve the built web build (webgl/dist/) for local testing.

    ./webgl/serve.py            # http://127.0.0.1:8080
    PORT=9000 ./webgl/serve.py

The .wasm MIME type matters: browsers refuse to compile a module served as
text/plain.  The build is single threaded, so no COOP/COEP headers are needed.
"""
import functools
import http.server
import os

PORT = int(os.environ.get("PORT", "8080"))
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "dist")


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".data": "application/octet-stream",
    }

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()


if __name__ == "__main__":
    if not os.path.isfile(os.path.join(ROOT, "index.html")):
        raise SystemExit(f"no site in {ROOT} - run ./webgl/build.sh first")
    print(f"serving {ROOT} on http://127.0.0.1:{PORT}/")
    http.server.ThreadingHTTPServer(
        ("127.0.0.1", PORT), functools.partial(Handler, directory=ROOT)
    ).serve_forever()
