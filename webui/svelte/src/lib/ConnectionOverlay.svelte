<script>
  import { wsConnected, wsRetrySeconds } from './ws.js';
</script>

{#if !$wsConnected}
  <div class="connection-overlay">
    <div class="connection-message">Can't connect to mhuxd. Retry in {$wsRetrySeconds} {$wsRetrySeconds === 1 ? 'second' : 'seconds'}</div>
  </div>
{/if}
