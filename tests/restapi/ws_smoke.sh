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


def recv_ws_frame(sock: socket.socket):
    try:
        hdr = sock.recv(2)
    except TimeoutError:
        return None
    except socket.timeout:
        return None
    if len(hdr) < 2:
        return None

    b0, b1 = hdr
    fin = (b0 >> 7) & 1
    opcode = b0 & 0x0F
    masked = (b1 >> 7) & 1
    plen = b1 & 0x7F

    if plen == 126:
        try:
            ext = sock.recv(2)
        except (TimeoutError, socket.timeout):
            return None
        if len(ext) < 2:
            return None
        plen = struct.unpack("!H", ext)[0]
    elif plen == 127:
        try:
            ext = sock.recv(8)
        except (TimeoutError, socket.timeout):
            return None
        if len(ext) < 8:
            return None
        plen = struct.unpack("!Q", ext)[0]

    mask = b""
    if masked:
        try:
            mask = sock.recv(4)
        except (TimeoutError, socket.timeout):
            return None
        if len(mask) < 4:
            return None

    payload = b""
    while len(payload) < plen:
        try:
            chunk = sock.recv(plen - len(payload))
        except (TimeoutError, socket.timeout):
            return None
        if not chunk:
            return None
        payload += chunk

    if masked:
        payload = bytes(payload[i] ^ mask[i % 4] for i in range(len(payload)))

    return fin, opcode, payload


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
    if b" 101 " not in rsp:
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


def read_until_texts(sock: socket.socket, deadline: float, predicates=None):
    predicates = predicates or []
    texts = []
    while time.time() < deadline:
        frame = recv_ws_frame(sock)
        if frame is None:
            continue
        _, opcode, payload = frame
        if opcode == 0x1:
            txt = payload.decode("utf-8", errors="replace")
            texts.append(txt)
            if predicates and all(pred(txts=texts) for pred in predicates):
                break
        elif opcode == 0x9:
            sock.sendall(ws_client_frame(0xA, payload))
        elif opcode == 0x8:
            break
    return texts


with socket.create_connection((HOST, PORT), timeout=TIMEOUT) as sock:
    sock.settimeout(TIMEOUT)
    ws_handshake(sock)

    part1 = b'{"type":"test","payload":'
    part2 = b'{"msg":"fragmented"}}'
    sock.sendall(ws_client_frame(0x1, part1, fin=False))
    sock.sendall(ws_client_frame(0x0, part2, fin=True))

    texts = read_until_texts(
        sock,
        time.time() + 4.0,
        predicates=[
            lambda txts: any('"type":"ack"' in t for t in txts),
            lambda txts: any('"type":"client_event"' in t for t in txts),
        ],
    )

    got_ack = any('"type":"ack"' in t for t in texts)
    got_client_event = any('"type":"client_event"' in t for t in texts)
    if not (got_ack and got_client_event):
        raise RuntimeError(f"Expected ack + client_event, got: {texts}")

    sock.sendall(ws_client_frame(0x1, b"\xff"))

    close_code = None
    close_reason = ""
    deadline = time.time() + 2.0
    while time.time() < deadline:
        frame = recv_ws_frame(sock)
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
