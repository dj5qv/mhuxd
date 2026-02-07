<script>
  import { onMount } from 'svelte';
  import StatusDot from './lib/StatusDot.svelte';

  let runtime = null;
  let daemonCfg = null;
  let devices = [];
  let configDevices = [];
  let connectors = [];
  let metadata = null;
  let keyers = [];
  let loading = true;
  let error = '';
  let loglevel = '';
  let loglevelStatus = '';
  let loglevelStatusKind = 'success';
  let loglevelSaving = false;
  let loglevelTimer;
  let keyerStatus = {};
  let keyerStatusTimer = {};
  let radioForm = {};
  let radioStatus = {};
  let radioStatusTimer = {};
  let pttForm = {};
  let pttStatus = {};
  let pttStatusTimer = {};
  let messageForm = {};
  let messageStatus = {};
  let messageStatusTimer = {};
  let allParamForm = {};
  let allParamStatus = {};
  let allParamStatusTimer = {};
  let lastSerial = '';

  let portForm = {
    type: 'VSP',
    serial: '',
    channel: 'R1',
    devname: '',
    maxcon: 1,
    ptt_rts: false,
    ptt_dtr: false,
    remote_access: false
  };
  let portStatus = '';
  let portStatusKind = 'success';
  let portRemoveStatus = '';
  let portRemoveStatusKind = 'success';
  let portSaving = false;
  let portStatusTimer;
  let selectedConnectorIds = [];

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

  const portTypeOptions = ['VSP', 'TCP'];

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
  const fskBaudOptions = [22, 45, 45.45, 50, 56.25, 75, 100, 150, 300];
  const fskDataBitsOptions = [8, 7, 6, 5];
  const fskStopBitsOptions = [1, 1.5, 2];
  const audioOptions = [
    { value: 2, label: 'A (Mic -> Radio Mic)' },
    { value: 1, label: 'B (Soundcard -> Radio Line)' },
    { value: 3, label: 'C (Soundcard -> Radio Mic)' },
    { value: 0, label: 'D (like A + B)' }
  ];
  const audioOptionsByType = {
    4: { cw: [2], voice: [1, 2, 3], digital: [1, 2, 3] },
    5: { cw: [2], voice: [0, 1, 2, 3], digital: [0, 1, 2, 3] },
    11: { cw: [2], voice: [0, 1, 2, 3], digital: [0, 1, 2, 3] },
    6: { cw: [2], voice: [1, 2, 3], digital: [1, 2, 3] },
    7: { cw: [2], voice: [1, 2, 3], digital: [1, 2, 3] }
  };
  const mk2MicSelOptions = [
    { value: 'auto', label: 'Auto' },
    { value: 'front', label: 'Front' },
    { value: 'rear', label: 'Rear' }
  ];
  const mk2rCodecOptions = [
    { value: 1, label: 'SC1' },
    { value: 0, label: 'SC2' }
  ];
  const winkeyModeOptions = [
    { value: 0, label: 'IambicB' },
    { value: 1, label: 'IambicA' },
    { value: 2, label: 'Ultimatic' },
    { value: 3, label: 'Bug Keyer' }
  ];
  const winkeyUltimaticOptions = [
    { value: 0, label: 'Normal' },
    { value: 1, label: 'Dah' },
    { value: 2, label: 'Dit' }
  ];
  const sideToneOptions = [
    { value: 0, label: 'Off' },
    { value: 1, label: '1350 Hz' },
    { value: 2, label: '675 Hz' },
    { value: 3, label: '450 Hz' },
    { value: 4, label: '338 Hz' }
  ];
  const displayLineOptions = [
    { value: 0, label: 'Upper Line' },
    { value: 1, label: 'Lower Line' }
  ];
  const messageSlots = [1, 2, 3, 4, 5, 6, 7, 8];
  const pttOptionLabels = {
    ptt: 'PTT',
    ptt1: 'PTT1 (mic jack)',
    ptt2: 'PTT2 (rear panel jack)',
    ptt12: 'PTT1+2 (both)',
    qsk: 'QSK',
    semi: 'Semi Break-in',
    noptt: 'No PTT'
  };
  const pttOptionsByType = {
    1: { cw: ['ptt', 'qsk'] },
    2: { cw: ['ptt', 'noptt'] },
    3: { cw: ['ptt', 'qsk', 'semi'] },
    4: { cw: ['ptt1', 'qsk', 'semi', 'ptt2'], voice: ['ptt1', 'ptt2'], digital: ['ptt1', 'ptt2', 'ptt12'] },
    5: { cw: ['ptt1', 'qsk', 'semi', 'ptt2'], voice: ['ptt1', 'ptt2'], digital: ['ptt1', 'ptt2', 'ptt12'] },
    6: { cw: ['ptt1', 'qsk', 'semi', 'ptt2'], voice: ['ptt1', 'ptt2'], digital: ['ptt1', 'ptt2', 'ptt12'] },
    7: { cw: ['ptt1', 'qsk', 'semi', 'ptt2'], voice: ['ptt1', 'ptt2'], digital: ['ptt1', 'ptt2', 'ptt12'] },
    11: { cw: ['ptt1', 'qsk', 'semi', 'ptt2'], voice: ['ptt1', 'ptt2'], digital: ['ptt1', 'ptt2', 'ptt12'] }
  };
  const digitalOverVoiceOptions = [
    { value: 0, label: 'Band Map' },
    { value: 1, label: 'Always VOICE' },
    { value: 2, label: 'Always DIGITAL' }
  ];

  const hasFlag = (serial, flag) => keyerFlagsForSerial(serial).includes(flag);

  const keyerConfigForSerial = (serial) => configDevices.find((d) => d.serial === serial) || {};

  const deviceNameForSerial = (serial) => {
    const dev = devices.find((d) => d.serial === serial);
    return dev?.name || serial || '—';
  };

  const channelOptionsForSerial = (serial) => {
    if (!serial) return [];
    const flags = keyerFlagsForSerial(serial);
    const opts = [];
    if (flags.includes('HAS_R1')) opts.push('CAT1');
    if (flags.includes('HAS_R2')) opts.push('CAT2');
    if (flags.includes('HAS_FLAGS_CHANNEL')) opts.push('PTT1');
    if (flags.includes('HAS_R2') || flags.includes('HAS_FSK2')) opts.push('PTT2');
    if (flags.includes('HAS_AUX')) opts.push('AUX');
    if (flags.includes('HAS_WINKEY')) opts.push('WK');
    if (flags.includes('HAS_FSK1')) opts.push('FSK1');
    if (flags.includes('HAS_FSK2')) opts.push('FSK2');
    if (flags.includes('HAS_MCP_SUPPORT')) opts.push('MCP');
    if (flags.includes('HAS_ROTATOR_SUPPORT')) opts.push('ROTATOR');
    return opts;
  };

  const displayPortChannel = (channel) => {
    if (!channel) return '—';
    if (channel === 'R1') return 'CAT1';
    if (channel === 'R2') return 'CAT2';
    return channel;
  };

  const checkMark = (value) => (value ? '✓' : '—');

  const channelConfig = (serial, chan) => {
    const cfg = keyerConfigForSerial(serial);
    if (!cfg?.channel || !cfg.channel[chan]) return {};
    return cfg.channel[chan];
  };

  const defaultRadioForm = (serial, chan) => {
    const ch = channelConfig(serial, chan);
    const isFsk = chan === 'fsk1' || chan === 'fsk2';
    return {
      baud: ch.baud ?? (isFsk ? 45.45 : 9600),
      databits: ch.databits ?? (isFsk ? 5 : 8),
      stopbits: ch.stopbits ?? (isFsk ? 1.5 : 1),
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

  const messageConfig = (serial, kind) => {
    const cfg = keyerConfigForSerial(serial);
    if (!cfg) return [];
    return kind === 'cw' ? cfg.cwMessages || [] : cfg.fskMessages || [];
  };

  const defaultMessageForm = (serial, kind) => {
    const map = {};
    messageSlots.forEach((idx) => {
      map[idx] = '';
    });
    const items = messageConfig(serial, kind);
    items.forEach((msg) => {
      if (msg?.index) map[msg.index] = msg.text ?? '';
    });
    return map;
  };

  const ensureMessageForm = (serial, kind) => {
    if (!serial) return;
    const perSerial = messageForm[serial] || {};
    if (!perSerial[kind]) {
      messageForm = {
        ...messageForm,
        [serial]: { ...perSerial, [kind]: defaultMessageForm(serial, kind) }
      };
    }
  };

  const updateMessageForm = (serial, kind, index, value) => {
    const perSerial = messageForm[serial] || {};
    const perKind = perSerial[kind] || defaultMessageForm(serial, kind);
    messageForm = {
      ...messageForm,
      [serial]: { ...perSerial, [kind]: { ...perKind, [index]: value } }
    };
  };

  const flattenParamEntries = (param = {}) => {
    const entries = [];
    Object.entries(param || {}).forEach(([key, value]) => {
      if (value && typeof value === 'object' && !Array.isArray(value)) {
        Object.entries(value).forEach(([subKey, subVal]) => {
          entries.push({ key: `${key}.${subKey}`, value: subVal });
        });
      } else {
        entries.push({ key, value });
      }
    });
    return entries;
  };

  const defaultAllParamForm = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    const param = cfg?.param || {};
    const entries = flattenParamEntries(param);
    const map = {};
    entries.forEach((entry) => {
      map[entry.key] = entry.value;
    });
    return map;
  };

  const ensureAllParamForm = (serial) => {
    if (!serial) return;
    if (!allParamForm[serial]) {
      allParamForm = { ...allParamForm, [serial]: defaultAllParamForm(serial) };
    }
  };

  const updateAllParamForm = (serial, key, value) => {
    const perSerial = allParamForm[serial] || defaultAllParamForm(serial);
    allParamForm = {
      ...allParamForm,
      [serial]: { ...perSerial, [key]: value }
    };
  };

  const setAllParamStatusMsg = (serial, msg, kind = 'success') => {
    allParamStatus = { ...allParamStatus, [serial]: { text: msg, kind } };
    if (allParamStatusTimer[serial]) clearTimeout(allParamStatusTimer[serial]);
    allParamStatusTimer[serial] = setTimeout(() => {
      const { [serial]: _removed, ...rest } = allParamStatus;
      allParamStatus = rest;
    }, 3000);
  };

  const applyAllParams = async (serial) => {
    const perSerial = allParamForm[serial] || defaultAllParamForm(serial);
    const payload = { ...perSerial };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, param: payload })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      allParamForm = { ...allParamForm, [serial]: defaultAllParamForm(serial) };
      setAllParamStatusMsg(serial, 'All parameters applied.', 'success');
    } catch (err) {
      setAllParamStatusMsg(serial, err?.message || 'Failed to apply parameters.', 'error');
    }
  };

  const setMessageStatusMsg = (serial, kind, msg, statusKind = 'success') => {
    const key = `${serial}:${kind}`;
    messageStatus = { ...messageStatus, [key]: { text: msg, kind: statusKind } };
    if (messageStatusTimer[key]) clearTimeout(messageStatusTimer[key]);
    messageStatusTimer[key] = setTimeout(() => {
      const { [key]: _removed, ...rest } = messageStatus;
      messageStatus = rest;
    }, 3000);
  };

  const applyMessages = async (serial, kind) => {
    const perSerial = messageForm[serial] || {};
    const perKind = perSerial[kind] || defaultMessageForm(serial, kind);
    const payload = messageSlots.map((idx) => ({ index: idx, text: perKind[idx] ?? '' }));
    const key = kind === 'cw' ? 'cwMessages' : 'fskMessages';

    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, [key]: payload })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      messageForm = {
        ...messageForm,
        [serial]: { ...messageForm[serial], [kind]: defaultMessageForm(serial, kind) }
      };
      setMessageStatusMsg(serial, kind, `${kind.toUpperCase()} messages applied.`, 'success');
    } catch (err) {
      setMessageStatusMsg(serial, kind, err?.message || `Failed to apply ${kind.toUpperCase()} messages.`, 'error');
    }
  };

  const audioOptionsFor = (type, mode) => {
    const list = audioOptionsByType[type]?.[mode];
    return list || [1, 2, 3];
  };

  const audioOptionLabel = (value) => audioOptions.find((o) => o.value === value)?.label || value;

  const mk2MicSelValue = (serial) => {
    if (keyerParam(serial, 'micSelAuto')) return 'auto';
    if (keyerParam(serial, 'micSelFront')) return 'front';
    return 'rear';
  };

  const displayBackgroundOptions = (type) => {
    if (!metadata?.devicetypes) return [];
    const devType = metadata.devicetypes.find((d) => d.type === type);
    return devType?.displaybackground || [];
  };

  const displayEventOptions = (type) => {
    if (!metadata?.devicetypes) return [];
    const devType = metadata.devicetypes.find((d) => d.type === type);
    return devType?.displayevent || [];
  };

  const pttOptionsFor = (type, mode) => {
    const byType = pttOptionsByType[type];
    if (byType) return byType[mode] || [];
    if (mode === 'cw') return ['ptt1', 'ptt2', 'ptt12', 'qsk', 'semi', 'ptt', 'noptt'];
    return ['ptt1', 'ptt2', 'ptt12'];
  };

  const pttConfigForSerial = (serial) => keyerConfigForSerial(serial)?.ptt || {};

  const defaultPttForm = (serial, chan) => {
    const cfg = keyerConfigForSerial(serial);
    const type = cfg?.type ?? 0;
    const ptt = pttConfigForSerial(serial)?.[chan] || {};
    const cwOpts = pttOptionsFor(type, 'cw');
    const voiceOpts = pttOptionsFor(type, 'voice');
    const digitalOpts = pttOptionsFor(type, 'digital');
    return {
      cw: ptt.cw ?? cwOpts[0] ?? 'ptt1',
      voice: voiceOpts.length ? (ptt.voice ?? voiceOpts[0]) : null,
      digital: digitalOpts.length ? (ptt.digital ?? digitalOpts[0]) : null
    };
  };

  const ensurePttForm = (serial, chan) => {
    if (!serial) return;
    const perSerial = pttForm[serial] || {};
    if (!perSerial[chan]) {
      pttForm = {
        ...pttForm,
        [serial]: { ...perSerial, [chan]: defaultPttForm(serial, chan) }
      };
    }
  };

  const updatePttForm = (serial, chan, key, value) => {
    const perSerial = pttForm[serial] || {};
    const perChan = perSerial[chan] || defaultPttForm(serial, chan);
    pttForm = {
      ...pttForm,
      [serial]: { ...perSerial, [chan]: { ...perChan, [key]: value } }
    };
  };

  const setPttStatusMsg = (serial, chan, msg, kind = 'success') => {
    const key = `${serial}:${chan}`;
    pttStatus = { ...pttStatus, [key]: { text: msg, kind } };
    if (pttStatusTimer[key]) clearTimeout(pttStatusTimer[key]);
    pttStatusTimer[key] = setTimeout(() => {
      const { [key]: _removed, ...rest } = pttStatus;
      pttStatus = rest;
    }, 3000);
  };

  const applyPttChannel = async (serial, chan) => {
    const cfg = keyerConfigForSerial(serial);
    const type = cfg?.type ?? 0;
    const form = pttForm[serial]?.[chan] || defaultPttForm(serial, chan);
    const cwOpts = pttOptionsFor(type, 'cw');
    const voiceOpts = pttOptionsFor(type, 'voice');
    const digitalOpts = pttOptionsFor(type, 'digital');
    const payload = { cw: form.cw };
    if (voiceOpts.length && form.voice) payload.voice = form.voice;
    if (digitalOpts.length && form.digital) payload.digital = form.digital;

    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, ptt: { [chan]: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      pttForm = {
        ...pttForm,
        [serial]: { ...pttForm[serial], [chan]: defaultPttForm(serial, chan) }
      };
      setPttStatusMsg(serial, chan, 'PTT settings applied.', 'success');
    } catch (err) {
      setPttStatusMsg(serial, chan, err?.message || 'Failed to apply PTT settings.', 'error');
    }
  };

  const applyPttChange = async (serial, chan, key, value) => {
    updatePttForm(serial, chan, key, value);
    await applyPttChannel(serial, chan);
  };

  const setRadioStatusMsg = (serial, chan, msg, kind = 'success') => {
    const key = `${serial}:${chan}`;
    radioStatus = { ...radioStatus, [key]: { text: msg, kind } };
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
      setRadioStatusMsg(serial, chan, 'Radio settings applied.', 'success');
    } catch (err) {
      setRadioStatusMsg(serial, chan, err?.message || 'Failed to apply radio settings.', 'error');
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
      connectors = configDevicesRsp.connectors || [];
      metadata = metadataRsp;

      if (!portForm.serial && devices.length) {
        const serial = devices[0].serial || '';
        const opts = channelOptionsForSerial(serial);
        portForm = { ...portForm, serial, channel: opts[0] || '' };
      }

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

  const buildConnectorPayload = () => {
    if (!portForm.serial || !portForm.channel || !portForm.type || !portForm.devname) return null;
    const base = {
      serial: portForm.serial,
      channel: portForm.channel,
      type: portForm.type,
      devname: String(portForm.devname)
    };
    if (portForm.maxcon) base.maxcon = Number(portForm.maxcon);
    if (portForm.type === 'VSP') {
      base.ptt_rts = portForm.ptt_rts ? 1 : 0;
      base.ptt_dtr = portForm.ptt_dtr ? 1 : 0;
    } else if (portForm.type === 'TCP') {
      base.remote_access = portForm.remote_access ? 1 : 0;
    }
    return base;
  };

  const toggleConnectorSelection = (id) => {
    if (id == null) return;
    if (selectedConnectorIds.includes(id)) {
      selectedConnectorIds = selectedConnectorIds.filter((v) => v !== id);
    } else {
      selectedConnectorIds = [...selectedConnectorIds, id];
    }
  };

  const applyPort = async () => {
    const connector = buildConnectorPayload();
    if (!connector) {
      portStatus = 'Please fill out all required fields.';
      portStatusKind = 'error';
      clearTimeout(portStatusTimer);
      portStatusTimer = setTimeout(() => (portStatus = ''), 3000);
      return;
    }

    portSaving = true;
    portStatus = '';
    portStatusKind = 'success';
    try {
      const res = await fetch('/api/v1/config/devices', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ connectors: [connector] })
      });
      if (!res.ok) throw new Error(`config/devices ${res.status}`);
      const data = await res.json();
      if (Array.isArray(data?.devices)) configDevices = data.devices;
      if (Array.isArray(data?.connectors)) connectors = data.connectors;
      portStatus = 'Port saved.';
      portStatusKind = 'success';
    } catch (err) {
      portStatus = err?.message || 'Failed to save port.';
      portStatusKind = 'error';
    } finally {
      portSaving = false;
      clearTimeout(portStatusTimer);
      portStatusTimer = setTimeout(() => (portStatus = ''), 3000);
    }
  };

  const updatePortSerial = (serial) => {
    const opts = channelOptionsForSerial(serial);
    const nextChannel = opts.includes(portForm.channel) ? portForm.channel : opts[0] || '';
    portForm = { ...portForm, serial, channel: nextChannel };
  };

  const removePorts = async () => {
    if (!selectedConnectorIds.length) return;
    portSaving = true;
    portRemoveStatus = '';
    portRemoveStatusKind = 'success';
    try {
      const res = await fetch('/api/v1/config/devices', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ connectorsRemove: selectedConnectorIds })
      });
      if (!res.ok) throw new Error(`config/devices ${res.status}`);
      const data = await res.json();
      if (Array.isArray(data?.devices)) configDevices = data.devices;
      if (Array.isArray(data?.connectors)) connectors = data.connectors;
      selectedConnectorIds = [];
      portRemoveStatus = 'Port(s) removed.';
      portRemoveStatusKind = 'success';
    } catch (err) {
      portRemoveStatus = err?.message || 'Failed to remove port(s).';
      portRemoveStatusKind = 'error';
    } finally {
      portSaving = false;
      clearTimeout(portStatusTimer);
      portStatusTimer = setTimeout(() => {
        portRemoveStatus = '';
      }, 3000);
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

  const setKeyerStatusMsg = (serial, msg, kind = 'success') => {
    keyerStatus = { ...keyerStatus, [serial]: { text: msg, kind } };
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
      setKeyerStatusMsg(serial, 'Keyer mode updated.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to update keyer mode.', 'error');
    }
  };

  const updateWinkeyParam = async (serial, key, value) => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, winkey: { [key]: value } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, 'Winkey settings updated.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to update winkey settings.', 'error');
    }
  };

  const winkeyValue = (serial, key, fallback = 0) => {
    const cfg = keyerConfigForSerial(serial);
    if (!cfg?.winkey || cfg.winkey[key] == null) return fallback;
    return cfg.winkey[key];
  };

  const updateKeyerParams = async (serial, params, msg = 'Settings updated.') => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, param: params })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, msg, 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to update settings.', 'error');
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
  $: hasAux = activeKeyerFlags.includes('HAS_AUX');
  $: hasFsk1 = activeKeyerFlags.includes('HAS_FSK1');
  $: hasFsk2 = activeKeyerFlags.includes('HAS_FSK2');
  $: hasPfsk = activeKeyerFlags.includes('HAS_PFSK');
  $: hasFollowTx = activeKeyerFlags.includes('HAS_FOLLOW_TX_MODE');
  $: hasLnaPaPtt = activeKeyerFlags.includes('HAS_LNA_PA_PTT');
  $: hasLnaPaPttTail = activeKeyerFlags.includes('HAS_LNA_PA_PTT_TAIL');
  $: hasSoundcardPtt = activeKeyerFlags.includes('HAS_SOUNDCARD_PTT');
  $: hasCwInVoice = activeKeyerFlags.includes('HAS_CW_IN_VOICE');
  $: isAudioMk1 = activeKeyer?.type === 4;
  $: isAudioMk2 = activeKeyer?.type === 5 || activeKeyer?.type === 11;
  $: isAudioMk2R = activeKeyer?.type === 6 || activeKeyer?.type === 7;
  $: activeR1 = activeSerial ? (radioForm[activeSerial]?.r1 || defaultRadioForm(activeSerial, 'r1')) : null;
  $: activeR2 = activeSerial ? (radioForm[activeSerial]?.r2 || defaultRadioForm(activeSerial, 'r2')) : null;
  $: activeAux = activeSerial ? (radioForm[activeSerial]?.aux || defaultRadioForm(activeSerial, 'aux')) : null;
  $: activeFsk1 = activeSerial ? (radioForm[activeSerial]?.fsk1 || defaultRadioForm(activeSerial, 'fsk1')) : null;
  $: activeFsk2 = activeSerial ? (radioForm[activeSerial]?.fsk2 || defaultRadioForm(activeSerial, 'fsk2')) : null;
  $: activePttR1 = activeSerial ? (pttForm[activeSerial]?.r1 || defaultPttForm(activeSerial, 'r1')) : null;
  $: activePttR2 = activeSerial ? (pttForm[activeSerial]?.r2 || defaultPttForm(activeSerial, 'r2')) : null;
  $: if (activeSerial) {
    ensureRadioForm(activeSerial, 'r1');
    if (hasR2) ensureRadioForm(activeSerial, 'r2');
    if (hasAux) ensureRadioForm(activeSerial, 'aux');
    if (hasFsk1) ensureRadioForm(activeSerial, 'fsk1');
    if (hasFsk2) ensureRadioForm(activeSerial, 'fsk2');
    ensurePttForm(activeSerial, 'r1');
    if (hasR2) ensurePttForm(activeSerial, 'r2');
    ensureMessageForm(activeSerial, 'cw');
    ensureMessageForm(activeSerial, 'fsk');
    ensureAllParamForm(activeSerial);
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
            <div class="panel table ports-grid">
              <div class="table-header">
                <div></div>
                <div>ID</div>
                <div>Type</div>
                <div>Port / Device</div>
                <div>Status</div>
                <div>Remote Access</div>
                <div>RTS-PTT</div>
                <div>DTR-PTT</div>
                <div>Destination Name</div>
                <div>Destination Serial</div>
                <div>Channel</div>
              </div>
              {#if connectors.length === 0}
                <div class="table-empty">No ports configured.</div>
              {:else}
                {#each connectors as c, i}
                  <div class={`table-row ${i % 2 ? 'alt' : ''}`}>
                    <div>
                      <input
                        type="checkbox"
                        checked={selectedConnectorIds.includes(c.id)}
                        disabled={c.id == null}
                        on:change={() => toggleConnectorSelection(c.id)}
                      />
                    </div>
                    <div>{c.id ?? '—'}</div>
                    <div>{c.type || '—'}</div>
                    <div>{c.devname || '—'}</div>
                    <div>—</div>
                    <div>{c.type === 'TCP' ? checkMark(c.remote_access) : '—'}</div>
                    <div>{c.type === 'VSP' ? checkMark(c.ptt_rts) : '—'}</div>
                    <div>{c.type === 'VSP' ? checkMark(c.ptt_dtr) : '—'}</div>
                    <div>{deviceNameForSerial(c.serial)}</div>
                    <div>{c.serial || '—'}</div>
                    <div>{displayPortChannel(c.channel)}</div>
                  </div>
                {/each}
              {/if}
            </div>
            <div class="button-row">
              <button class="btn" on:click={removePorts} disabled={!selectedConnectorIds.length || portSaving}>
                Remove
              </button>
            </div>
            {#if portRemoveStatus}
              <div class={`inline-status ${portRemoveStatusKind === 'error' ? 'error' : ''}`}>{portRemoveStatus}</div>
            {/if}
          </section>

          <section class="section">
            <div class="section-title">Add Port</div>
            <div class="panel">
              <div class="row">
                <div class="label">Port Type:</div>
                <div class="value">
                  <div class="inline-list">
                    {#each portTypeOptions as opt}
                      <label class="radio-option">
                        <input type="radio" bind:group={portForm.type} value={opt} />
                        {opt === 'VSP' ? 'VSP Virtual Serial Port' : 'TCP Network Port'}
                      </label>
                    {/each}
                  </div>
                </div>
              </div>

              <div class="row">
                <div class="label">Destination Keyer:</div>
                <div class="value">
                  <select class="select" value={portForm.serial} on:change={(e) => updatePortSerial(e.target.value)}>
                    {#if keyers.length === 0}
                      <option value="">No keyers available</option>
                    {:else}
                      {#each keyers as k}
                        <option value={k.serial}>{k.name} ({k.serial})</option>
                      {/each}
                    {/if}
                  </select>
                </div>
              </div>

              <div class="row">
                <div class="label">Destination Channel:</div>
                <div class="value">
                  <select class="select" bind:value={portForm.channel} disabled={channelOptionsForSerial(portForm.serial).length === 0}>
                    {#if channelOptionsForSerial(portForm.serial).length === 0}
                      <option value="">No channels</option>
                    {:else}
                      {#each channelOptionsForSerial(portForm.serial) as ch}
                        <option value={ch}>{ch}</option>
                      {/each}
                    {/if}
                  </select>
                </div>
              </div>

              {#if portForm.type === 'VSP'}
                <div class="row">
                  <div class="label">Device Path:</div>
                  <div class="value">
                    /dev/mhuxd/
                    <input class="input" type="text" bind:value={portForm.devname} placeholder="ttyMHUXD0" />
                  </div>
                </div>
                <div class="row">
                  <div class="label">PTT via RTS:</div>
                  <div class="value">
                    <input type="checkbox" bind:checked={portForm.ptt_rts} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">PTT via DTR:</div>
                  <div class="value">
                    <input type="checkbox" bind:checked={portForm.ptt_dtr} />
                  </div>
                </div>
              {:else}
                <div class="row">
                  <div class="label">Port Number:</div>
                  <div class="value">
                    <input class="input" type="number" min="1" step="1" inputmode="numeric" bind:value={portForm.devname} placeholder="9001" />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Remote Accessible:</div>
                  <div class="value">
                    <input type="checkbox" bind:checked={portForm.remote_access} />
                  </div>
                </div>
              {/if}

              <div class="row">
                <div class="label">Max Connections:</div>
                <div class="value">
                  <input class="input" type="number" min="1" bind:value={portForm.maxcon} />
                </div>
              </div>

              <div class="button-row">
                <button class="btn" on:click={applyPort} disabled={portSaving || keyers.length === 0}>
                  {portSaving ? 'Saving…' : 'Create'}
                </button>
              </div>
              {#if portStatus}
                <div class={`inline-status ${portStatusKind === 'error' ? 'error' : ''}`}>{portStatus}</div>
              {/if}
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
                <div class={`inline-status ${loglevelStatusKind === 'error' ? 'error' : ''}`}>{loglevelStatus}</div>
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
                    <div class={`inline-status ${keyerStatus[activeSerial].kind === 'error' ? 'error' : ''}`}>
                      {keyerStatus[activeSerial].text}
                    </div>
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
                      <div class={`inline-status ${keyerStatus[activeSerial].kind === 'error' ? 'error' : ''}`}>
                        {keyerStatus[activeSerial].text}
                      </div>
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
                  <div class={`inline-status ${radioStatus[`${activeSerial}:r1`].kind === 'error' ? 'error' : ''}`}>
                    {radioStatus[`${activeSerial}:r1`].text}
                  </div>
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
                  <div class={`inline-status ${radioStatus[`${activeSerial}:r2`].kind === 'error' ? 'error' : ''}`}>
                    {radioStatus[`${activeSerial}:r2`].text}
                  </div>
                {/if}
              </section>
            {/if}
          {:else if activeMenuId === 'aux'}
            {#if hasAux}
              <section class="section">
                <div class="section-title">AUX</div>
                <div class="panel">
                  <div class="row">
                    <div class="label">Baud Rate:</div>
                    <div class="value">
                      <select class="select" value={activeAux?.baud} on:change={(e) => updateRadioForm(activeSerial, 'aux', 'baud', Number(e.target.value))}>
                        {#each radioBaudOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Data Bits:</div>
                    <div class="value">
                      <select class="select" value={activeAux?.databits} on:change={(e) => updateRadioForm(activeSerial, 'aux', 'databits', Number(e.target.value))}>
                        {#each radioDataBitsOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Stop Bits:</div>
                    <div class="value">
                      <select class="select" value={activeAux?.stopbits} on:change={(e) => updateRadioForm(activeSerial, 'aux', 'stopbits', Number(e.target.value))}>
                        {#each radioStopBitsOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">RTS/CTS Handshake:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!activeAux?.rtscts} on:change={(e) => updateRadioForm(activeSerial, 'aux', 'rtscts', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                </div>

                <div class="button-row">
                  <button class="btn" type="button" on:click={() => applyRadioChannel(activeSerial, 'aux')}>Apply</button>
                </div>
                {#if radioStatus[`${activeSerial}:aux`]}
                  <div class={`inline-status ${radioStatus[`${activeSerial}:aux`].kind === 'error' ? 'error' : ''}`}>
                    {radioStatus[`${activeSerial}:aux`].text}
                  </div>
                {/if}
              </section>
            {:else}
              <section class="section">
                <div class="section-title">AUX</div>
                <div class="panel">
                  <div class="placeholder">AUX is not supported on this device.</div>
                </div>
              </section>
            {/if}
          {:else if activeMenuId === 'fsk'}
            {#if hasFsk1}
              <section class="section">
                <div class="section-title">FSK 1</div>
                <div class="panel">
                  <div class="row">
                    <div class="label">Baud Rate:</div>
                    <div class="value">
                      <select class="select" value={activeFsk1?.baud} on:change={(e) => updateRadioForm(activeSerial, 'fsk1', 'baud', Number(e.target.value))}>
                        {#each fskBaudOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Data Bits:</div>
                    <div class="value">
                      <select class="select" value={activeFsk1?.databits} on:change={(e) => updateRadioForm(activeSerial, 'fsk1', 'databits', Number(e.target.value))}>
                        {#each fskDataBitsOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Stop Bits:</div>
                    <div class="value">
                      <select class="select" value={activeFsk1?.stopbits} on:change={(e) => updateRadioForm(activeSerial, 'fsk1', 'stopbits', Number(e.target.value))}>
                        {#each fskStopBitsOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">RTS/CTS Handshake:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!activeFsk1?.rtscts} on:change={(e) => updateRadioForm(activeSerial, 'fsk1', 'rtscts', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Invert FSK:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!keyerParam(activeSerial, 'r1InvertFsk')} on:change={(e) => updateKeyerParam(activeSerial, 'r1InvertFsk', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                  {#if hasPfsk}
                    <div class="row">
                      <div class="label">Pseudo FSK:</div>
                      <div class="value">
                        <input type="checkbox" checked={!!keyerParam(activeSerial, 'usePFsk')} on:change={(e) => updateKeyerParam(activeSerial, 'usePFsk', e.target.checked ? 1 : 0)} />
                      </div>
                    </div>
                  {/if}
                </div>

                <div class="button-row">
                  <button class="btn" type="button" on:click={() => applyRadioChannel(activeSerial, 'fsk1')}>Apply</button>
                </div>
                {#if radioStatus[`${activeSerial}:fsk1`]}
                  <div class={`inline-status ${radioStatus[`${activeSerial}:fsk1`].kind === 'error' ? 'error' : ''}`}>
                    {radioStatus[`${activeSerial}:fsk1`].text}
                  </div>
                {/if}
              </section>
            {/if}

            {#if hasFsk2}
              <section class="section">
                <div class="section-title">FSK 2</div>
                <div class="panel">
                  <div class="row">
                    <div class="label">Baud Rate:</div>
                    <div class="value">
                      <select class="select" value={activeFsk2?.baud} on:change={(e) => updateRadioForm(activeSerial, 'fsk2', 'baud', Number(e.target.value))}>
                        {#each fskBaudOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Data Bits:</div>
                    <div class="value">
                      <select class="select" value={activeFsk2?.databits} on:change={(e) => updateRadioForm(activeSerial, 'fsk2', 'databits', Number(e.target.value))}>
                        {#each fskDataBitsOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Stop Bits:</div>
                    <div class="value">
                      <select class="select" value={activeFsk2?.stopbits} on:change={(e) => updateRadioForm(activeSerial, 'fsk2', 'stopbits', Number(e.target.value))}>
                        {#each fskStopBitsOptions as opt}
                          <option value={opt}>{opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">RTS/CTS Handshake:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!activeFsk2?.rtscts} on:change={(e) => updateRadioForm(activeSerial, 'fsk2', 'rtscts', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Invert FSK:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!keyerParam(activeSerial, 'r2InvertFsk')} on:change={(e) => updateKeyerParam(activeSerial, 'r2InvertFsk', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                  {#if hasPfsk}
                    <div class="row">
                      <div class="label">Pseudo FSK:</div>
                      <div class="value">
                        <input type="checkbox" checked={!!keyerParam(activeSerial, 'usePFsk')} on:change={(e) => updateKeyerParam(activeSerial, 'usePFsk', e.target.checked ? 1 : 0)} />
                      </div>
                    </div>
                  {/if}
                </div>

                <div class="button-row">
                  <button class="btn" type="button" on:click={() => applyRadioChannel(activeSerial, 'fsk2')}>Apply</button>
                </div>
                {#if radioStatus[`${activeSerial}:fsk2`]}
                  <div class={`inline-status ${radioStatus[`${activeSerial}:fsk2`].kind === 'error' ? 'error' : ''}`}>
                    {radioStatus[`${activeSerial}:fsk2`].text}
                  </div>
                {/if}
              </section>
            {/if}

            {#if !hasFsk1 && !hasFsk2}
              <section class="section">
                <div class="section-title">FSK</div>
                <div class="panel">
                  <div class="placeholder">FSK is not supported on this device.</div>
                </div>
              </section>
            {/if}
          {:else if activeMenuId === 'ptt'}
            <section class="section">
              <div class="section-title">PTT Radio 1</div>
              <div class="panel">
                <div class="row">
                  <div class="label">PTT CW:</div>
                  <div class="value">
                    <select
                      class="select"
                      value={activePttR1?.cw}
                      on:change={(e) => applyPttChange(activeSerial, 'r1', 'cw', e.target.value)}
                    >
                      {#each pttOptionsFor(activeKeyer?.type ?? 0, 'cw') as opt}
                        <option value={opt}>{pttOptionLabels[opt] || opt}</option>
                      {/each}
                    </select>
                  </div>
                </div>
                {#if pttOptionsFor(activeKeyer?.type ?? 0, 'voice').length}
                  <div class="row">
                    <div class="label">PTT Voice:</div>
                    <div class="value">
                      <select
                        class="select"
                        value={activePttR1?.voice}
                        on:change={(e) => applyPttChange(activeSerial, 'r1', 'voice', e.target.value)}
                      >
                        {#each pttOptionsFor(activeKeyer?.type ?? 0, 'voice') as opt}
                          <option value={opt}>{pttOptionLabels[opt] || opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                {/if}
                {#if pttOptionsFor(activeKeyer?.type ?? 0, 'digital').length}
                  <div class="row">
                    <div class="label">PTT Digital:</div>
                    <div class="value">
                      <select
                        class="select"
                        value={activePttR1?.digital}
                        on:change={(e) => applyPttChange(activeSerial, 'r1', 'digital', e.target.value)}
                      >
                        {#each pttOptionsFor(activeKeyer?.type ?? 0, 'digital') as opt}
                          <option value={opt}>{pttOptionLabels[opt] || opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                {/if}

                {#if hasLnaPaPtt}
                  <div class="row">
                    <div class="label">LNA PTT:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!keyerParam(activeSerial, 'r1LnaPtt')} on:change={(e) => updateKeyerParam(activeSerial, 'r1LnaPtt', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">PA PTT:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!keyerParam(activeSerial, 'r1PaPtt')} on:change={(e) => updateKeyerParam(activeSerial, 'r1PaPtt', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                {/if}

                {#if hasLnaPaPttTail}
                  <div class="row">
                    <div class="label">PA PTT Tail (x 10ms):</div>
                    <div class="value">
                      <input class="input" type="number" value={keyerParam(activeSerial, 'r1PaPttTail')} on:input={(e) => updateKeyerParam(activeSerial, 'r1PaPttTail', Number(e.target.value))} />
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">LNA PTT Tail (x 10ms):</div>
                    <div class="value">
                      <input class="input" type="number" value={keyerParam(activeSerial, 'r1LnaPttTail')} on:input={(e) => updateKeyerParam(activeSerial, 'r1LnaPttTail', Number(e.target.value))} />
                    </div>
                  </div>
                {/if}

                <div class="row">
                  <div class="label">PTT Lead (x 10ms):</div>
                  <div class="value">
                    <input class="input" type="number" value={keyerParam(activeSerial, 'r1PttDelay')} on:input={(e) => updateKeyerParam(activeSerial, 'r1PttDelay', Number(e.target.value))} />
                  </div>
                </div>

                {#if hasCwInVoice}
                  <div class="row">
                    <div class="label">CW in Voice:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!keyerParam(activeSerial, 'r1AllowCwInVoice')} on:change={(e) => updateKeyerParam(activeSerial, 'r1AllowCwInVoice', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                {/if}

                {#if hasSoundcardPtt}
                  <div class="row">
                    <div class="label">Sound Card PTT:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!keyerParam(activeSerial, 'useAutoPtt')} on:change={(e) => updateKeyerParam(activeSerial, 'useAutoPtt', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Downstream over Footswitch:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!keyerParam(activeSerial, 'downstreamOverFootSw')} on:change={(e) => updateKeyerParam(activeSerial, 'downstreamOverFootSw', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                {/if}
              </div>

              {#if pttStatus[`${activeSerial}:r1`]}
                <div class={`inline-status ${pttStatus[`${activeSerial}:r1`].kind === 'error' ? 'error' : ''}`}>
                  {pttStatus[`${activeSerial}:r1`].text}
                </div>
              {/if}
            </section>

            {#if hasR2}
              <section class="section">
                <div class="section-title">PTT Radio 2</div>
                <div class="panel">
                  <div class="row">
                    <div class="label">PTT CW:</div>
                    <div class="value">
                      <select
                        class="select"
                        value={activePttR2?.cw}
                        on:change={(e) => applyPttChange(activeSerial, 'r2', 'cw', e.target.value)}
                      >
                        {#each pttOptionsFor(activeKeyer?.type ?? 0, 'cw') as opt}
                          <option value={opt}>{pttOptionLabels[opt] || opt}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  {#if pttOptionsFor(activeKeyer?.type ?? 0, 'voice').length}
                    <div class="row">
                      <div class="label">PTT Voice:</div>
                      <div class="value">
                        <select
                          class="select"
                          value={activePttR2?.voice}
                          on:change={(e) => applyPttChange(activeSerial, 'r2', 'voice', e.target.value)}
                        >
                          {#each pttOptionsFor(activeKeyer?.type ?? 0, 'voice') as opt}
                            <option value={opt}>{pttOptionLabels[opt] || opt}</option>
                          {/each}
                        </select>
                      </div>
                    </div>
                  {/if}
                  {#if pttOptionsFor(activeKeyer?.type ?? 0, 'digital').length}
                    <div class="row">
                      <div class="label">PTT Digital:</div>
                      <div class="value">
                        <select
                          class="select"
                          value={activePttR2?.digital}
                          on:change={(e) => applyPttChange(activeSerial, 'r2', 'digital', e.target.value)}
                        >
                          {#each pttOptionsFor(activeKeyer?.type ?? 0, 'digital') as opt}
                            <option value={opt}>{pttOptionLabels[opt] || opt}</option>
                          {/each}
                        </select>
                      </div>
                    </div>
                  {/if}

                  {#if hasLnaPaPtt}
                    <div class="row">
                      <div class="label">LNA PTT:</div>
                      <div class="value">
                        <input type="checkbox" checked={!!keyerParam(activeSerial, 'r2LnaPtt')} on:change={(e) => updateKeyerParam(activeSerial, 'r2LnaPtt', e.target.checked ? 1 : 0)} />
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">PA PTT:</div>
                      <div class="value">
                        <input type="checkbox" checked={!!keyerParam(activeSerial, 'r2PaPtt')} on:change={(e) => updateKeyerParam(activeSerial, 'r2PaPtt', e.target.checked ? 1 : 0)} />
                      </div>
                    </div>
                  {/if}

                  {#if hasLnaPaPttTail}
                    <div class="row">
                      <div class="label">PA PTT Tail (x 10ms):</div>
                      <div class="value">
                        <input class="input" type="number" value={keyerParam(activeSerial, 'r2PaPttTail')} on:input={(e) => updateKeyerParam(activeSerial, 'r2PaPttTail', Number(e.target.value))} />
                      </div>
                    </div>
                    <div class="row">
                      <div class="label">LNA PTT Tail (x 10ms):</div>
                      <div class="value">
                        <input class="input" type="number" value={keyerParam(activeSerial, 'r2LnaPttTail')} on:input={(e) => updateKeyerParam(activeSerial, 'r2LnaPttTail', Number(e.target.value))} />
                      </div>
                    </div>
                  {/if}

                  <div class="row">
                    <div class="label">PTT Lead (x 10ms):</div>
                    <div class="value">
                      <input class="input" type="number" value={keyerParam(activeSerial, 'r2PttDelay')} on:input={(e) => updateKeyerParam(activeSerial, 'r2PttDelay', Number(e.target.value))} />
                    </div>
                  </div>

                  {#if hasCwInVoice}
                    <div class="row">
                      <div class="label">CW in Voice:</div>
                      <div class="value">
                        <input type="checkbox" checked={!!keyerParam(activeSerial, 'r2AllowCwInVoice')} on:change={(e) => updateKeyerParam(activeSerial, 'r2AllowCwInVoice', e.target.checked ? 1 : 0)} />
                      </div>
                    </div>
                  {/if}
                </div>

                {#if pttStatus[`${activeSerial}:r2`]}
                  <div class={`inline-status ${pttStatus[`${activeSerial}:r2`].kind === 'error' ? 'error' : ''}`}>
                    {pttStatus[`${activeSerial}:r2`].text}
                  </div>
                {/if}
              </section>
            {/if}

            <section class="section">
              <div class="section-title">Footswitch Sequencer</div>
              <div class="panel">
                <div class="row">
                  <div class="label">Mute serial CW:</div>
                  <div class="value">
                    <input type="checkbox" checked={!!keyerParam(activeSerial, 'muteCompCw')} on:change={(e) => updateKeyerParam(activeSerial, 'muteCompCw', e.target.checked ? 1 : 0)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Mute serial FSK:</div>
                  <div class="value">
                    <input type="checkbox" checked={!!keyerParam(activeSerial, 'muteCompFsk')} on:change={(e) => updateKeyerParam(activeSerial, 'muteCompFsk', e.target.checked ? 1 : 0)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Restore serial PTT and audio:</div>
                  <div class="value">
                    <input type="checkbox" checked={!!keyerParam(activeSerial, 'restorePtt')} on:change={(e) => updateKeyerParam(activeSerial, 'restorePtt', e.target.checked ? 1 : 0)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Restore serial CW:</div>
                  <div class="value">
                    <input type="checkbox" checked={!!keyerParam(activeSerial, 'restoreCompCw')} on:change={(e) => updateKeyerParam(activeSerial, 'restoreCompCw', e.target.checked ? 1 : 0)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Restore serial FSK:</div>
                  <div class="value">
                    <input type="checkbox" checked={!!keyerParam(activeSerial, 'restoreCompFsk')} on:change={(e) => updateKeyerParam(activeSerial, 'restoreCompFsk', e.target.checked ? 1 : 0)} />
                  </div>
                </div>
              </div>
            </section>
          {:else if activeMenuId === 'audio'}
            {#if isAudioMk2}
              <section class="section">
                <div class="section-title">Audio Switching</div>
                <div class="panel table audio-grid-4">
                  <div class="table-header">
                    <div>Mode</div>
                    <div>Receive</div>
                    <div>Transmit</div>
                    <div>Transmit with Footswitch</div>
                  </div>
                  {#each ['cw', 'voice', 'digital'] as mode}
                    <div class="table-row">
                      <div>{mode.toUpperCase()}</div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRx`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRx`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTx`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTx`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSw`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSw`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                    </div>
                  {/each}
                </div>
              </section>

              <section class="section">
                <div class="section-title">Audio Options</div>
                <div class="panel table audio-grid-4">
                  <div class="table-header">
                    <div></div>
                    <div>CW</div>
                    <div>Voice</div>
                    <div>Digital</div>
                  </div>
                  <div class="table-row">
                    <div>Enable On Air Recording</div>
                    <div><input type="checkbox" checked={!!keyerParam(activeSerial, 'r1FrMpkExtra_Cw.onAirRecActive')} on:change={(e) => updateKeyerParam(activeSerial, 'r1FrMpkExtra_Cw.onAirRecActive', e.target.checked ? 1 : 0)} /></div>
                    <div><input type="checkbox" checked={!!keyerParam(activeSerial, 'r1FrMpkExtra_Voice.onAirRecActive')} on:change={(e) => updateKeyerParam(activeSerial, 'r1FrMpkExtra_Voice.onAirRecActive', e.target.checked ? 1 : 0)} /></div>
                    <div><input type="checkbox" checked={!!keyerParam(activeSerial, 'r1FrMpkExtra_Digital.onAirRecActive')} on:change={(e) => updateKeyerParam(activeSerial, 'r1FrMpkExtra_Digital.onAirRecActive', e.target.checked ? 1 : 0)} /></div>
                  </div>
                  <div class="table-row alt">
                    <div>Recording enabled by Software (Logger)</div>
                    <div><input type="checkbox" checked={!!keyerParam(activeSerial, 'r1FrMpkExtra_Cw.onAirRecControlByRouter')} on:change={(e) => updateKeyerParam(activeSerial, 'r1FrMpkExtra_Cw.onAirRecControlByRouter', e.target.checked ? 1 : 0)} /></div>
                    <div><input type="checkbox" checked={!!keyerParam(activeSerial, 'r1FrMpkExtra_Voice.onAirRecControlByRouter')} on:change={(e) => updateKeyerParam(activeSerial, 'r1FrMpkExtra_Voice.onAirRecControlByRouter', e.target.checked ? 1 : 0)} /></div>
                    <div><input type="checkbox" checked={!!keyerParam(activeSerial, 'r1FrMpkExtra_Digital.onAirRecControlByRouter')} on:change={(e) => updateKeyerParam(activeSerial, 'r1FrMpkExtra_Digital.onAirRecControlByRouter', e.target.checked ? 1 : 0)} /></div>
                  </div>
                  <div class="table-row">
                    <div>Audio Monitor Level (0-25)</div>
                    <div><input class="input" type="number" value={keyerParam(activeSerial, 'r1FrMpkExtra_Cw.pwmMonctr')} on:input={(e) => updateKeyerParam(activeSerial, 'r1FrMpkExtra_Cw.pwmMonctr', Number(e.target.value))} /></div>
                    <div><input class="input" type="number" value={keyerParam(activeSerial, 'r1FrMpkExtra_Voice.pwmMonctr')} on:input={(e) => updateKeyerParam(activeSerial, 'r1FrMpkExtra_Voice.pwmMonctr', Number(e.target.value))} /></div>
                    <div><input class="input" type="number" value={keyerParam(activeSerial, 'r1FrMpkExtra_Digital.pwmMonctr')} on:input={(e) => updateKeyerParam(activeSerial, 'r1FrMpkExtra_Digital.pwmMonctr', Number(e.target.value))} /></div>
                  </div>
                </div>

                <div class="panel" style="margin-top: 12px;">
                  <div class="row">
                    <div class="label">Microphone Selector:</div>
                    <div class="value">
                      <select
                        class="select"
                        value={mk2MicSelValue(activeSerial)}
                        on:change={(e) =>
                          updateKeyerParams(activeSerial, {
                            micSelAuto: e.target.value === 'auto' ? 1 : 0,
                            micSelFront: e.target.value === 'front' ? 1 : 0
                          })
                        }
                      >
                        {#each mk2MicSelOptions as opt}
                          <option value={opt.value}>{opt.label}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Sound Card PTT:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!keyerParam(activeSerial, 'useAutoPtt')} on:change={(e) => updateKeyerParam(activeSerial, 'useAutoPtt', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Downstream over Footswitch:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!keyerParam(activeSerial, 'downstreamOverFootSw')} on:change={(e) => updateKeyerParam(activeSerial, 'downstreamOverFootSw', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                </div>
              </section>
            {:else if isAudioMk1}
              <section class="section">
                <div class="section-title">Audio Switching</div>
                <div class="panel table audio-grid-8">
                  <div class="table-header">
                    <div>Mode</div>
                    <div>Receive</div>
                    <div>Transmit</div>
                    <div>Transmit with Footswitch</div>
                  </div>
                  {#each ['cw', 'voice', 'digital'] as mode}
                    <div class="table-row">
                      <div>{mode.toUpperCase()}</div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRx`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRx`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                      <div>
                        <input type="checkbox" checked={!!keyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRxMic`)} on:change={(e) => updateKeyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRxMic`, e.target.checked ? 1 : 0)} />
                      </div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTx`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTx`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                      <div>
                        <input type="checkbox" checked={!!keyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxMic`)} on:change={(e) => updateKeyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxMic`, e.target.checked ? 1 : 0)} />
                      </div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSw`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSw`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                      <div>
                        <input type="checkbox" checked={!!keyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSwMic`)} on:change={(e) => updateKeyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSwMic`, e.target.checked ? 1 : 0)} />
                      </div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.voiceCodec`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.voiceCodec`, Number(e.target.value))}
                        >
                          {#each mk2rCodecOptions as opt}
                            <option value={opt.value}>{opt.label}</option>
                          {/each}
                        </select>
                      </div>
                    </div>
                  {/each}
                </div>
              </section>
            {:else if isAudioMk2R}
              <section class="section">
                <div class="section-title">Audio Switching Radio 1</div>
                <div class="panel table audio-grid-8">
                  <div class="table-header">
                    <div>Mode</div>
                    <div>Receive</div>
                    <div>Mic</div>
                    <div>Transmit</div>
                    <div>Mic</div>
                    <div>Transmit with Footswitch</div>
                    <div>Mic</div>
                    <div>Codec</div>
                  </div>
                  {#each ['cw', 'voice', 'digital'] as mode}
                    <div class="table-row">
                      <div>{mode.toUpperCase()}</div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRx`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRx`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                      <div>
                        <input type="checkbox" checked={!!keyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRxMic`)} on:change={(e) => updateKeyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRxMic`, e.target.checked ? 1 : 0)} />
                      </div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTx`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTx`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                      <div>
                        <input type="checkbox" checked={!!keyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxMic`)} on:change={(e) => updateKeyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxMic`, e.target.checked ? 1 : 0)} />
                      </div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSw`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r1FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSw`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                      <div>
                        <input type="checkbox" checked={!!keyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSwMic`)} on:change={(e) => updateKeyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSwMic`, e.target.checked ? 1 : 0)} />
                      </div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.voiceCodec`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r1FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.voiceCodec`, Number(e.target.value))}
                        >
                          {#each mk2rCodecOptions as opt}
                            <option value={opt.value}>{opt.label}</option>
                          {/each}
                        </select>
                      </div>
                    </div>
                  {/each}
                </div>
              </section>

              <section class="section">
                <div class="section-title">Audio Switching Radio 2</div>
                <div class="panel table">
                  <div class="table-header">
                    <div>Mode</div>
                    <div>Receive</div>
                    <div>Mic</div>
                    <div>Transmit</div>
                    <div>Mic</div>
                    <div>Transmit with Footswitch</div>
                    <div>Mic</div>
                    <div>Codec</div>
                  </div>
                  {#each ['cw', 'voice', 'digital'] as mode}
                    <div class="table-row">
                      <div>{mode.toUpperCase()}</div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r2FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRx`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r2FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRx`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                      <div>
                        <input type="checkbox" checked={!!keyerParam(activeSerial, `r2FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRxMic`)} on:change={(e) => updateKeyerParam(activeSerial, `r2FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioRxMic`, e.target.checked ? 1 : 0)} />
                      </div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r2FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTx`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r2FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTx`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                      <div>
                        <input type="checkbox" checked={!!keyerParam(activeSerial, `r2FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxMic`)} on:change={(e) => updateKeyerParam(activeSerial, `r2FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxMic`, e.target.checked ? 1 : 0)} />
                      </div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r2FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSw`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r2FrBase_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSw`, Number(e.target.value))}
                        >
                          {#each audioOptionsFor(activeKeyer?.type ?? 0, mode) as opt}
                            <option value={opt}>{audioOptionLabel(opt)}</option>
                          {/each}
                        </select>
                      </div>
                      <div>
                        <input type="checkbox" checked={!!keyerParam(activeSerial, `r2FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSwMic`)} on:change={(e) => updateKeyerParam(activeSerial, `r2FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.audioTxFootSwMic`, e.target.checked ? 1 : 0)} />
                      </div>
                      <div>
                        <select
                          class="select"
                          value={keyerParam(activeSerial, `r2FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.voiceCodec`)}
                          on:change={(e) => updateKeyerParam(activeSerial, `r2FrMokExtra_${mode === 'cw' ? 'Cw' : mode === 'voice' ? 'Voice' : 'Digital'}.voiceCodec`, Number(e.target.value))}
                        >
                          {#each mk2rCodecOptions as opt}
                            <option value={opt.value}>{opt.label}</option>
                          {/each}
                        </select>
                      </div>
                    </div>
                  {/each}
                </div>
              </section>
            {:else}
              <section class="section">
                <div class="section-title">Audio</div>
                <div class="panel">
                  <div class="placeholder">Audio switching is not supported on this device.</div>
                </div>
              </section>
            {/if}
          {:else if activeMenuId === 'display'}
            <section class="section">
              <div class="section-title">Display Options</div>
              <div class="panel">
                <div class="row">
                  <div class="label">Light (0-25):</div>
                  <div class="value">
                    <input class="input" type="number" value={keyerParam(activeSerial, 'pwmBlight')} on:input={(e) => updateKeyerParam(activeSerial, 'pwmBlight', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Contrast (0-25):</div>
                  <div class="value">
                    <input class="input" type="number" value={keyerParam(activeSerial, 'pwmContrast')} on:input={(e) => updateKeyerParam(activeSerial, 'pwmContrast', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Background Upper Line:</div>
                  <div class="value">
                    <select class="select" value={keyerParam(activeSerial, 'dispBg0')} on:change={(e) => updateKeyerParam(activeSerial, 'dispBg0', Number(e.target.value))}>
                      {#each displayBackgroundOptions(activeKeyer?.type ?? 0) as opt}
                        <option value={opt.value}>{opt.display}</option>
                      {/each}
                    </select>
                  </div>
                </div>
                <div class="row">
                  <div class="label">Background Lower Line:</div>
                  <div class="value">
                    <select class="select" value={keyerParam(activeSerial, 'dispBg1')} on:change={(e) => updateKeyerParam(activeSerial, 'dispBg1', Number(e.target.value))}>
                      {#each displayBackgroundOptions(activeKeyer?.type ?? 0) as opt}
                        <option value={opt.value}>{opt.display}</option>
                      {/each}
                    </select>
                  </div>
                </div>
              </div>
            </section>

            <section class="section">
              <div class="section-title">Display Reports</div>
              <div class="panel table display-grid">
                <div class="table-header">
                  <div>Event</div>
                  <div>Enabled</div>
                  <div>Line</div>
                </div>
                {#each displayEventOptions(activeKeyer?.type ?? 0) as opt}
                  <div class="table-row">
                    <div>{opt.display}</div>
                    <div>
                      <input
                        type="checkbox"
                        checked={!!keyerParam(activeSerial, `dispEv.${opt.value}`)}
                        on:change={(e) => updateKeyerParam(activeSerial, `dispEv.${opt.value}`, e.target.checked ? 1 : 0)}
                      />
                    </div>
                    <div>
                      <select
                        class="select"
                        value={keyerParam(activeSerial, `dispEvLn.${opt.value}`)}
                        on:change={(e) => updateKeyerParam(activeSerial, `dispEvLn.${opt.value}`, Number(e.target.value))}
                      >
                        {#each displayLineOptions as lineOpt}
                          <option value={lineOpt.value}>{lineOpt.label}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                {/each}
              </div>
            </section>
          {:else if activeMenuId === 'cw'}
            <section class="section">
              <div class="section-title">Winkey</div>
              <div class="panel">
                {#if activeKeyer?.type === 3 || activeKeyer?.type === 5}
                  <div class="row">
                    <div class="label">Paddle Only Side Tone:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!keyerParam(activeSerial, 'paddleOnlySideTone')} on:change={(e) => updateKeyerParam(activeSerial, 'paddleOnlySideTone', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                {/if}
                <div class="row">
                  <div class="label">Paddle Mode:</div>
                  <div class="value">
                    <select class="select" value={winkeyValue(activeSerial, 'keyMode')} on:change={(e) => updateWinkeyParam(activeSerial, 'keyMode', Number(e.target.value))}>
                      {#each winkeyModeOptions as opt}
                        <option value={opt.value}>{opt.label}</option>
                      {/each}
                    </select>
                  </div>
                </div>
                <div class="row">
                  <div class="label">Ultimatic Mode:</div>
                  <div class="value">
                    <select class="select" value={winkeyValue(activeSerial, 'ultimaticMode')} on:change={(e) => updateWinkeyParam(activeSerial, 'ultimaticMode', Number(e.target.value))}>
                      {#each winkeyUltimaticOptions as opt}
                        <option value={opt.value}>{opt.label}</option>
                      {/each}
                    </select>
                  </div>
                </div>
                <div class="row">
                  <div class="label">PTT Lead (x 10ms):</div>
                  <div class="value">
                    <input class="input" type="number" value={winkeyValue(activeSerial, 'leadInTime')} on:input={(e) => updateWinkeyParam(activeSerial, 'leadInTime', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">PTT Tail (x 10ms):</div>
                  <div class="value">
                    <input class="input" type="number" value={winkeyValue(activeSerial, 'tailTime')} on:input={(e) => updateWinkeyParam(activeSerial, 'tailTime', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Hang Time:</div>
                  <div class="value">
                    <input class="input" type="number" value={winkeyValue(activeSerial, 'hangTime')} on:input={(e) => updateWinkeyParam(activeSerial, 'hangTime', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Swap Paddles:</div>
                  <div class="value">
                    <input type="checkbox" checked={!!winkeyValue(activeSerial, 'paddleSwap')} on:change={(e) => updateWinkeyParam(activeSerial, 'paddleSwap', e.target.checked ? 1 : 0)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Auto Space:</div>
                  <div class="value">
                    <input type="checkbox" checked={!!winkeyValue(activeSerial, 'autoSpace')} on:change={(e) => updateWinkeyParam(activeSerial, 'autoSpace', e.target.checked ? 1 : 0)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">CT Spacing:</div>
                  <div class="value">
                    <input type="checkbox" checked={!!winkeyValue(activeSerial, 'ctSpacing')} on:change={(e) => updateWinkeyParam(activeSerial, 'ctSpacing', e.target.checked ? 1 : 0)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Disable Paddle Watchdog:</div>
                  <div class="value">
                    <input type="checkbox" checked={!!winkeyValue(activeSerial, 'disablePaddleWatchdog')} on:change={(e) => updateWinkeyParam(activeSerial, 'disablePaddleWatchdog', e.target.checked ? 1 : 0)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Speed Pot Min WPM:</div>
                  <div class="value">
                    <input class="input" type="number" value={winkeyValue(activeSerial, 'minWpm')} on:input={(e) => updateWinkeyParam(activeSerial, 'minWpm', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Speed Pot Range WPM:</div>
                  <div class="value">
                    <input class="input" type="number" value={winkeyValue(activeSerial, 'wpmRange')} on:input={(e) => updateWinkeyParam(activeSerial, 'wpmRange', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Farnsworth Speed WPM:</div>
                  <div class="value">
                    <input class="input" type="number" value={winkeyValue(activeSerial, 'farnsWpm')} on:input={(e) => updateWinkeyParam(activeSerial, 'farnsWpm', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">DAH/DIT = 3*(nn/50):</div>
                  <div class="value">
                    <input class="input" type="number" value={winkeyValue(activeSerial, 'ditDahRatio')} on:input={(e) => updateWinkeyParam(activeSerial, 'ditDahRatio', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Weighting:</div>
                  <div class="value">
                    <input class="input" type="number" value={winkeyValue(activeSerial, 'weight')} on:input={(e) => updateWinkeyParam(activeSerial, 'weight', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">1st Extension:</div>
                  <div class="value">
                    <input class="input" type="number" value={winkeyValue(activeSerial, '1stExtension')} on:input={(e) => updateWinkeyParam(activeSerial, '1stExtension', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Keying Compensation:</div>
                  <div class="value">
                    <input class="input" type="number" value={winkeyValue(activeSerial, 'keyComp')} on:input={(e) => updateWinkeyParam(activeSerial, 'keyComp', Number(e.target.value))} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Paddle Setpoint:</div>
                  <div class="value">
                    <input class="input" type="number" value={winkeyValue(activeSerial, 'paddleSetpoint')} on:input={(e) => updateWinkeyParam(activeSerial, 'paddleSetpoint', Number(e.target.value))} />
                  </div>
                </div>
              </div>
            </section>

            <section class="section">
              <div class="section-title">Side Tone</div>
              <div class="panel">
                <div class="row">
                  <div class="label">Side Tone:</div>
                  <div class="value">
                    <select class="select" value={keyerParam(activeSerial, 'sideTone')} on:change={(e) => updateKeyerParam(activeSerial, 'sideTone', Number(e.target.value))}>
                      {#each sideToneOptions as opt}
                        <option value={opt.value}>{opt.label}</option>
                      {/each}
                    </select>
                  </div>
                </div>
                {#if activeKeyer?.type === 3 || activeKeyer?.type === 5}
                  <div class="row">
                    <div class="label">Paddle Only Side Tone:</div>
                    <div class="value">
                      <input type="checkbox" checked={!!keyerParam(activeSerial, 'paddleOnlySideTone')} on:change={(e) => updateKeyerParam(activeSerial, 'paddleOnlySideTone', e.target.checked ? 1 : 0)} />
                    </div>
                  </div>
                {/if}
              </div>
            </section>
          {:else if activeMenuId === 'cw_messages'}
            <section class="section">
              <div class="section-title">CW Messages</div>
              <div class="panel">
                {#each messageSlots as idx}
                  <div class="row">
                    <div class="label">Message #{idx}:</div>
                    <div class="value">
                      <input
                        class="input"
                        style="width: 100%;"
                        type="text"
                        value={messageForm[activeSerial]?.cw?.[idx] ?? ''}
                        on:input={(e) => updateMessageForm(activeSerial, 'cw', idx, e.target.value)}
                      />
                    </div>
                  </div>
                {/each}
                <div class="button-row">
                  <button class="btn" type="button" on:click={() => applyMessages(activeSerial, 'cw')}>Apply</button>
                </div>
                {#if messageStatus[`${activeSerial}:cw`]}
                  <div class={`inline-status ${messageStatus[`${activeSerial}:cw`].kind === 'error' ? 'error' : ''}`}>
                    {messageStatus[`${activeSerial}:cw`].text}
                  </div>
                {/if}
              </div>
            </section>

            <section class="section">
              <div class="section-title">FSK Messages</div>
              <div class="panel">
                {#each messageSlots as idx}
                  <div class="row">
                    <div class="label">Message #{idx}:</div>
                    <div class="value">
                      <input
                        class="input"
                        style="width: 100%;"
                        type="text"
                        value={messageForm[activeSerial]?.fsk?.[idx] ?? ''}
                        on:input={(e) => updateMessageForm(activeSerial, 'fsk', idx, e.target.value)}
                      />
                    </div>
                  </div>
                {/each}
                <div class="button-row">
                  <button class="btn" type="button" on:click={() => applyMessages(activeSerial, 'fsk')}>Apply</button>
                </div>
                {#if messageStatus[`${activeSerial}:fsk`]}
                  <div class={`inline-status ${messageStatus[`${activeSerial}:fsk`].kind === 'error' ? 'error' : ''}`}>
                    {messageStatus[`${activeSerial}:fsk`].text}
                  </div>
                {/if}
              </div>
            </section>
          {:else if activeMenuId === 'all_params'}
            <section class="section">
              <div class="section-title">All Keyer Parameters</div>
              <div class="panel">
                {#each Object.entries(allParamForm[activeSerial] || {}) as entry}
                  <div class="row">
                    <div class="label">{entry[0]}:</div>
                    <div class="value">
                      <input
                        class="input"
                        style="width: 100%;"
                        type="number"
                        value={entry[1]}
                        on:input={(e) => updateAllParamForm(activeSerial, entry[0], Number(e.target.value))}
                      />
                    </div>
                  </div>
                {/each}
                <div class="button-row">
                  <button class="btn" type="button" on:click={() => applyAllParams(activeSerial)}>Apply</button>
                </div>
                {#if allParamStatus[activeSerial]}
                  <div class={`inline-status ${allParamStatus[activeSerial].kind === 'error' ? 'error' : ''}`}>
                    {allParamStatus[activeSerial].text}
                  </div>
                {/if}
              </div>
            </section>
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
