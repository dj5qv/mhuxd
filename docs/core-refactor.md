# Core Refactor Proposal (Robust I/O + FD Ownership)

## Why this refactor

The current connector stack works, but robustness depends on each connector implementing subtle rules correctly:

- who owns and closes each fd
- when libev watchers are started/stopped
- how nonblocking read/write errors are interpreted
- how teardown is coordinated across `con_*`, `conmgr`, and `mhrouter`

This has already produced failure modes that are hard to reason about (invalid fd in watcher updates, double-close risks, delayed asserts after error paths).

## Goals

1. Keep current behavior and protocol semantics.
2. Reduce crash surface under induced/fault conditions.
3. Make ownership and teardown rules explicit and enforceable.
4. Remove duplicate callback logic for nonblocking I/O.
5. Enable safer future connectors with less custom boilerplate.

## Core design changes

### 1) Explicit endpoint state machine

Introduce a shared state model used by all connectors/endpoints:

- `OPEN`: normal I/O
- `DRAINING`: no new input, pending output flush
- `FAILED`: fatal I/O or internal error, teardown pending/ongoing
- `CLOSED`: fully torn down

Rules:

- Every callback checks state first.
- No watcher re-arm when state is `FAILED` or `CLOSED`.
- Transitions are one-way and idempotent.

### 2) Single fd ownership contract

For each fd, exactly one module is the closer.

- Owner is defined at fd creation/handoff time.
- Non-owners may signal close intent, but never call `close()` directly.
- Close helpers are idempotent and safe if called multiple times.

Suggested convention:

- Connector-local fds: owned by that connector session object.
- Router side fds registered in router tables: owned by router endpoint lifecycle.
- Shared-pair teardown must go through one authoritative function.

#### CR-01 execution plan (fd ownership definition)

Ownership rule:

- fd creator is owner by default.
- ownership can be transferred exactly once per handoff edge.
- only current owner may call `close()`.
- non-owners may request teardown, never close directly.

Minimum ownership matrix for current code:

- `con_tcp` listener fd: owned by `con_tcp` connector object.
- `con_tcp` accepted client fd: owned by `ctcp_session`.
- connector side of `socketpair` (`fd_data` in connector): owned by connector instance (`con_tcp` / `con_vsp`).
- router side of `socketpair`: ownership transfers to `mhrouter` after `mhr_add_*` succeeds.
- vsp local device fds (`fd_data`, `fd_ptt`): owned by `con_vsp` instance/session.

Teardown ordering contract (must be uniform):

1. mark state as terminal (`FAILED` or `CLOSED`)
2. stop all watchers bound to that endpoint
3. deregister endpoint from router/lists
4. close only owned fds and set fd to `-1`
5. free memory

Implementation notes:

- add tiny ownership metadata per fd handle (owner enum + tag string for debug logs)
- make close helper idempotent (`fd < 0` is no-op)
- log and ignore close attempts by non-owner in debug mode
- enforce "watcher stop before close" in common teardown helper

Immediate CR-01 rollout steps:

1. annotate ownership in structs and constructor paths (`conmgr`, `mhrouter`, `con_tcp`, `con_vsp`)
2. replace raw `close()` in these paths with ownership helper
3. add one debug assertion/log site per close path to catch violations early
4. verify with forced-error scenario and normal startup/shutdown

### 3) Shared nonblocking I/O helpers

Create reusable helpers for read/write result handling:

- `io_read_result(...)`
- `io_write_result(...)`

Normalize semantics once:

- `> 0`: progress
- `== 0`: peer closed / no-progress policy (explicit per callsite)
- `< 0 && errno in {EAGAIN,EWOULDBLOCK}`: retry later
- `< 0` otherwise: hard error

This removes per-callback ad-hoc conditionals that currently diverge.

### 4) Centralized watcher lifecycle wrappers

Wrap libev start/stop operations with guard helpers:

- start only if `fd >= 0`, state allows I/O, and not already active
- stop is always safe and idempotent
- optional debug counters/logging for invariant checks

### 5) Unified teardown path

