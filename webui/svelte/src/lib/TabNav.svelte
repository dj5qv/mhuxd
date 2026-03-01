<script>
  import StatusDot from './StatusDot.svelte';

  /** @type {Array<{id: string, label: string, disabled?: boolean, status?: string}>} */
  export let tabs = [];

  /** @type {string} */
  export let activeTab = '';

  import { createEventDispatcher } from 'svelte';
  const dispatch = createEventDispatcher();

  const select = (tab) => {
    if (!tab.disabled) dispatch('select', tab.id);
  };
</script>

<nav class="tabs">
  {#each tabs as tab}
    <button
      class={`tab ${activeTab === tab.id ? 'active' : ''}`}
      on:click={() => select(tab)}
      disabled={tab.disabled}
      type="button"
    >
      {#if tab.status !== undefined}
        <StatusDot status={tab.status} />
      {/if}
      {tab.label}
    </button>
  {/each}
</nav>
