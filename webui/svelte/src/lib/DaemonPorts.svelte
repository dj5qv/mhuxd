<script>
  /** @type {Array} */
  export let connectors = [];
  /** @type {Array} */
  export let keyers = [];
  /** @type {Array} */
  export let devices = [];
  /** @type {object|null} */
  export let metadata = null;
  /** @type {function} */
  export let reloadData = async () => {};

  const portTypeOptions = ['VSP', 'TCP'];

  let portForm = {
    type: 'VSP',
    serial: '',
    channel: 'R1',
    devname: '',
    maxcon: 1,
    ptt_rts: false,
    ptt_dtr: false,
    remote_access: false
  };
  let portStatus = '';
  let portStatusKind = 'success';
  let portRemoveStatus = '';
  let portRemoveStatusKind = 'success';
  let portSaving = false;
  let portStatusTimer;
  let selectedConnectorIds = [];
  let showAddPortOverlay = false;

  // Auto-select first keyer when portForm has no serial yet
  $: if (!portForm.serial && keyers.length) {
    const serial = keyers[0].serial || '';
    const opts = channelOptionsForSerial(serial);
    portForm = { ...portForm, serial, channel: opts[0] || '' };
  }

  const keyerFlagsForSerial = (serial) => {
    const cfg = keyers.find((k) => k.serial === serial);
    if (!cfg || cfg.type == null || !metadata?.devicetypes) return [];
    const devType = metadata.devicetypes.find((d) => d.type === cfg.type);
    return devType?.flags || [];
  };

  const deviceNameForSerial = (serial) => {
    const dev = devices.find((d) => d.serial === serial);
    return dev?.name || serial || '—';
  };

  const channelOptionsForSerial = (serial) => {
    if (!serial) return [];
    const flags = keyerFlagsForSerial(serial);
    const opts = [];
    if (flags.includes('HAS_R1')) opts.push('CAT1');
    if (flags.includes('HAS_R2')) opts.push('CAT2');
    if (flags.includes('HAS_FLAGS_CHANNEL')) opts.push('PTT1');
    if (flags.includes('HAS_R2') || flags.includes('HAS_FSK2')) opts.push('PTT2');
    if (flags.includes('HAS_AUX')) opts.push('AUX');
    if (flags.includes('HAS_WINKEY')) opts.push('WK');
    if (flags.includes('HAS_FSK1')) opts.push('FSK1');
    if (flags.includes('HAS_FSK2')) opts.push('FSK2');
    if (flags.includes('HAS_MCP_SUPPORT')) opts.push('MCP');
    if (flags.includes('HAS_ROTATOR_SUPPORT')) opts.push('ROTATOR');
    return opts;
  };

  const displayPortChannel = (channel) => {
    if (!channel) return '—';
    if (channel === 'R1') return 'CAT1';
    if (channel === 'R2') return 'CAT2';
    return channel;
  };

  const checkMark = (value) => (value ? '✓' : '—');

  const buildConnectorPayload = () => {
    if (!portForm.serial || !portForm.channel || !portForm.type || !portForm.devname) return null;
    const base = {
      serial: portForm.serial,
      channel: portForm.channel,
      type: portForm.type,
      devname: String(portForm.devname)
    };
    if (portForm.maxcon) base.maxcon = Number(portForm.maxcon);
    if (portForm.type === 'VSP') {
      base.ptt_rts = portForm.ptt_rts ? 1 : 0;
      base.ptt_dtr = portForm.ptt_dtr ? 1 : 0;
    } else if (portForm.type === 'TCP') {
      base.remote_access = portForm.remote_access ? 1 : 0;
    }
    return base;
  };

  const toggleConnectorSelection = (id) => {
    if (id == null) return;
    if (selectedConnectorIds.includes(id)) {
      selectedConnectorIds = selectedConnectorIds.filter((v) => v !== id);
    } else {
      selectedConnectorIds = [...selectedConnectorIds, id];
    }
  };

  const applyPort = async () => {
    const connector = buildConnectorPayload();
    if (!connector) {
      portStatus = 'Please fill out all required fields.';
      portStatusKind = 'error';
      clearTimeout(portStatusTimer);
      portStatusTimer = setTimeout(() => (portStatus = ''), 3000);
      return;
    }

    portSaving = true;
    portStatus = '';
    portStatusKind = 'success';
    try {
      const res = await fetch('/api/v1/config/connectors', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify(connector)
      });
      if (!res.ok) throw new Error(`config/connectors ${res.status}`);
      await reloadData();
      portStatus = 'Port saved.';
      portStatusKind = 'success';
      showAddPortOverlay = false;
    } catch (err) {
      portStatus = err?.message || 'Failed to save port.';
      portStatusKind = 'error';
    } finally {
      portSaving = false;
      clearTimeout(portStatusTimer);
      portStatusTimer = setTimeout(() => (portStatus = ''), 3000);
    }
  };

  const updatePortSerial = (serial) => {
    const opts = channelOptionsForSerial(serial);
    const nextChannel = opts.includes(portForm.channel) ? portForm.channel : opts[0] || '';
    portForm = { ...portForm, serial, channel: nextChannel };
  };

  const removePorts = async () => {
    if (!selectedConnectorIds.length) return;
    portSaving = true;
    portRemoveStatus = '';
    portRemoveStatusKind = 'success';
    try {
      await Promise.all(
        selectedConnectorIds.map(async (id) => {
          const res = await fetch(`/api/v1/config/connectors/${id}`, {
            method: 'DELETE',
            headers: { Accept: 'application/json' }
          });
          if (!res.ok) throw new Error(`config/connectors/${id} ${res.status}`);
        })
      );
      await reloadData();
      selectedConnectorIds = [];
      portRemoveStatus = 'Port(s) removed.';
      portRemoveStatusKind = 'success';
    } catch (err) {
      portRemoveStatus = err?.message || 'Failed to remove port(s).';
      portRemoveStatusKind = 'error';
    } finally {
      portSaving = false;
      clearTimeout(portStatusTimer);
      portStatusTimer = setTimeout(() => {
        portRemoveStatus = '';
      }, 3000);
    }
  };
