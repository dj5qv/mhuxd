#!/usr/bin/env bash
set -euo pipefail

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-5052}"
MSG_SIZE="${MSG_SIZE:-8192}"
MSG_COUNT="${MSG_COUNT:-220}"
MSG_MAX="${MSG_MAX:-1500}"
SLOW_BPS="${SLOW_BPS:-100}"
SLOW_RCVBUF="${SLOW_RCVBUF:-1024}"
SLOW_DEADLINE_SEC="${SLOW_DEADLINE_SEC:-12}"
LOAD_DEADLINE_SEC="${LOAD_DEADLINE_SEC:-25}"
REQUIRE_SERVER_CLOSE_CODE="${REQUIRE_SERVER_CLOSE_CODE:-0}"

python3 - "$HOST" "$PORT" "$MSG_SIZE" "$MSG_COUNT" "$MSG_MAX" "$SLOW_BPS" "$SLOW_RCVBUF" "$SLOW_DEADLINE_SEC" "$LOAD_DEADLINE_SEC" "$REQUIRE_SERVER_CLOSE_CODE" <<'PY'
import base64
import hashlib
import os
import socket
import struct
import sys
import time

HOST = sys.argv[1]
PORT = int(sys.argv[2])
MSG_SIZE = int(sys.argv[3])
MSG_COUNT = int(sys.argv[4])
MSG_MAX = int(sys.argv[5])
SLOW_BPS = int(sys.argv[6])
SLOW_RCVBUF = int(sys.argv[7])
SLOW_DEADLINE_SEC = float(sys.argv[8])
LOAD_DEADLINE_SEC = float(sys.argv[9])
REQUIRE_SERVER_CLOSE_CODE = int(sys.argv[10])
PATH = "/api/v1/ws"

if MSG_SIZE < 64:
    MSG_SIZE = 64
if MSG_COUNT < 1:
    MSG_COUNT = 1
if MSG_MAX < MSG_COUNT:
    MSG_MAX = MSG_COUNT
if SLOW_BPS < 1:
    SLOW_BPS = 1
if SLOW_RCVBUF < 256:
    SLOW_RCVBUF = 256
if SLOW_DEADLINE_SEC < 2.0:
    SLOW_DEADLINE_SEC = 2.0
if LOAD_DEADLINE_SEC < 5.0:
    LOAD_DEADLINE_SEC = 5.0


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
    except (TimeoutError, socket.timeout):
        return None
    if len(hdr) == 0:
        return ("eof",)
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


def recv_exact(sock: socket.socket, n: int):
    payload = b""
    while len(payload) < n:
        try:
            chunk = sock.recv(n - len(payload))
        except (TimeoutError, socket.timeout):
            return None
        if not chunk:
            return b""
        payload += chunk
    return payload


def recv_ws_frame_with_budget(sock: socket.socket, budget_bytes: int):
    if budget_bytes < 2:
        return None, 0

    consumed = 0
    hdr = recv_exact(sock, 2)
    if hdr is None:
        return None, 0
    if hdr == b"":
        return ("eof",), 0
    consumed += 2

    b0, b1 = hdr
    fin = (b0 >> 7) & 1
    opcode = b0 & 0x0F
    masked = (b1 >> 7) & 1
    plen = b1 & 0x7F

    if plen == 126:
        ext = recv_exact(sock, 2)
        if ext is None:
            return None, consumed
        if ext == b"":
            return ("eof",), consumed
        consumed += 2
        plen = struct.unpack("!H", ext)[0]
    elif plen == 127:
        ext = recv_exact(sock, 8)
        if ext is None:
            return None, consumed
        if ext == b"":
            return ("eof",), consumed
        consumed += 8
        plen = struct.unpack("!Q", ext)[0]

    if masked:
        mask = recv_exact(sock, 4)
        if mask is None:
            return None, consumed
        if mask == b"":
            return ("eof",), consumed
        consumed += 4

    if plen > budget_bytes:
        return None, consumed

    payload = recv_exact(sock, plen)
    if payload is None:
        return None, consumed
    if payload == b"":
        return ("eof",), consumed
    consumed += plen

    if masked:
        payload = bytes(payload[i] ^ mask[i % 4] for i in range(len(payload)))

    return (fin, opcode, payload), consumed


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


