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
  <div class="panel table">
    <div class="table-header">
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
          <div>{d.name || 'Device'}</div>
          <div>{d.serial || '—'}</div>
          <div>{fwString(d)}</div>
          <div><StatusDot status={d.status} />{d.status || '—'}</div>
        </div>
      {/each}
    {/if}
  </div>
</section>
