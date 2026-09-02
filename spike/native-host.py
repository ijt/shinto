#!/usr/bin/env python3
"""Stdio native-messaging host plus a unix socket the `shinto` CLI pokes."""

from __future__ import annotations

import json
import os
import socket
import struct
import sys
import threading

SOCK = os.path.join(os.environ.get("XDG_RUNTIME_DIR", "/tmp"), "shinto.sock")


def send(msg: dict) -> None:
    data = json.dumps(msg).encode()
    sys.stdout.buffer.write(struct.pack("<I", len(data)))
    sys.stdout.buffer.write(data)
    sys.stdout.buffer.flush()


def sock_server() -> None:
    try:
        os.unlink(SOCK)
    except FileNotFoundError:
        pass
    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(SOCK)
    os.chmod(SOCK, 0o600)
    server.listen(8)
    while True:
        conn, _ = server.accept()
        with conn:
            buf = b""
            while True:
                chunk = conn.recv(4096)
                if not chunk:
                    break
                buf += chunk
        if not buf:
            continue
        try:
            msg = json.loads(buf.decode())
        except json.JSONDecodeError:
            continue
        if isinstance(msg, dict):
            send(msg)


def main() -> None:
    try:
        with open("/tmp/shinto-host.log", "a") as log:
            log.write("start\n")
    except OSError:
        pass
    threading.Thread(target=sock_server, daemon=True).start()
    stdin = sys.stdin.buffer
    while True:
        header = stdin.read(4)
        if not header:
            break
        if len(header) < 4:
            break
        (n,) = struct.unpack("<I", header)
        stdin.read(n)


if __name__ == "__main__":
    main()
