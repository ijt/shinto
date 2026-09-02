#!/usr/bin/env python3
"""Serve the new-tab page over loopback so --app windows stay in-scope."""

from __future__ import annotations

import json
import mimetypes
import os
import pathlib
import subprocess
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import unquote, urlparse

ROOT = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
SHINTO_BIN = sys.argv[2] if len(sys.argv) > 2 else "shinto"
PORT = int(os.environ.get("SHINTO_PORT", "18764"))
PORT_FILE = os.path.join(os.environ.get("XDG_RUNTIME_DIR", "/tmp"), "shinto.port")


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *_args) -> None:
        return

    def _from_extension(self) -> bool:
        origin = self.headers.get("Origin", "")
        if origin.startswith("chrome-extension://"):
            return True
        # Extension service-worker fetch to loopback sometimes omits Origin.
        return origin == "" and self.client_address[0] in ("127.0.0.1", "::1")

    def do_POST(self) -> None:
        path = unquote(urlparse(self.path).path)
        if path != "/open" or not self._from_extension():
            self.send_error(403)
            return
        length = int(self.headers.get("Content-Length", "0") or 0)
        raw = self.rfile.read(min(length, 8192)) if length else b""
        url = ""
        if raw:
            try:
                body = json.loads(raw.decode())
            except (json.JSONDecodeError, UnicodeDecodeError):
                body = {}
            if isinstance(body, dict):
                candidate = body.get("url")
                if isinstance(candidate, str) and candidate.startswith(("http://", "https://")):
                    url = candidate[:2048]
        cmd = [SHINTO_BIN]
        if url:
            cmd.extend(["--edit", url])
        subprocess.Popen(
            cmd,
            start_new_session=True,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            close_fds=True,
        )
        self.send_response(204)
        self.send_header("Content-Length", "0")
        self.end_headers()

    def do_GET(self) -> None:
        path = unquote(urlparse(self.path).path)
        if path == "/open":
            self.send_error(405)
            return
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