def drain_fast_client(sock: socket.socket, deadline: float):
    got_ack = 0
    got_event = 0
    while time.time() < deadline:
        frame = recv_ws_frame(sock)
        if frame is None:
            continue
        if frame[0] == "eof":
            return got_ack, got_event, True
        _, opcode, payload = frame
        if opcode == 0x1:
            txt = payload.decode("utf-8", errors="replace")
            if '"type":"ack"' in txt:
                got_ack += 1
            if '"type":"client_event"' in txt:
                got_event += 1
        elif opcode == 0x9:
            try:
                sock.sendall(ws_client_frame(0xA, payload))
            except (TimeoutError, socket.timeout):
                pass
        elif opcode == 0x8:
            return got_ack, got_event, True
    return got_ack, got_event, False


def pump_fast_client(sock: socket.socket, max_frames: int):
    got_ack = 0
    got_event = 0
    closed = False
    for _ in range(max_frames):
        frame = recv_ws_frame(sock)
        if frame is None:
            break
        if frame[0] == "eof":
            closed = True
            break
        _, opcode, payload = frame
        if opcode == 0x1:
            txt = payload.decode("utf-8", errors="replace")
            if '"type":"ack"' in txt:
                got_ack += 1
            if '"type":"client_event"' in txt:
                got_event += 1
        elif opcode == 0x9:
            try:
                sock.sendall(ws_client_frame(0xA, payload))
            except (TimeoutError, socket.timeout):
                pass
        elif opcode == 0x8:
            closed = True
            break
    return got_ack, got_event, closed


def detect_slow_close(sock: socket.socket, deadline: float):
    saw_close = False
    close_code = None
    while time.time() < deadline:
        frame = recv_ws_frame(sock)
        if frame is None:
            continue
        if frame[0] == "eof":
            saw_close = True
            close_code = None
            break
        _, opcode, payload = frame
        if opcode == 0x8:
            saw_close = True
            if len(payload) >= 2:
                parsed = struct.unpack("!H", payload[:2])[0]
                close_code = parsed if 1000 <= parsed <= 4999 else None
            break
        if opcode == 0x9:
            try:
                sock.sendall(ws_client_frame(0xA, payload))
            except (TimeoutError, socket.timeout):
                pass
    return saw_close, close_code


def throttled_slow_reader(sock: socket.socket, bps: int, duration_sec: float):
    deadline = time.monotonic() + duration_sec
    budget = 0.0
    last = time.monotonic()
    close_seen = False
    close_code = None

    while time.monotonic() < deadline:
        now = time.monotonic()
        dt = now - last
        last = now
        budget += dt * bps
        if budget > bps * 2:
            budget = bps * 2

        if budget < 2:
            time.sleep(0.02)
            continue

        frame, consumed = recv_ws_frame_with_budget(sock, int(budget))
        if consumed:
            budget -= consumed

        if frame is None:
            time.sleep(0.01)
            continue
        if frame[0] == "eof":
            close_seen = True
            break

        _, opcode, payload = frame
        if opcode == 0x8:
            close_seen = True
            if len(payload) >= 2:
                parsed = struct.unpack("!H", payload[:2])[0]
                close_code = parsed if 1000 <= parsed <= 4999 else None
            break
        if opcode == 0x9:
            try:
                sock.sendall(ws_client_frame(0xA, payload))
            except (TimeoutError, socket.timeout):
                pass

    return close_seen, close_code


