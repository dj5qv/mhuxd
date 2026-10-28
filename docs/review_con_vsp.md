# Review: `src/con_vsp.c` — CUSE usage and related findings

Date: 2026-09-17
Branch: v0.90beta (at 605a995)
Reviewed file: `src/con_vsp.c` (1093 lines)
Context also read: `buffer.[ch]`, `conmgr.c` (socketpair setup), `mhrouter.c` (PTT byte handling),
`ptt_byte.h`, `linux_termios.h`, installed libfuse **2.9.9**.

No code was changed as part of this review.

Reproduction cases for #12, #13 and #17 live in `tests/vsp` (see its README); #6 has a
regression test there too.

Ranked shortlist, if only a few items get fixed:
**#16** (stuck PTT), **#1** (`fi->fh`), **#2** (dangling write buffer), **#7** (CLOEXEC),
**#12/#13** (read/write semantics). Items #3/#4/#5 are small, self-contained ioctl corrections.

---

## CUSE / libfuse contract issues

### 1. `fi->fh` is never assigned — every session aliases the first one
`src/con_vsp.c:456`

`dv_open` does `vs->fd = fi->fh;`, but `fh` is an *output* of `open`: the `FUSE_OPEN` request has no
`fh` field, so libfuse's `do_open()` hands the callback a zeroed `struct fuse_file_info` with only
`fi.flags` filled in, and the *filesystem* is supposed to store a handle
(`fuse_lowlevel.h:425`: "Filesystem may store an arbitrary file handle ... in fi->fh"). So `fi->fh`
is always 0 here, `fuse_reply_open` hands 0 back to the kernel, and every subsequent request
arrives with `fh == 0`. `find_vs()` (`src/con_vsp.c:90`) therefore always returns the *first* node
in `session_list`.

(`fi->flags` is *not* affected — `do_open` sets it from the request, as documented at
`fuse_lowlevel.h:422`. It is likewise populated for `read`, `write` and `release`, but not for
`poll` or `ioctl`. Blocking vs. non-blocking must be read from the per-request `fi->flags`, which
`dv_read:556` and `dv_write:612` already do correctly: `fcntl(F_SETFL)` generates no FUSE request,
so the open-time copy in `vs->fd_flags` goes stale. `vs->fd_flags` is only sound for the access-mode
test at `src/con_vsp.c:323`, which cannot change after open.)

With the default `maxcon == 1` this is invisible. With `maxcon > 1`, client #2's reads, writes,
ioctls, polls and its `release` all operate on client #1's session. The `release` case is the nasty
one: closing the second client frees the first client's `vs`, and the first client then does
read/write/ioctl against freed memory.

Fix: `fi->fh = (uintptr_t)vs;` in `dv_open` (and match `find_vs`, or cast back directly).
(fixed)

### 2. `pending_in_buf` points into the channel receive buffer
`src/con_vsp.c:615` (stored), `src/con_vsp.c:420` (used)

`dv_write`'s blocking path stores the `buf` argument and reads from it later, from `data_out_cb`.
That pointer is inside `vsp->chan_buf` — `fuse_session_process(se, vsp->chan_buf, ...)` passes a
pointer into that buffer straight through to the write op, and it is only valid for the duration of
the callback. The next `fuse_chan_recv()` in `chan_in_cb` overwrites it. Any FUSE request arriving
while a blocking write is draining (a `poll` or `ioctl` from another thread of the same app, a
second session, an `interrupt`) silently corrupts the data still to be written.

It survives today because the router usually drains the socket before another request shows up, and
because the negotiated `max_write` (~4 KiB) vs. the 512-byte `struct buffer` means the pend path is
hit but resolved quickly. Latent data corruption, not theoretical.

Fix: copy the tail into a dynamically sized buffer, exactly like `pending_out_buf` already does.
(fixed)

### 3. `TIOCGICOUNT` declares the wrong transfer direction — always fails with EIO
`src/con_vsp.c:873`

