### Native CAT support: decode, own polls, multi-client CAT port

Status: phase 1 done (2026-10-09): decoders for Icom CI-V and the Kenwood-style ASCII dialects
(Kenwood, Elecraft, newer Yaesu, Flex/PowerSDR), radio state, trace tests, the listen-only
backend (`rig_mode_sync.backend: "native"`), CAT monitor, radio state in REST and WebSocket,
web UI. `src/cat*.c`, `tests/cat/`. Tested with IC-7300 + DK2 and WSJT-X (keyer mode follows
the radio, keepalive holds it after WSJT-X exits, monitor, UI). The ASCII dialects are tested
against the manuals, Hamlib's backends and Hamlib's rig simulators only, no real radio yet.
Phase 2 done (2026-10-10): own queries with client data held back and the answers taken out of
the radio data, through two router hooks instead of a separate broker (see "Phase 2" below).
Tested with the stub tests, end to end through the real router with an emulated keyer, IC-7300
and client (`tests/cat/router_test.c`), and with DK2 + IC-7300 + WSJT-X.
Phase 3 implemented (2026-10-10): several applications, each on a connector of its own, take
turns on the CAT line; answers go to the application that asked (see "Phase 3" below). Tested
with traces, end to end through the real router with two emulated clients, and with DK2 +
IC-7300: three applications on `cat1`, `cat1_2`, `cat1_3` (QLog got its queries through while
`rigctl` sent `f m` back to back). Own queries also go in right after an application's query
was answered, so a client that never pauses doesn't starve them (item 10 there).
Related: `docs/task_rigctld_support.md` (the current rig mode sync via rigctld).

## Reason

- Keyer mode (CW/VOICE/FSK/DIGITAL) and the keyer's frequency info should follow the radio,
  like the microHAM Windows Router does.
- Today this is done with hamlib's rigctld (`rig_mode_sync`, `src/rigctld_client.c`). It works,
  but:
  - beginners struggle with the setup (rig model number, rigctld options, PTT port, applications
    configured for "Hamlib NET rigctl" instead of the serial port),
  - rigctld occupies the CAT port, so applications can't use `/dev/mhuxd/cat1` directly anymore,
  - troubleshooting spans three programs (application, rigctld, mhuxd).
- The keyer's own CAT decoder (`usedecoderifconnected`) is not an alternative while a computer
  is connected. Measured with DK2 fw 3.14 + IC-7300 (2026-10-09):
  - with `usedecoderifconnected=1` the keyer polls the radio after ~0.96 s of CAT idle and then
    every ~1.05 s while idle, each poll taking ~150 ms,
  - it does **not** hold back host data meanwhile, so host commands collide on the CI-V bus
    (radio answers `FC`); host commands sent in those windows failed 50-87 %,
  - `dontinterfereusbcontrol=1` made no difference,
  - with the decoder off: 0 failures.
  - Clients that never leave the line idle for ~1 s (fldigi, most loggers) are not affected,
    WSJT-X with its 1 s poll is.
  - Conclusion: the keyer decoder is meant for stand-alone use. With a computer connected,
    the host side (Windows Router, mhuxd) has to do the work, and it has to hold client data
    while its own query is in flight.

## What the Windows Router does (public MK3 manual, "CAT Port" / "2nd CAT Port")

- The radio is selected in Router (radio type, baud rate, CI-V address).
- Router monitors the application's CAT traffic and polls the radio only for information the
  application doesn't request ("Disable router queries" turns that off while the port is open;
  with the virtual port closed Router always polls).
- While Router's own poll is in flight, application data is buffered and sent after the
  response arrived. If a response doesn't come or isn't understood: "oldest query discarded",
  but all data is still forwarded to the application.
- "2nd CAT" is a managed software Y-connector: polls/commands of both applications are processed
  alternately, responses go only to the application that sent the command. Unsolicited data
  (Icom transceive, Kenwood/Elecraft/Yaesu auto-information) can be forwarded per port
  ("Forward autoinformation to CAT port").
- A CAT monitor shows host traffic (H1/H2) and Router's own traffic (R) in different colors.
- "SDR tracking": a further port that emulates a TS-2000 (or Perseus) for SDR software,
  read-only.

## Scope

mhuxd does **not** need to control the radio. It needs to:

1. frame the byte stream and find transaction boundaries (which response belongs to which
   request),
2. decode frequency, mode and data flag (and split) from the traffic,
3. generate a few queries of its own when no client asks for that information.

Frequency/mode commands are the most stable part of every CAT protocol. Everything else
(menus, extended commands) is passed through without being understood.

rigctld stays supported. Native CAT becomes another `rig_mode_sync` backend and the default for
the protocol families it supports; rigctld remains the fallback for everything else.

## Protocol families

Derived from the radio types the keyers know (`src/radiotypes.c`):

| Family | Radios (`rig_types`) | Framing | Phase |
|---|---|---|---|
| Icom CI-V | all Icom, Ten-Tec OMNI VI (CI-V, address `04`) | `FE FE <to> <from> <cmd> [sub] [data] FD`, addressed | 1 |
| Kenwood-style ASCII | Kenwood, Elecraft, Yaesu FT-450/950/991/2000/dx1200/dx3000/dx5000/dx9000, Flex, ANAN, Expert MB1, "TS 2000 compatible SDR" | terminated by `;` | 1 |
| Legacy Yaesu binary | FT-757GXII/767/817/840/847/857/890/897/900/920/990/1000/1000MP | 5-byte commands, response length depends on opcode and model | 4 |
| Ten-Tec ASCII | ORION, ORION II, OMNI VII, JUPITER, ARGONAUT V | terminated by CR, but some replies carry 4-byte binary values | 4 (low priority) |
| Others | JRC, Hilberling, Perseus | various | rigctld only |

