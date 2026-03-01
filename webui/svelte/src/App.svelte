<script>
  import { onMount, onDestroy } from 'svelte';
  import ConnectionOverlay from './lib/ConnectionOverlay.svelte';
  import TopBar from './lib/TopBar.svelte';
  import TabNav from './lib/TabNav.svelte';
  import SideMenu from './lib/SideMenu.svelte';
  import HomeSummary from './lib/HomeSummary.svelte';
  import DaemonPorts from './lib/DaemonPorts.svelte';
  import DaemonSettings from './lib/DaemonSettings.svelte';
  import KeyerPage from './lib/KeyerPage.svelte';
  import { connectWs, disconnectWs, onWsEvent } from './lib/ws.js';
  import { loadAllData, apiGet } from './lib/api.js';

  let runtime = null;
  let daemonCfg = null;
  let devices = [];
  let configDevices = [];
  let connectors = [];
  let metadata = null;
  let keyers = [];
  let loading = true;
  let error = '';
  let lastSerial = '';



  const parseHash = () => {
    const h = location.hash.replace(/^#\/?/, '');
    const [t, m] = h.split('/');
    return { tab: t || 'home', menu: m || '' };
  };
  const initHash = parseHash();
  let activeTab = initHash.tab;
  let activeMenu = initHash.menu || 'summary';
  let hashRestoredMenu = !!initHash.menu;

  const baseTabs = [
    { id: 'home', label: 'Home' },
    { id: 'daemon', label: 'Daemon' }
  ];

  const homeMenus = [{ id: 'summary', label: 'Summary', title: 'Summary' }];
  const daemonMenus = [
    { id: 'ports', label: 'Ports', title: 'Daemon Ports Configuration and Routing' },
    { id: 'settings', label: 'Settings', title: 'Daemon Settings' },
    { id: 'action', label: 'Action', title: 'Daemon Actions', disabled: true }
  ];



  const keyerMenus = [
    { id: 'mode', label: 'Keyer Mode', title: 'Keyer Mode', depends: 'has.keyer_mode' },
    { id: 'radio', label: 'Radio', title: 'Radio Communication Parameters', depends: 'has.r1' },
    { id: 'aux', label: 'AUX', title: 'AUX Communication Parameters', depends: 'has.aux' },
    { id: 'fsk', label: 'FSK', title: 'FSK Channel Parameters', depends: 'has.fsk1' },
    { id: 'ptt', label: 'PTT', title: 'PTT Configuration', depends: 'has.ptt_settings' },
    { id: 'audio', label: 'Audio', title: 'Audio Configuration', depends: 'has.audio_switching' },
    { id: 'display', label: 'Display', title: 'Display Configuration', depends: 'has.display' },
    { id: 'cw', label: 'CW', title: 'CW Settings', depends: 'has.winkey' },
    { id: 'cw_messages', label: 'CW/FSK Messages', title: 'Messages', depends: 'has.winkey' },
    { id: 'sm_antsw_load_store', label: 'Switching / Load, Store', title: 'Antenna Switching / Load & Store', depends: 'has.sm_commands' },
    { id: 'sm_antsw_settings', label: 'Switching / Settings', title: 'Antenna Switching / Settings', depends: 'has.sm_commands' },
    { id: 'sm_antsw_outputs', label: 'Switching / Outputs', title: 'Antenna Switching / Outputs', depends: 'has.sm_commands' },
    { id: 'sm_antsw_ant_list', label: 'Switching / Antennas', title: 'Antenna Switching / Antennas', depends: 'has.sm_commands' },
    { id: 'sm_antsw_vr_list', label: 'Switching / Virt. Rotators', title: 'Antenna Switching / Virtual Rotators', depends: 'has.sm_commands' },
    { id: 'sm_antsw_grp_list', label: 'Switching / Ant. Groups', title: 'Antenna Switching / Antenna Groups', depends: 'has.sm_commands' },
    { id: 'sm_antsw_band_list', label: 'Switching / Bands', title: 'Antenna Switching / Bands', depends: 'has.sm_commands' },
    { id: 'all_params', label: 'All Parameters', title: 'All Keyer Parameters' }
  ];

  const dependsMap = {
    'has.keyer_mode': 'HAS_KEYER_MODE',
    'has.r1': 'HAS_R1',
    'has.aux': 'HAS_AUX',
    'has.fsk1': 'HAS_FSK1',
    'has.ptt_settings': 'HAS_PTT_SETTINGS',
    'has.audio_switching': 'HAS_AUDIO_SWITCHING',
    'has.display': 'HAS_DISPLAY',
    'has.winkey': 'HAS_WINKEY',
    'has.sm_commands': 'HAS_SM_COMMANDS'
  };



  const updateHash = () => {
    const h = `#${activeTab}/${activeMenu}`;
    if (location.hash !== h) history.replaceState(null, '', h);
  };

  const setTab = (id) => {
    activeTab = id;
    if (id === 'daemon') activeMenu = 'ports';
    else if (id === 'home') activeMenu = 'summary';
    else if (id.startsWith('keyer:')) activeMenu = 'mode';
    else activeMenu = 'summary';
  };

  const refreshDeviceConfig = async (serial) => {
    try {
      const updated = await apiGet(`/api/v1/config/devices/${serial}`);
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
    } catch {
      // silently ignore — we're already in an error handler
    }
  };

  const fwString = (d) => {
    if (!d) return '';
    const major = d.verFwMajor ?? 0;
    const minor = d.verFwMinor ?? 0;
    const beta = d.verFwBeta ? 'b' : '';
    return `${major}.${minor}${beta}`;
  };

  const reloadData = async () => {
    try {
      const data = await loadAllData();
      runtime = data.runtime;
      daemonCfg = data.daemonCfg;
      devices = data.devices;
      configDevices = data.configDevices;
      connectors = data.connectors;
      metadata = data.metadata;
    } catch (err) {
      console.error('Failed to reload data:', err);
      throw err;
    }
  };

  // Subscribe to WS events
  const unsubStatus = onWsEvent('status', (data) => {
    devices = devices.map(d =>
      d.serial === data.serial ? { ...d, status: data.status } : d
    );
  });
  const unsubUsb = onWsEvent('usb_connection', (data) => {
    console.log('USB Event:', data);
  });

  onMount(async () => {
    try {
      await reloadData();
      connectWs(async () => {
        try { await reloadData(); } catch { /* pick up state from WS events */ }
      });
    } catch (err) {
      error = err?.message || 'Failed to load data';
      // connectWs will keep retrying automatically
      connectWs(async () => {
        try { await reloadData(); } catch { }
      });
    } finally {
      loading = false;
    }

    const onHashChange = () => {
      const h = parseHash();
      if (h.tab !== activeTab) activeTab = h.tab;
      if (h.menu && h.menu !== activeMenu) activeMenu = h.menu;
    };
    window.addEventListener('hashchange', onHashChange);
    return () => {
      window.removeEventListener('hashchange', onHashChange);
    };
  });

  onDestroy(() => {
    disconnectWs();
    unsubStatus();
    unsubUsb();
  });

  const buildKeyers = (cfgDevices, devs) => {
    const bySerial = new Map((devs || []).map((d) => [d.serial, d]));
    if (cfgDevices && cfgDevices.length) {
      return cfgDevices.map((cfg) => {
        const dev = bySerial.get(cfg.serial) || {};
        return {
          serial: cfg.serial,
          type: cfg.type,
          name: dev.name || cfg.serial,
          status: dev.status || ''
        };
      });
    }
    return (devs || []).map((dev) => ({
      serial: dev.serial,
      type: null,
      name: dev.name || dev.serial,
      status: dev.status || ''
    }));
  };

  $: keyers = buildKeyers(configDevices, devices);
  $: tabs = [...baseTabs, ...keyers.map((k) => ({ id: `keyer:${k.serial}`, label: k.name, serial: k.serial, status: k.status }))];

  const keyerFlagsForSerial = (serial) => {
    const cfg = keyers.find((k) => k.serial === serial);
    if (!cfg || cfg.type == null || !metadata?.devicetypes) return [];
    const devType = metadata.devicetypes.find((d) => d.type === cfg.type);
    return devType?.flags || [];
  };


  const visibleKeyerMenus = (serial) => {
    const flags = keyerFlagsForSerial(serial);
    return keyerMenus.filter((m) => {
      if (!m.depends) return true;
      const flag = dependsMap[m.depends];
      return flag ? flags.includes(flag) : false;
    });
  };

  $: activeSerial = activeTab.startsWith('keyer:') ? activeTab.split(':')[1] : '';
  $: activeKeyer = activeSerial ? keyers.find((k) => k.serial === activeSerial) : null;
  $: activeKeyerFlags = activeSerial && keyers && metadata ? keyerFlagsForSerial(activeSerial) : [];
  $: activeKeyerMenus = activeSerial && keyers && metadata ? visibleKeyerMenus(activeSerial) : [];

  $: activeMenuId = (activeMenu || '').toLowerCase();


  $: if (activeTab.startsWith('keyer:') && activeSerial && activeSerial !== lastSerial) {
    lastSerial = activeSerial;
    if (hashRestoredMenu) {
      hashRestoredMenu = false;
    } else {
      activeMenu = activeKeyerMenus[0]?.id || 'summary';
    }
  }

  $: activeTab, activeMenu, updateHash();

  $: activeMenuMeta =
    activeTab === 'home'
      ? homeMenus.find((m) => m.id === activeMenuId) || null
      : activeTab === 'daemon'
        ? daemonMenus.find((m) => m.id === activeMenuId) || null
        : activeTab.startsWith('keyer:')
          ? activeKeyerMenus.find((m) => m.id === activeMenuId) || null
          : null;

  $: activeTabLabel =
    activeTab === 'home'
      ? 'Home'
      : activeTab === 'daemon'
        ? 'Daemon'
        : activeTab.startsWith('keyer:')
          ? (activeKeyer?.name || activeSerial || 'Keyer')
          : '';

  $: activePageTitle = activeMenuMeta?.title || (activeTab.startsWith('keyer:') ? 'Keyer' : '');
  $: breadcrumb = activeTabLabel
    ? (activeMenuMeta?.label ? `${activeTabLabel} >> ${activeMenuMeta.label}` : activeTabLabel)
    : '';

  $: sideMenuItems =
    activeTab === 'home' ? homeMenus
    : activeTab === 'daemon' ? daemonMenus
    : activeTab.startsWith('keyer:') ? activeKeyerMenus
    : [{ id: 'summary', label: 'Summary' }];

</script>

<div class="page">
  <ConnectionOverlay />

  <TopBar hostname={runtime?.hostname || '—'} version={runtime?.daemon?.version || '—'} />

  <TabNav {tabs} {activeTab} on:select={(e) => setTab(e.detail)} />

  <div class="layout">
    <SideMenu items={sideMenuItems} activeId={activeMenuId} on:select={(e) => activeMenu = e.detail} />

    <main class="main">
      <div class="breadcrumbs">{breadcrumb}</div>

      <h1>{activePageTitle}</h1>

      {#if loading}
        <div class="loading">Loading…</div>
      {:else if error}
        <div class="error">{error}</div>
      {:else}
        {#if activeTab === 'home'}
          <HomeSummary {runtime} {daemonCfg} {devices} {fwString} />
        {:else if activeTab === 'daemon' && activeMenu === 'ports'}
          <DaemonPorts {connectors} {keyers} {devices} {metadata} {reloadData} />
        {:else if activeTab === 'daemon' && activeMenu === 'settings'}
          <DaemonSettings bind:daemonCfg />
        {:else if activeTab.startsWith('keyer:')}
          <KeyerPage
            {activeSerial}
            {activeMenuId}
            {activeKeyer}
            {activeKeyerFlags}
            bind:configDevices
            {metadata}
            {keyers}
            {refreshDeviceConfig}
          />
        {:else}
          <section class="section">
            <div class="section-title">Keyer</div>
            <div class="panel">
              <div class="placeholder">Keyer pages will be implemented next.</div>
            </div>
          </section>
        {/if}
      {/if}
    </main>
  </div>
</div>
