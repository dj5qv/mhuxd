/**
 * WebSocket connection manager for mhuxd.
 *
 * Provides a single shared connection to /api/v1/ws with:
 *  - Auto-reconnect on close (configurable delay)
 *  - Event dispatch by message "type" field
 *  - Svelte-store-based connection status
 */
import { writable } from 'svelte/store';

/** Whether the WebSocket connection is currently open (optimistic default). */
export const wsConnected = writable(true);

/** Countdown seconds until next reconnect attempt. */
export const wsRetrySeconds = writable(0);

let socket = null;
let reconnectTimer = null;
const listeners = new Map();       // type -> Set<handler>
let onOpenCallback = null;         // optional: called on (re)connect

function wsUrl() {
  const loc = window.location;
  const proto = loc.protocol === 'https:' ? 'wss:' : 'ws:';
  return `${proto}//${loc.host}/api/v1/ws`;
}

function scheduleReconnect() {
  const delay = 5;
  let seconds = delay;
  wsRetrySeconds.set(seconds);
  if (reconnectTimer) clearInterval(reconnectTimer);
  reconnectTimer = setInterval(() => {
    seconds -= 1;
    wsRetrySeconds.set(seconds);
    if (seconds <= 0) {
      clearInterval(reconnectTimer);
      reconnectTimer = null;
      connect();
    }
  }, 1000);
}

function connect() {
  if (socket) { socket.onclose = null; socket.close(); }

  socket = new WebSocket(wsUrl());

  socket.onopen = () => {
    wsConnected.set(true);
    wsRetrySeconds.set(0);
    if (reconnectTimer) { clearInterval(reconnectTimer); reconnectTimer = null; }
    if (onOpenCallback) onOpenCallback();
  };

  socket.onmessage = (event) => {
    try {
      const data = JSON.parse(event.data);
      const type = data.type || 'message';
      const typeHandlers = listeners.get(type);
      if (typeHandlers) typeHandlers.forEach(fn => fn(data));
      const wildcard = listeners.get('*');
      if (wildcard) wildcard.forEach(fn => fn(data));
    } catch (e) {
      console.error('Failed to parse WS message', e);
    }
  };

  socket.onerror = (err) => {
    console.error('WebSocket error:', err);
  };

  socket.onclose = () => {
    wsConnected.set(false);
    scheduleReconnect();
  };
}

/**
 * Start the WebSocket connection.
 * @param {Function} [onOpen] - Optional callback invoked each time the
 *   connection opens (initial + reconnects). Useful for reloading data.
 */
export function connectWs(onOpen) {
  onOpenCallback = onOpen || null;
  connect();
}

/**
 * Close the WebSocket and stop reconnecting.
 */
export function disconnectWs() {
  if (reconnectTimer) { clearInterval(reconnectTimer); reconnectTimer = null; }
  if (socket) { socket.onclose = null; socket.close(); socket = null; }
  wsConnected.set(false);
  wsRetrySeconds.set(0);
}

/**
 * Subscribe to messages of a given type.
 * @param {string} type - Message type to listen for, or '*' for all.
 * @param {Function} handler - Receives the parsed JSON message object.
 * @returns {Function} Unsubscribe function.
 */
export function onWsEvent(type, handler) {
  if (!listeners.has(type)) listeners.set(type, new Set());
  listeners.get(type).add(handler);
  return () => {
    const set = listeners.get(type);
    if (set) {
      set.delete(handler);
      if (set.size === 0) listeners.delete(type);
    }
  };
}

/**
 * Send a JSON message over the WebSocket.
 * @param {object} msg - Object to serialize and send.
 * @returns {boolean} true if sent, false if not connected.
 */
export function wsSend(msg) {
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send(JSON.stringify(msg));
    return true;
  }
  return false;
}
