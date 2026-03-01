<script>
  import { createEventDispatcher } from 'svelte';
  const dispatch = createEventDispatcher();

  /** @type {Array<{id: string, label: string, disabled?: boolean}>} */
  export let items = [];

  /** @type {string} */
  export let activeId = '';

  const select = (item) => {
    if (!item.disabled) dispatch('select', item.id);
  };
</script>

<aside class="left">
  <div class="left-menu">
    {#each items as item}
      <button
        class={`left-menu-item ${activeId === item.id ? 'active' : ''}`}
        on:click={() => select(item)}
        disabled={item.disabled}
        type="button"
      >
        {item.label}
      </button>
    {/each}
  </div>
</aside>
