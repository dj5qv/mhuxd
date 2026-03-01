/**
 * REST API helpers for mhuxd.
 *
 * Thin wrappers around fetch() that handle JSON parsing and errors
 * in a consistent way.  These do NOT update stores — callers decide
 * what to do with the response.
 */

/**
 * GET a JSON endpoint.  Throws on non-2xx.
 * @param {string} url
 * @returns {Promise<any>}
 */
export async function apiGet(url) {
  const res = await fetch(url, { headers: { Accept: 'application/json' } });
  if (!res.ok) throw new Error(`${url} ${res.status}`);
  return res.json();
}

/**
 * POST/PUT/PATCH a JSON body.  Returns parsed response.
 * @param {string} url
 * @param {string} method - 'POST' | 'PUT' | 'PATCH'
 * @param {object} body
 * @returns {Promise<any>}
 */
export async function apiWrite(url, method, body) {
  const res = await fetch(url, {
    method,
    headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
    body: JSON.stringify(body)
  });
  if (!res.ok) throw new Error(`${method} ${url} ${res.status}`);
  return res.json();
}

/**
 * DELETE a resource.
 * @param {string} url
 * @returns {Promise<any>}
 */
export async function apiDelete(url) {
  const res = await fetch(url, {
    method: 'DELETE',
    headers: { Accept: 'application/json' }
  });
  if (!res.ok) throw new Error(`DELETE ${url} ${res.status}`);
  return res.json();
}

/**
 * Load all initial data in parallel.
 * Returns an object with { runtime, daemonCfg, devices, configDevices, connectors, metadata }.
 */
export async function loadAllData() {
  const [runtime, daemonCfg, devicesRsp, configDevicesRsp, metadata] = await Promise.all([
    apiGet('/api/v1/runtime'),
    apiGet('/api/v1/config/daemon'),
    apiGet('/api/v1/devices'),
    apiGet('/api/v1/config/devices'),
    apiGet('/api/v1/metadata')
  ]);
  return {
    runtime,
    daemonCfg,
    devices: devicesRsp.devices || [],
    configDevices: configDevicesRsp.devices || [],
    connectors: configDevicesRsp.connectors || [],
    metadata
  };
}
