/**
 * Svelte stores for shared application state.
 *
 * These stores are the single source of truth for data that multiple
 * components need.  They are updated by API calls and WebSocket events.
 */
import { writable, derived } from 'svelte/store';

// ── Core data from the REST API ──────────────────────────────────────

/** Runtime info (daemon version, uptime, hostname). */
export const runtime = writable(null);

/** Daemon configuration (loglevel, …). */
export const daemonCfg = writable(null);

/** Live device list from /api/v1/devices (with runtime status). */
export const devices = writable([]);

/** Persisted device configs from /api/v1/config/devices. */
export const configDevices = writable([]);

/** Connector list from /api/v1/config/devices (connectors array). */
export const connectors = writable([]);

/** Metadata (rigtypes, devicetypes, displayoptions). */
export const metadata = writable(null);

// ── Connection state ─────────────────────────────────────────────────

/** Whether the UI is connected to the daemon (WS open + initial load OK). */
export const connectedToDaemon = writable(true);

/** General loading flag for the initial data fetch. */
export const loading = writable(true);

/** Top-level error message (shown in the error banner). */
export const error = writable('');

// ── Derived stores ───────────────────────────────────────────────────

/** Helper: available rig types from metadata. */
export const rigtypes = derived(metadata, ($m) => $m?.rigtypes || []);

/** Helper: device type info from metadata. */
export const devicetypes = derived(metadata, ($m) => $m?.devicetypes || []);