It retries with an **in**_iov (`fuse_reply_ioctl_retry(req, &iov, 1, NULL, 0)`) and then replies with
`sizeof(struct serial_icounter_struct)` bytes of payload. The retry sets the kernel's `out_size` to
0, and the kernel rejects a reply whose payload exceeds `out_size` with `-EIO`. `TIOCGICOUNT` is a
read ioctl and must use the `out_bufsz`/out_iov form, like `TIOCMGET` at `src/con_vsp.c:787`.
(fixed)

### 4. `TCFLSH` reads the argument from the wrong place
`src/con_vsp.c:827`

```c
switch((int)(long)in_buf)
```

`TCFLSH` passes its selector *by value*, so it is in `arg`, not `in_buf`. No retry is requested
here, so `in_buf` is `NULL` and the expression is always 0 == `TCIFLUSH`. `TCOFLUSH` and
`TCIOFLUSH` therefore never flush the output buffer. Should be `(int)(long)arg`.
(fixed)

### 5. `TIOCMIWAIT` treats a value argument as a user pointer
`src/con_vsp.c:845`

Same root cause as #4: the retry asks the kernel to `copy_from_user(arg, sizeof(int))` where `arg`
is the bitmask itself (e.g. `0x20`), so the ioctl fails with EFAULT and the second branch is
unreachable — hence the two `warn("TIOCMIWAIT 1/2!")` lines that presumably never both fired.

Separately, even once fixed, replying immediately with success is wrong for `TIOCMIWAIT` (it must
block until a modem-line change); an app looping on it would spin at 100% CPU. Returning
`ENOTTY`/`EINVAL` is safer than a fake success.
(fixed: replies ENOTTY)

### 6. Poll handling
`src/con_vsp.c:635`

- On the `vs == NULL` path the code `goto out` with `ph` still owned by the callee — leaked.
  `fuse_lowlevel.h`: "The callee is responsible for destroying ph with `fuse_pollhandle_destroy()`".
  libfuse mallocs a fresh handle per poll request, so this leaks per poll on a stale `fh`.
- `POLLOUT` is reported from `buf_size_avail(&vs->buf_in)`, but nothing ever calls
  `fuse_lowlevel_notify_poll()` when space frees up — `data_out_cb` notifies nobody. Since
  `struct buffer` only reclaims space when it drains completely (`buf_consume` -> `buf_reset` only
  at `rpos == size`), a client polling for writability on a full buffer can wait forever.

(fixed: both parts)

### 7. The `/dev/cuse` fd is not `FD_CLOEXEC` — it leaks into `rigctld`
`src/con_vsp.c:1007`

Disassembly of the installed `libfuse.so.2` (2.9.9) shows both `open64` calls in
`cuse_lowlevel_setup` use flags `0x2` (plain `O_RDWR`), no `O_CLOEXEC`. So the CUSE channel fd
survives the `fork()`/`execvp()` in `rigctld_client.c:341`, and the spawned `rigctld` holds the
*server* side of the device open.

Consequences: after `vsp_destroy()` the kernel still considers the CUSE server alive,
`/dev/mhuxd/<name>` does not go away, and re-creating the connector fails in `check_cuse_dev()` with
"already exist". `PR_SET_PDEATHSIG` covers mhuxd dying, but not deleting/recreating a VSP from the
web UI while rigctld runs.

Fix: `fcntl(fuse_chan_fd(ch), F_SETFD, FD_CLOEXEC)` right after `cuse_lowlevel_setup`.
Related: the `socketpair()`s in `conmgr.c:166` and `conmgr.c:195` have the same problem — no
`SOCK_CLOEXEC`, and `fcntl(F_SETFL)` only sets status flags.
(fixed: fcntl F_SETFD on the cuse channel fd, SOCK_CLOEXEC on both socketpairs)

### 8. `chan_in_cb` treats normal session exit as an I/O error
`src/con_vsp.c:283`

