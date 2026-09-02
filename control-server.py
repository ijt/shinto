#!/usr/bin/env python3
"""Serve the new-tab page over loopback so --app windows stay in-scope."""

from __future__ import annotations

import mimetypes
import os
import pathlib
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlparse

ROOT = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
PORT = int(os.environ.get("SHINTO_PORT", "18764"))
PORT_FILE = os.path.join(os.environ.get("XDG_RUNTIME_DIR", "/tmp"), "shinto.port")


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args) -> None:
        return

    def do_GET(self) -> None:
        path = unquote(urlparse(self.path).path)
        if path in ("", "/"):
            path = "/newtab.html"
        rel = path.lstrip("/")
        target = (ROOT / rel).resolve()
        if ROOT not in target.parents and target != ROOT:
            self.send_error(403)
            return
        if not target.is_file():
            self.send_error(404)
            return
        data = target.read_bytes()
        ctype = mimetypes.guess_type(target.name)[0] or "application/octet-stream"
        self.send_response(200)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)


def main() -> None:
    server = ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    with open(PORT_FILE, "w") as fh:
        fh.write(str(PORT))
    server.serve_forever()


if __name__ == "__main__":
    main()
