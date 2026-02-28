

I've analyzed the current callback-heavy design and the research reveals that you're right — we are indeed heading toward a "callback hell" because restapi.c is becoming a massive "glue" layer that manually wires up every single low-level event to high-level JSON.

The current design has several issues:
1.  **Tight Coupling**: restapi.c must know about the internals of `devmgr`, `conmgr`, and even individual device control structures (`mhc`).
2.  **Order of Destruction**: The manual registration/unregistration in restapi.c's `*_create` and `*_destroy` is becoming fragile.
3.  **Dynamic Registration**: When a new device is hot-plugged, restapi.c must manually attach new callbacks to that specific device, which is a common source of memory leaks or double-frees during teardown.

### Proposal: A Minimalist Internal Event Bus

Since `mhuxd` is a single-threaded `libev` application, we don't need a heavy message broker (like RabbitMQ/DBus). Instead, we can implement a **Central Event Bus** inside a unified **App Context**. This fits perfectly with the existing `PGList` infrastructure.

## Plan: Central Event Bus & App Context

This plan introduces a central `struct app_ctx` to manage global state and a `struct event_bus` to decouple event producers (drivers/managers) from consumers (REST API/UI).

**Steps**

1.  **Define the App Context and Event Bus**
    - Create a new header `src/app_ctx.h` and source `src/app_ctx.c`.
    - Define `struct app_ctx` containing pointers to `ev_loop`, `conmgr`, `devmgr`, and `cfgmgrj`.
    - Define a `struct event_bus` within `app_ctx` using `PGList` to manage subscribers.

2.  **Implement the Event Bus API**
    - Create an `app_publish(ctx, event_type, data)` function.
    - Create an `app_subscribe(ctx, event_type, callback, user_data)` function.
    - Standardize `event_type` constants (e.g., `EV_DEV_STATUS`, `EV_CON_STATUS`, `EV_KEYER_MODE`).

3.  **Refactor Producers (Managers/Drivers)**
    - Update conmgr.c, `devmgr.c`, and `mhcontrol.c` to accept a pointer to `app_ctx` instead of individual dependencies.
    - Replace the custom callback lists in conmgr.c and `mhcontrol.c` with a simple call to `app_publish`.
    - Example: Instead of `conmgr_notify_status_changed` iterating its own list, it calls `app_publish(ctx, EV_CON_STATUS, &status)`.

4.  **Refactor the Consumer (REST API)**
    - In restapi.c, replace the multiple `*_add_cb` calls in `restapi_create` with a single set of subscriptions to the central bus.
    - restapi.c no longer needs to track individual device connections to attach callbacks; it just listens for `EV_DEV_STATUS` globally.

5.  **Simplify Lifecycle in main.c**
    - Update main.c to initialize the `app_ctx` first, then pass it to all modules.
    - The shutdown sequence becomes a simple `app_ctx_destroy`, which can safely unregister all bus subscribers in a single pass.

**Verification**
- **Compile-time**: Ensure all circular dependencies are removed (modules only depend on `app_ctx`).
- **Runtime**: Verify that SSE events are still triggered correctly when a connector fails or a keyer mode changes.
- **Leak Check**: Run with `valgrind` to ensure the new subscription list is cleared correctly on exit.

**Decisions**
- **Single Context**: Chose a single `app_ctx` over multiple "manager pointers" to provide a consistent service-locator pattern.
- **Implicit Subscriptions**: Chose a global event bus over per-object observers to avoid the "re-register on connect" logic currently polluting restapi.c.