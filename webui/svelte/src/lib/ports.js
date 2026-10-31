/**
 * Helpers shared by the Port List (Summary) and the Port Config (Ports) tables.
 */

export const displayPortChannel = (channel) => {
  if (!channel) return '—';
  if (channel === 'R1') return 'CAT1';
  if (channel === 'R2') return 'CAT2';
  return channel;
};

export const deviceNameForSerial = (devices, serial) => {
  const dev = devices.find((d) => d.serial === serial);
  return dev?.name || serial || '—';
};

// "Shack (DigiKeyer II, D2UL4ED0)", "D2UL4ED0 (DigiKeyer II)" without a custom name.
export const keyerLabel = (keyer) => {
  const extra = [keyer.model, keyer.name !== keyer.serial && keyer.serial].filter(Boolean).join(', ');
  return extra ? `${keyer.name} (${extra})` : keyer.name;
};

// The application on a port, see struct con_client in src/con_client.h:
// "wsjtx (PID 4711, mattes)", "127.0.0.1:53422 (mattes)", "192.168.1.20:50112".
export const clientLabel = (client) => {
  if (!client) return '—';
  const name = client.program || client.peer || 'unknown';
  const extra = [];
  if (client.pid) extra.push(`PID ${client.pid}`);
  if (client.user) extra.push(client.user);
  else if (client.uid != null) extra.push(`uid ${client.uid}`);
  return extra.length ? `${name} (${extra.join(', ')})` : name;
};

export const clientTitle = (client) =>
  client?.since ? `connected since ${new Date(client.since * 1000).toLocaleString()}` : '';