The family (and Icom CI-V address) defaults from the CAT channel's `rigtype` / `icomaddress`,
which users already configure. Newer radios missing from the keyer's list (IC-705, IC-9700,
FTdx10, FTdx101, TS-890, K4, ...) are configured as a compatible type, so an mhuxd-side
`protocol` override is useful (see Configuration).

### Icom CI-V

- Self-delimiting frames; collision jam `FC`; `FB` = OK, `FA` = NG.
- Transceive broadcasts: `to = 00`, cmd `00` (frequency) / `01` (mode).
- Reads to decode: `03` (frequency), `04` (mode/filter), `1A 06` (data mode, newer models),
  `0F` (split), `25 00/01` (selected/unselected VFO frequency), `26 00/01` (selected/unselected
  VFO mode + data + filter). Hamlib 4 uses `25`/`26` where available. The keyer decodes `26`
  answers too.
- Sets to decode once the radio answered `FB`: `05`, `06`, `25`, `26`, `1A 06`.
- **Echo:** the keyer talks to the radio's CI-V jack, a single-wire bus. The DK2 trace confirmed
  that command echoes reach the host on the CAT channel. Hamlib relies on that: with echo
  expected and none arriving it returns `RIG_BUSERROR` immediately
  (`rigs/icom/frame.c`, `icom_one_transaction`). Consequences for the broker:
  - an echo goes to the client that sent the frame, never to another client,
  - echoes of mhuxd's own queries are swallowed,
  - Hamlib also flushes and resends when it sees a reply addressed to a different controller,
    so mhuxd's own replies must never leak to clients.
- **Echo off:** Icom's "CI-V USB Echo Back" setting covers the radio's USB ports only. The
  keyers' CI-V port is an open-collector bus (DK2 and MK3 specifications), so there the echo
  comes from the wiring. Still, mhuxd must not depend on it: keyer radio ports can be
  jumpered to TTL/RS232, and other keyers are not checked yet. Other projects:
  - Hamlib detects the echo once in `rig_open()` (`icom_get_usb_echo_off()`: sends a command
    and checks whether its own frame comes back) and then expects that state. Its dynamic
    re-detection is disabled (`#if 0` in `rigs/icom/frame.c`, broke IC-706 + WSJT-X).
  - flrig: user option "Echo" (`serial_echo`), adds the command length to the expected reply.
  - wfview: classifies frames by address (to = radio: echo, to = itself: reply, to = 00:
    transceive), so the echo does not matter.
  - OmniRig: the ini files count the echo in `ReplyLength` and switch USB echo on in `[INIT]`.

  mhuxd does as wfview: the decoder classifies frames by address and also gets the client data
  (host tap, phase 1 step 2), so a query is known with or without its echo. The broker
  (phase 2) never waits for an echo and swallows the echoes of its own queries when they come.
- Data mode: the keyer needs USB vs. USB-D (VOICE vs. DIGITAL). On CI-V that is `1A 06` or the
  data byte of `26`, not part of `04`.

Decoder rules as implemented (`src/cat_civ.c`):

- A reply is only accepted for a pending query from the same controller (same cmd, same sub
  command), seen as host data or as bus echo. Replies are in order: a reply also closes older
  queries of that controller. Pending queries expire after 2 s.
- A set is applied only after the radio's `FB`. `FB` never answers a query, `FA` may.
- Strict layout checks: frequency length (5 BCD bytes, 4 on IC-731/IC-735), BCD digits, known
  mode bytes, data byte 0-3. In the DK2 recordings every frame damaged by a collision was
  rejected this way, none changed the state.
- Replies to the keyer's own polls (`26 01`, `25 01` with `usedecoderifconnected=1`) reach the
  host, addressed to `E0` like Hamlib's. Without a matching query they are rejected.
- Data flag: a mode change without data byte (`06`, transceive `01`) leaves the data flag
  unknown (counts as off) when the base mode changes, and keeps it when only the filter
  changes. `26` without data byte means DATA OFF (Icom manual).
- `07 00/01` (VFO A/B) swap the selected/unselected slots, `07 B0` exchanges, `07 A0`
  equalizes. Main/sub band radios (IC-7610, IC-7851, IC-9700, IC-7760: `07 D0/D1`, `25`/`26`
  address main/sub) are not modeled yet.
- The radio address comes from the configuration; with address 0 it is learned from a
  transceive frame, `FB`/`FA`, or a command/reply pair.

### Kenwood-style ASCII

- Frames end at `;`. Error replies: `?;` (also `E;`, `O;` on Kenwood).
- Replies carry the command prefix (`FA00014074000;`): 2 letters, 4 for `ZZxx` (PowerSDR/Flex).
- Set commands produce no reply. A query usually has no parameters, but many Yaesu reads take
  one (`MD0;`, `AG0;`), so "has parameters" does not mean "is a set".
- Hamlib follows every set with a verify query (`ID;`, `FA;` or `;` depending on the rig,
  `rigs/kenwood/kenwood.c`), so Hamlib clients never leave a set "open". Other applications may.
