# Native CAT decoder tests

Replays CAT traces through the native CAT decoders (`src/cat_civ.c`,
`src/cat_state.c`) and checks assertions. The decoders are pure code (no
libev, no I/O), so no keyer, radio or mhuxd build is needed.

```
make -C tests/cat check            # all traces in traces/, native_test, router_test
tests/cat/cat_replay -v some.trace # every frame with classification and state
```

`native_test` checks the glue between decoder and mhcontrol (`src/cat_native.c`:
publish delay, waiting for the data flag, keepalive, rig type changes, keyer
state, CAT monitor, own queries) with router, mhcontrol and event bus stubbed
(`stubs.c`, shared). `router_test` runs end to end through the real router
(`src/mhrouter.c`): an emulated keyer (microHAM protocol, channel R1) with an
IC-7300 answering CI-V with bus echo, and Hamlib-like clients on connector
socketpairs. With one client it checks that the client never sees a frame of
mhuxd's queries and that every client command is answered in time; with two
clients on two connectors, that they take turns (no command reaches the radio
while its answer is due), each gets only the echo and answers of its own
commands, and an unanswered command or a removed connector doesn't block the
other client for long; a client without transceive (forward_unsolicited false)
gets none of the broadcasts. With the VFO knob "turned" (a broadcast every
30 ms, a command arriving meanwhile collides: cut-off broadcast and five FC),
two polling clients see no timeout and no debris while mhuxd resends the
destroyed commands (civ_collision_resend), and time out without it. Both need libev, are built with short intervals
(`NATIVE_TEST_DEFS` in the Makefile) and run for about 20 and 25 s.

See `docs/task_native_cat.md` for the design.

## Trace format

One item per line. `#` starts a comment, also at the end of a line. The CAT
monitor's own comment lines (decoder starts over, ports, collisions, given-up
commands) begin with the time: `# 14:23:02.237 H2: collision, command sent again`.

```
@civ addr=94                       # protocol and options, before the first data line
0.000 H1 fe fe 94 e0 25 00 fd      # [time] tag bytes
0.015 R  fe fe 94 e0 25 00 fd
0.031 R  fe fe e0 94 25 00 00 40 07 14 00 fd
= rx=14074000 unsel=? keyer=VOICE  # assertions on the state at this point
```

- **Time**: seconds (`12.345`) or `HH:MM:SS.mmm`, optional. Lines without a time
  are 10 ms after the previous line. Time must not go backwards.
- **Tags**: `H1`..`H9` client to radio, `M` mhuxd's own query to radio, `R`
  radio to host. On Icom behind the keyer, `R` includes the bus echo of every
  command. Each client is an application on a connector of its own, with a
  framer of its own. Radio frames answering an `M` query are shown as
  `# own reply` etc., those answering a client's command as `# H2 reply`.
- **Bytes**: hex, with or without spaces (`fe fe 94` or `fefe94`), and quoted
  text with `\r`, `\n`, `\t`, `\xNN`, `\\`, `\"` escapes (`"FA;"`), for the
  ASCII protocols later.
- **Protocol lines**: `@civ` (Icom), `@kenwood`, `@elecraft`, `@yaesu`, `@flex`
  (the Kenwood-style ASCII dialects, `src/cat_ascii.h`).
- **Options** for `@civ`: `addr=<hex>` radio address (`0` = learn from the
  traffic), `freq_len=<n>` BCD frequency bytes (default 5, 4 on IC-731/IC-735).
  For `@kenwood`: `nodata`, the radio has no data mode (TS-2000 and older).
  A later `@civ` line starts the decoder over (state and counters), as mhuxd
  does when the keyer reconnects or the rig type changes.
- **Time** stamps `HH:MM:SS.mmm` may wrap past midnight.

`cat_replay -v` prints every frame in this format with the classification and
the decoded state as a trailing comment, so its output replays as a trace.
The CAT monitor (rig mode sync backend native, option `monitor`) writes the
same format to `LOGDIR/cat-<serial>-r<n>.trace`, so user traces become test
fixtures without conversion: add `=` lines and put the file in `traces/`.

## Assertions

`=` lines hold `key=value` pairs. `?` means unknown. Keys:

| key | value |
|---|---|
| `rx`, `unsel` | frequency (Hz) of the selected / unselected VFO |
| `tx` | TX frequency (unselected VFO when split is on) |
| `mode`, `mode_unsel` | mode token as rigctld names it (`USB`, `PKTUSB`, `CW`, ...) |
| `data` | data mode flag of the selected VFO (`0`/`1`) |
| `keyer` | resulting keyer mode: `CW`, `VOICE`, `FSK`, `DIGITAL` |
| `split` | `0`/`1` |
| `vfo` | `A`/`B` |
| `addr` | radio address (hex), e.g. after learning |
| `frames`, `replies`, `applied`, `unmatched`, `bad`, `jams`, `dropped`, `foreign`, `errors` | decoder counters |
| `own` | counter: radio frames answering `M` queries (echo, reply, acknowledge, error) |
| `pending` | commands waiting for an answer: `none`, `client`, `own`, `both` |
| `lost` | CI-V: commands destroyed in a bus collision (not echoed, a jam followed), mhuxd sends them again |
| `busy` | clients with a frame partly received or a command waiting for its answer: `none`, `H1`, `H1,H2`, ... (mhuxd gives the line to the next client when its client isn't busy any more) |
| `client` | whose command the last radio frame answers (echo, reply, acknowledge, error): `H1`..`H9`, `M`, or `-` for none (transceive, auto information, unmatched) |

## Traces

- `civ_*.trace`: hand-written cases (queries, sets, transceive, VFO selection,
  framing, host tap, pipelining, address learning, 4-byte frequencies, own
  queries, two clients, bus collisions).
- `ascii_*.trace`: hand-written cases for the ASCII dialects.
- `hamlib_sim_*.trace`: Hamlib 4.6 talking to Hamlib's rig simulators
  (`simulators/` in the Hamlib source: simts590, simelecraft, simft991,
  simpowersdr), so the command sequences of the most common client library are
  covered. The simulators answer some commands with canned frames, see the
  comments in the files.
- `ic7300_*.trace`: excerpts of real traffic, IC-7300 behind a DK2, recorded with
  `rigctl -Z -vvvvv` and converted with `hamlib2trace.py`:

```
tests/cat/hamlib2trace.py --addr 94 --time 52.5:70 rigctl.log > excerpt.trace
tests/cat/hamlib2trace.py --dialect kenwood --text rigctl.log > session.trace
```
