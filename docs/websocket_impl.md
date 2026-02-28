## 2026-02-28
Implemented. WebSocket support is now in the server, and REST events are bidirectional.

What I changed

Added upgrade-aware transport and WebSocket API in http_server.h and http_server.c.
Implemented RFC6455 handshake (Sec-WebSocket-Key → Sec-WebSocket-Accept), using parser upgrade semantics exactly as you requested.
Added frame handling in server (text/binary receive, ping/pong, close, masking enforcement, size limits).
Kept SSE endpoint unchanged and added WS endpoint /api/v1/ws in restapi.c.
Outbound server events now fan out to both SSE subscribers and WS subscribers.
Inbound WS text JSON messages are accepted and rebroadcast as type client_event, with an ack sent back to sender.
Validation

Build passes for changed C files (no diagnostics in http_server.c, http_server.h, restapi.c).
Web UI build also runs during make.
Current limits (intentional first cut)

No fragmented WS message reassembly yet.
Text/binary frames are bounded by current payload limits.
Incoming WS protocol is JSON-only at the REST layer.
Want me to add a strict message schema for inbound WS commands next (for example action, target, payload with per-action validators)?

### What's left
Great progress. For production readiness, a few crucial items remain:

High Priority

Implement fragmented frame reassembly (currently non-fragmented messages only).
Enforce full close-handshake behavior and idle timeouts (ping interval + pong deadline).
Add outbound backpressure limits per client (queue cap + disconnect policy for slow readers).
Validate text frames as UTF-8 and return proper close codes for protocol violations.
Security / Robustness

Add origin/auth checks on upgrade (especially if exposed beyond localhost).
Define and validate a strict inbound message schema (type, id, payload) with bounded sizes.
Add rate limiting for inbound client messages to prevent event-loop starvation.
Quality

Add WS integration tests: upgrade, ping/pong, large frames, close, malformed frames, fanout to multiple clients.
Add metrics/log counters (connected WS clients, dropped frames, parse errors, close reasons).
If you want, I can implement the first two now (fragmentation + heartbeat/timeout), which gives the biggest protocol-compliance win.

## 2026-02-28 Follow-up

Implemented follow-up hardening in `src/http_server.c`:

- Fragmented data message reassembly (`TEXT/BINARY` + `CONT`)
- Idle ping + pong timeout handling
- UTF-8 validation for WebSocket text payloads (single-frame and fragmented)
- Close code `1007` for invalid UTF-8 text

### Smoke test

Use the new smoke test:

`tests/restapi/ws_smoke.sh`

It verifies:

1. WebSocket handshake succeeds
2. Fragmented text message is accepted and processed
3. Server sends `ack` and `client_event`
4. Invalid UTF-8 text frame is rejected with close code `1007`

Run:

`HOST=localhost PORT=5052 bash tests/restapi/ws_smoke.sh`

### Backpressure stress test

Use:

`HOST=localhost PORT=5052 bash tests/restapi/ws_backpressure_stress.sh`

Optional tuning:

- `MSG_SIZE` (default `8192`)
- `MSG_COUNT` (default `220`)

Example:

`HOST=localhost PORT=5052 MSG_SIZE=12000 MSG_COUNT=260 bash tests/restapi/ws_backpressure_stress.sh`

Expected outcome:

- slow subscriber is closed under outbound pressure
- normal subscriber stays alive and still receives `ack` and `client_event`

## Outbound backpressure (next)

Yes — we should add this next. Right now outbound frames are queued on `http_connection.response_list` without a hard per-connection cap, so a slow client can grow memory.

### Recommended policy

1. Track queued bytes per connection (`queued_bytes`) in `http_server.c`.
2. Enforce soft/hard limits per connection, e.g.:
	 - soft: `256 KiB` (log `dbg0` once per connection)
	 - hard: `1 MiB` (disconnect slow client)
3. On hard limit:
	 - WS client: send close `1013` (`try again later`) and terminate
	 - SSE client: terminate connection