Each session/endpoint gets one teardown entrypoint:

- marks state as `FAILED` or `CLOSED`
- stops watchers
- closes owned fds
- unlinks from lists/router tables
- frees memory

No partial teardown scattered across callbacks.

## Incremental migration plan

### Phase 0: Hardening baseline (done/in progress)

- Keep existing architecture.
- Fix unsafe negative-length consume paths.
- Fix `EAGAIN` handling in all connector callbacks.
- Remove duplicate closes across `conmgr`/router boundaries.

### Phase 1: Introduce shared primitives (no behavior change)

- Add small internal module (e.g. `src/io_state.[ch]`):
  - state enum + transition helpers
  - fd owner/close wrappers
  - nonblocking I/O result helpers
  - watcher guard wrappers
- Convert one connector first (`con_tcp`) to validate API shape.

### Phase 2: Apply to VSP and router edges

- Migrate `con_vsp` callbacks to state-gated re-arm behavior.
- Replace duplicated close/stop logic with common teardown helper.
- Align router producer/consumer endpoint teardown to ownership wrapper.

### Phase 3: Conmgr integration cleanup

- Restrict `conmgr` to orchestration, not fd-closing of router-owned ends.
- Make handoff/ownership explicit in constructor interfaces.

### Phase 4: Invariants and diagnostics

Add cheap runtime checks (debug builds or log-level gated):

- watcher active on invalid fd
- illegal state transition
- close called by non-owner
- callback executed after terminal state

## Suggested acceptance criteria

1. Forced error tests no longer trigger libev invalid-fd assertions.
2. Teardown is idempotent across repeated callbacks/events.
3. Each fd has exactly one close site by design.
4. Connector callback code size shrinks (less duplicated edge-case code).
5. Existing integration behavior remains unchanged in normal operation.

## Risks and mitigations

- **Risk:** behavior regressions during migration.
  - **Mitigation:** migrate one connector at a time; compile and fault-test per step.
- **Risk:** over-abstraction in C can reduce clarity.
  - **Mitigation:** keep helpers tiny and mechanical; no framework-like layer.
- **Risk:** hidden ownership assumptions in old code.
  - **Mitigation:** document ownership in structs and constructor comments.

## Practical first implementation target

Start with `con_tcp` as the template:

- smallest surface area
- already revealed concrete nonblocking/I/O safety issues
- good proving ground for helper signatures before `con_vsp`

Once stable, apply same pattern to `con_vsp`, then router + conmgr boundaries.

## CR-11 Fault-Injection Validation Checklist

Use this checklist after each refactor increment that touches fd/watcher lifecycle.

### Preconditions

- Build clean: `make -C build -j4`
- Run with debug logs enabled (at least `dbg0`)
- Keep one baseline config with:
  - at least one VSP connector
  - at least one TCP connector
  - optional PTT-mapped VSP (`fd_ptt` path)

### Scenario A: VSP forced CUSE read failure

Goal: verify terminal-state transition + no watcher rearm + no invalid-fd asserts.

Steps:

1. Start `mhuxd` under valgrind (fd tracking enabled).
2. Trigger the known CUSE error path (small buffer/fault injection as used in prior repro).
  - Deterministic option: run with `MHUXD_VSP_FORCE_CUSE_FAIL_ONCE=1` to force one CUSE read failure per VSP connector.
3. Continue runtime for ~30s after error.

Expected:

- single transition to `FAILED` (debug log if repeated attempts)
- connector shutdown logs once per connector
- no `epoll_modify ... invalid fd`
- no "fd already closed" from valgrind

### Scenario B: TCP remote close + error churn

Goal: verify nonblocking helper behavior and teardown idempotency under repeated close events.

Steps:

1. Connect one or more TCP clients.
2. Force abrupt remote disconnects (multiple times).
3. Repeat while traffic is active.

Expected:

- clean client session teardown
- no double-close warnings
- no stale watcher activity on closed fds

### Scenario C: Full destroy path

Goal: verify orderly shutdown with mixed connector states.

Steps:

1. Start with VSP + TCP active.
2. Force one connector into error state.
3. Trigger process shutdown (`conmgr_destroy_all`).

Expected:

- no duplicate close on aliased fds (`fd_ptt == fd_data` case)
- all connectors report closed once
- no valgrind fd double-close diagnostics

### Scenario D: Repeated create/destroy cycle

Goal: catch latent ownership transfer regressions.

Steps:

1. Start/stop `mhuxd` in loop with same config (10+ cycles).
2. Include one cycle with induced VSP or TCP fault.

Expected:

- stable startup and shutdown each cycle
- no accumulation of warnings/asserts
- no new fd lifecycle regressions

### Quick pass/fail criteria

Pass if all are true:

- no libev invalid-fd assert
- no valgrind "already closed" fd errors
- no ownership-violation logs except intentional injected cases
- terminal-state callbacks do not restart I/O watchers

Fail if any are true:

- repeated close of same underlying fd
- illegal state transitions without guarded handling
- callback activity continues after `FAILED`/`CLOSED`


### Implementation todos

CR-01) Define fd ownership contract in code comments and docs:
       - each fd has exactly one closer
       - non-owners can request teardown but never close directly

CR-02) Add shared endpoint state enum + helpers (`OPEN`, `DRAINING`, `FAILED`, `CLOSED`).

CR-03) Add small nonblocking I/O helpers (read/write result normalization):
       - `>0` progress
       - `==0` EOF/no-progress policy
       - `EAGAIN`/`EWOULDBLOCK` retry
       - other `<0` hard error

CR-04) Add watcher lifecycle wrappers:
       - guarded `start` (valid fd + allowed state)
       - idempotent `stop`

CR-05) Convert `con_tcp.c` to shared helpers first (no behavior change intended).

CR-06) Unify `con_tcp` teardown into one idempotent close path.

CR-07) Convert `con_vsp.c` to state-gated watcher re-arm logic and shared teardown helpers.

CR-08) Align `mhrouter.c` endpoint teardown to ownership wrappers only.

CR-09) Restrict `conmgr.c` to orchestration only (no closing router-owned fds).

CR-10) Add debug/invariant checks (gated by loglevel/debug build):
       - watcher on invalid fd
       - invalid state transition
       - close by non-owner
       - callback after terminal state

CR-11) Fault-injection validation pass:
       - forced connector read/write errors
       - repeated teardown calls
       - verify no libev invalid-fd assert

CR-12) Final cleanup:
       - remove obsolete duplicate error handling paths
       - document final architecture in `docs/core-refactor.md`

### CR-11 execution log template

Date:
Build:
Config:
Scope:

Scenario A (VSP forced CUSE read failure): PASS/FAIL
- Notes:

Scenario B (TCP remote close churn): PASS/FAIL
- Notes:

Scenario C (full destroy path): PASS/FAIL
- Notes:

Scenario D (repeat create/destroy cycles): PASS/FAIL
- Notes:

Regressions observed:
Follow-up actions:

### CR-11 latest run (example)

Date: 2026-02-17
Build: make -C build -j4 (PASS)
Config: baseline (VSP + TCP + PTT mapped)
Scope: CR-01..CR-04 in-progress validation

Scenario A (VSP forced CUSE read failure): PASS
- Notes: connector entered terminal state, no watcher rearm observed.

Scenario B (TCP remote close churn): PASS
- Notes: repeated connect/disconnect stable; no stale fd activity.

Scenario C (full destroy path): PASS
- Notes: no duplicate close for aliased VSP fds (`fd_ptt == fd_data`).

Scenario D (repeat create/destroy cycles): PASS
- Notes: 10 cycles, no invalid-fd assert.

Regressions observed:
- none

Follow-up actions:
- continue CR-02/CR-04 rollout to remaining callback edges
- keep running CR-11 after each lifecycle-related patch

### CR-11 latest run (2026-02-17, CR-07 validation)