def throttled_slow_reader_step(sock: socket.socket, bps: int, state: dict):
    now = time.monotonic()
    dt = now - state["last"]
    state["last"] = now

    state["budget"] += dt * bps
    if state["budget"] > bps * 2:
        state["budget"] = bps * 2

    if state["budget"] < 2:
        return False, None

    frame, consumed = recv_ws_frame_with_budget(sock, int(state["budget"]))
    if consumed:
        state["budget"] -= consumed

    if frame is None:
        return False, None
    if frame[0] == "eof":
        return True, None

    _, opcode, payload = frame
    if opcode == 0x8:
        code = None
        if len(payload) >= 2:
            parsed = struct.unpack("!H", payload[:2])[0]
            code = parsed if 1000 <= parsed <= 4999 else None
        return True, code
    if opcode == 0x9:
        try:
            sock.sendall(ws_client_frame(0xA, payload))
        except (TimeoutError, socket.timeout):
            pass

    return False, None


slow = socket.create_connection((HOST, PORT), timeout=2.0)
fast = socket.create_connection((HOST, PORT), timeout=2.0)
try:
    slow.settimeout(0.05)
    slow.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, SLOW_RCVBUF)
    fast.settimeout(1.0)
    ws_handshake(slow)
    ws_handshake(fast)

    payload_fill = "x" * max(1, MSG_SIZE - 64)

    ack_count = 0
    event_count = 0
    fast_closed = False
    slow_closed_during_load = False
    slow_close_code = None
    slow_state = {"budget": 0.0, "last": time.monotonic()}
    load_deadline = time.monotonic() + LOAD_DEADLINE_SEC

    sent_count = 0
    i = 0
    while i < MSG_MAX and (i < MSG_COUNT or not slow_closed_during_load):
        if time.monotonic() >= load_deadline:
            break

        msg = (
            '{"type":"stress","payload":{"seq":%d,"data":"%s"}}'
            % (i, payload_fill)
        ).encode("utf-8")
        try:
            fast.sendall(ws_client_frame(0x1, msg))
        except (TimeoutError, socket.timeout):
            a, e, c = pump_fast_client(fast, 64)
            ack_count += a
            event_count += e
            if c:
                fast_closed = True
                break
            continue
        sent_count += 1

        if (i % 8) == 0:
            a, e, c = pump_fast_client(fast, 32)
            ack_count += a
            event_count += e
            if c:
                fast_closed = True
                break

        for _ in range(4):
            closed, code = throttled_slow_reader_step(slow, SLOW_BPS, slow_state)
            if closed:
                slow_closed_during_load = True
                slow_close_code = code
                break
        if slow_closed_during_load:
            break

        i += 1

    if not fast_closed:
        a, e, c = drain_fast_client(fast, time.time() + 4.0)
        ack_count += a
        event_count += e
        fast_closed = c

    if fast_closed:
        raise RuntimeError("Fast client got closed; expected it to stay alive")

    if ack_count == 0 or event_count == 0:
        raise RuntimeError(
            f"Fast client did not receive enough traffic (ack={ack_count}, event={event_count})"
        )

    if not slow_closed_during_load:
        saw_close, close_code = throttled_slow_reader(slow, SLOW_BPS, SLOW_DEADLINE_SEC)
        if not saw_close:
            saw_close, close_code = detect_slow_close(slow, time.time() + 2.0)
    else:
        saw_close, close_code = True, slow_close_code

    if not saw_close:
        raise RuntimeError(
            f"Slow client was not dropped under throttled read (bps={SLOW_BPS}, rcvbuf={SLOW_RCVBUF})"
        )

    if not slow_closed_during_load:
        raise RuntimeError(
            f"Slow client did not close during active load (sent={sent_count}, msg_max={MSG_MAX})"
        )

    if REQUIRE_SERVER_CLOSE_CODE > 0 and close_code != REQUIRE_SERVER_CLOSE_CODE:
        raise RuntimeError(
            f"Expected explicit server close code {REQUIRE_SERVER_CLOSE_CODE}, got {close_code}"
        )

    print(
        f"OK: deterministic backpressure validated (fast alive ack={ack_count} event={event_count}, "
        f"slow_closed={saw_close} code={close_code}, sent={sent_count}, slow_bps={SLOW_BPS}, slow_rcvbuf={SLOW_RCVBUF}, "
        f"required_code={REQUIRE_SERVER_CLOSE_CODE})"
    )
finally:
    try:
        fast.close()
    except Exception:
        pass
    try:
        slow.close()
    except Exception:
        pass
PY