4. Keep policy per client (not global), so one slow consumer does not impact other subscribers.

### Minimal implementation shape

- Add fields to `struct http_connection`:
	- `size_t queued_bytes`
	- `unsigned int backpressure_warned : 1`
- In queue path (`queue_response_data`):
	- reject/close when `queued_bytes + len > HARD_LIMIT`
	- set warning flag above `SOFT_LIMIT`
	- increment `queued_bytes` when enqueue succeeds
- In write path (`hcon_out_cb`):
	- decrement `queued_bytes` by bytes fully flushed/freed
	- clear `backpressure_warned` when below soft limit again

### Validation

- Add a smoke/stress test with one intentionally slow WS reader and one normal reader.
- Verify only slow reader is disconnected, while normal reader keeps receiving events.



   

Read [](file:///home/mattes/Devel/mhuxd/src/http_server.c#1-1), lines 1 to 100

Read [](file:///home/mattes/Devel/mhuxd/src/http_server.h)

Read [](file:///home/mattes/Devel/mhuxd/src/http_server.c#100-100), lines 100 to 300

Read [](file:///home/mattes/Devel/mhuxd/src/http_server.c#300-300), lines 300 to 600

Read [](file:///home/mattes/Devel/mhuxd/src/http_server.c#600-600), lines 600 to 900

Read [](file:///home/mattes/Devel/mhuxd/src/http_server.c#900-900), lines 900 to 1200

Read [](file:///home/mattes/Devel/mhuxd/src/http_server.c#1200-1200), lines 1200 to 1500

Read [](file:///home/mattes/Devel/mhuxd/src/http_server.c#1500-1500), lines 1500 to 1779

Now let me also check how websocket is used elsewhere in the codebase to understand the integration points. 

Search complete: Find websocket callers

Searched for text `hs_ws_upgrade`, 3 results

Searched for text `hs_ws_send_text`, 5 results

Searched for text `hs_ws_send_binary`, 2 results

Read [](file:///home/mattes/Devel/mhuxd/src/restapi.c#200-200), lines 200 to 350

Read [](file:///home/mattes/Devel/mhuxd/docs/websocket_impl.md#1-1), lines 1 to 100

I've reviewed the entire file. Here's my analysis:

---

## Review of WebSocket Implementation in http_server.c

### Logical Errors / Bugs

**1. Timer callback doesn't check `terminate` — causes spurious repeated warnings**

In http_server.c, after a pong timeout triggers `hs_ws_close()` successfully, the timer keeps running. Since `ws_waiting_pong` remains 1 and `ws_enabled` remains 1, the callback fires every second, re-logging "websocket pong timeout" and re-calling `hs_ws_close()` (which returns 0 because `terminate` is already set). Harmless but noisy and wasteful.

**Fix:** Add a `terminate` check at the top of the timer callback:

```c
if(!hcon || !hcon->ws_enabled || hcon->terminate)
    return;
```

**2. `hs_send_response` and `hs_send_headers` bypass `queue_response_data` — outbound queue tracking is inconsistent**

These functions http_server.c without updating `outq_bytes`. This means the backpressure tracking (`outq_bytes`, soft/hard limits) only applies to WebSocket frames and `hs_send_chunk` calls. A flood of regular HTTP responses could grow unbounded. For this project (few HTTP clients) it's unlikely to be a problem, but it's an inconsistency to be aware of.

**3. Recursive call to `queue_response_data` when hard limit is hit on WS connections**

At http_server.c:
```c
if(hcon->ws_enabled && !hcon->terminate) {
    clear_response_queue(hcon);
    if(hs_ws_close(hcon, 1013, "backpressure") == 0)
        return -1;
}
```
`hs_ws_close` → `ws_send_frame` → `queue_response_data` (recursive). It works because `clear_response_queue` reset `outq_bytes` to 0 first, so the close frame fits. But this recursion is fragile and non-obvious. A comment would help at minimum, or better, have `hs_ws_close` use a direct-write path.

**4. Client close frame processing doesn't break the parse loop**

At http_server.c, after receiving `WS_OP_CLOSE`, the code sends a close response, sets `terminate=1`, stops `w_in`, then falls through to continue processing more frames in the same buffer. Subsequent frames would be processed needlessly (though `queue_response_data` will reject sends due to `terminate`). Should `break` out of the `while` loop via a flag or early return.

**5. Missing check: `hs_ws_close` after UTF-8 validation failure doesn't return**

At http_server.c:
```c
if(opcode == WS_OP_TEXT && !ws_validate_utf8(...)) {
    hs_ws_close(hcon, 1007, "invalid UTF-8");
    return 0;
}
```
After `hs_ws_close`, `return 0` is used. But the caller (`hcon_in_cb`) continues looping on `return 0`. Since `hs_ws_close` sets `terminate=1` and stops `w_in`, the next `read()` in the loop will likely fail with EAGAIN or return 0 (EOF), which is handled. This is OK but relies on side effects rather than explicit control flow.

---

### Efficiency

**1. XOR unmasking loop uses `% 4`**

http_server.c: `payload[i] ^= mask[i % 4]` — could use `& 3` instead of `% 4`. Compilers usually optimize this, but `& 3` makes the intent clearer for a bitwise operation.

**2. Per-connection 1-second timer**

The `ws_timer` fires every 1.0s for every WS connection. For this project's expected connection count (likely single digits), it's fine. For higher scale, a single global timer scanning all connections would be better.

**3. Duplicate buffer growth logic**

`ws_append_buf()` and the buffer growth in `ws_raw_data_cb` are essentially the same doubling-realloc pattern. Could share a common `dbuf_append` or similar, but the duplication is minor (~10 lines).

---

### Simplicity

**The file is 1779 lines** and now includes:
- HTTP server lifecycle (~200 lines)
- HTTP connection management (~300 lines)  
- HTTP request parsing callbacks (~200 lines)
- File serving (~150 lines)
- Response formatting (~150 lines)
- SHA1 + Base64 (~130 lines)
- UTF-8 validation (~65 lines)
- WebSocket protocol (frame parse, send, handshake, timer, fragmentation) (~350 lines)
- Misc utilities (date formatting, host:port parsing) (~80 lines)

The WebSocket code alone is **~550 lines** (SHA1, Base64, UTF-8, framing, handshake, timer, buffer management).

---

### Should WebSocket be a separate module?

**Yes — strongly recommended.** Reasons:

1. **Clear boundary exists.** The WS code interfaces with `http_connection` through a narrow API: it installs via `hs_set_upgraded_mode`, sends via `queue_response_data`, and receives raw bytes through the `upgraded_data_cb`. This is a natural module boundary.

2. **Self-contained protocol layer.** SHA1, Base64, UTF-8 validation, frame encoding/decoding, fragmentation reassembly, and ping/pong are all WebSocket-specific and independent of HTTP request handling.

3. **Testability.** Frame encoding/decoding, UTF-8 validation, and fragmentation could have unit tests if isolated.

4. **Maintainability.** The HTTP server was ~700 lines before. The WS addition nearly tripled its cognitive load.

**Suggested split:**

| New file | Contents |
|---|---|
| `ws.c` / `ws.h` | SHA1, Base64, UTF-8 validation, frame encode/decode, `ws_raw_data_cb`, handshake logic, timer, fragmentation, `hs_ws_*` public API |
| http_server.c | HTTP server, connections, request parsing, file serving, response formatting (keeps `hs_set_upgraded_mode` as the integration point) |

The `ws.c` module would need access to `queue_response_data` and a few `http_connection` fields (`ws_*` members, the ev_loop, fd). This could be handled by either making `http_connection` visible to `ws.c`, or by passing an opaque context + function pointers for sending data.

Would you like me to implement any of the fixes or the module extraction?