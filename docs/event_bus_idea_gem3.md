
## Plan: Event Bus Structs & Architecture

Based on the codebase analysis, we have several distinct categories of callbacks. To avoid "callback hell" while maintaining performance, we will introduce a hybrid approach: **State/Lifecycle Events** will move to the global Event Bus, while **Data/Command Paths** will use a structured pipeline or direct callbacks.

### 1. State & Lifecycle Events (Moving to Event Bus)

These are low-frequency, broadcast-style events that multiple modules (like `restapi`, `logger`, or future orchestrators) care about.

**Required Event Types & Structs (`src/events.h`)**

```c
// Event Types
enum app_event_type {
    EV_DEV_HOTPLUG,       // OS-level connect/disconnect (udev)
    EV_DEV_ADDED,         // Device fully initialized by devmgr
    EV_DEV_REMOVED,       // Device removed from devmgr
    EV_ROUTER_STATUS,     // Router connection state changed
    EV_KEYER_STATE,       // Keyer lifecycle (ONLINE, OFFLINE, etc.)
    EV_KEYER_MODE,        // Radio mode changed (CW, VOICE, etc.)
    EV_KEYER_MOK_STATE,   // microKEYER internal state changed
    EV_KEYER_ACC_STATE,   // Accessory state changed
    EV_CON_STATUS         // Connector (TCP/VSP) status changed
};

// Event Structs
struct ev_dev_hotplug {
    const char *serial;
    int status; // DEVMON_CONNECTED / DEVMON_DISCONNECTED
};

struct ev_dev_lifecycle {
    struct device *dev; // The fully initialized device
};

struct ev_router_status {
    struct mh_router *router;
    int status; // MHROUTER_CONNECTED / MHROUTER_DISCONNECTED
};

struct ev_keyer_state {
    const char *serial;
    int state; // mhc_keyer_state
};

struct ev_keyer_mode {
    const char *serial;
    uint8_t mode_cur;
    uint8_t mode_r1;
    uint8_t mode_r2;
};

struct ev_keyer_hw_state {
    const char *serial;
    const uint8_t *state; // Payload
    uint8_t state_len;
};

struct ev_con_status {
    int connector_id;
    int status; // e.g., CON_CONNECTED, CON_ERROR
};
```

### 2. Data Path Pipeline (Filters)

Since you plan to introduce filters that can modify, drop, or inject data between connectors and `mhrouter`, a global Event Bus is the wrong tool. It is designed for 1-to-many broadcasting, not sequential mutation.

Instead, we will formalize the `mhr_add_consumer_cb` into a **Filter Pipeline** pattern.

**Design:**
- Keep the direct callback approach for raw data, but wrap it in a `struct data_filter` chain.
- A connector pushes data to the first filter in the chain.
- Each filter processes the data and calls the next filter's callback.
- The final "filter" in the chain is the `mhrouter` consumer.

### 3. Command Completions (Direct Callbacks)

As agreed, asynchronous commands (e.g., `mhc_set_speed`, `wkm_read_cfg`) are 1:1 request/response patterns. They will remain as direct callbacks (`mhc_cmd_completion_cb_fn`). Publishing these to a global bus would require complex request ID matching and state tracking in the caller.

**Steps**

1.  **Create `src/events.h`**: Define the `enum app_event_type` and the structs listed above.
2.  **Implement Event Bus**: Create `src/eventbus.c/h` with `eventbus_publish` and `eventbus_subscribe`.
3.  **Refactor State Callbacks**: 
    - Remove `dmgr_add_on_device_connect_cb`, `mhc_add_keyer_state_changed_cb`, `mhc_add_mode_changed_cb`, etc.
    - Replace their internal invocations with `eventbus_publish(bus, EV_..., &struct)`.
4.  **Update Consumers**: Update restapi.c to subscribe to the Event Bus instead of registering individual callbacks on every device.
5.  **Design Filter Interface (Future)**: Draft a `struct data_filter` interface for the high-frequency data path, keeping it separate from the Event Bus.

**Verification**
- Ensure `restapi` still correctly broadcasts SSE events for mode changes and device connections after the refactor.
- Verify that high-frequency data (CW/FSK payloads) does not traverse the Event Bus, ensuring performance is maintained.