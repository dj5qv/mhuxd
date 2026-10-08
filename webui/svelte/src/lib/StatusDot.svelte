<script>
  export let status = 'UNKNOWN';

  $: s = String(status || '').toUpperCase();
  $: statusClass = s.includes('ONLINE') ? 'ok' : (s.includes('OFFLINE') ? 'warn' : 'bad');
</script>

<span class="status-dot {statusClass}" title={status} aria-label={status}></span>

<style>
  .status-dot {
    display: inline-block;
    width: 10px;
    height: 10px;
    border-radius: 50%;
    margin-right: 6px;
    vertical-align: middle;
    position: relative;
    top: -1px;
    box-shadow: inset 0 0 0 1px rgba(0, 0, 0, 0.2);
  }
  .status-dot.ok { background: #2da44e; }
  .status-dot.warn { background: #d29922; }
  .status-dot.bad { background: #d1242f; }
</style>