Date: 2026-02-17
Build: make -C build -j4 (PASS)
Config: /usr/local/mhuxd/var/lib/mhuxd-state.json (demo run `-2`, connectors mapped to `M3_DEMO_MK3_1`)
Scope: CR-07 con_vsp watcher/state/teardown refactor validation

Scenario A (VSP forced CUSE read failure): NOT EXECUTED
- Notes: VSP path was exercised (`cat1` open/TCSETS/release), but no explicit CUSE fault was injected in this run.

Scenario B (TCP remote close churn): NOT EXECUTED
- Notes: TCP connector created; churn sequence not driven in this run.

Scenario C (full destroy path): PASS
- Notes: valgrind run + SIGTERM shutdown showed single close sequence for `cat1`, `wk`, `ptt1`, `fsk1`, and TCP connector; no `epoll_modify ... invalid fd` and no valgrind "already closed" fd diagnostics observed.

Scenario D (repeat create/destroy cycles): NOT EXECUTED
- Notes: single create/destroy cycle only.

Regressions observed:
- none in CR-07 shutdown path under this run.

Follow-up actions:
- Add a dedicated fault-injection toggle for Scenario A (forced CUSE read failure) and rerun under valgrind.
- Run Scenario B churn and Scenario D multi-cycle pass with the same config.

### CR-11 latest run (2026-02-17, deterministic Scenario A)

Date: 2026-02-17
Build: make -C build -j4 (PASS)
Config: /usr/local/mhuxd/var/lib/mhuxd-state.json + env `MHUXD_VSP_FORCE_CUSE_FAIL_ONCE=1`
Scope: CR-07 validation with explicit VSP CUSE-read fault injection

Scenario A (VSP forced CUSE read failure): PASS
- Notes: deterministic injection fired on `cat1`, `wk`, `ptt1`, `fsk1`; connectors entered terminal/fail path; no `epoll_modify ... invalid fd` observed.

Scenario B (TCP remote close churn): NOT EXECUTED
- Notes: TCP connector created, but churn sequence not exercised in this run.

Scenario C (full destroy path): PASS
- Notes: SIGTERM shutdown completed with connector close logs; valgrind showed no fd double-close diagnostics.

Scenario D (repeat create/destroy cycles): NOT EXECUTED
- Notes: single cycle only.

Regressions observed:
- none in fd/watcher safety paths; router emitted `mhr_rem_* fd not found` warnings during forced-fail teardown sequence.

Follow-up actions:
- Run Scenario B and Scenario D next.
- Triage whether `mhr_rem_* fd not found` should be debug-level in forced-failure paths.

### CR-11 latest run (2026-02-18, Scenario B + D)

Date: 2026-02-18
Build: make -C build -j4 (PASS, from prior step)
Config: /usr/local/mhuxd/var/lib/mhuxd-state.json (demo `-2`)
Scope: Execute remaining Scenario B and Scenario D checks after deterministic Scenario A/C pass

Scenario A (VSP forced CUSE read failure): PASS (covered in previous run)
- Notes: deterministic env-toggle run already validated.

Scenario B (TCP remote close churn): PASS
- Notes: valgrind run on `localhost:5156` with repeated `nc` connect/disconnect to `127.0.0.1:6000` showed stable incoming/closed pairs and clean shutdown.

Scenario C (full destroy path): PASS
- Notes: present in this run as SIGTERM shutdown path; connectors closed once, no invalid-fd symptoms observed.

Scenario D (repeat create/destroy cycles): PASS
- Notes: 10/10 cycles completed (`CYCLE_1_DONE`..`CYCLE_10_DONE`) across unique web ports (`5161`..`5170`), including one injected-fault cycle (`i=5`).

Regressions observed:
- no `invalid fd` / `already closed` lines observed in Scenario B/D logs.
- `mhr_rem_* fd not found` appeared in forced-failure teardown (8 lines in Scenario D log), consistent with earlier observation.

Follow-up actions:
- Optional: downgrade forced-failure `mhr_rem_* fd not found` to debug-level if we treat it as expected during already-removed endpoints.
- Continue CR-10 invariant tightening (especially callback-after-terminal and router teardown diagnostics).
