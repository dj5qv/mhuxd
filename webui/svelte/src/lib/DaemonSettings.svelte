<script>
  export let daemonCfg;

  let loglevel = daemonCfg?.loglevel || '';
  let loglevelStatus = '';
  let loglevelStatusKind = 'success';
  let loglevelSaving = false;
  let loglevelTimer;

  // Keep loglevel in sync when parent pushes new daemonCfg
  $: loglevel = daemonCfg?.loglevel || loglevel;

  const applyLoglevel = async () => {
    if (!loglevel) return;
    loglevelSaving = true;
    loglevelStatus = '';
    loglevelStatusKind = 'success';
    try {
      const res = await fetch('/api/v1/config/daemon', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ loglevel })
      });
      if (!res.ok) throw new Error(`config/daemon ${res.status}`);
      const data = await res.json();
      daemonCfg = data;
      loglevel = data?.loglevel || loglevel;
      loglevelStatus = 'Log level updated.';
      loglevelStatusKind = 'success';
    } catch (err) {
      loglevelStatus = err?.message || 'Failed to update log level.';
      loglevelStatusKind = 'error';
    } finally {
      loglevelSaving = false;
      clearTimeout(loglevelTimer);
      loglevelTimer = setTimeout(() => (loglevelStatus = ''), 3000);
    }
  };
</script>

<section class="section">
  <div class="section-title">Logging</div>
  <div class="panel">
    <div class="row">
      <div class="label">Log Level:</div>
      <div class="value">
        <select class="select" bind:value={loglevel} on:change={applyLoglevel} disabled={loglevelSaving}>
          <option value="CRIT">CRIT</option>
          <option value="ERROR">ERROR</option>
          <option value="WARN">WARN</option>
          <option value="INFO">INFO</option>
          <option value="DEBUG0">DEBUG0</option>
          <option value="DEBUG1">DEBUG1</option>
        </select>
      </div>
    </div>
    {#if loglevelStatus}
      <div class={`inline-status ${loglevelStatusKind === 'error' ? 'error' : ''}`}>{loglevelStatus}</div>
    {/if}
  </div>
</section>
