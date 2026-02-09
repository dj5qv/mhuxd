# JSON Cache Plan for cfgmgrj/restapi

Just a plan layed out. Probably not worth the complexity.

Goal: avoid rebuilding the full config JSON for every request. Build once, reuse for save/response, and cache across requests. Also return per-device objects (or error JSON) in `cb_config_device()`.

## Summary
- Cache a built config JSON in `cfgmgrj` and return it using `json_incref()`.
- Add a `cfgmgrj_save_cfg_from_json()` API to save a prebuilt JSON without rebuilding.
- Provide `cfgmgrj_get_device_json()` to return only the requested device as JSON (or an error object).
- Update REST handlers to use the cache and per-device accessor.

## Steps

### 1) Add cache to `cfgmgrj`
- File: [src/cfgmgrj.c](src/cfgmgrj.c) and [src/cfgmgrj.h](src/cfgmgrj.h)
- Add to `struct cfgmgrj`:
  - `json_t *config_cache;`
  - `int config_cache_valid;`
- Update `cfgmgrj_build_json()`:
  - If `config_cache_valid` and `config_cache` set: return `json_incref(config_cache)`.
  - Else build with `build_config_json()`, store as cache, mark valid, return `json_incref(cache)`.
- Ensure `config_cache` is released in `cfgmgrj_destroy()`.

### 2) Invalidate cache on any mutation
- File: [src/cfgmgrj.c](src/cfgmgrj.c)
- Invalidate on:
  - `cfgmgrj_apply_json()` (after successful `apply_config_json()`)
  - `cfgmgrj_load_cfg()` (after load)
  - `cfgmgrj_save_cfg()` (after save)
- Invalidation steps:
  - If cache exists, `json_decref(config_cache)` and set to NULL.
  - Set `config_cache_valid = 0`.

### 3) Save from prebuilt JSON
- File: [src/cfgmgrj.c](src/cfgmgrj.c), [src/cfgmgrj.h](src/cfgmgrj.h)
- Add API:
  - `int cfgmgrj_save_cfg_from_json(struct cfgmgrj *cfgmgrj, json_t *root);`
- Implementation:
  - Expect `root` to be a valid config JSON.
  - `json_dump_file(root, CFGFILE, JSON_INDENT(2) | JSON_PRESERVE_ORDER)`.
  - Return 0 on success, -1 on error.
- Keep `cfgmgrj_save_cfg()` as wrapper:
  - Use `cfgmgrj_build_json()` to get cached or built JSON, then call `cfgmgrj_save_cfg_from_json()`.

### 4) Per-device JSON accessor with error object
- File: [src/cfgmgrj.c](src/cfgmgrj.c), [src/cfgmgrj.h](src/cfgmgrj.h)
- Add:
  - `json_t *cfgmgrj_get_device_json(struct cfgmgrj *cfgmgrj, const char *serial);`
- Behavior:
  - Uses cached root (`cfgmgrj_build_json()`), finds device in `devices` array.
  - If found, return `json_incref(device)`.
  - If missing, return a JSON error object, e.g.:
    - `{"error":"not_found","serial":"<serial>"}`
  - Caller owns the returned reference.

### 5) Update `restapi` handlers to reuse cache
- File: [src/restapi.c](src/restapi.c)
- `cb_config_devices()`:
  - GET: no change in behavior; will use cached `cfgmgrj_build_json()`.
  - POST/PATCH/PUT:
    - After `cfgmgrj_apply_json()`, build once with `cfgmgrj_build_json()` and call `cfgmgrj_save_cfg_from_json()`.
    - Use the same built root for the response (devices/connectors), no second build.
- `cb_config_device()`:
  - GET: replace full build + deep copy with `cfgmgrj_get_device_json()`.
  - POST/PATCH/PUT: after apply/save, respond with `cfgmgrj_get_device_json()`.
  - If error object returned, respond with 404 and the error JSON.

## Error JSON shape
Suggested standard payload for missing device:
- `{"error":"not_found","serial":"<serial>"}`

## Notes
- `json_incref()` is safe for returning cached objects as long as callers `json_decref()`.
- The cache assumes all config mutations originate from REST/JSON (per current system behavior).
- This approach removes duplicate `build_config_json()` calls in both `cb_config_devices()` and `cb_config_device()` write paths.
