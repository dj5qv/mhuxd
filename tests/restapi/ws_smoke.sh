#!/usr/bin/env bash
set -euo pipefail

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-5052}"

python3 - "$HOST" "$PORT" <<'PY'
import base64
import hashlib
import os
import socket
import struct
import sys
import time

HOST = sys.argv[1]
PORT = int(sys.argv[2])
PATH = "/api/v1/ws"
TIMEOUT = 2.0


def recv_until(sock: socket.socket, marker: bytes) -> bytes:
    data = b""
    while marker not in data:
        chunk = sock.recv(4096)
        if not chunk:
            break
        data += chunk
    return data


def ws_client_frame(opcode: int, payload: bytes, fin: bool = True) -> bytes:
    first = (0x80 if fin else 0x00) | (opcode & 0x0F)
    mask_bit = 0x80
    plen = len(payload)

    header = bytearray([first])
    if plen < 126:
        header.append(mask_bit | plen)
    elif plen <= 0xFFFF:
        header.append(mask_bit | 126)
        header.extend(struct.pack("!H", plen))
    else:
        header.append(mask_bit | 127)
        header.extend(struct.pack("!Q", plen))

    mask = os.urandom(4)
    header.extend(mask)
    masked = bytes(payload[i] ^ mask[i % 4] for i in range(plen))
    return bytes(header) + masked


def parse_ws_frame(buf: bytes):
    """Return (frame_size, (fin, opcode, payload)) if buf starts with a complete frame, else None."""
    if len(buf) < 2:
        return None

    fin = (buf[0] >> 7) & 1
    opcode = buf[0] & 0x0F
    masked = (buf[1] >> 7) & 1
    plen = buf[1] & 0x7F
    pos = 2

    if plen == 126:
        if len(buf) < 4:
            return None
        plen = struct.unpack("!H", buf[2:4])[0]
        pos = 4
    elif plen == 127:
        if len(buf) < 10:
            return None
        plen = struct.unpack("!Q", buf[2:10])[0]
        pos = 10

    mask = b""
    if masked:
        mask = buf[pos:pos + 4]
        pos += 4

    if len(buf) < pos + plen:
        return None

    payload = buf[pos:pos + plen]
    if masked:
        payload = bytes(payload[i] ^ mask[i % 4] for i in range(len(payload)))

    return pos + plen, (fin, opcode, payload)


class WsReader:
    """Buffers socket data so that partial reads never split a frame."""

    def __init__(self, sock: socket.socket, buf: bytes = b""):
        self.sock = sock
        self.buf = buf

    def recv_frame(self):
        """Return (fin, opcode, payload), or None on timeout or EOF."""
        while True:
            frame = parse_ws_frame(self.buf)
            if frame is not None:
                size, result = frame
                self.buf = self.buf[size:]
                return result
            try:
                chunk = self.sock.recv(4096)
            except (TimeoutError, socket.timeout):
                return None
            if not chunk:
                return None
            self.buf += chunk


def ws_handshake(sock: socket.socket):
    key = base64.b64encode(os.urandom(16)).decode("ascii")
    req = (
        f"GET {PATH} HTTP/1.1\r\n"
        f"Host: {HOST}:{PORT}\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        f"Sec-WebSocket-Key: {key}\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n"
    ).encode("ascii")

    sock.sendall(req)
    rsp = recv_until(sock, b"\r\n\r\n")
    head, _, rest = rsp.partition(b"\r\n\r\n")
    if b" 101 " not in head:
        raise RuntimeError(f"Handshake failed:\n{rsp.decode(errors='replace')}")

    accept = None
    for line in rsp.decode(errors="replace").split("\r\n"):
        if line.lower().startswith("sec-websocket-accept:"):
            accept = line.split(":", 1)[1].strip()
            break
    if not accept:
        raise RuntimeError("Missing Sec-WebSocket-Accept")

    expected = base64.b64encode(
        hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")).digest()
    ).decode("ascii")

    if accept != expected:
        raise RuntimeError("Sec-WebSocket-Accept mismatch")

    # Frames sent right after the 101 response may arrive in the same read.
    return rest


def read_until_texts(reader: WsReader, deadline: float, predicates=None):
    predicates = predicates or []
    texts = []
    while time.time() < deadline:
        frame = reader.recv_frame()
        if frame is None:
            continue
        _, opcode, payload = frame
        if opcode == 0x1:
            txt = payload.decode("utf-8", errors="replace")
            texts.append(txt)
            if predicates and all(pred(txts=texts) for pred in predicates):
                break
        elif opcode == 0x9:
            reader.sock.sendall(ws_client_frame(0xA, payload))
        elif opcode == 0x8:
            break
    return texts


with socket.create_connection((HOST, PORT), timeout=TIMEOUT) as sock:
    sock.settimeout(TIMEOUT)
    reader = WsReader(sock, ws_handshake(sock))

    part1 = b'{"type":"test","payload":'
    part2 = b'{"msg":"fragmented"}}'
    sock.sendall(ws_client_frame(0x1, part1, fin=False))
    sock.sendall(ws_client_frame(0x0, part2, fin=True))

    texts = read_until_texts(
        reader,
        time.time() + 4.0,
        predicates=[
            lambda txts: any('"type":"ack"' in t for t in txts),
        ],
    )

    if not any('"type":"ack"' in t and '"fragmented"' in t for t in texts):
        raise RuntimeError(f"Expected ack echoing the fragmented message, got: {texts}")

    sock.sendall(ws_client_frame(0x1, b"\xff"))

    close_code = None
    close_reason = ""
    deadline = time.time() + 2.0
    while time.time() < deadline:
        frame = reader.recv_frame()
        if frame is None:
            break
        _, opcode, payload = frame
        if opcode == 0x8:
            if len(payload) >= 2:
                close_code = struct.unpack("!H", payload[:2])[0]
                close_reason = payload[2:].decode("utf-8", errors="replace")
            break
        if opcode == 0x9:
            sock.sendall(ws_client_frame(0xA, payload))

    if close_code != 1007:
        raise RuntimeError(f"Expected close code 1007, got {close_code} ({close_reason})")

print("OK: WS fragmented text + UTF-8 close(1007) checks passed")
PY
