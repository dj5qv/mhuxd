<script>
  import { onMount } from 'svelte';
  import StatusDot from './lib/StatusDot.svelte';

  let runtime = null;
  let daemonCfg = null;
  let devices = [];
  let configDevices = [];
  let metadata = null;
  let keyers = [];
  let loading = true;
  let error = '';
  let loglevel = '';
  let loglevelStatus = '';
  let loglevelSaving = false;
  let loglevelTimer;
  let keyerStatus = {};
  let keyerStatusTimer = {};
  let radioForm = {};
  let radioStatus = {};
  let radioStatusTimer = {};
  let lastSerial = '';

  let activeTab = 'home';
  let activeMenu = 'summary';

  const baseTabs = [
    { id: 'home', label: 'Home' },
    { id: 'daemon', label: 'Daemon' }
  ];

  const homeMenus = [{ id: 'summary', label: 'Summary' }];
  const daemonMenus = [
    { id: 'ports', label: 'Ports' },
    { id: 'settings', label: 'Settings' },
    { id: 'action', label: 'Action', disabled: true }
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

  const keyerModeOptions = [
    { value: 0, label: 'CW' },
    { value: 1, label: 'VOICE' },
    { value: 2, label: 'FSK' },
    { value: 3, label: 'DIGITAL' }
  ];

  const radioBaudOptions = [1200, 2400, 4800, 9600, 19200, 38400, 57600];
  const radioDataBitsOptions = [8, 7, 6, 5];
  const radioStopBitsOptions = [1, 2];
  const digitalOverVoiceOptions = [
    { value: 0, label: 'Band Map' },
    { value: 1, label: 'Always VOICE' },
    { value: 2, label: 'Always DIGITAL' }
  ];

  const hasFlag = (serial, flag) => keyerFlagsForSerial(serial).includes(flag);

  const keyerConfigForSerial = (serial) => configDevices.find((d) => d.serial === serial) || {};

  const channelConfig = (serial, chan) => {
    const cfg = keyerConfigForSerial(serial);
    if (!cfg?.channel || !cfg.channel[chan]) return {};
    return cfg.channel[chan];
  };

  const defaultRadioForm = (serial, chan) => {
    const ch = channelConfig(serial, chan);
    return {
      baud: ch.baud ?? 9600,
      databits: ch.databits ?? 8,
      stopbits: ch.stopbits ?? 1,
      rtscts: ch.rtscts ?? 0,
      rigtype: ch.rigtype ?? 0,
      icomaddress: ch.icomaddress ?? 0,
      icomsimulateautoinfo: ch.icomsimulateautoinfo ?? 0,
      digitalovervoicerule: ch.digitalovervoicerule ?? 0,
      usedecoderifconnected: ch.usedecoderifconnected ?? 0,
      dontinterfereusbcontrol: ch.dontinterfereusbcontrol ?? 0
    };
  };

  const ensureRadioForm = (serial, chan) => {
    if (!serial) return;
    const perSerial = radioForm[serial] || {};
    if (!perSerial[chan]) {
      radioForm = {
        ...radioForm,
        [serial]: { ...perSerial, [chan]: defaultRadioForm(serial, chan) }
      };
    }
  };

  const updateRadioForm = (serial, chan, key, value) => {
    const perSerial = radioForm[serial] || {};
    const perChan = perSerial[chan] || defaultRadioForm(serial, chan);
    radioForm = {
      ...radioForm,
      [serial]: { ...perSerial, [chan]: { ...perChan, [key]: value } }
    };
  };

  const setRadioStatusMsg = (serial, chan, msg) => {
    const key = `${serial}:${chan}`;
    radioStatus = { ...radioStatus, [key]: msg };
    if (radioStatusTimer[key]) clearTimeout(radioStatusTimer[key]);
    radioStatusTimer[key] = setTimeout(() => {
      const { [key]: _removed, ...rest } = radioStatus;
      radioStatus = rest;
    }, 3000);
  };

  const applyRadioChannel = async (serial, chan) => {
    const form = radioForm[serial]?.[chan] || defaultRadioForm(serial, chan);
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({
          serial,
          channel: {
            [chan]: {
              baud: Number(form.baud),
              databits: Number(form.databits),
              stopbits: Number(form.stopbits),
              rtscts: Number(form.rtscts),
              rigtype: Number(form.rigtype),
              icomaddress: Number(form.icomaddress),
              icomsimulateautoinfo: Number(form.icomsimulateautoinfo),
              digitalovervoicerule: Number(form.digitalovervoicerule),
              usedecoderifconnected: Number(form.usedecoderifconnected),
              dontinterfereusbcontrol: Number(form.dontinterfereusbcontrol)
            }
          }
        })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      radioForm = {
        ...radioForm,
        [serial]: { ...radioForm[serial], [chan]: defaultRadioForm(serial, chan) }
      };
      setRadioStatusMsg(serial, chan, 'Radio settings applied.');
    } catch (err) {
      setRadioStatusMsg(serial, chan, err?.message || 'Failed to apply radio settings.');
    }
  };

  const rigtypes = () => metadata?.rigtypes || [];

  const setRigType = (serial, chan, rigId) => {
    const rig = rigtypes().find((r) => r.id === Number(rigId));
    const icomAddr = rig?.icomAddr ?? 0;
    updateRadioForm(serial, chan, 'rigtype', Number(rigId));
    updateRadioForm(serial, chan, 'icomaddress', icomAddr);
  };

  const setTab = (id) => {
    activeTab = id;
    if (id === 'daemon') activeMenu = 'ports';
    else if (id === 'home') activeMenu = 'summary';
    else if (id.startsWith('keyer:')) activeMenu = 'mode';
    else activeMenu = 'summary';
  };

  const safeFetch = async (url) => {
    const res = await fetch(url, { headers: { Accept: 'application/json' } });
    if (!res.ok) throw new Error(`${url} ${res.status}`);
    return res.json();
  };

  const fwString = (d) => {
    if (!d) return '';
    const major = d.verFwMajor ?? 0;
    const minor = d.verFwMinor ?? 0;
    const beta = d.verFwBeta ? 'b' : '';
    return `${major}.${minor}${beta}`;
  };

  onMount(async () => {
    try {
      const [runtimeRsp, daemonRsp, devicesRsp, configDevicesRsp, metadataRsp] = await Promise.all([
        safeFetch('/api/v1/runtime'),
        safeFetch('/api/v1/config/daemon'),
        safeFetch('/api/v1/devices'),
        safeFetch('/api/v1/config/devices'),
        safeFetch('/api/v1/metadata')
      ]);
      runtime = runtimeRsp;
      daemonCfg = daemonRsp;
      loglevel = daemonRsp?.loglevel || '';
      devices = devicesRsp.devices || [];
      configDevices = configDevicesRsp.devices || [];
      metadata = metadataRsp;

    } catch (err) {
      error = err?.message || 'Failed to load data';
    } finally {
      loading = false;
    }
  });

  const applyLoglevel = async () => {
    if (!loglevel) return;
    loglevelSaving = true;
    loglevelStatus = '';
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
    } catch (err) {
      loglevelStatus = err?.message || 'Failed to update log level.';
    } finally {
      loglevelSaving = false;
      clearTimeout(loglevelTimer);
      loglevelTimer = setTimeout(() => (loglevelStatus = ''), 3000);
    }
  };

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
  $: tabs = [...baseTabs, ...keyers.map((k) => ({ id: `keyer:${k.serial}`, label: k.name, serial: k.serial }))];

  const keyerFlagsForSerial = (serial) => {
    const cfg = keyers.find((k) => k.serial === serial);
    if (!cfg || cfg.type == null || !metadata?.devicetypes) return [];
    const devType = metadata.devicetypes.find((d) => d.type === cfg.type);
    return devType?.flags || [];
  };

  const keyerParam = (serial, key, fallback = 0) => {
    const cfg = keyerConfigForSerial(serial);
    if (!cfg?.param || cfg.param[key] == null) return fallback;
    return cfg.param[key];
  };

  const setKeyerStatusMsg = (serial, msg) => {
    keyerStatus = { ...keyerStatus, [serial]: msg };
    if (keyerStatusTimer[serial]) clearTimeout(keyerStatusTimer[serial]);
    keyerStatusTimer[serial] = setTimeout(() => {
      const { [serial]: _removed, ...rest } = keyerStatus;
      keyerStatus = rest;
    }, 3000);
  };

  const updateKeyerParam = async (serial, paramKey, value) => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, param: { [paramKey]: value } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, 'Keyer mode updated.');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to update keyer mode.');
    }
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
  $: activeKeyerFlags = activeSerial ? keyerFlagsForSerial(activeSerial) : [];
  $: activeKeyerMenus = activeSerial ? visibleKeyerMenus(activeSerial) : [];
  $: hasKeyerMode = activeKeyerFlags.includes('HAS_KEYER_MODE');
  $: hasR2 = activeKeyerFlags.includes('HAS_R2');
  $: hasFollowTx = activeKeyerFlags.includes('HAS_FOLLOW_TX_MODE');
  $: activeR1 = activeSerial ? (radioForm[activeSerial]?.r1 || defaultRadioForm(activeSerial, 'r1')) : null;
  $: activeR2 = activeSerial ? (radioForm[activeSerial]?.r2 || defaultRadioForm(activeSerial, 'r2')) : null;
  $: if (activeSerial) {
    ensureRadioForm(activeSerial, 'r1');
    if (hasR2) ensureRadioForm(activeSerial, 'r2');
  }

  $: activeMenuId = (activeMenu || '').toLowerCase();

  $: if (activeTab.startsWith('keyer:')) {
    console.debug('keyer-ui', {
      activeTab,
      activeMenu,
      activeMenuId,
      activeSerial,
      activeKeyerFlags,
      activeKeyerMenus: activeKeyerMenus.map((m) => m.id)
    });
  }

  $: if (activeTab.startsWith('keyer:') && activeSerial && activeSerial !== lastSerial) {
    lastSerial = activeSerial;
    activeMenu = activeKeyerMenus[0]?.id || 'summary';
  }

  const activeKeyerMenuTitle = () => {
    const menu = activeKeyerMenus.find((m) => m.id === activeMenuId);
    return menu?.title || 'Keyer';
  };

  const breadcrumb = () => {
    if (activeTab === 'home') return 'Home » Summary';
    if (activeTab === 'daemon' && activeMenu === 'ports') return 'Daemon » Ports';
    if (activeTab === 'daemon' && activeMenu === 'settings') return 'Daemon » Settings';
    if (activeTab.startsWith('keyer:')) {
      const name = activeKeyer?.name || activeSerial;
      const menu = activeKeyerMenus.find((m) => m.id === activeMenuId);
      return menu ? `${name} » ${menu.label}` : name;
    }
    return '';
  };
