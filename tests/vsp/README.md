# VSP test suite

Exercises the CUSE/tty behaviour of `src/con_vsp.c` through real syscalls, without
starting the daemon.

`con_vsp.c` does not depend on conmgr, mhrouter, devmgr or cfgmgr — `vsp_create()`
only needs `{loop, fd_data, fd_ptt, vsp cfg}`, and the socketpair is the seam
between the connector and the router. So:

* `vsp_harness` creates the socketpair, hands the connector side to `vsp_create()`
  and keeps the router side. It does on command what mhrouter would do — including
  what a well behaved router never does, such as refusing to read so the
  connector's buffers fill up.
* `run_tests.py` is the client application. It opens `/dev/mhuxd/<name>` and uses
  ordinary `read`/`write`/`poll`/`ioctl`, so the tests see exactly what fldigi or
  hamlib would see.

## Running

```sh
modprobe cuse                 # once, if /dev/cuse is missing
make -C build                 # the harness links against build/src/*.o
make -C tests/vsp
sudo ./tests/vsp/run_tests.py
```

You need access to two things, so either run as root or arrange both:

* `/dev/cuse`, which is `0600 root:root` by default. An ACL granting your user
  access works too — check with `getfacl /dev/cuse`.
* the test devices under `/dev/mhuxd/`. They are named `mhuxd/vsptest*`, so
  `/lib/udev/rules.d/59-mhuxd.rules` applies to them exactly as it does to real
  VSPs (`SUBSYSTEM=="cuse", KERNEL=="mhuxd*", GROUP="mhuxd"`) and they end up
  `0660 root:mhuxd`. Being in the `mhuxd` group is therefore enough.

Note that udev applies that rule *after* the kernel has created the node: for a
few milliseconds it is still `0600 root:root`. `Harness._wait_for_device()` waits
for the node to be openable rather than just present, because waiting for
existence alone loses that race and opens fail with EACCES at random.

Options: `-v` echoes the harness protocol, `--only NAME` runs a subset,
`--allow-known` exits 0 when only known-issue tests fail.

The suite skips with exit code 77 when the harness is not built or `/dev/cuse` is
not accessible, so it never fails for environmental reasons.

Each test gets a fresh harness process and a pid-unique device name, so a test that
leaves a client wedged cannot affect the next one, and a crashed run does not block
the next one with a leftover device node.

## Expected result

Tests tagged with a finding number from `docs/review_con_csp.md` are expected to
fail until that finding is fixed — they are the reproduction cases:

| Test | Finding |
|---|---|
| `blocking_read_returns_available` | #12 blocking read waits for the full count instead of VMIN=1 |
| `nonblocking_read_empty_is_eagain` | #13 read replies 0 bytes (EOF) instead of EAGAIN |
| `nonblocking_write_full_is_eagain` | #13 write replies 0 instead of EAGAIN |
| `two_clients_one_idle` | #17 `size` clobbered in the fan-out starves later sessions |

The others must always pass: `smoke_roundtrip` and `poll_pollin` validate the rig
itself, `poll_pollout_after_drain` is the regression test for #6, and
`throughput_smoke` reports the rate and how much the connector drops when a client
cannot keep up (it measures, it does not assert).

## Throughput ramp

`ramp.py` is a measurement tool rather than a pass/fail test. It steps through the
standard baud rates in both directions and reports where data starts being lost,
which is the number that matters when sizing `buf_out` or diagnosing a slow
machine (an older Raspberry Pi, say).

```sh
./ramp.py                                  # both directions, defaults
./ramp.py --direction in --reader-delay 50 # model an application that stalls 50 ms
./ramp.py --seconds 1.0 --max-baud 460800
```

Inbound is the direction that can lose data silently. The drop count is exact,
taken from `TIOCGICOUNT`'s `buf_overrun`, and the harness also reports how much it
managed to push, which separates the two possible bottlenecks:

| Symptom | Meaning |
|---|---|
| `dropped > 0` | the reader is not draining `buf_out` fast enough |
| `sent < offered` | the connector is not draining the router socket fast enough |

Outbound cannot lose data — a full buffer shows up as a short write or EAGAIN — so
what it reports is the ceiling an application can sustain, plus a sequence check on
what actually arrived.

`--reader-delay` is the useful knob: it stalls the reader between reads, which is
what a real application does when it repaints a window or writes to disk. A stall
of D seconds at B bytes/s needs D×B bytes of buffer to ride out, so the results map
straight onto `BUFFER_CAPACITY`:

```
inbound  (rig -> application), 0.4s per step, reader stalls 50 ms between reads
       baud    offered  delivered    dropped  verdict
      57600       2304       2304          0  ok
     115200       4608       4150        458  reader too slow (buf_out overflow)
     230400       9216       4213       5003  reader too slow (buf_out overflow)
  -> highest clean inbound rate: 57600 baud
     50 ms of stall needs 576 bytes of buf_out at 115200 baud (BUFFER_CAPACITY is 512)
```

The client is Python, so its own overhead is included; a C application gets further
on the same machine. The absolute numbers are therefore a floor, but the
*comparison* between rates, and between machines, is meaningful.

## Harness protocol

Line based, one response line per command on stdout. The logger is pointed at a
file so it cannot corrupt the stream.

| Command | Effect | Response |
|---|---|---|
| `ping` | — | `ok` |
| `send <hex>` | write bytes to the router end | `ok <n>` |
| `flood <n>` | queue n bytes of a 0..255 ramp | `ok <n>` |
| `paced <bps> <ms>` | emit a ramp at a fixed byte rate | `ok <total>` |
| `recv` | take everything received so far | `data <hex>` |
| `pending` | bytes received but not taken yet | `ok <n>` |
| `sink on\|off` | count and verify instead of buffering | `ok` |
| `stats` | counters | `ok <sent> <received> <seqerr> <queued>` |
| `reset` | zero the counters | `ok` |
| `stopreading` | stop draining the router end | `ok` |
| `resume` | resume draining | `ok` |
| `shutdown` | close the router end | `ok` |
| `quit` | destroy the vsp and exit | `ok` |

The socketpair buffers are set to 1 kB so backpressure happens after a few
kilobytes rather than a few hundred.

## What this cannot cover

Anything above the connector: conmgr lifecycle, router interaction and config
handling need a device and are out of scope here. Findings #7 (CLOEXEC), #8 and
#10 also need fork or fault-injection tests rather than client-side ones.
`MHUXD_VSP_FORCE_CUSE_FAIL_ONCE` is available in the connector if you want to
drive the failure path.
