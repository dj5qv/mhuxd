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