</script>

<div class="page">
  <header class="topbar">
    <div class="title"><span class="title-italic">m</span>huxd Device Router</div>
    <div class="top-right">
      <div class="hostname">Hostname: {runtime?.hostname || '—'} | {runtime?.daemon?.version || '—'}</div>
      <a class="help" href="http://mhuxd.dj5qv.de/doc/" target="_blank" rel="noreferrer">Help</a>
    </div>
  </header>

  <nav class="tabs">
    {#each tabs as tab}
      <button
        class={`tab ${activeTab === tab.id ? 'active' : ''}`}
        on:click={() => !tab.disabled && setTab(tab.id)}
        disabled={tab.disabled}
        type="button"
      >
        {tab.label}
      </button>
    {/each}
  </nav>

  <div class="layout">
    <aside class="left">
      <div class="left-menu">
        {#if activeTab === 'home'}
          {#each homeMenus as item}
            <button
              class={`left-menu-item ${activeMenuId === item.id ? 'active' : ''}`}
              on:click={() => (activeMenu = item.id)}
              type="button"
            >
              {item.label}
            </button>
          {/each}
        {:else if activeTab === 'daemon'}
          {#each daemonMenus as item}
            <button
              class={`left-menu-item ${activeMenuId === item.id ? 'active' : ''}`}
              on:click={() => !item.disabled && (activeMenu = item.id)}
              disabled={item.disabled}
              type="button"
            >
              {item.label}
            </button>
          {/each}
        {:else if activeTab.startsWith('keyer:')}
          {#each activeKeyerMenus as item}
            <button
              class={`left-menu-item ${activeMenuId === item.id ? 'active' : ''}`}
              on:click={() => (activeMenu = item.id)}
              type="button"
            >
              {item.label}
            </button>
          {/each}
        {:else}
          <div class="left-menu-item active">Summary</div>
        {/if}
      </div>
    </aside>

    <main class="main">
      <div class="breadcrumbs">{breadcrumb()}</div>

      {#if activeTab === 'home'}
        <h1>Summary</h1>
      {:else if activeTab === 'daemon' && activeMenu === 'ports'}
        <h1>Daemon Ports Configuration and Routing</h1>
      {:else if activeTab === 'daemon' && activeMenu === 'settings'}
        <h1>Daemon Settings</h1>
      {:else if activeTab.startsWith('keyer:')}
        <h1>{activeKeyerMenuTitle()}</h1>
      {:else}
        <h1>Keyer</h1>
      {/if}

      {#if loading}
        <div class="loading">Loading…</div>
      {:else if error}
        <div class="error">{error}</div>
      {:else}
        {#if activeTab === 'home'}
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
        {:else if activeTab === 'daemon' && activeMenu === 'ports'}
          <section class="section">
            <div class="section-title">Port List</div>
            <div class="panel">
              <div class="placeholder">Ports view not implemented yet.</div>
            </div>
          </section>

          <section class="section">
            <div class="section-title">Add Port</div>
            <div class="panel">
              <div class="placeholder">Port creation not implemented yet.</div>
            </div>
          </section>
        {:else if activeTab === 'daemon' && activeMenu === 'settings'}
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
                <div class="inline-status">{loglevelStatus}</div>
              {/if}
            </div>
          </section>
        {:else if activeTab.startsWith('keyer:')}
          {#if activeMenuId === 'mode'}
            {#if hasKeyerMode}
              <section class="section">
                <div class="section-title">Keyer Mode Radio 1</div>
                <div class="panel">
                  {#if hasFollowTx}
                    <div class="row">
                      <div class="label">Keyer Mode Follows RIG:</div>
                      <div class="value">
                        <input
                          type="checkbox"
                          checked={!!keyerParam(activeSerial, 'r1FollowTxMode')}
                          on:change={(e) => updateKeyerParam(activeSerial, 'r1FollowTxMode', e.target.checked ? 1 : 0)}
                        />
                      </div>
                    </div>
                  {/if}
                  <div class="row">
                    <div class="label">Current Mode:</div>
                    <div class="value">
                      <select
                        class="select"
                        value={keyerParam(activeSerial, 'r1KeyerMode')}
                        on:change={(e) => updateKeyerParam(activeSerial, 'r1KeyerMode', Number(e.target.value))}
                      >
                        {#each keyerModeOptions as opt}
                          <option value={opt.value}>{opt.label}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  {#if keyerStatus[activeSerial]}
                    <div class="inline-status">{keyerStatus[activeSerial]}</div>
                  {/if}
                </div>
              </section>

              {#if hasR2}
                <section class="section">
                  <div class="section-title">Keyer Mode Radio 2</div>
                  <div class="panel">
                    {#if hasFollowTx}
                      <div class="row">
                        <div class="label">Keyer Mode Follows RIG:</div>
                        <div class="value">
                          <input
                            type="checkbox"
                            checked={!!keyerParam(activeSerial, 'r2FollowTxMode')}
                            on:change={(e) => updateKeyerParam(activeSerial, 'r2FollowTxMode', e.target.checked ? 1 : 0)}
                          />
                        </div>
                      </div>
                    {/if}
                    <div class="row">
                      <div class="label">Current Mode:</div>
                      <div class="value">
                        <select
                          class="select"
                          value={keyerParam(activeSerial, 'r2KeyerMode')}
                          on:change={(e) => updateKeyerParam(activeSerial, 'r2KeyerMode', Number(e.target.value))}
                        >
                          {#each keyerModeOptions as opt}
                            <option value={opt.value}>{opt.label}</option>
                          {/each}
                        </select>
                      </div>
                    </div>
                    {#if keyerStatus[activeSerial]}
                      <div class="inline-status">{keyerStatus[activeSerial]}</div>
                    {/if}
                  </div>
                </section>
              {/if}
            {:else}
              <section class="section">
                <div class="section-title">Keyer Mode</div>
                <div class="panel">
                  <div class="placeholder">Keyer Mode is not supported on this device.</div>
                </div>
              </section>
            {/if}
          {:else if activeMenuId === 'radio'}
            {#if hasFlag(activeSerial, 'HAS_R1')}
              <section class="section">
                <div class="section-title">Radio 1</div>
                <div class="panel">
                  <div class="row">
                    <div class="label">Baud Rate:</div>
                    <div class="value">
                      <select class="select" value={activeR1?.baud} on:change={(e) => updateRadioForm(activeSerial, 'r1', 'baud', Number(e.target.value))}>
                        {#each radioBaudOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Data Bits:</div>
                    <div class="value">
                      <select class="select" value={activeR1?.databits} on:change={(e) => updateRadioForm(activeSerial, 'r1', 'databits', Number(e.target.value))}>
                        {#each radioDataBitsOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Stop Bits:</div>
                    <div class="value">
                      <select class="select" value={activeR1?.stopbits} on:change={(e) => updateRadioForm(activeSerial, 'r1', 'stopbits', Number(e.target.value))}>
                        {#each radioStopBitsOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">RTS/CTS Handshake:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!activeR1?.rtscts} on:change={(e) => updateRadioForm(activeSerial, 'r1', 'rtscts', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                </div>

                {#if hasFlag(activeSerial, 'HAS_R1_RADIO_SUPPORT')}
                  <div class="panel" style="margin-top: 12px;">
                    <div class="row">
                      <div class="label">Rig Type:</div>
                      <div class="value">
                        <select class="select" value={activeR1?.rigtype} on:change={(e) => setRigType(activeSerial, 'r1', e.target.value)}>
                          {#each rigtypes() as rig}
                            <option value={rig.id}>{rig.name}</option>
                          {/each}
                        </select>
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">Icom Address:</div>
                      <div class="value">
                        <input class="input" type="number" value={activeR1?.icomaddress} on:input={(e) => updateRadioForm(activeSerial, 'r1', 'icomaddress', Number(e.target.value))} />
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">PW1 Connected:</div>
                      <div class="value">
                        <input type="checkbox" checked={!!activeR1?.icomsimulateautoinfo} on:change={(e) => updateRadioForm(activeSerial, 'r1', 'icomsimulateautoinfo', e.target.checked ? 1 : 0)} />
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">Digital Over Voice:</div>
                      <div class="value">
                        <select class="select" value={activeR1?.digitalovervoicerule} on:change={(e) => updateRadioForm(activeSerial, 'r1', 'digitalovervoicerule', Number(e.target.value))}>
                          {#each digitalOverVoiceOptions as opt}
                            <option value={opt.value}>{opt.label}</option>
                          {/each}
                        </select>
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">Use Decoder if Connected:</div>
                      <div class="value">
                        <input type="checkbox" checked={!!activeR1?.usedecoderifconnected} on:change={(e) => updateRadioForm(activeSerial, 'r1', 'usedecoderifconnected', e.target.checked ? 1 : 0)} />
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">Don't Interfere USB control:</div>
                      <div class="value">
                        <input type="checkbox" checked={!!activeR1?.dontinterfereusbcontrol} on:change={(e) => updateRadioForm(activeSerial, 'r1', 'dontinterfereusbcontrol', e.target.checked ? 1 : 0)} />
                      </div>
                    </div>
                  </div>
                {/if}

                <div class="button-row">
                  <button class="btn" type="button" on:click={() => applyRadioChannel(activeSerial, 'r1')}>Apply</button>
                </div>
                {#if radioStatus[`${activeSerial}:r1`]}
                  <div class="inline-status">{radioStatus[`${activeSerial}:r1`]}</div>
                {/if}
              </section>
            {/if}

            {#if hasR2}
              <section class="section">
                <div class="section-title">Radio 2</div>
                <div class="panel">
                  <div class="row">
                    <div class="label">Baud Rate:</div>
                    <div class="value">
                      <select class="select" value={activeR2?.baud} on:change={(e) => updateRadioForm(activeSerial, 'r2', 'baud', Number(e.target.value))}>
                        {#each radioBaudOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Data Bits:</div>
                    <div class="value">
                      <select class="select" value={activeR2?.databits} on:change={(e) => updateRadioForm(activeSerial, 'r2', 'databits', Number(e.target.value))}>
                        {#each radioDataBitsOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Stop Bits:</div>
                    <div class="value">
                      <select class="select" value={activeR2?.stopbits} on:change={(e) => updateRadioForm(activeSerial, 'r2', 'stopbits', Number(e.target.value))}>
                        {#each radioStopBitsOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">RTS/CTS Handshake:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!activeR2?.rtscts} on:change={(e) => updateRadioForm(activeSerial, 'r2', 'rtscts', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                </div>

                {#if hasFlag(activeSerial, 'HAS_R2_RADIO_SUPPORT')}
                  <div class="panel" style="margin-top: 12px;">
                    <div class="row">
                      <div class="label">Rig Type:</div>
                      <div class="value">
                        <select class="select" value={activeR2?.rigtype} on:change={(e) => setRigType(activeSerial, 'r2', e.target.value)}>
                          {#each rigtypes() as rig}
                            <option value={rig.id}>{rig.name}</option>
                          {/each}
                        </select>
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">Icom Address:</div>
                      <div class="value">
                        <input class="input" type="number" value={activeR2?.icomaddress} on:input={(e) => updateRadioForm(activeSerial, 'r2', 'icomaddress', Number(e.target.value))} />
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">PW1 Connected:</div>
                      <div class="value">
                        <input type="checkbox" checked={!!activeR2?.icomsimulateautoinfo} on:change={(e) => updateRadioForm(activeSerial, 'r2', 'icomsimulateautoinfo', e.target.checked ? 1 : 0)} />
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">Digital Over Voice:</div>
                      <div class="value">
                        <select class="select" value={activeR2?.digitalovervoicerule} on:change={(e) => updateRadioForm(activeSerial, 'r2', 'digitalovervoicerule', Number(e.target.value))}>
                          {#each digitalOverVoiceOptions as opt}
                            <option value={opt.value}>{opt.label}</option>
                          {/each}
                        </select>
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">Use Decoder if Connected:</div>
                      <div class="value">
                        <input type="checkbox" checked={!!activeR2?.usedecoderifconnected} on:change={(e) => updateRadioForm(activeSerial, 'r2', 'usedecoderifconnected', e.target.checked ? 1 : 0)} />
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">Don't Interfere USB control:</div>
                      <div class="value">
                        <input type="checkbox" checked={!!activeR2?.dontinterfereusbcontrol} on:change={(e) => updateRadioForm(activeSerial, 'r2', 'dontinterfereusbcontrol', e.target.checked ? 1 : 0)} />
                      </div>
                    </div>
                  </div>
                {/if}

                <div class="button-row">
                  <button class="btn" type="button" on:click={() => applyRadioChannel(activeSerial, 'r2')}>Apply</button>
                </div>
                {#if radioStatus[`${activeSerial}:r2`]}
                  <div class="inline-status">{radioStatus[`${activeSerial}:r2`]}</div>
                {/if}
              </section>
            {/if}
          {:else}
            <section class="section">
              <div class="section-title">{activeKeyerMenuTitle()}</div>
              <div class="panel">
                <div class="placeholder">Keyer page placeholder.</div>
              </div>
            </section>
          {/if}
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