</script>

<section class="section">
  <div class="section-title">Port List</div>
  <div class="panel table ports-grid">
    <div class="table-header">
      <div></div>
      <div>ID</div>
      <div>Type</div>
      <div>Port / Device</div>
      <div>Status</div>
      <div>Remote Access</div>
      <div>RTS-PTT</div>
      <div>DTR-PTT</div>
      <div>Destination Name</div>
      <div>Destination Serial</div>
      <div>Channel</div>
    </div>
    {#if connectors.length === 0}
      <div class="table-empty">No ports configured.</div>
    {:else}
      {#each connectors as c, i}
        <div class={`table-row ${i % 2 ? 'alt' : ''}`}>
          <div>
            <input
              type="checkbox"
              checked={selectedConnectorIds.includes(c.id)}
              disabled={c.id == null}
              on:change={() => toggleConnectorSelection(c.id)}
            />
          </div>
          <div>{c.id ?? '—'}</div>
          <div>{c.type || '—'}</div>
          <div>{c.devname || '—'}</div>
          <div class={c.status === 'failed' ? 'text-error' : ''}>{c.status || '—'}</div>
          <div>{c.type === 'TCP' ? checkMark(c.remote_access) : '—'}</div>
          <div>{c.type === 'VSP' ? checkMark(c.ptt_rts) : '—'}</div>
          <div>{c.type === 'VSP' ? checkMark(c.ptt_dtr) : '—'}</div>
          <div>{deviceNameForSerial(c.serial)}</div>
          <div>{c.serial || '—'}</div>
          <div>{displayPortChannel(c.channel)}</div>
        </div>
      {/each}
    {/if}
  </div>
  <div class="button-row">
    <button class="btn" on:click={() => (showAddPortOverlay = true)}>Add</button>
    <button class="btn" on:click={removePorts} disabled={!selectedConnectorIds.length || portSaving}>
      Remove
    </button>
  </div>
  {#if portRemoveStatus}
    <div class={`inline-status ${portRemoveStatusKind === 'error' ? 'error' : ''}`}>{portRemoveStatus}</div>
  {/if}
</section>

{#if showAddPortOverlay}
<!-- svelte-ignore a11y-click-events-have-key-events -->
<!-- svelte-ignore a11y-no-static-element-interactions -->
<div class="add-port-backdrop" on:click|self={() => (showAddPortOverlay = false)}>
  <div class="add-port-modal">
  <div class="section-title add-port-modal-title">
    Add Port
    <button class="overlay-close-btn" on:click={() => (showAddPortOverlay = false)}>✕</button>
  </div>
  <div class="panel">
    <div class="row">
      <div class="label">Port Type:</div>
      <div class="value">
        <div class="inline-list">
          {#each portTypeOptions as opt}
            <label class="radio-option">
              <input type="radio" bind:group={portForm.type} value={opt} />
              {opt === 'VSP' ? 'VSP Virtual Serial Port' : 'TCP Network Port'}
            </label>
          {/each}
        </div>
      </div>
    </div>

    <div class="row">
      <div class="label">Destination Keyer:</div>
      <div class="value">
        <select class="select" value={portForm.serial} on:change={(e) => updatePortSerial(e.target.value)}>
          {#if keyers.length === 0}
            <option value="">No keyers available</option>
          {:else}
            {#each keyers as k}
              <option value={k.serial}>{k.name} ({k.serial})</option>
            {/each}
          {/if}
        </select>
      </div>
    </div>

    <div class="row">
      <div class="label">Destination Channel:</div>
      <div class="value">
        <select class="select" bind:value={portForm.channel} disabled={channelOptionsForSerial(portForm.serial).length === 0}>
          {#if channelOptionsForSerial(portForm.serial).length === 0}
            <option value="">No channels</option>
          {:else}
            {#each channelOptionsForSerial(portForm.serial) as ch}
              <option value={ch}>{ch}</option>
            {/each}
          {/if}
        </select>
      </div>
    </div>

    {#if portForm.type === 'VSP'}
      <div class="row">
        <div class="label">Device Path:</div>
        <div class="value">
          /dev/mhuxd/
          <input class="input" type="text" bind:value={portForm.devname} placeholder="ttyMHUXD0" />
        </div>
      </div>
      <div class="row">
        <div class="label">PTT via RTS:</div>
        <div class="value">
          <input type="checkbox" bind:checked={portForm.ptt_rts} />
        </div>
      </div>
      <div class="row">
        <div class="label">PTT via DTR:</div>
        <div class="value">
          <input type="checkbox" bind:checked={portForm.ptt_dtr} />
        </div>
      </div>
    {:else}
      <div class="row">
        <div class="label">Port Number:</div>
        <div class="value">
          <input class="input" type="number" min="1" step="1" inputmode="numeric" bind:value={portForm.devname} placeholder="9001" />
        </div>
      </div>
      <div class="row">
        <div class="label">Remote Accessible:</div>
        <div class="value">
          <input type="checkbox" bind:checked={portForm.remote_access} />
        </div>
      </div>
    {/if}

    <div class="row">
      <div class="label">Max Connections:</div>
      <div class="value">
        <input class="input" type="number" min="1" bind:value={portForm.maxcon} />
      </div>
    </div>

    <div class="button-row">
      <button class="btn" on:click={applyPort} disabled={portSaving || keyers.length === 0}>
        {portSaving ? 'Saving…' : 'Create'}
      </button>
      <button class="btn" on:click={() => (showAddPortOverlay = false)} disabled={portSaving}>
        Cancel
      </button>
    </div>
    {#if portStatus}
      <div class={`inline-status ${portStatusKind === 'error' ? 'error' : ''}`}>{portStatus}</div>
    {/if}
  </div>
  </div>
</div>
{/if}

<style>
  .add-port-backdrop {
    position: fixed;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    background: rgba(0, 0, 0, 0.45);
    display: flex;
    justify-content: center;
    align-items: center;
    z-index: 1000;
  }

  .add-port-modal {
    background: #fff;
    border: 1px solid #555;
    border-radius: 4px;
    min-width: 480px;
    max-width: 600px;
    width: 100%;
    box-shadow: 0 4px 16px rgba(0, 0, 0, 0.3);
    overflow: hidden;
  }

  .add-port-modal-title {
    display: flex;
    justify-content: space-between;
    align-items: center;
    border-radius: 0;
  }

  .overlay-close-btn {
    background: transparent;
    border: none;
    color: #fff;
    font-size: 16px;
    cursor: pointer;
    padding: 0 4px;
    line-height: 1;
  }

  .overlay-close-btn:hover {
    opacity: 0.7;
  }

  .button-row {
    display: flex;
    gap: 6px;
    margin-top: 8px;
  }
</style>