`fuse_chan_recv()` returns `0` when the session has exited or the device got `ENODEV` (module
unloaded, connection aborted) — an orderly shutdown, not a failure. It also returns `-errno`, so
`vsp_fail(vsp, errno, ...)` logs a stale/unrelated `errno`; use `-res`. `-EAGAIN` should not be
fatal either.

(fixed: res == 0 is treated as a failure — the only reachable cause is the kernel aborting
the connection, which mhuxd never asks for — so it goes through fail() with no errno.
-EAGAIN is transient, the error path uses -res. Telling conmgr to drop the dead connector
is still open, see #9.)

### 9. `fail()` leaves the CUSE device registered but unserviced
`src/con_vsp.c:152`

It sets `MHUXD_IO_FAILED`, stops the watchers, closes the router sockets and calls
`fuse_session_exit()` — but `fuse_session_exit()` only sets a flag; the channel fd stays open, so
the device node remains in `/dev/mhuxd/`. Nothing services it any more, so open clients hang in
`read()`/`write()` indefinitely, still-pending `pending_in_req`/`pending_out_req` are never
answered, and a recreate hits "already exist" in `check_cuse_dev()`.

Note this is reachable from more than the dead-channel path of #8: router read/write errors and
the fault injection call `fail()` too. All of them are rare, so this is a priority-low but
correctness-real item.

**Remedy (agreed 2026-09-18): tear down the CUSE session, keep the connector registered.**

**Correction (2026-09-20): the config-loss objection recorded here was wrong.** The persisted
config is `cfgmgrj->connectors`, a JSON array that `apply_connector_from_json()` (`cfgmgrj.c:988`)
calls a "Registry of Intent" and updates *regardless of whether `conmgr_create_con_cfg()`
succeeded*. `build_config_json()` (`cfgmgrj.c:1218`) saves that array, never the live conmgr list.
`cfgmgrj_sync_from_conmgr()` is the only path that builds config from the live list, and it runs
once, during legacy config migration (`app_ctx.c:139`), when there is no JSON intent list to
preserve. Verified in the field: a VSP on the MCP channel that fails the new PTT guard stays in
the config and is retried on restart.

That inverts the conclusion. The status view (`cfgmgrj.c:1229-1244`) deep copies the connectors
array and stamps `status: "ok"|"failed"` per entry from `conmgr_exists(id)`, so a connector that
is *absent* from conmgr already renders as failed while keeping its config. Letting a failed VSP
drop out of conmgr therefore produces exactly the "shown as failed, user can restart it" UX
below, and needs no new state — intent lives in the JSON array, liveness in conmgr.

The `con_vsp.c` half is unchanged and still needed: on failure, reply `EIO` to pending requests,
free the `vsp_session`s, and `fuse_session_destroy()` the session so the channel fd closes and the
device node disappears. Clients then get a clean error instead of hanging, and the name becomes
free for a restart. Whether the connector then also leaves conmgr is now a free design choice
rather than something the config format forbids.

**Planned follow-up (not scheduled): stop/start VSPs from the web UI.**

Keep a failed VSP registered with conmgr in a failed/stopped state and let the user restart it from
the port management page, instead of being stuck until mhuxd is restarted. `conmgr_foreach()`
builds `struct con_info` purely from the connector and never looks at `ctr->instance`, so
`instance == NULL` can mean "stopped" with no effect on the config sync or the UI listing.

Two constraints for that work:

- `FAILED -> OPEN` is not a legal transition (`io_state_can_transition`, `util.c:91`), so a
  restart cannot reuse the same `struct vsp`. Stop should tear the instance down to `CLOSED` and
  free it; start should call `vsp_create()` fresh.
- Start/stop also has to redo the socketpairs and `mhr_add_endpoint_fd()`/`mhr_rem_endpoint_fd()`,
  since `vsp_destroy()` closes the connector-side fds. The seam therefore belongs in conmgr —
  splitting `conmgr_create_con_cfg()` into "register the connector (config)" and "start the
  connector (resources)" — not in `con_vsp.c`. The UI endpoints drive the second half only.

(fixed, 2026-10-28: `fail()` now drops the sessions through `end_sessions()`, shared with
`vsp_destroy()`. That sends PTT off for every session that keyed it, answers held reads/writes
with `EIO`, then closes the router sockets and calls `fuse_session_destroy()`, so the device node
goes away and the name is free again. The order matters: PTT off goes out before the sockets close,
and the replies go before the session is destroyed, because a `fuse_req_t` does not outlive its
session. EOF on the router data socket is now a failure as well. Before, it was ignored, and the
level-triggered, always-readable fd made `data_in_cb` spin.

conmgr finds out through a new `connector_spec.on_fail` callback. It keeps the connector, marks it
failed and publishes `EV_CON_STATUS`, which restapi sends as a WebSocket `connector_status` event.
The Ports page updates without a reload, and `/api/v1/config/connectors` reports `status: "failed"`
through `conmgr_get_status()`, which replaces `conmgr_exists()`. The configuration is not touched.

conmgr does not remove the router endpoints right away. The router finds the EOF itself, and reads
the PTT off that is still in the socket before it closes its end. Removing the endpoint
synchronously would discard that byte. The catch is that the router may close the fd before conmgr
gets to it, and the number can then be reused by another endpoint. `conmgr_destroy_con()` therefore
only removes an endpoint the router still holds under this connector's tag pointer
(`rem_router_endpoint()`).

Still open, same root cause: deleting a VSP while its PTT is keyed. `conmgr_destroy_con()` removes
the router endpoints before `vsp_destroy()` writes PTT off, and the router would not read it in
time anyway. That needs the router to track PTT per producer. TCP connectors do not report
`ctcp_fail()` yet. tests/vsp: failure_ends_sessions_and_device)

(fixed, 2026-10-28: the router tracks PTT per producer. `struct Producer` holds the PTT bits it
keys (`ptt_mask`), and the keyer gets their OR, so one connector releasing PTT no longer unkeys a
radio another one still keys. `mhr_rem_producer()` releases what a producer still holds when it
goes away: deleted, failed, or with its PTT off still unread in the socket. That covers deleting a
keyed VSP, and also a failure on a port whose data and PTT share one socket, where `fail()` cannot
write PTT off any more. `conmgr_destroy_con()` now destroys the instance before removing the router
endpoints, so the connector's own PTT off reaches an open socket instead of failing with EPIPE.
Also fixed on the way: on `CH_PTT_FOCUS`, PTT off released the radio that had the focus *now*, so
a focus change while keyed left the other radio keyed. And held PTT is forgotten when the keyer is
lost, otherwise any later flags byte would key the radio again after the reconnect. Several
clients on one `maxcon > 1` port still share one producer, and only `maxcon` 1 is planned for PTT
ports. tests/ptt: router_ptt_test)

### 10. Signal handlers: do NOT call `cuse_lowlevel_teardown()`
`src/con_vsp.c:1007` (setup) / `src/con_vsp.c:1146` (teardown)

`cuse_lowlevel_setup()` calls `fuse_set_signal_handlers()`, which sets the *global*
`fuse_instance = se` and installs handlers for SIGHUP/INT/TERM — but only where the current
disposition is `SIG_DFL`. `vsp_destroy()` calls `fuse_session_destroy()` directly and never
`fuse_remove_signal_handlers()`, so the global is left dangling.

**Verified 2026-09-18: latent, not live — and the obvious "fix" is harmful.**

`cuse_lowlevel_teardown()` is `fuse_remove_signal_handlers()` + `fuse_session_destroy()`. The first
half calls `set_one_signal_handler(SIGPIPE, SIG_IGN, remove=1)`, which restores `SIG_DFL` whenever
the current disposition equals the handler libfuse would have installed — i.e. `SIG_IGN`. It
assumes it was the one that ignored SIGPIPE. `main.c:45` ignores SIGPIPE at startup on purpose, so
tearing down a single VSP connector would re-arm SIGPIPE and the daemon would die on the next
EPIPE (an aborted HTTP request is enough). Measured against the installed libfuse 2.9.9:

```
before: SIGPIPE = SIG_IGN      before: SIGINT = custom (ours)
after:  SIGPIPE = SIG_DFL      after:  SIGINT = custom (ours)
```

The SIGINT column is the good half: a foreign handler is left alone, because the current
disposition is not libfuse's `exit_handler`.

Why the dangling `fuse_instance` is unreachable today:

- `app_ctx_init()` starts the `ev_signal` watchers (`app_ctx.c:118-124`) before the config is
  loaded, therefore before any connector exists.
- `ev_default_loop(EVFLAG_AUTO)` does not request `EVFLAG_SIGNALFD`, so libev installs real
  `sigaction` handlers rather than blocking the signals.
- `fuse_set_signal_handlers()` therefore sees non-`SIG_DFL` dispositions for HUP/INT/TERM and
  installs nothing, so `exit_handler` — the only reader of `fuse_instance` — can never run.

Action taken: the commented-out `cuse_lowlevel_teardown()` call in `vsp_create()`'s failure path
was replaced with a comment recording the SIGPIPE hazard. `fuse_session_destroy()` stays as the
correct call in both `vsp_create()`'s failure path and `vsp_destroy()`. No public API clears
`fuse_instance` without the SIGPIPE side effect, and it is not worth exploiting internals to do so
while the pointer is unreachable.

Keep in mind if the startup order ever changes: creating a VSP *before* the `ev_signal` watchers
start would let libfuse install its handlers for real, and then the dangling pointer becomes live.

### 11. `FUSE_IOCTL_COMPAT` is ignored
`src/con_vsp.c:844` (`(void)flags`)

A 32-bit client on a 64-bit kernel gets native-layout structs back. `termios`/`termios2` happen to
be identical, but `struct serial_struct` contains a pointer (`iomem_base`) and does differ.
Replying `ENOSYS` when `flags & FUSE_IOCTL_COMPAT` is the usual approach.

(fixed: `dv_ioctl()` refuses compat ioctls up front. Only `serial_struct` actually differs —
it holds a pointer — while `termios`, `termios2`, `int` and `serial_icounter_struct` are ABI
identical, but sorting out which the caller meant is not worth it for a case no 64-bit-only
setup hits.)

---

## tty-semantics deviations (most likely to be user-visible)

### 12. A blocking read waits for the *full* requested count
`src/con_vsp.c:556` (pend path) + `src/con_vsp.c:345` (completion condition)

`data_in_cb` only replies when `pending_out_processed == pending_out_size`. A real tty with the
default `VMIN=1` returns as soon as >=1 byte is available. So `read(fd, buf, 512)` on a blocking fd —
the common `select()`-then-read pattern — blocks until 512 bytes have arrived rather than returning
the 5 bytes the rig just sent. Apps that use `O_NONBLOCK` or one-byte reads (much of hamlib) dodge
this, which likely explains why it has not bitten yet.

`c_cc` / `VMIN` / `VTIME` are never consulted at all. Replying as soon as
`pending_out_processed > 0` would match `VMIN=1`.

(reproduced by tests/vsp: blocking_read_returns_available)

(fixed: dv_read now honours c_cc[VMIN]/c_cc[VTIME] for all four POSIX cases. A per session
ev_timer serves VTIME, armed on the read for VMIN 0 and restarted per byte for VMIN >0, and
it is stopped in every path that ends a held read (reply, interrupt, release, destroy) so it
cannot fire on a freed session. vsp_create now also seeds c_cc[VMIN]=1 / c_cc[VTIME]=0 the
way the kernel does — without that the zeroed c_cc would have meant VMIN 0, turning every
blocking read into a poll. tests/vsp: blocking_read_returns_available,
vmin0_vtime0_returns_immediately, vtime_timeout_returns_empty, vmin_waits_for_min_bytes,
vmin_vtime_interbyte)

### 13. Non-blocking read/write return 0 instead of `EAGAIN`
`src/con_vsp.c:580`, `src/con_vsp.c:630`

With `O_NONBLOCK` and an empty `buf_out`, `dv_read` replies with a 0-byte buffer — on a character
device that reads as **EOF**, and well-behaved apps close the port. Likewise `dv_write` replies `0`
when `buf_size_avail()` is 0, which makes writers spin. Both should be
`fuse_reply_err(req, EAGAIN)`.

(reproduced by tests/vsp: nonblocking_read_empty_is_eagain, nonblocking_write_full_is_eagain)

(fixed: both reply EAGAIN. Note this applies only to O_NONBLOCK — a 0 byte reply stays
correct for VMIN 0 / VTIME 0 and for a VTIME expiry, so "never return 0" would have been the
wrong generalisation)

### 14. `TIOCMSET` silently does not stick when no PTT line is mapped
`src/con_vsp.c:222`

The `if(!vsp->rts_is_ptt && !vsp->dtr_is_ptt) return 0;` early-out happens *before*
`vsp->mbits = bits`, so on a VSP without RTS/DTR-PTT a subsequent `TIOCMGET` reports stale bits and
`TIOCMBIS`/`TIOCMBIC` compute from a stale mask.

Also `mbits` never reports `TIOCM_CTS|TIOCM_DSR|TIOCM_CAR` as asserted; apps that gate transmit on
CD/CTS will think the line is dead.

Related: if `ch_ptt_channel()` returns -1 while `ptt_rts` is set, `fd_ptt` stays -1 and every
`set_bits` logs a write error.

(partly fixed: `vsp->mbits = bits` now happens before the PTT-mapping early return, so TIOCMGET
and the TIOCMBIS/TIOCMBIC arithmetic are correct on a VSP without a PTT mapping. The `fd_ptt ==
-1` case is now rejected in two places — `conmgr_create_con_cfg()` fails the connector when
`ch_ptt_channel()` returns -1 with PTT requested, and `vsp_create()` refuses a spec that asks
for RTS/DTR PTT without a PTT fd. Failing the connector is deliberate: a VSP whose keying
silently does nothing would be worse than none. tests/vsp:
modem_bits_stick_without_ptt_mapping, verified to fail with the old ordering.)

(fixed: `TIOCMGET` now ORs `TIOCM_INPUT_LINES` (CTS|DSR|CAR) into the reply. They are never
stored in `mbits` — `set_bits()` masks them out, as `uart_tiocmset()` does on a real UART — so
the TIOCMBIS/TIOCMBIC mask stays a pure output-line cache and an application's read-modify-write
cycle cannot smear input bits into it. RNG stays deasserted, an asserted ring indicator would
mean an incoming call. tests/vsp: input_modem_lines_reported)

Reachability note: `ch_ptt_channel()` returns -1 for anything outside R1/R2/their FSK variants,
WINKEY and PTT1/PTT2 — the MCP channel included, so a VSP on MCP with `ptt_rts` reaches the new
guard in normal use, not just in a hand-edited config.

### 15. `fuse_reply_ioctl(req, -1, ...)`
`src/con_vsp.c:732`, `src/con_vsp.c:760`

The `result` field is returned to the caller verbatim, so `-1` surfaces as `EPERM` from
`tcsetattr()`. Use `-EINVAL`.

(fixed: both TCSETS and TCSETS2 branches return `-EINVAL`. tests/vsp:
unsupported_baud_reports_einval, verified to fail with the old `-1` — the caller got
"Operation not permitted" from a bad baud rate.)

---

## Plain bugs in the same file

### 16. PTT is not released on close — wrong byte value
`src/con_vsp.c:486` (`dv_release`) and `src/con_vsp.c:1058` (`vsp_destroy`)

Both places send `uint8_t state = '0';` — the ASCII character `0x30`, not `PTT_OFF_BYTE` (`0x00`).
`mhrouter.c:237` drops it: *"ignoring invalid ptt state 48"*. So closing a VSP (or destroying the
connector) while PTT is asserted via RTS/DTR **leaves the transmitter keyed**. Given what this
daemon does, this is the highest-priority item in the file.

Fix: use `PTT_OFF_BYTE`.

Note: `set_bits` at `src/con_vsp.c:235` uses the constants correctly, so `vs->ptt_status == 1` in
`dv_release` is right; only the byte written is wrong. `vsp_destroy`'s `if(vs->ptt_status)` is fine
too, since the enum values are 0/1.

(fixed: both sites send PTT_OFF_BYTE and gate on ptt_status == PTT_ON_BYTE)

### 17. `size` is clobbered inside the per-session loop
`src/con_vsp.c:328`

```c
if(size > avail) { warn(...); size = avail; }
```

`size` is the outer read length, shared by every session in the scan *and* by the `while(size > 0)`
loop condition. If one session's buffer is full (`avail == 0`), `size` becomes 0: the remaining
sessions get nothing, `vs->sis.rx += size` stops counting, and the drain loop exits early — with
data still queued on the router socket. Use a per-session local.

(reproduced by tests/vsp: two_clients_one_idle — the second client receives nothing at all
once the first one's buffer is full)

(fixed: the clamp uses a per-session local, so a full session no longer robs the sessions
scanned after it nor ends the read loop. Dropped bytes are now counted in
vs->sis.buf_overrun, which TIOCGICOUNT reports, matching how a tty driver counts flip
buffer drops. tests/vsp: two_clients_one_idle, overrun_is_counted)

Follow-up worth considering: draining the router socket and discarding also gives up the
socket buffer as an overflow reservoir. See #21.

### 18. PTT scan reads the wrong bytes
`src/con_vsp.c:404`

```c
for(i = 0; i < (b->size - b->rpos); i++)
    switch(b->data[i])
```

Indexes from `data[0]` instead of `data[b->rpos + i]`, and iterates over everything still buffered
rather than the `size` bytes just written. `ptt_status` ends up derived from already-consumed bytes,
which feeds straight back into the stuck-PTT path in #16.

(fixed: scans [b->rpos, b->rpos + size) and still ignores non-PTT bytes, so a stray byte
cannot strand ptt_status at a value that is neither ON nor OFF)

### 19. Access-mode test
`src/con_vsp.c:323`

`vs->fd_flags & O_WRONLY` works only because `O_RDWR` is 2 and `O_WRONLY` is 1. Correct form is
`(vs->fd_flags & O_ACCMODE) == O_WRONLY`.

(fixed as part of #1: fd_flags is gone, dv_open stores
vs->is_readable = (fi->flags & O_ACCMODE) != O_WRONLY)

### 22. `set_bits()` swallowed a repeated TIOCMSET, leaving PTT dead
`src/con_vsp.c` set_bits

`set_bits()` started with `if(bits == vsp->mbits) return 0;`, an optimisation that assumed
`mbits` is the only thing that ever moves the PTT line. It is not: `dv_release()` and
`vsp_destroy()` send PTT off directly, and `data_out_cb()` tracks PTT bytes written to a
shared data/PTT channel. None of them update `mbits`.

Reachable at the default `maxcon 1`, no second session needed:

1. client asserts RTS -> PTT on, `mbits = TIOCM_RTS`, `vs->ptt_status = PTT_ON_BYTE`
2. client exits without dropping RTS -> `dv_release()` sends PTT off, `mbits` still says RTS
3. client restarts and asserts RTS -> `bits == mbits`, early return, **no byte sent**

The rig never keys again for that VSP until something else changes `mbits`. An application
that keys PTT and exits without clearing RTS reproduces it on its next start.

(fixed: the early return is gone. Writing the byte on every TIOCMSET/TIOCMBIS/TIOCMBIC keeps
`vs->ptt_status` and the rig in step whatever else touched the line; a redundant byte only
re-pushes an identical flags byte to the keyer (`mhrouter.c:265-278`), so the cost is
negligible. tests/vsp: ptt_rekeys_after_reopen, verified to fail with the early return
restored)

Follow-up, same root cause: `dv_release()` dropped PTT but left `mbits` claiming RTS/DTR, so
the next client's `TIOCMGET` was told the line was still asserted. A real UART lowers DTR/RTS
on last close when `HUPCL` is set, which is the kernel default (`tty_port_shutdown()` calls
`tty_port_lower_dtr_rts()` under `C_HUPCL`); mhuxd's `c_cflag` default does not even include
`HUPCL` (`src/con_vsp.c:1136`).

(fixed: `dv_release()` clears RTS/DTR from `mbits` when `open_cnt` reaches 0. Deliberately not
full HUPCL fidelity — PTT is dropped on last close whether or not `HUPCL` is set, because an
application clearing it and exiting with the transmitter keyed is not worth being faithful
about. If `HUPCL` is ever exposed in the default `c_cflag` it has to be honoured for
everything *except* the PTT drop, otherwise `tcgetattr` advertises a flag that does nothing.
tests/vsp: modem_bits_clear_after_last_close, verified to fail without the clear)

Keeping `mbits` per VSP rather than per session is right: on a real UART the modem control
lines belong to the device, not to each open file description, and TIOCMGET reports the
physical state to everyone. Any OR-aggregation needed for `maxcon > 1` on a PTT port (see
#20) is an *additional* per session layer of PTT requests, not a replacement for `mbits`.

### 21. Overflow handling gives up the socket buffer as a reservoir
`src/con_vsp.c` data_in_cb

Fixing #17 means `data_in_cb` now keeps draining the router socket and discards whatever
does not fit in a session's 512 byte `buf_out`. Before the fix a full buffer ended the read
loop, so the surplus stayed in the socketpair receive buffer (default ~208 kB) and was
delivered once the client caught up — accidental, but effective, extra buffering.

Net effect for a single client that stalls: at 19200 baud (~1.9 kB/s) a one second stall
used to be absorbed almost entirely by the socket buffer; now everything past the first
512 bytes is dropped. The starvation bug had to go, but this part of the old behaviour was
worth keeping.

Principled version: only read from the router while at least one readable session has
space. When every readable session is full, stop `w_data_in` and restart it when space
frees up (`dv_read`, the `TCFLSH` input flush, `dv_release`). No session is starved,
because reading only stops when nobody can take data, and the socket buffer goes back to
being the overflow reservoir. Costs a `watch_start()` in the places that free space.

Measured with `tests/vsp/ramp.py --direction in --reader-delay 50` (an application that
stalls 50 ms between reads, which a GUI repaint or a synchronous disk write easily does):
57600 baud is clean, 115200 loses ~10% and 230400 loses ~54%. A D second stall at B bytes/s
needs D*B bytes of buffer, so 115200 baud needs 576 bytes and 230400 needs 1152 — against
the current 512.

Cheaper alternative: give `vsp_session.buf_out` its own larger buffer. Note
`BUFFER_CAPACITY` is shared by 39 embedded `struct buffer` members, including
`channel_buf_out[ALL_NUM_CHANNELS]` in mhrouter, so raising the constant multiplies memory
across the daemon — the VSP needs its own buffer, not a bigger global one.

### 20. `data_out_cb` writes every session's `buf_in` to the same `fd_data`
`src/con_vsp.c:381`

With `maxcon > 1` two clients' byte streams interleave arbitrarily on the router socket. Probably
inherent to the design, but worth being explicit about; combined with #1 it is another reason
`maxcon > 1` is not really usable today.
