# Native CAT decoder tests

Replays CAT traces through the native CAT decoders (`src/cat_civ.c`,
`src/cat_state.c`) and checks assertions. The decoders are pure code (no
libev, no I/O), so no keyer, radio or mhuxd build is needed.

```
make -C tests/cat check            # all traces in traces/, then native_test
tests/cat/cat_replay -v some.trace # every frame with classification and state
```

`native_test` checks the glue between decoder and mhcontrol (`src/cat_native.c`:
publish delay, waiting for the data flag, keepalive, rig type changes, keyer
state) with router, mhcontrol and event bus stubbed. It needs libev and runs
for about 9 s.

See `docs/task_native_cat.md` for the design.

## Trace format

One item per line. `#` starts a comment, also at the end of a line.

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
  command.
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

## Traces

- `civ_*.trace`: hand-written cases (queries, sets, transceive, VFO selection,
  framing, host tap, pipelining, address learning, 4-byte frequencies).
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