- Auto-information (AI) frames look exactly like replies.
- Reads to decode (to verify per model against manuals and Hamlib backends): `IF;` (frequency,
  mode, split in one answer on Kenwood/Elecraft/Yaesu), `FA;`, `FB;`, `MD;`/`MD0;`, data mode
  (`DA;` Kenwood, `DT;` Elecraft, DATA modes in `MD` on Yaesu), `FT;`/`FR;` (TX/RX VFO),
  `ZZFA;`/`ZZMD;`/`ZZIF;` (Flex/PowerSDR).

Decoder rules as implemented (`src/cat_ascii.c`, dialect from the rig type):

- Every well-formed frame from the radio is applied, reply or auto information: it names its
  command and carries the full value, and RS232 has no bus collisions. Client frames that set
  frequency, mode or data mode (`FA…`, `MD…`, `DA…`, `ZZMD…`) are applied too, there is no
  acknowledge to wait for; the next reply corrects a rejected set. VFO selection and split are
  only taken from the radio (an `FR` set also switches the TS-590 to simplex, `FT` to split).
- Client data only tells replies from auto information apart (monitor), decoding doesn't
  need it.
- After a broken frame (binary byte, too long) the framer starts over right away: there is no
  start marker to wait for, and so every byte stays in the CAT monitor.
- VFOs are addressed as A/B (`FA`/`FB`, `IF` with its VFO field, `FR`/`VS`), VFO A is assumed
  until the radio tells otherwise. Split from `IF`, `ST`, `ZZSP`, or `FR` ≠ `FT`.
- Kenwood (`IF` 37 characters: frequency 2-12, TX 28, mode 29, VFO 30, split 32, TS-590
  reference): `MD`/`IF` codes 1-9, the data flag from `DA` (TS-590, TS-990) or `DT` (TS-950).
  The other Kenwoods have no data mode: their codes are final (`no_data`). TS-990 `OM` and
  TS-890 `SF` have codes of their own for the data modes (C-F), so their 1-9 mean data off;
  Hamlib 4.6 reads the TS-590 mode with `SF0`. While transmitting in split, `IF` shows the TX
  VFO (Hamlib): only its split flag is taken then.
- Elecraft K2/K3: `MD6`/`MD9` (DATA, DATA-REV) with the sub mode from `DT` as Hamlib maps it:
  DATA A and PSK D → PKTUSB, AFSK A → PKTLSB, FSK D → RTTY. DATA A until `DT` is known.
  `MD$`/`DT$` for VFO B.
- Yaesu newcat: `FA`/`FB` with 8 or 9 digits, `MD0x`/`MD1x` (main/sub), codes with data
  variants (8 DATA-LSB, C DATA-USB, A/F DATA-FM), `IF`/`OI` 26/27 characters (VFO A/B), `FT`
  (TX VFO), `ST` (split), `VS` (VFO).
- Flex/PowerSDR/Thetis/other SDRs: Kenwood with FSK/FSK-R codes as DIGL/DIGU (Hamlib's
  emulation handling), plus `ZZFA`/`ZZFB`/`ZZMD`/`ZZME`/`ZZSP`, `ZZMD` codes as PowerSDR's.

### Ten-Tec (low priority)

An ORION is available for testing once it has been repaired.

