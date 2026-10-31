<script>
  import StatusDot from './StatusDot.svelte';
  import { displayPortChannel, deviceNameForSerial, clientLabel, clientTitle } from './ports.js';

  /** @type {object|null} */
  export let runtime = null;
  /** @type {object|null} */
  export let daemonCfg = null;
  /** @type {Array} */
  export let devices = [];
  /** @type {Array} */
  export let connectors = [];
  /** @type {function} */
  export let fwString = () => '';
  /** @type {function} */
  export let reloadData = async () => {};

  let removing = false;
  let keyerStatus = '';
  let keyerStatusKind = 'success';
  let keyerStatusTimer;

  let menuSerial = '';
  let menuStyle = '';
  let menuButton = null;

  let renameSerial = '';
  let renameValue = '';
  let renameSaving = false;

  // The daemon refuses to remove a keyer that is plugged in.
  const isRemovable = (d) => d.status === 'DISCONNECTED';

  // Forget a menu or a rename of a keyer that is gone.
  $: if (menuSerial && !devices.some((d) => d.serial === menuSerial)) menuSerial = '';
  $: if (renameSerial && !devices.some((d) => d.serial === renameSerial)) renameSerial = '';

  $: menuDevice = devices.find((d) => d.serial === menuSerial);

  const setKeyerStatus = (message, kind = 'success') => {
    keyerStatus = message;
    keyerStatusKind = kind;
    clearTimeout(keyerStatusTimer);
    keyerStatusTimer = setTimeout(() => {
      keyerStatus = '';
    }, 5000);
  };

  const closeMenu = () => {
    menuSerial = '';
  };

  const toggleMenu = (d, event) => {
    if (menuSerial === d.serial) {
      closeMenu();
      return;
    }
    menuButton = event.currentTarget;
    // Fixed position, as the table panel would clip the menu.
    const rect = menuButton.getBoundingClientRect();
    menuStyle = `top: ${rect.bottom}px; right: ${document.documentElement.clientWidth - rect.right}px;`;
    menuSerial = d.serial;
  };

  const onWindowKeydown = (event) => {
    if (event.key === 'Escape' && menuSerial) {
      closeMenu();
      menuButton?.focus();
    }
  };

  const focusFirstButton = (el) => {
    el.querySelector('button:not(:disabled)')?.focus();
  };

  const focusAndSelect = (el) => {
    el.focus();
    el.select();
  };

  const startRename = (d) => {
    closeMenu();
    keyerStatus = '';
    renameValue = d.name;
    renameSerial = d.serial;
  };

  const cancelRename = () => {
    renameSerial = '';
  };

  // Also called when the input loses focus, which happens as well when it goes away.
  const commitRename = async (d) => {
    if (renameSerial !== d.serial || renameSaving) return;

    const name = renameValue.trim();
    if (name === d.name) {
      cancelRename();
      return;
    }

    renameSaving = true;
    try {
      const res = await fetch(`/api/v1/config/devices/${encodeURIComponent(d.serial)}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial: d.serial, name })
      });
      if (!res.ok) {
        const body = await res.json().catch(() => ({}));
        throw new Error(body.error || `Failed to rename keyer (${res.status}).`);
      }
      renameSerial = '';
      await reloadData().catch(() => {});
    } catch (err) {
      setKeyerStatus(err?.message || 'Failed to rename keyer.', 'error');
    } finally {
      renameSaving = false;
    }
  };

  const onRenameKeydown = (event, d) => {
    if (event.key === 'Enter') {
      commitRename(d);
    } else if (event.key === 'Escape') {
      cancelRename();
    }
  };

  const removeKeyer = async (d) => {
    closeMenu();
    const label = d.name === d.serial ? d.serial : `${d.name} (${d.serial})`;
    if (!confirm(`Remove ${label}?\n\nAll settings of the keyer and its ports will be discarded. ` +
                 'A removed keyer shows up again with default settings when it gets plugged in.')) return;

    removing = true;
    keyerStatus = '';
    try {
      const res = await fetch(`/api/v1/config/devices/${encodeURIComponent(d.serial)}`, {
        method: 'DELETE',
        headers: { Accept: 'application/json' }
      });
      if (!res.ok) {
        const body = await res.json().catch(() => ({}));
        throw new Error(`${d.serial}: ${body.error || res.status}`);
      }
      setKeyerStatus('Keyer removed.');
    } catch (err) {
      setKeyerStatus(err?.message || 'Failed to remove keyer.', 'error');
    } finally {
      removing = false;
      await reloadData().catch(() => {});
    }
  };
</script>

<svelte:window
  on:click={closeMenu}
  on:keydown={onWindowKeydown}
  on:resize={closeMenu}
  on:scroll|capture={closeMenu}
/>

<section class="section">
  <div class="section-title">Summary</div>
  <div class="panel">
    <div class="row">
      <div class="label">Version:</div>
      <div class="value">{runtime?.daemon?.name || 'mhuxd'} {runtime?.daemon?.version || '—'}</div>
    </div>
    <div class="row">
      <div class="label">Hostname:</div>
      <div class="value">{runtime?.hostname || '—'}</div>
    </div>
    <div class="row">
      <div class="label">Process ID:</div>
      <div class="value">{runtime?.daemon?.pid ?? '—'}</div>
    </div>
    <div class="row">
      <div class="label">Log File:</div>
      <div class="value">{runtime?.daemon?.logfile || '—'}</div>
    </div>
    <div class="row">
      <div class="label">Loglevel:</div>
      <div class="value">{daemonCfg?.loglevel || '—'}</div>
    </div>
  </div>
</section>

<section class="section">
  <div class="section-title">Keyer List</div>
  <div class="panel table keyers-grid">
    <div class="table-header">
      <div>Name</div>
      <div>Model</div>
      <div>Serial</div>
      <div>Firmware</div>
      <div>Status</div>
      <div></div>
    </div>
    {#if devices.length === 0}
      <div class="table-empty">No keyers found.</div>
    {:else}
      {#each devices as d, i (d.serial)}
        <div class={`table-row ${i % 2 ? 'alt' : ''}`}>
          <div class="keyer-name">
            {#if renameSerial === d.serial}
              <input
                class="input keyer-name-input"
                type="text"
                maxlength="32"
                placeholder={d.serial}
                title="Enter to save, Esc to cancel. Empty: use the serial number."
                aria-label={`Name of keyer ${d.serial}`}
                bind:value={renameValue}
                readonly={renameSaving}
                use:focusAndSelect
                on:keydown={(e) => onRenameKeydown(e, d)}
                on:blur={() => commitRename(d)}
              />
            {:else}
              {d.name || d.serial || '—'}
            {/if}
          </div>
          <div>{d.model || '—'}</div>
          <div>{d.serial || '—'}</div>
          <div>{fwString(d)}</div>
          <div><StatusDot status={d.status} />{d.status || '—'}</div>
          <div>
            <button
              class="btn keyer-menu-button"
              type="button"
              title="Keyer actions"
              aria-label={`Actions for keyer ${d.name || d.serial}`}
              aria-expanded={menuSerial === d.serial}
              on:click|stopPropagation={(e) => toggleMenu(d, e)}
            >
              <svg viewBox="0 0 16 16" width="14" height="14" aria-hidden="true">
                <path d="M2 4h12M2 8h12M2 12h12" stroke="currentColor" stroke-width="1.5" />
              </svg>
            </button>
          </div>
        </div>
      {/each}
    {/if}
  </div>
  {#if menuDevice}
    <div class="keyer-menu" style={menuStyle} use:focusFirstButton>
      <button type="button" on:click={() => startRename(menuDevice)}>Rename</button>
      <button
        type="button"
        disabled={!isRemovable(menuDevice) || removing}
        title={isRemovable(menuDevice) ? '' : 'Unplug the keyer to remove it'}
        on:click={() => removeKeyer(menuDevice)}
      >Remove</button>
    </div>
  {/if}
  {#if keyerStatus}
    <div class={`inline-status ${keyerStatusKind === 'error' ? 'error' : ''}`}>{keyerStatus}</div>
  {/if}
</section>

<section class="section">
  <div class="section-title">Port List</div>
  <div class="panel table port-list-grid">
    <div class="table-header">
      <div>ID</div>
      <div>Type</div>
      <div>Port / Device</div>
      <div>Status</div>
      <div>Client</div>
      <div>Destination Name</div>
      <div>Destination Serial</div>
      <div>Channel</div>
    </div>
    {#if connectors.length === 0}
      <div class="table-empty">No ports configured.</div>
    {:else}
      {#each connectors as c, i}
        <div class={`table-row ${i % 2 ? 'alt' : ''}`}>
          <div>{c.id ?? '—'}</div>
          <div>{c.type || '—'}</div>
          <div>{c.devname || '—'}</div>
          <div class={c.status === 'failed' ? 'text-error' : ''}>{c.status || '—'}</div>
          <div title={clientTitle(c.client)}>{clientLabel(c.client)}</div>
          <div>{deviceNameForSerial(devices, c.serial)}</div>
          <div>{c.serial || '—'}</div>
          <div>{displayPortChannel(c.channel)}</div>
        </div>
      {/each}
    {/if}
  </div>
</section>
