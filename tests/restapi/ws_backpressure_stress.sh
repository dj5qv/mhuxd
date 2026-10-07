#!/usr/bin/env bash
set -euo pipefail

# A slow websocket client sends large messages without reading the acks,
# which echo each message back. Its outbound queue on the server must hit
# the hard limit and the server must drop it, while a fast client on the
# same server keeps getting its acks.

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-5052}"
MSG_SIZE="${MSG_SIZE:-8192}"
MSG_COUNT="${MSG_COUNT:-400}"
SLOW_RCVBUF="${SLOW_RCVBUF:-4096}"
DRAIN_DEADLINE_SEC="${DRAIN_DEADLINE_SEC:-5}"

python3 - "$HOST" "$PORT" "$MSG_SIZE" "$MSG_COUNT" "$SLOW_RCVBUF" "$DRAIN_DEADLINE_SEC" <<'PY'
import base64
import hashlib
import json
import os
import socket
import struct
import sys
import time

HOST = sys.argv[1]
PORT = int(sys.argv[2])
MSG_SIZE = int(sys.argv[3])
MSG_COUNT = int(sys.argv[4])
SLOW_RCVBUF = int(sys.argv[5])
DRAIN_DEADLINE_SEC = float(sys.argv[6])
PATH = "/api/v1/ws"


def ws_client_frame(opcode: int, payload: bytes) -> bytes:
    plen = len(payload)
    header = bytearray([0x80 | opcode])
    if plen < 126:
        header.append(0x80 | plen)
    elif plen <= 0xFFFF:
        header.append(0x80 | 126)
        header.extend(struct.pack("!H", plen))
    else:
        header.append(0x80 | 127)
        header.extend(struct.pack("!Q", plen))
    mask = os.urandom(4)
    header.extend(mask)
    return bytes(header) + bytes(payload[i] ^ mask[i % 4] for i in range(plen))


def parse_ws_frame(buf: bytes):
    """Return (frame_size, opcode, payload) if buf starts with a complete server frame, else None."""
    if len(buf) < 2:
        return None
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
    if len(buf) < pos + plen:
        return None
    return pos + plen, buf[0] & 0x0F, buf[pos:pos + plen]


class WsClient:
    def __init__(self, rcvbuf: int = 0):
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        if rcvbuf:
            # Before connect(), so that the advertised window stays small.
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, rcvbuf)
        self.sock.settimeout(2.0)
        self.sock.connect((HOST, PORT))
        self.buf = b""
        self.handshake()

    def handshake(self):
        key = base64.b64encode(os.urandom(16)).decode("ascii")
        self.sock.sendall((
            f"GET {PATH} HTTP/1.1\r\n"
            f"Host: {HOST}:{PORT}\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        ).encode("ascii"))
        while b"\r\n\r\n" not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError("Connection closed during handshake")
            self.buf += chunk
        head, _, self.buf = self.buf.partition(b"\r\n\r\n")
        if b" 101 " not in head:
            raise RuntimeError(f"Handshake failed:\n{head.decode(errors='replace')}")
        expected = base64.b64encode(
            hashlib.sha1((key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11").encode("ascii")).digest()
        )
        if b"Sec-WebSocket-Accept: " + expected not in head:
            raise RuntimeError("Sec-WebSocket-Accept mismatch")

    def send_json(self, obj):
        self.sock.sendall(ws_client_frame(0x1, json.dumps(obj).encode()))

    def recv_frame(self):
        """Return (opcode, payload). Raises socket.timeout, EOFError or OSError."""
        while True:
            frame = parse_ws_frame(self.buf)
            if frame is not None:
                size, opcode, payload = frame
                self.buf = self.buf[size:]
                if opcode == 0x9:
                    self.sock.sendall(ws_client_frame(0xA, payload))
                    continue
                return opcode, payload
            chunk = self.sock.recv(65536)
            if not chunk:
                raise EOFError
            self.buf += chunk


def fast_roundtrip(fast: WsClient, seq: int):
    """Send a small message and wait for its ack; the fast client must never stall."""
    fast.send_json({"type": "fast", "seq": seq})
    while True:
        opcode, payload = fast.recv_frame()
        if opcode == 0x8:
            raise RuntimeError(f"Fast client got closed at seq {seq}: {payload!r}")
        msg = json.loads(payload)
        if msg.get("type") == "ack" and msg.get("echo", {}).get("seq") == seq:
            return


fast = WsClient()
slow = WsClient(rcvbuf=SLOW_RCVBUF)
slow.sock.settimeout(0.5)
fill = "x" * max(1, MSG_SIZE - 64)

fast_roundtrip(fast, -1)

sent = 0
for i in range(MSG_COUNT):
    try:
        slow.send_json({"type": "stress", "seq": i, "data": fill})
    except (socket.timeout, OSError):
        break   # the server stopped reading from the slow client
    sent += 1
    if i % 16 == 0:
        fast_roundtrip(fast, i)

acks = 0
dropped_by = None
deadline = time.monotonic() + DRAIN_DEADLINE_SEC
while dropped_by is None and time.monotonic() < deadline:
    try:
        opcode, payload = slow.recv_frame()
    except socket.timeout:
        continue
    except EOFError:
        dropped_by = "EOF"
        break
    except OSError as e:
        dropped_by = type(e).__name__
        break
    if opcode == 0x8:
        code = struct.unpack("!H", payload[:2])[0] if len(payload) >= 2 else None
        dropped_by = f"close frame {code}"
    elif opcode == 0x1 and b'"type":"ack"' in payload:
        acks += 1

if dropped_by is None:
    raise RuntimeError(f"Slow client was not dropped (sent={sent}, acks read={acks})")
if acks >= sent:
    raise RuntimeError(f"Slow client got every ack (sent={sent}), queue limit never hit")

fast_roundtrip(fast, MSG_COUNT)

print(f"OK: slow client dropped by {dropped_by} after {acks}/{sent} acks, fast client kept its acks")
PY
