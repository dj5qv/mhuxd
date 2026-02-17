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
