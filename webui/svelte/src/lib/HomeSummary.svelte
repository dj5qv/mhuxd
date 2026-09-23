<script>
  import StatusDot from './StatusDot.svelte';

  /** @type {object|null} */
  export let runtime = null;
  /** @type {object|null} */
  export let daemonCfg = null;
  /** @type {Array} */
  export let devices = [];
  /** @type {function} */
  export let fwString = () => '';
  /** @type {function} */
  export let reloadData = async () => {};

  let selectedSerials = [];
  let removing = false;
  let removeStatus = '';
  let removeStatusKind = 'success';
  let removeStatusTimer;

  // The daemon refuses to remove a keyer that is plugged in.
  const isRemovable = (d) => d.status === 'DISCONNECTED';

  // Drop selections of keyers that are gone or got plugged in meanwhile.
  $: selectedSerials = selectedSerials.filter((serial) =>
    devices.some((d) => d.serial === serial && isRemovable(d))
  );

  const toggleSelection = (serial) => {
    selectedSerials = selectedSerials.includes(serial)
      ? selectedSerials.filter((s) => s !== serial)
      : [...selectedSerials, serial];
  };

  const removeKeyers = async () => {
    if (!selectedSerials.length) return;
    const names = selectedSerials.join(', ');
    if (!confirm(`Remove ${names}?\n\nAll settings of the keyer(s) and their ports will be discarded. ` +
                 'A removed keyer shows up again with default settings when it gets plugged in.')) return;

    removing = true;
    removeStatus = '';
    removeStatusKind = 'success';
    try {
      await Promise.all(
        selectedSerials.map(async (serial) => {
          const res = await fetch(`/api/v1/config/devices/${encodeURIComponent(serial)}`, {
            method: 'DELETE',
            headers: { Accept: 'application/json' }
          });
          if (!res.ok) {
            const body = await res.json().catch(() => ({}));
            throw new Error(`${serial}: ${body.error || res.status}`);
          }
        })
      );
      selectedSerials = [];
      removeStatus = 'Keyer(s) removed.';
    } catch (err) {
      removeStatus = err?.message || 'Failed to remove keyer(s).';
      removeStatusKind = 'error';
    } finally {
      removing = false;
      await reloadData().catch(() => {});
      clearTimeout(removeStatusTimer);
      removeStatusTimer = setTimeout(() => {
        removeStatus = '';
      }, 5000);
    }
  };
</script>

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
      <div></div>
      <div>Name</div>
      <div>Serial</div>
      <div>Firmware</div>
      <div>Status</div>
    </div>
    {#if devices.length === 0}
      <div class="table-empty">No keyers found.</div>
    {:else}
      {#each devices as d, i}
        <div class={`table-row ${i % 2 ? 'alt' : ''}`}>
          <div>
            <input
              type="checkbox"
              checked={selectedSerials.includes(d.serial)}
              disabled={!isRemovable(d) || removing}
              title={isRemovable(d) ? '' : 'Unplug the keyer to remove it'}
              on:change={() => toggleSelection(d.serial)}
            />
          </div>
          <div>{d.name || 'Device'}</div>
          <div>{d.serial || '—'}</div>
          <div>{fwString(d)}</div>
          <div><StatusDot status={d.status} />{d.status || '—'}</div>
        </div>
      {/each}
    {/if}
  </div>
  <div class="button-row">
    <button class="btn" on:click={removeKeyers} disabled={!selectedSerials.length || removing}>
      Remove
    </button>
  </div>
  {#if removeStatus}
    <div class={`inline-status ${removeStatusKind === 'error' ? 'error' : ''}`}>{removeStatus}</div>
  {/if}
</section>
