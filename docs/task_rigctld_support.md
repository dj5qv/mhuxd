### Support rigctld to sync keyer mode with radio mode

## Reason:
- Currently mhuxd does not maintain the keyer mode (CW/DIGITAL/VOICE) to follow 
  the radios current mode.
- This can be worked around by enabling r1FollowTxMode (Keyer Mode Follows RIG in UI)
  and enabling autonomous CAT queries by the keyer by enabling usedecoderifconnected 
  ((Use Decoder if Connected in UI) in the CAT channel configuration.
  Problem with that is that these queries can interfere with CAT queries coming from
  computer programs. That is even when dontinterfereusbcontrol is enabled.

## Solution:

- Similar to the original microHam router, mhuxd should be able to determine the radio
  mode and set the keyer mode accordingly, whenever the radio mode changes.
- We don't want implement CAT protocols in mhuxd, that would be challenging to do and
  maintain.

# hamlib rigctld
- rigctld runs as a daemon and allows several programs simultaneously run CAT requests.
- We can utilize rigctld (if available) to poll the radio for it's current mode.
- state mapping (keyer -- rigctld):

| Keyer mode | rigctld mode values |
|------------|-----------------------|
| CW         | CW, CWR |
| VOICE      | USB, LSB, AM, FM, WFM, AMS |
| FSK        | RTTY, RTTYR |
| DIGITAL    | PKTUSB, PKTLSB, PKTFM, PKTAM, FAX |


## JSON integration proposal

Store this per device (not per connector), because this behavior belongs to a keyer/radio pair.

Suggested shape under devices[]:

```json
{
  "serial": "12345678",
  "type": 31,
  "rig_mode_sync": {
    "backend": "rigctld",
    "enabled": true,
    "radios": {
      "r1": {
        "enabled": true,
        "host": "127.0.0.1",
        "port": 4532,
        "connect_timeout_ms": 1500,
        "io_timeout_ms": 1500,
        "poll_ms": 500
      },
      "r2": {
        "enabled": false,
        "host": "127.0.0.1",
        "port": 4533,
        "connect_timeout_ms": 1500,
        "io_timeout_ms": 1500,
        "poll_ms": 500
      }
    }
  }
}
```

Notes:
- Missing rig_mode_sync means feature disabled (backward compatible).
- Single-radio keyers only use radios.r1.
- Two-radio keyers may use both r1 and r2.


## Defaults and validation

Defaults:
- backend: rigctld (enum, future backends possible)
- enabled: false
- radios.r1.enabled: false
- radios.r1.host: 127.0.0.1
- radios.r1.port: 4532
- radios.r1.connect_timeout_ms: 1500
- radios.r1.io_timeout_ms: 1500
- radios.r1.poll_ms: 500
- radios.r2.* same defaults except port 4533

Validation:
- backend: enum, currently allowed: rigctld
- host: non-empty string
- port: integer 1..65535
- connect_timeout_ms: integer >= 100
- io_timeout_ms: integer >= 100
- poll_ms: integer >= 100
- if device does not support 2 radios, ignore or reject radios.r2 with warning


## Runtime behavior

- Lifecycle: Polling starts once keyer becomes ONLINE via mhc_add_keyer_state_changed_cb.
- Polling stops when keyer goes non-ONLINE or connector/device is destroyed.
- On mode change:
  - single-radio: call mhc_set_mode
  - dual-radio: call mhc_set_mode_on_radio with r1/r2
- De-duplicate writes: do not send if mapped keyer mode has not changed.
- On rigctld error/timeouts: keep running with retry, log at debug/info with rate limiting.
- Connection establishment uses connect_timeout_ms; read/write operations use io_timeout_ms; polling cadence uses poll_ms.


## Schema/API touch points

1) docs/rest-api-schemas.json
- Extend ConfigDevices entries with optional rig_mode_sync object.

2) src/cfgmgrj.c
- apply_device_from_json: parse and validate rig_mode_sync.
- build_config_json: emit rig_mode_sync for persistence.

3) UI
- Add fields on keyer/radio page for enabled/host/port/poll_ms per radio.


## TODOs rigctld integration (implementation order)

1) Extend JSON config and schema with rig_mode_sync (device-level).
2) Extend UI for rigctld settings on keyer/radio page.
3) Add support for two radios (r1 and r2 config blocks).
4) Create module rigctld_client.c (nonblocking socket + libev timer based polling).
5) Wire lifecycle to keyer ONLINE/OFFLINE state callbacks.
6) Apply mode mapping and call mhc_set_mode / mhc_set_mode_on_radio accordingly.
7) Add logging, retry/backoff, and minimal acceptance tests.

## Open decisions (small)

- For unsupported/unknown rigctld modes, keep previous keyer mode and log once per change.
- Backend remains enum to keep room for future endpoints (e.g. flrig/XML-RPC).

   