- OMNI VI speaks CI-V and comes with the Icom codec.
- ORION/ORION II (programmer's reference 565prg Rev 1.2): commands are 2-3 characters plus data,
  terminated by CR. `*` = set (no reply), `?` = query, `@` = reply (`?RMM` → `@RMM<mode>`),
  `Z!<first 2 chars>` = error. 57600 Bd.
- Not purely text: `?A`/`?B` answer with a 4-byte binary frequency, which may contain `0D`. So
  CR alone is not a safe frame end; those replies need a fixed length. The ORION also offers
  ASCII alternatives (`?AF` → `@AF14200000`), preferable for own queries.
- OMNI VII replies without the `@` prefix (`?A` → `A` + 4 bytes + CR, per OmniRig).
  JUPITER/ARGONAUT V use the older Jupiter command set (to check against Hamlib's `tentec`
  backends).
- The ORION reference warns that firmware can be reprogrammed and the command set may change:
  query the firmware version. Unsolicited `ORION START` after a restart.

### Mode mapping

Family codecs map raw modes to a normalized mode (the same tokens rigctld uses), then reuse one
mapping to the keyer mode, shared with `mode_from_rigctld_token()` in `src/rigctld_client.c`:

| Keyer mode | normalized modes |
|---|---|
| CW | CW, CWR |
| VOICE | USB, LSB, AM, FM, WFM, AMS |
| FSK | RTTY, RTTYR |
| DIGITAL | PKTUSB, PKTLSB, PKTFM, PKTAM, FAX |

## Reference material

Local copies, not in the repo:

- Vendor manuals, `~/Devel/hamradio/docs/CAT_Protocols/`:
  - Icom: `IC-7760_CI-V_reference_guide_2024.pdf` (current CI-V command set), IC-78 manual,
    `CI-V manual.pdf` (2004 general CI-V reference; scanned, no text layer, needs OCR to search).
  - Kenwood: TS-2000 user manual (includes the PC command reference), `ts_590_g_pc_command_e.pdf`.
  - Yaesu: FT-1000MP Mark-V user manual (legacy binary CAT).
  - Ten-Tec: `565prg_Rev_1.2.pdf` (ORION), `tt_599_program_ref_v1.001.pdf` (Eagle).
- Hamlib source, `~/Src/Hamlib`: the backends (`rigs/icom`, `rigs/kenwood`, `rigs/yaesu`,
  `rigs/tentec`) show how the most common client library actually talks. `simulators/` for
  tests.
- OmniRig, `~/Devel/hamradio/OmniRig/`: source (Pascal, MIT license) and ~170 rig ini files.

### What OmniRig offers

- OmniRig owns the port. It is a poller like rigctld, not a sniffer. Its engine (`CustRig.pas`)
  is strict and simple: one command at a time; a reply is complete by fixed `ReplyLength` or by
  the `ReplyEnd` terminator; timeout; unexpected bytes are purged before the next command. That
  is the discipline the broker needs for its own queries.
- The per-model ini files describe exactly what mhuxd's own queries need: `[STATUSn]` sections
  with the command bytes, reply length or terminator, a `Validate` mask, and field positions and
  formats (`Value=offset|len|vfBcdLU|...`, `Flag=mask|value|param`). Most valuable where the
  protocol has no framing: legacy Yaesu (FT-1000MP: `00000002.10`, 16-byte reply) and Ten-Tec.
- For Icom, `ReplyLength` includes the echo (the ini files switch USB CI-V echo on). That
  matches the CI-V bus behind the keyer.

Limits:

- OmniRig's mode set (`CW_U/L`, `SSB_U/L`, `DIG_U/L`, `AM`, `FM`) folds RTTY and data into
  `DIG`, and the meaning depends on the ini variant: in `IC-7300.ini` DIG means RTTY, in
  `IC-7300-DATA.ini` both RTTY and USB-D/LSB-D map to DIG. The keyer needs FSK vs. DIGITAL, so
  mhuxd decodes the raw fields with its own mode tables and uses OmniRig only to learn where the
  fields are.
- Only the `[STATUSn]` (read) sections are relevant. `[INITn]` and the `pm*` write sections
  change radio settings (`IC-7300.ini` switches CI-V transceive off and sets the CW sideband)
  and must never be sent by mhuxd.
- License: the source is MIT. The ini files are contributions of various authors without a
  stated license. Don't copy them into the repo; derive mhuxd's own tables, cross-checked with
  the vendor manuals and Hamlib. Option for the long tail (phase 4+): load user-supplied OmniRig
  ini files at runtime for own queries.

## Architecture

### Where it sits in the data path

Today each connector gets a socketpair; conmgr hands the router side to the router with
`mhr_add_endpoint_fd()` (`src/conmgr.c`). Several connectors on the same CAT channel already
"work": the router sends radio data to all of them and interleaves their writes, with no
arbitration.

With native CAT enabled for a channel (`MH_CHANNEL_R1` / `MH_CHANNEL_R2`):

```
 client A ── VSP cat1   ──┐
 client B ── VSP cat1_2 ──┼── cat broker ──(mhr_send_in / consumer cb)── mhrouter ── keyer ── radio
 client C ── TCP :7001  ──┘        │
                                   └── decoded state ──> mhc_update_radio_info(ctl, "cat", &info)
```

- conmgr hands the connector's router-side fd to the broker instead of the router
  (`catb_add_client()` vs. `mhr_add_endpoint_fd()`).
- The broker registers one consumer callback on the channel (radio → host) and writes with
  `mhr_send_in()` (host → radio). It must respect `mhr_send_in()`'s buffer limits and the channel
  bps limit. On data channels the consumer callback delivers one byte per call, so the framers
  are incremental. `src/wkman.c` is a precedent for a module sending its own commands on a data
  channel this way.
- PTT via RTS/DTR uses a separate socketpair (`s_fd_ptt`) and is not affected.
- One broker per CAT channel; on MK2R/U2R the radio number (1/2) comes from the channel.
- `rigtype` "none", an unsupported family, or native CAT disabled: no broker, today's direct
  wiring, no behavior change.
- **Client granularity is the connector:** all sessions of one VSP or TCP connector share one
  `fd_data`. With `maxcon > 1` the broker cannot tell them apart. A second application gets its
  own connector (`cat1_2`, or a TCP connector), mirroring the Router's "2nd CAT". Validation:
  warn (or reject) `maxcon > 1` on a brokered channel.
- A TCP connector on the channel gives network CAT with arbitration for free.

### Output: existing sinks

- `mhc_update_radio_info(ctl, source, info)` (`src/mhcontrol.c`) is source-agnostic: it sets the
  keyer mode when it changes and sends frequency info when frequencies change. rigctld is one
  source, the broker is another. Per radio only one of them is active.
- Frequency info reaches only keyers with `MHF_HAS_CAT_CMD` (`src/mhinfo.c`: MK2, MK3,
  MK2R/MK2R+, SM/SMD). DK2 and U2R get mode sync only, same as with rigctld today.

### Module split

All protocol code is pure (no libev, no I/O) so it can be tested with byte arrays and recorded
traces:

- `cat_civ.c`, `cat_ascii.c`: incremental framers and decoders. Input: bytes plus direction
  (host→radio / radio→host). Output: frames, transaction hints (expects reply / is reply / is
  echo / is unsolicited), decoded values.
- `cat_state.c`: normalized radio state (rx/tx frequency, VFO A/B, mode, data flag, split) with
  per-item age. Fills `struct mhc_radio_info`.
- `cat_broker.c`: client sessions, transaction arbitration, own queries, forwarding rules. Talks
  to the radio through a small interface (write bytes / bytes-received callback), bound to
  mhrouter in mhuxd and to a pty in the test harness.
- `cat_mon.c`: CAT monitor (log + WebSocket stream) in the trace format used by the tests.

### Broker behavior

- **Fast path:** one active client and no own query pending: data is forwarded immediately in
  both directions without waiting for frame boundaries. The parsers only listen. No added
  latency.
- **Transactions:** one at a time. A client transaction starts when the client sends a frame and
  ends when
  - the matching reply (CI-V: same cmd/sub from the radio address, or `FB`/`FA`; ASCII: the
    prefix of the last query in the burst, or an error reply) arrived, or
  - nothing is expected and the line was quiet for a short gap, or
  - a timeout fires (reply expected but not received): log "oldest query discarded", release the
    line.
- **Holding:** data from other clients, and from the owner after its transaction ended while
  mhuxd's own query is in flight, is queued (bounded). Queues are served round-robin, as the
  Router does ("neither port has priority").
- **Routing (radio → host):**
  - echoes and replies → transaction owner only,
  - replies to own queries → consumed, decoded,
  - unsolicited (CI-V `to = 00`, ASCII AI frames outside a transaction) → each client with
    `forward_unsolicited` set,
  - anything unparseable → owner if there is one, otherwise all clients (fail open).
- **Known ambiguity (ASCII):** an AI frame with the same prefix as an outstanding query is taken
  as the reply; the real reply then counts as unsolicited. Both carry valid values. mhuxd never
  enables AI itself.

### Own queries

As implemented in phase 2: see "Phase 2" below. The original plan:

- Every decoded value (from client replies, acknowledged sets, transceive/AI, own queries)
  refreshes the item's age.
- No client connected: poll every `poll_ms`.
- Clients connected: query only items older than `stale_ms`, only when no transaction is open
  and the line was idle for `idle_gap_ms`. WSJT-X, which polls frequency and mode every second,
  then never triggers an own query.
- `own_queries: never` matches the Router's "Disable router queries".
- Query sets per family (to verify per model):
  - CI-V: `25 00`, `25 01`, `26 00` where supported, else `03`, `04`, `1A 06`, `0F`.
  - Kenwood/Elecraft/Yaesu: `IF;` plus the family's data-mode query; `FB;` where `IF;` lacks it.
  - Flex/PowerSDR: `ZZIF;` or `ZZFA;` + `ZZMD;`.
- Cost of one CI-V poll: a few frames of ~10 bytes each way at 9600-19200 Bd plus keyer USB
  latency, i.e. tens of ms. Hamlib's Icom timeouts are in the order of a second.
- With native CAT active and `usedecoderifconnected=1` on the same channel there would be a
  third party on the line: warn in log and UI (or force it off, see Open decisions).

## Configuration proposal

Extend the existing device-level `rig_mode_sync` with a new backend value:

```json
"rig_mode_sync": {
  "enabled": true,
  "backend": "native",
  "radios": {
    "r1": {
      "enabled": true,
      "protocol": "auto",
      "own_queries": "auto",
      "poll_ms": 1000,
      "stale_ms": 2000,
      "idle_gap_ms": 100,
      "monitor": false
    }
  }
}
```

- `protocol`: `auto` (from `rigtype`) | `civ` | `kenwood` | `yaesu` | `elecraft` | `flex` |
  `tentec`.
- CI-V address: from the channel's `icomaddress` (or the `rig_types` default), optional override.
- `own_queries`: `auto` | `never` (`never` = Router's "Disable router queries").
- Per connector (connector config): `forward_unsolicited` (default true, which preserves today's
  behavior of forwarding everything).
- Missing `backend` keeps meaning `rigctld` (backward compatible).

UI and REST:
- Keyer radio page: backend selector (Native / rigctld / off), decoded state shown live
  ("14.074.000 USB-D → DIGITAL"). That is the first troubleshooting step for beginners: "does
  mhuxd see my radio?".
- REST: GET decoded radio state; WebSocket: CAT monitor stream (H1/H2/... for clients, M for
  mhuxd's own traffic, R for radio).

## Testing

Only an IC-7300 is available, so most coverage must come from traces and simulators.

1. **Unit tests on traces** (`tests/cat/`, `make -C tests/cat check`): framers, decoders and the
   broker state machine are fed recorded byte streams. Trace format = CAT monitor format
   (timestamp, direction tag, hex or quoted text, `=` assertion lines; `tests/cat/README.md`),
   so traces from users with other radios become test fixtures without conversion.
   `tests/cat/hamlib2trace.py` converts `rigctl -Z -vvvvv` logs.
2. **Simulator integration:** Hamlib ships rig simulators (`simulators/` in the Hamlib source:
   `simic7300`, `simic7610`, `simts590`, `simts990`, `simft991`, `simftdx101`, `simelecraft`,
   `simflex`, `simpowersdr`, `simft817`, ...). They open a serial port given on the command line.
   Harness: simulator ↔ pty ↔ broker radio side; clients (`rigctl`/`rigctld` with the matching
   model, plus a raw-bytes client) on broker client ptys or TCP. To check: whether the Icom
   simulators emulate CI-V bus echo; if not, the harness adds it.
3. **Real hardware:** IC-7300 via DK2 and MK3, with WSJT-X, fldigi and QLog, plus the paced
   `rigctl` stress runs already used for the keyer decoder investigation (note: `rigctl` 4.6
   needs `-C poll_interval=0,cache_timeout=0` or it answers from its cache). ORION for the
   Ten-Tec family once it is repaired.
4. **Community:** users with Kenwood/Yaesu/Elecraft radios enable the monitor and send traces.

## Phases (implementation order)

### Phase 1: listen only (no bytes sent to the radio)

1. `cat_civ.c`, `cat_ascii.c`, `cat_state.c` + unit tests. Done. `rigctld_client.c` uses the
   shared mode table in `cat_state.c`. Frame types and counters shared by the decoders in
   `cat.h`. ASCII test traces: hand-written, plus Hamlib 4.6 against Hamlib's simulators
   (simts590, simelecraft, simft991, simpowersdr), which found the `SF` data semantics.
2. Attach as a consumer callback on the CAT channel (radio → host) plus a read-only tap on the
   router's producer path (client → radio), so queries are known without relying on the CI-V
   echo. No change to the data path. Done: `src/cat_native.c`, `mhr_add_producer_tap_cb()`.
3. Feed `mhc_update_radio_info(ctl, "catn", ...)`. Done:
   - Decoded frames are published 200 ms after the first one of a burst, so continuous
     polling publishes five times a second at most and is never starved.
   - Hamlib sets a data mode with `06` and then `1A 06`; in between the data flag is unknown.
     While it is unknown on LSB/USB/AM/FM the keyer mode is kept (mode -1) for up to 1 s,
     then the mode counts as without data. An application that never asks for the data flag
     (only `04`) therefore yields VOICE for USB-D until phase 2 adds own queries.
   - The channel's `rigtype`/`icomaddress` are rechecked at most once a second on incoming
     data and when the keyer comes online (the channel configuration is applied after
     `rig_mode_sync` at startup). Keyer state changes reset the decoder and the state.
   - Warns in the log while the channel's `usedecoderifconnected` is on.
   - Keepalive: the last decoded state is sent again every 5 s. mhcontrol drops radio info after
     10 s without an update (`RADIO_INFO_TIMEOUT`) and sets `r1ForceKeyerMode=1` again: the
     keyer then pins its mode to the configured `r1KeyerMode`, ignores SET KEYER MODE and
     manual switching. With rigctld that only happens when rigctld dies; listening only, it
     would happen whenever no application polls (seen in the first test with WSJT-X closed),
     and every application start/stop would rewrite the keyer settings. The keepalive stops
     when the backend is removed, the keyer goes offline or the rig type changes, so the
     fallback still works then. Without polling, front panel changes are only seen through
     CI-V transceive (IC-7300 default: on); phase 2's own queries close that gap.
4. CAT monitor: log + WebSocket stream in the trace format, fed from the same two taps.
   Done: per radio option `monitor`. Every frame of the decoder as one trace line
   (`src/cat_trace.c`, shared with `cat_replay -v`), with classification and the decoded state
   as comment, plus `@civ` and comment lines when the decoder starts over. Lines go to
   `LOGDIR/cat-<serial>-r<n>.trace` (stops at 20 MB, restarted when larger at open) and as
   WebSocket events `cat_monitor`. Client data is tagged `H1`..`H9` per connector since phase 3.
5. Config: `backend: "native"` (device level, so per keyer either rigctld or native), per
   radio `enabled`, `protocol` (`auto` from the rig type, or `civ`) and `civ_address` (0 = the
   channel's `icomaddress`). Done in `src/cfgmgrj.c` and `docs/rest-api-schemas.json`.
   REST and WebSocket, for both backends: mhcontrol publishes `EV_RADIO_INFO` when the radio
   info it gets changes or times out; `/api/v1/devices` lists `radios` (frequencies, mode name,
   keyer mode, source, valid), WebSocket event `radio_info`. `struct mhc_radio_info` got the
   mode name (`mode_str`), the source is the backend name (`rigctld`, `native`).
   Web UI: backend selector with native, per radio protocol / CI-V address / monitor, live
   "Radio state", monitor view. README section.

Result: no rigctld needed whenever an application polls (WSJT-X, fldigi, loggers), and the
application keeps `/dev/mhuxd/cat1`.

### Phase 2: own queries

Planned: move the CAT connectors behind a broker that owns their sockets (conmgr wiring), with
a fast path for a single client. Implemented instead (2026-10-10) with two hooks in the router,
which keeps its buffering, flow control and EOF handling, and leaves the data path unchanged
outside an own query:

- `mhr_hold_producers(router, channel, hold)`: while held, the router doesn't read the
  connectors of the channel; their data waits in the socket buffers.
- `mhr_intercept_consumers(router, channel, on)` + `mhr_pass_out()`: while intercepted, radio
  data of the channel only goes to the consumer callbacks; `cat_native.c` passes on what the
  applications should get.

Own queries (`src/cat_native.c`):

1. **When:** checked every 100 ms. A query is sent when the CAT line was quiet in both
   directions for 150 ms, no client command waits for its answer (decoders:
   `cat_civ_pending()` / `cat_ascii_pending()`, `CAT_PENDING_CLIENT`; an ASCII set has no
   answer and counts until it expires after 2 s, 200 ms since phase 3), no earlier own query
   may still be answered, the keyer is online and the radio address is known (CI-V). Since
   phase 3 also right after a client's query was answered (item 10 of phase 3).
2. **What:** values of the selected VFO older than 1.5 s (`CATN_OWN_STALE`, item time stamps
   in `cat_state`), each query at most every 1.5 s. Any decoded frame refreshes a value, so
   an application polling every second (WSJT-X) never triggers an own query.
   - CI-V: `03` (frequency), `26 00` (mode with data flag; after three `FA` from the radio
     `04` plus `1A 06` instead). Controller address `E5`, applications use `E0`.
   - Kenwood: `IF;`, plus `DA;` (TS-590, TS-990) or `DT;` (TS-950) for the data flag of
     LSB/USB/AM/FM. Elecraft: `IF;`, plus `DT;` while in DATA mode (the sub mode only changes
     on the radio). Yaesu: `IF;` (the mode includes data). Flex: `FA;`, `MD;`.
   - A query rejected three times in a row (`FA`, `?;`) is not sent again until the decoder
     starts over; a single rejection may be the radio's state (TX, menu).
3. **One at a time:** client data is held (`mhr_hold_producers`) from sending the query until
   the answer is in (reply, `FB`/`FA`, error) or 300 ms passed. The decoders mark frames that
   belong to an own query (`struct cat_frame.own`: echo, reply, acknowledge, error; counter
   `own`), matched per pending command, so a client reply in flight still goes to the client.
4. **Routing radio data:** from sending the query until no own query can be answered any more
   (also after a timeout, while the decoder keeps it pending for 2 s), radio data is
   intercepted: bytes are held until their frame is complete, frames of the own query are
   dropped, everything else (client replies, transceive, AI, unparseable data) is passed on.
   Outside these windows the router passes radio data on as before.
5. **Radio silent:** after three timeouts in a row queries go out every 10 s only. With own
   queries on, the keepalive stops when nothing came from the radio for 10 s, so mhcontrol's
   radio info expires and the keyer falls back to its configured mode, as with rigctld.
6. **Config:** per radio `own_queries`: `auto` (default) or `never` (listen only, the Router's
   "Disable router queries"). The intervals are constants; `poll_ms`, `stale_ms` and
   `idle_gap_ms` from the proposal above were not needed. Web UI: "Query the radio".
7. **Not needed:** telling the broker when applications open/close a port (`EV_CON_STATUS`):
   with values refreshed by the applications' own polls, the stale rule covers both cases.
8. **Tests:** `tests/cat/traces/civ_own.trace`, `ascii_own.trace` (decoders: own marking,
   pending); `native_test` (policy with stubbed router); `router_test` (real router, keyer
   protocol, emulated IC-7300 with bus echo, a Hamlib-like client: idle, polling faster and
   slower than the stale time, back to back, transceive broadcasts during own queries; checks
   that the client never sees a frame of an own query and every client command is answered).

Not done yet: keyer `usedecoderifconnected` still only warns (a third party on the line).

Found in the hardware test (2026-10-10, DK2 fw 3.14, both backends affected): the DK2 has no
VOICE mode. Asked for VOICE (LSB/USB/AM/FM without data) it switches to FSK and reports FSK.
`mhc_update_radio_info()` sent SET KEYER MODE whenever the keyer's last reported mode differed
from the radio's, and the keyer reports only changes, so it sent VOICE on every update until
the radio left voice. Now it sends when the radio's mode changes, when the forced mode was
just lifted, or when the keyer changed its mode by itself later (a report more than 1 s after
the request, e.g. its mode button); a failed command is sent again. The same applied to any
mode a keyer doesn't take, and to MK2R with the two radios in different modes (the old check
also compared the focused radio's mode). In addition, `mhc_update_radio_info()` maps VOICE to
FSK on keyers with `MHF_HAS_NO_VOICE_USE_FSK` (DK2), so mhuxd requests and shows the mode the
keyer actually takes.

### Phase 3: multiple clients (software Y-connector)

Planned: several connectors per brokered channel, round robin, reply routing,
`forward_unsolicited`, `maxcon > 1` validation. Implemented (2026-10-10) with more router hooks,
as phase 2, not with a broker owning the connector sockets:

1. **Clients are connectors.** Each application gets a connector of its own: a second VSP on
   the channel (`cat1_2`) or a TCP connector. The router identifies connectors by their endpoint
   fd (the tap callback gets it); `cat_native.c` numbers them H1..H9 in the order they first send
   data. conmgr passes the VSP device name or `tcp:<port>` as router tag, so the CAT monitor names
   them (`# H2: connector cat1_2`).
2. **Decoders** (`cat_civ.c`, `cat_ascii.c`): a framer per client, and every pending command
   carries its client; echo, reply, FB/FA and error replies name it (`struct cat_frame.client`,
   trace comment `# H2 reply`). `cat_*_client_busy()`: a frame partly received or a command
   waiting for its answer; `cat_*_client_discard()` gives them up.
   - CI-V: a reply closes the older commands of its controller and client only.
   - ASCII: a reply goes to the oldest pending query (no parameters) with its prefix, else to
     the oldest command with parameters (a read like Yaesu `MD0;`). A set gets no answer, so a
     query sent after it gets the reply, from whichever client. A command with parameters counts
     as pending for 200 ms only (`CAT_ASCII_SET_WAIT_MS`; before: 2 s), so a set doesn't block
     the line. A radio frame equal to a pending set is the radio repeating it (auto
     information): unsolicited, to all.
3. **Turns** (`mhr_set_producer_turn()`): with more than one connector on the channel and a
   decoder, the first application that sends gets the line. The router then reads only its
   connector, the others' data waits in their sockets (no queue in mhuxd). The turn ends when its
   commands are answered (checked on every radio byte and every 20 ms); the next connector with
   data waiting (`mhr_next_waiting_producer()`: FIONREAD, round robin after the current one) gets
   it, otherwise the line is free for whoever sends first. Two applications polling back to back
   alternate command by command. No answer 0.5 s after the application's last data
   (`CATN_TURN_TIMEOUT`): its commands are given up (the Router's "oldest query discarded"),
   the next application gets the line. An ASCII set has the line for up to 200 ms, enough for
   Hamlib's set + verify.
4. **Routing** radio → applications: in this mode radio data is always routed (the interception
   of phase 2): echo and answers to the application whose command they answer, answers to own
   queries dropped, transceive and AI to all, anything else (unmatched, collisions, broken
   frames) to the application that has the line, to all when none has. Radio data that doesn't
   complete a frame within 100 ms is passed on the same way.
5. Own queries only while no application has the line, see item 10.
6. **Connector removed** while its command is on the line: the router keeps the turn with it,
   cat_native passes it on once the answer is in or timed out (the next command would collide
   with the answer on the CI-V bus), the late answer is dropped. Routing goes on until then even
   when only one connector is left.
7. **One connector:** unchanged, no turns, radio data routed only around own queries. No decoder
   for the rig type: no turns either, as before phase 3.
8. **`maxcon > 1`** on a connector of the channel: warning in the log, since its applications
   share one data stream and can't be told apart. Not rejected, existing configurations keep
   working as before.
9. **Tests:** `traces/civ_clients.trace`, `traces/ascii_clients.trace` (decoders, with the new
   assertions `client=` and `busy=`); `router_test`: two clients on two connectors asking
   different commands, both back to back (they alternate), slow next to back to back (latency
   below 150 ms), both idle (own queries, transceive to both), an unanswered command (the other
   client waits less than 0.8 s), a connector removed while its command is on the bus. The
   emulated radio counts commands that arrive while its answer is due (bus collisions): none.

10. **Own queries between commands** (after the hardware test): a client that polls without
    pause and never asks for frequency, mode or data flag (meters, or `04` without `1A 06`)
    never left the 150 ms gap, so the values went stale (USB-D counting as VOICE after 1 s).
    The decoders know when a command is answered, so the line is free right then: when a radio
    frame answers a client's query (reply, FA, error; not FB, a radio with transceive on may
    report the change it just set), no other command waits for its answer and a value is
    stale, one own query goes out at once (`catn_own_try()`). The client's next command waits
    in its socket meanwhile, so it is delayed by one query (a few ten ms) now and then; with
    several clients the query goes in before the next one's turn. The 150 ms rule stays for an
    idle line and for traffic the decoders don't follow. `router_test`: one client and two
    clients polling only meters back to back (front panel change found, latency below 0.1 and
    0.15 s, nothing of the queries at the clients); a client polling frequency and mode back to
    back still triggers none.

11. **`forward_unsolicited`** per connector (default true; JSON `forward_unsolicited`, C
    `con_cfg.no_unsolicited`, web UI "Transceive / AI" in Add Port for CAT channels): false
    keeps CI-V transceive and AI frames (`CAT_FRAME_UNSOLICITED`) from that connector. Hamlib
    and rigctld tend to trip over transceive frames between their commands. conmgr sets the
    router consumer flag `MHR_CNS_NO_UNSOLICITED`; while any consumer of the channel has it
    and there is a decoder, radio data is routed (also with a single connector) and
    unsolicited frames go to `MHR_UNSOLICITED`, i.e. all consumers without the flag. The
    decoder still sees them, so the radio state follows the front panel. Without a decoder
    (rig type not supported) the frames reach all connectors, logged as a warning. ASCII: a
    reply that comes more than 200 ms after a read with parameters (Yaesu `MD0;`) counts as
    auto information and is held back as well. `router_test`: a polling client without
    transceive gets none of 10 broadcasts and all its answers while the state follows the
    broadcasts; two clients, one with and one without.

### Phase 4: extensions

- Legacy Yaesu binary (per-model response length tables; the OmniRig ini files list them).
- Ten-Tec ASCII, ORION first (low priority).
- Optional: own queries from user-supplied OmniRig ini files for radios without a built-in
  codec.
- SDR tracking port: read-only TS-2000 emulation built on the decoded state.
- Expose the decoded frequency via MCP / REST for other tools.

## Open decisions

- mhuxd's own CI-V controller address: decided `0xE5` (phase 2), distinct from the
  applications' `E0`, so the decoder tells the answers apart and traces are readable.
- Keyer `usedecoderifconnected` with native CAT active: warn only, or force off while connected.
- Interplay with keyer options `icomsimulateautoinfo` and `digitalovervoicerule`: to test.
- Bounds for queued client data and maximum hold time: decided in phases 2 and 3, client data
  waits in the connectors' socket buffers (no queue in mhuxd), at most 0.3 s for an own query
  and 0.5 s for another application's unanswered command.
- Quiet-gap and timeout defaults per family (to be measured: keyer USB frame latency, radio
  response times at 9600/19200/38400/115200 Bd).
- First MK3 trace with the IC-7300: confirm echo behavior matches the DK2.
