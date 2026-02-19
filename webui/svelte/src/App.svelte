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
  let connectedToDaemon = true;
  let retrySeconds = 5;
  let radioForm = {};
  let radioFormGen = 0;
  let radioStatus = {};
  let radioStatusTimer = {};
  let rigModeSyncForm = {};
  let rigModeSyncFormGen = 0;
  let rigModeSyncStatus = {};
  let rigModeSyncStatusTimer = {};
  let pttForm = {};
  let pttFormGen = 0;
  let pttStatus = {};
  let pttStatusTimer = {};
  let messageForm = {};
  let messageFormGen = 0;
  let messageStatus = {};
  let messageStatusTimer = {};
  let allParamForm = {};
  let allParamFormGen = 0;
  let allParamStatus = {};
  let allParamStatusTimer = {};
  let smActionStatus = {};
  let smActionStatusTimer = {};
  let smActionBusy = {};
  let smAntMode = {};  // 'add' | 'edit' | null per serial
  let smAntEditForms = {}; // { [serial]: { [id]: { label, display, ... } } }  edit mode
  let smAntAddForm = {}; // { [serial]: { label, display, steppir, rxonly, pa_ant_number, rotator, rotator_offset, output: {} } }
  let smAntSelected = {}; // { [serial]: [id, ...] }  checkboxes for remove
  let smVrMode = {};  // 'add' | 'edit' | null per serial
  let smVrAddForm = {}; // { [serial]: { label, display } }
  let smVrEditForms = {}; // { [serial]: { [vrId]: { label, display, refs: { [refId]: { min_azimuth, max_azimuth, dest_id } } } } }
  let smVrSelected = {}; // { [serial]: [id, ...] }  checkboxes for remove (VR ids and ref compound keys)
  let smVrAddAnt = {}; // { [serial]: { vrId, min_azimuth: 0, max_azimuth: 0, dest_id: 0 } } or null
  let smGrpMode = {};  // 'add' | 'edit' | null per serial
  let smGrpAddForm = {}; // { [serial]: { label, display } }
  let smGrpEditForms = {}; // { [serial]: { [grpId]: { label, display, refs: { [refId]: { dest_id } } } } }
  let smGrpSelected = {}; // { [serial]: [id, ...] }  checkboxes for remove (group ids and ref compound keys)
  let smGrpAddAnt = {}; // { [serial]: { grpId, dest_id: 0 } } or null
  let smBndMode = {};  // 'add' | 'edit' | null per serial
  let smBndAddForm = {}; // { [serial]: { display, low_freq, high_freq, bcd_code, pa_power, keyout } }
  let smBndEditForms = {}; // { [serial]: { [bndId]: { display, low_freq, high_freq, bcd_code, pa_power, keyout, bpf_seq:{}, refs:{} } } }
  let smBndSelected = {}; // { [serial]: [id, ...] }  checkboxes for remove (band ids and ref compound keys)
  let smBndAddRef = {}; // { [serial]: { bndId, dest_id: 0 } } or null
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

  const smCivFuncOptions = [
    { value: 0, label: 'none' },
    { value: 1, label: 'TX frequency' },
    { value: 2, label: 'RX frequency' },
    { value: 3, label: 'operation frequency' },
    { value: 4, label: 'VFO A frequency' },
    { value: 5, label: 'VFO B frequency' },
    { value: 6, label: 'sub RX frequency' }
  ];
  const smCivBaudOptions = [
    { value: 192, label: '1200' },
    { value: 96, label: '2400' },
    { value: 48, label: '4800' },
    { value: 24, label: '9600' },
    { value: 12, label: '19200' }
  ];
  const smExtSerFuncOptions = [
    { value: 0, label: 'none' },
    { value: 1, label: 'auxiliary port' },
    { value: 2, label: 'Acom 2000' },
    { value: 17, label: 'CI-V TX frequency' },
    { value: 18, label: 'CI-V RX frequency' },
    { value: 19, label: 'CI-V operation frequency' },
    { value: 20, label: 'CI-V VFO A frequency' },
    { value: 21, label: 'CI-V VFO B frequency' },
    { value: 22, label: 'CI-V sub RX frequency' },
    { value: 33, label: 'SteppIR Dipole' },
    { value: 34, label: 'SteppIR Dipole with 40m-30m Dipole Adder' },
    { value: 35, label: 'SteppIR Yagi' },
    { value: 36, label: 'SteppIR Yagi with 40m-30m Dipole Adder' },
    { value: 37, label: 'SteppIR BigIR Vertical' },
    { value: 38, label: 'SteppIR BigIR Vertical with 80m Coil' },
    { value: 39, label: 'SteppIR SmallIR Vertical' },
    { value: 40, label: 'SteppIR MonstIR Yagi' }
  ];
  const smOutputClassOptions = [
    { value: 0, label: 'ANT' },
    { value: 1, label: 'BPF' },
    { value: 2, label: 'SEQ' },
    { value: 3, label: 'SEQ inverted' }
  ];
  const smOutputNames = [
    'A1','A2','A3','A4','A5','A6','A7','A8','A9','A10',
    'B1','B2','B3','B4','B5','B6','B7','B8','B9','B10'
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

  const resetRadioForms = () => {
    radioForm = {};
    radioFormGen++;
  };

  const updateRadioForm = (serial, chan, key, value) => {
    const perSerial = radioForm[serial] || {};
    const perChan = perSerial[chan] || defaultRadioForm(serial, chan);
    radioForm = {
      ...radioForm,
      [serial]: { ...perSerial, [chan]: { ...perChan, [key]: value } }
    };
  };

  const boolish = (v) => !!Number(v || 0) || v === true;

  const defaultRigModeSyncForm = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    const src = cfg?.rig_mode_sync || {};
    const radios = src.radios || {};
    return {
      backend: src.backend ?? 'rigctld',
      enabled: boolish(src.enabled),
      r1: {
        enabled: boolish(radios?.r1?.enabled),
        host: radios?.r1?.host ?? '127.0.0.1',
        port: Number(radios?.r1?.port ?? 4532),
        connect_timeout_ms: Number(radios?.r1?.connect_timeout_ms ?? 1500),
        io_timeout_ms: Number(radios?.r1?.io_timeout_ms ?? 1500),
        poll_ms: Number(radios?.r1?.poll_ms ?? 500)
      },
      r2: {
        enabled: boolish(radios?.r2?.enabled),
        host: radios?.r2?.host ?? '127.0.0.1',
        port: Number(radios?.r2?.port ?? 4533),
        connect_timeout_ms: Number(radios?.r2?.connect_timeout_ms ?? 1500),
        io_timeout_ms: Number(radios?.r2?.io_timeout_ms ?? 1500),
        poll_ms: Number(radios?.r2?.poll_ms ?? 500)
      }
    };
  };

  const ensureRigModeSyncForm = (serial) => {
    if (!serial) return;
    if (!rigModeSyncForm[serial]) {
      rigModeSyncForm = { ...rigModeSyncForm, [serial]: defaultRigModeSyncForm(serial) };
    }
  };

  const resetRigModeSyncForms = () => {
    rigModeSyncForm = {};
    rigModeSyncFormGen++;
  };

  const updateRigModeSyncTop = (serial, key, value) => {
    const form = rigModeSyncForm[serial] || defaultRigModeSyncForm(serial);
    rigModeSyncForm = { ...rigModeSyncForm, [serial]: { ...form, [key]: value } };
  };

  const updateRigModeSyncRadio = (serial, radio, key, value) => {
    const form = rigModeSyncForm[serial] || defaultRigModeSyncForm(serial);
    const cur = form[radio] || {};
    rigModeSyncForm = {
      ...rigModeSyncForm,
      [serial]: { ...form, [radio]: { ...cur, [key]: value } }
    };
  };

  const setRigModeSyncStatusMsg = (serial, msg, kind = 'success') => {
    rigModeSyncStatus = { ...rigModeSyncStatus, [serial]: { text: msg, kind } };
    if (rigModeSyncStatusTimer[serial]) clearTimeout(rigModeSyncStatusTimer[serial]);
    rigModeSyncStatusTimer[serial] = setTimeout(() => {
      const { [serial]: _removed, ...rest } = rigModeSyncStatus;
      rigModeSyncStatus = rest;
    }, 3000);
  };

  const applyRigModeSync = async (serial) => {
    const form = rigModeSyncForm[serial] || defaultRigModeSyncForm(serial);
    const payload = {
      backend: form.backend || 'rigctld',
      enabled: form.enabled ? 1 : 0,
      radios: {
        r1: {
          enabled: form.r1?.enabled ? 1 : 0,
          host: String(form.r1?.host || '127.0.0.1').trim() || '127.0.0.1',
          port: Number(form.r1?.port || 4532),
          connect_timeout_ms: Number(form.r1?.connect_timeout_ms || 1500),
          io_timeout_ms: Number(form.r1?.io_timeout_ms || 1500),
          poll_ms: Number(form.r1?.poll_ms || 500)
        }
      }
    };

    if (hasR2) {
      payload.radios.r2 = {
        enabled: form.r2?.enabled ? 1 : 0,
        host: String(form.r2?.host || '127.0.0.1').trim() || '127.0.0.1',
        port: Number(form.r2?.port || 4533),
        connect_timeout_ms: Number(form.r2?.connect_timeout_ms || 1500),
        io_timeout_ms: Number(form.r2?.io_timeout_ms || 1500),
        poll_ms: Number(form.r2?.poll_ms || 500)
      };
    }

    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, rig_mode_sync: payload })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      rigModeSyncForm = { ...rigModeSyncForm, [serial]: defaultRigModeSyncForm(serial) };
      setRigModeSyncStatusMsg(serial, 'Rig mode sync settings applied.', 'success');
    } catch (err) {
      setRigModeSyncStatusMsg(serial, err?.message || 'Failed to apply rig mode sync settings.', 'error');
    }
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

  const resetMessageForms = () => {
    messageForm = {};
    messageFormGen++;
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

  const resetAllParamForms = () => {
    allParamForm = {};
    allParamFormGen++;
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

  const resetPttForms = () => {
    pttForm = {};
    pttFormGen++;
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

  const reloadData = async () => {
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
      console.error('Failed to reload data:', err);
      throw err;
    }
  };

  onMount(async () => {
    let eventSource;
    let reconnectTimer;

    const initEventSource = () => {
      if (eventSource) eventSource.close();
      eventSource = new EventSource('/api/v1/events');
      
      eventSource.onopen = async () => {
        connectedToDaemon = true;
        retrySeconds = 5;
        if (reconnectTimer) clearInterval(reconnectTimer);
        try {
          await reloadData();
        } catch (err) {
          // If reload fails, maybe the daemon is not quite ready yet, 
          // eventSource might close again or we just wait for next event.
        }
      };

      eventSource.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          if (data.type === 'status') {
            devices = devices.map(d => 
              d.serial === data.serial ? { ...d, status: data.status } : d
            );
          } else if (data.type === 'usb_connection') {
             console.log('USB Event:', data);
          }
        } catch (e) {
          console.error('Failed to parse event data', e);
        }
      };

      eventSource.onerror = (err) => {
        console.error('EventSource failed:', err);
        connectedToDaemon = false;
        eventSource.close();
        // Try to reconnect in 5 seconds
        retrySeconds = 5;
        if (reconnectTimer) clearInterval(reconnectTimer);
        reconnectTimer = setInterval(() => {
          retrySeconds -= 1;
          if (retrySeconds <= 0) {
            clearInterval(reconnectTimer);
            initEventSource();
          }
        }, 1000);
      };
    };

    try {
      await reloadData();
      initEventSource();
    } catch (err) {
      error = err?.message || 'Failed to load data';
      connectedToDaemon = false;
      // Start trying to reconnect even if initial load failed
      retrySeconds = 5;
      if (reconnectTimer) clearInterval(reconnectTimer);
      reconnectTimer = setInterval(() => {
        retrySeconds -= 1;
        if (retrySeconds <= 0) {
          clearInterval(reconnectTimer);
          initEventSource();
        }
      }, 1000);
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
      if (eventSource) eventSource.close();
      if (reconnectTimer) clearTimeout(reconnectTimer);
    };
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
      const res = await fetch('/api/v1/config/connectors', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify(connector)
      });
      if (!res.ok) throw new Error(`config/connectors ${res.status}`);
      await reloadData();
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
      await Promise.all(
        selectedConnectorIds.map(async (id) => {
          const res = await fetch(`/api/v1/config/connectors/${id}`, {
            method: 'DELETE',
            headers: { Accept: 'application/json' }
          });
          if (!res.ok) throw new Error(`config/connectors/${id} ${res.status}`);
        })
      );
      await reloadData();
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
  $: tabs = [...baseTabs, ...keyers.map((k) => ({ id: `keyer:${k.serial}`, label: k.name, serial: k.serial, status: k.status }))];

  // When configDevices changes (initial load or after API updates), clear cached
  // forms so they get re-populated from the fresh config data.
  $: if (configDevices) {
    resetRadioForms();
    resetRigModeSyncForms();
    resetPttForms();
    resetMessageForms();
    resetAllParamForms();
  }
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

  const setSmActionStatus = (serial, msg, kind = 'success') => {
    smActionStatus = { ...smActionStatus, [serial]: { text: msg, kind } };
    if (smActionStatusTimer[serial]) clearTimeout(smActionStatusTimer[serial]);
    smActionStatusTimer[serial] = setTimeout(() => {
      const { [serial]: _removed, ...rest } = smActionStatus;
      smActionStatus = rest;
    }, 5000);
  };

  const smAction = async (serial, action) => {
    smActionBusy = { ...smActionBusy, [serial]: true };
    try {
      const res = await fetch(`/api/v1/devices/${serial}/actions`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ action })
      });
      const result = await res.json();
      if (!res.ok || result.status === 'error') {
        setSmActionStatus(serial, result.message || 'Action failed.', 'error');
      } else {
        setSmActionStatus(serial, result.message || 'Action completed.', 'success');
      }
    } catch (err) {
      setSmActionStatus(serial, err?.message || 'Action failed.', 'error');
    } finally {
      const { [serial]: _removed, ...rest } = smActionBusy;
      smActionBusy = rest;
    }
  };

  const smFixed = (serial, key, fallback = 0) => {
    const cfg = keyerConfigForSerial(serial);
    return cfg?.sm?.fixed?.[key] ?? fallback;
  };

  const smSequencerVal = (serial, phase, output, fallback = 0) => {
    const cfg = keyerConfigForSerial(serial);
    return cfg?.sm?.fixed?.sequencer?.[phase]?.[output] ?? fallback;
  };

  const smOutputVal = (serial, output, fallback = 0) => {
    const cfg = keyerConfigForSerial(serial);
    return cfg?.sm?.output?.[output] ?? fallback;
  };

  const smSequencerOutputs = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    const outputs = cfg?.sm?.output || {};
    return smOutputNames.filter((name) => {
      const val = outputs[name];
      return val === 2 || val === 3;
    });
  };

  const applySmFixed = async (serial, key, value) => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, sm: { fixed: { [key]: Number(value) } } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, 'Setting updated.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to update setting.', 'error');
    }
  };

  const applySmSequencer = async (serial, phase, output, value) => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, sm: { fixed: { sequencer: { [phase]: { [output]: Number(value) } } } } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, 'Sequencer setting updated.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to update sequencer setting.', 'error');
    }
  };

  const applySmOutput = async (serial, output, value) => {
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, sm: { output: { [output]: Number(value) } } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      setKeyerStatusMsg(serial, 'Output setting updated.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to update output setting.', 'error');
    }
  };

  /* --- SM Antenna List helpers --- */

  const smAntennas = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    return (cfg?.sm?.obj || []).filter((o) => o.type === 0);
  };

  const smAntOutputColumns = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    const outputs = cfg?.sm?.output || {};
    return smOutputNames.filter((name) => outputs[name] === 0);
  };

  const smAntNewForm = () => ({
    label: '', display: '', steppir: 0, rxonly: 0, pa_ant_number: 0,
    rotator: 0, rotator_offset: 0, output: {}
  });

  const smAntStartAdd = (serial) => {
    smAntMode = { ...smAntMode, [serial]: 'add' };
    smAntAddForm = { ...smAntAddForm, [serial]: smAntNewForm() };
  };

  const smAntStartEdit = (serial) => {
    const ants = smAntennas(serial);
    const forms = {};
    for (const a of ants) {
      forms[a.id] = {
        label: a.label || '', display: a.display || '', steppir: a.steppir || 0,
        rxonly: a.rxonly || 0, pa_ant_number: a.pa_ant_number || 0,
        rotator: a.rotator || 0, rotator_offset: a.rotator_offset || 0,
        output: { ...(a.output || {}) }
      };
    }
    smAntEditForms = { ...smAntEditForms, [serial]: forms };
    smAntMode = { ...smAntMode, [serial]: 'edit' };
  };

  const smAntCancel = (serial) => {
    smAntMode = { ...smAntMode, [serial]: null };
  };

  const smAntSaveAdd = async (serial) => {
    const form = smAntAddForm[serial];
    if (!form) return;
    const payload = { action: 'add', type: 0, ...form };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      smAntMode = { ...smAntMode, [serial]: null };
      setKeyerStatusMsg(serial, 'Antenna added.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to add antenna.', 'error');
    }
  };

  const smAntSaveEdit = async (serial) => {
    const forms = smAntEditForms[serial];
    if (!forms) return;
    try {
      for (const [idStr, form] of Object.entries(forms)) {
        const payload = { action: 'mod', type: 0, id: Number(idStr), ...form };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      }
      smAntMode = { ...smAntMode, [serial]: null };
      setKeyerStatusMsg(serial, 'Antennas updated.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to update antennas.', 'error');
    }
  };

  const smAntRemoveSelected = async (serial) => {
    const sel = smAntSelected[serial];
    if (!sel || sel.length === 0) return;
    try {
      for (const id of sel) {
        const payload = { action: 'rem', id };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      }
      smAntSelected = { ...smAntSelected, [serial]: [] };
      setKeyerStatusMsg(serial, 'Antenna(s) removed.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to remove antenna.', 'error');
    }
  };

  const smAntToggleSelect = (serial, id) => {
    const sel = smAntSelected[serial] || [];
    if (sel.includes(id)) {
      smAntSelected = { ...smAntSelected, [serial]: sel.filter((x) => x !== id) };
    } else {
      smAntSelected = { ...smAntSelected, [serial]: [...sel, id] };
    }
  };

  /* --- SM Virtual Rotator helpers --- */

  const smVirtualRotators = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    return (cfg?.sm?.obj || []).filter((o) => o.type === 1 && o.virtual_rotator === 1);
  };

  const smAllObjects = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    return cfg?.sm?.obj || [];
  };

  const smObjDisplay = (serial, destId) => {
    const obj = smAllObjects(serial).find((o) => o.id === destId);
    return obj?.display || obj?.label || `#${destId}`;
  };

  const smObjRxOnly = (serial, destId) => {
    const obj = smAllObjects(serial).find((o) => o.id === destId);
    return obj?.rxonly ? 'Yes' : 'No';
  };

  const smVrStartAdd = (serial) => {
    smVrMode = { ...smVrMode, [serial]: 'add' };
    smVrAddForm = { ...smVrAddForm, [serial]: { label: '', display: '' } };
    smVrAddAnt = { ...smVrAddAnt, [serial]: null };
  };

  const smVrStartEdit = (serial) => {
    const vrs = smVirtualRotators(serial);
    const forms = {};
    for (const vr of vrs) {
      const refs = {};
      for (const ref of (vr.refs || [])) {
        refs[ref.id] = { min_azimuth: ref.min_azimuth || 0, max_azimuth: ref.max_azimuth || 0, dest_id: ref.dest_id || 0 };
      }
      forms[vr.id] = { label: vr.label || '', display: vr.display || '', refs };
    }
    smVrEditForms = { ...smVrEditForms, [serial]: forms };
    smVrMode = { ...smVrMode, [serial]: 'edit' };
    smVrAddAnt = { ...smVrAddAnt, [serial]: null };
  };

  const smVrCancel = (serial) => {
    smVrMode = { ...smVrMode, [serial]: null };
    smVrAddAnt = { ...smVrAddAnt, [serial]: null };
  };

  const smVrSaveAdd = async (serial) => {
    const form = smVrAddForm[serial];
    if (!form) return;
    const payload = { action: 'add', type: 1, virtual_rotator: 1, ...form };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      smVrMode = { ...smVrMode, [serial]: null };
      setKeyerStatusMsg(serial, 'Virtual rotator added.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to add virtual rotator.', 'error');
    }
  };

  const smVrSaveEdit = async (serial) => {
    const forms = smVrEditForms[serial];
    if (!forms) return;
    try {
      for (const [idStr, form] of Object.entries(forms)) {
        const refs = Object.entries(form.refs || {}).map(([rid, r]) => ({ id: Number(rid), ...r }));
        const payload = { action: 'mod', type: 1, id: Number(idStr), label: form.label, display: form.display, refs };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      }
      smVrMode = { ...smVrMode, [serial]: null };
      setKeyerStatusMsg(serial, 'Virtual rotators updated.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to update virtual rotators.', 'error');
    }
  };

  const smVrRemoveSelected = async (serial) => {
    const sel = smVrSelected[serial];
    if (!sel || sel.length === 0) return;
    try {
      // Separate VR removals from ref removals
      const refRemovals = sel.filter((s) => typeof s === 'string' && s.includes(':'));
      const vrRemovals = sel.filter((s) => typeof s === 'number');
      // Remove refs first
      for (const key of refRemovals) {
        const [objId, refId] = key.split(':').map(Number);
        const payload = { action: 'rem_ref', obj_id: objId, ref_id: refId };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      }
      // Then remove VRs
      for (const id of vrRemovals) {
        const payload = { action: 'rem', id };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      }
      smVrSelected = { ...smVrSelected, [serial]: [] };
      setKeyerStatusMsg(serial, 'Item(s) removed.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to remove item(s).', 'error');
    }
  };

  const smVrToggleSelect = (serial, key) => {
    const sel = smVrSelected[serial] || [];
    const idx = sel.indexOf(key);
    if (idx >= 0) {
      smVrSelected = { ...smVrSelected, [serial]: sel.filter((x) => x !== key) };
    } else {
      smVrSelected = { ...smVrSelected, [serial]: [...sel, key] };
    }
  };

  const smVrStartAddAnt = (serial, vrId) => {
    smVrAddAnt = { ...smVrAddAnt, [serial]: { vrId, min_azimuth: 0, max_azimuth: 0, dest_id: 0 } };
  };

  const smVrSaveAddAnt = async (serial) => {
    const aa = smVrAddAnt[serial];
    if (!aa) return;
    // mod the group, adding a new ref with id=0 to let the server assign one
    const vr = smVirtualRotators(serial).find((v) => v.id === aa.vrId);
    if (!vr) return;
    const existingRefs = (vr.refs || []).map((r) => ({ id: r.id, dest_id: r.dest_id, min_azimuth: r.min_azimuth, max_azimuth: r.max_azimuth }));
    const newRef = { id: 0, dest_id: aa.dest_id, min_azimuth: aa.min_azimuth, max_azimuth: aa.max_azimuth };
    const payload = { action: 'mod', type: 1, id: aa.vrId, refs: [...existingRefs, newRef] };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      smVrAddAnt = { ...smVrAddAnt, [serial]: null };
      setKeyerStatusMsg(serial, 'Antenna reference added.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to add antenna reference.', 'error');
    }
  };

  const smVrCancelAddAnt = (serial) => {
    smVrAddAnt = { ...smVrAddAnt, [serial]: null };
  };

  /* --- SM Antenna Groups helpers --- */

  const smGroups = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    return (cfg?.sm?.obj || []).filter((o) => o.type === 1 && !o.virtual_rotator);
  };

  const smGrpStartAdd = (serial) => {
    smGrpMode = { ...smGrpMode, [serial]: 'add' };
    smGrpAddForm = { ...smGrpAddForm, [serial]: { label: '', display: '' } };
    smGrpAddAnt = { ...smGrpAddAnt, [serial]: null };
  };

  const smGrpStartEdit = (serial) => {
    const grps = smGroups(serial);
    const forms = {};
    for (const grp of grps) {
      const refs = {};
      for (const ref of (grp.refs || [])) {
        refs[ref.id] = { dest_id: ref.dest_id || 0 };
      }
      forms[grp.id] = { label: grp.label || '', display: grp.display || '', refs };
    }
    smGrpEditForms = { ...smGrpEditForms, [serial]: forms };
    smGrpMode = { ...smGrpMode, [serial]: 'edit' };
    smGrpAddAnt = { ...smGrpAddAnt, [serial]: null };
  };

  const smGrpCancel = (serial) => {
    smGrpMode = { ...smGrpMode, [serial]: null };
    smGrpAddAnt = { ...smGrpAddAnt, [serial]: null };
  };

  const smGrpSaveAdd = async (serial) => {
    const form = smGrpAddForm[serial];
    if (!form) return;
    const payload = { action: 'add', type: 1, virtual_rotator: 0, ...form };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      smGrpMode = { ...smGrpMode, [serial]: null };
      setKeyerStatusMsg(serial, 'Antenna group added.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to add antenna group.', 'error');
    }
  };

  const smGrpSaveEdit = async (serial) => {
    const forms = smGrpEditForms[serial];
    if (!forms) return;
    try {
      for (const [idStr, form] of Object.entries(forms)) {
        const refs = Object.entries(form.refs || {}).map(([rid, r]) => ({ id: Number(rid), ...r }));
        const payload = { action: 'mod', type: 1, id: Number(idStr), label: form.label, display: form.display, refs };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      }
      smGrpMode = { ...smGrpMode, [serial]: null };
      setKeyerStatusMsg(serial, 'Antenna groups updated.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to update antenna groups.', 'error');
    }
  };

  const smGrpRemoveSelected = async (serial) => {
    const sel = smGrpSelected[serial];
    if (!sel || sel.length === 0) return;
    try {
      const refRemovals = sel.filter((s) => typeof s === 'string' && s.includes(':'));
      const grpRemovals = sel.filter((s) => typeof s === 'number');
      for (const key of refRemovals) {
        const [objId, refId] = key.split(':').map(Number);
        const payload = { action: 'rem_ref', obj_id: objId, ref_id: refId };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      }
      for (const id of grpRemovals) {
        const payload = { action: 'rem', id };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      }
      smGrpSelected = { ...smGrpSelected, [serial]: [] };
      setKeyerStatusMsg(serial, 'Item(s) removed.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to remove item(s).', 'error');
    }
  };

  const smGrpToggleSelect = (serial, key) => {
    const sel = smGrpSelected[serial] || [];
    const idx = sel.indexOf(key);
    if (idx >= 0) {
      smGrpSelected = { ...smGrpSelected, [serial]: sel.filter((x) => x !== key) };
    } else {
      smGrpSelected = { ...smGrpSelected, [serial]: [...sel, key] };
    }
  };

  const smGrpStartAddAnt = (serial, grpId) => {
    smGrpAddAnt = { ...smGrpAddAnt, [serial]: { grpId, dest_id: 0 } };
  };

  const smGrpSaveAddAnt = async (serial) => {
    const aa = smGrpAddAnt[serial];
    if (!aa) return;
    const grp = smGroups(serial).find((g) => g.id === aa.grpId);
    if (!grp) return;
    const existingRefs = (grp.refs || []).map((r) => ({ id: r.id, dest_id: r.dest_id }));
    const newRef = { id: 0, dest_id: aa.dest_id };
    const payload = { action: 'mod', type: 1, id: aa.grpId, refs: [...existingRefs, newRef] };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      smGrpAddAnt = { ...smGrpAddAnt, [serial]: null };
      setKeyerStatusMsg(serial, 'Antenna reference added.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to add antenna reference.', 'error');
    }
  };

  const smGrpCancelAddAnt = (serial) => {
    smGrpAddAnt = { ...smGrpAddAnt, [serial]: null };
  };

  /* --- SM Bands helpers --- */

  const smBands = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    return (cfg?.sm?.obj || []).filter((o) => o.type === 2);
  };

  const smBpfOutputs = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    const outputs = cfg?.sm?.output || {};
    return smOutputNames.filter((name) => outputs[name] === 1);
  };

  const smAntOutputs = (serial) => {
    const cfg = keyerConfigForSerial(serial);
    const outputs = cfg?.sm?.output || {};
    return smOutputNames.filter((name) => outputs[name] === 0);
  };

  const smObjTypedDisplay = (serial, destId) => {
    const obj = smAllObjects(serial).find((o) => o.id === destId);
    if (!obj) return `#${destId}`;
    const name = obj.display || obj.label || `#${destId}`;
    if (obj.type === 0) return `ANT: ${name}`;
    if (obj.type === 1 && obj.virtual_rotator) return `VIR: ${name}`;
    if (obj.type === 1) return `GRP: ${name}`;
    return name;
  };

  const smAntsAndGroups = (serial) => {
    return smAllObjects(serial).filter((o) => o.type === 0 || o.type === 1);
  };

  const smBndStartAdd = (serial) => {
    smBndMode = { ...smBndMode, [serial]: 'add' };
    smBndAddForm = { ...smBndAddForm, [serial]: { display: '', low_freq: 3500000, high_freq: 3800000, bcd_code: 0, pa_power: 0, keyout: 0 } };
    smBndAddRef = { ...smBndAddRef, [serial]: null };
  };

  const smBndStartEdit = (serial) => {
    const bnds = smBands(serial);
    const forms = {};
    for (const bnd of bnds) {
      const refs = {};
      for (const ref of (bnd.refs || [])) {
        refs[ref.id] = { dest_id: ref.dest_id || 0, rxonly: ref.rxonly || 0 };
      }
      forms[bnd.id] = {
        display: bnd.display || '', low_freq: bnd.low_freq || 0, high_freq: bnd.high_freq || 0,
        bcd_code: bnd.bcd_code || 0, pa_power: bnd.pa_power || 0, keyout: bnd.keyout || 0,
        bpf_seq: { ...(bnd.bpf_seq || {}) }, refs
      };
    }
    smBndEditForms = { ...smBndEditForms, [serial]: forms };
    smBndMode = { ...smBndMode, [serial]: 'edit' };
    smBndAddRef = { ...smBndAddRef, [serial]: null };
  };

  const smBndCancel = (serial) => {
    smBndMode = { ...smBndMode, [serial]: null };
    smBndAddRef = { ...smBndAddRef, [serial]: null };
  };

  const smBndSaveAdd = async (serial) => {
    const form = smBndAddForm[serial];
    if (!form) return;
    const payload = { action: 'add', type: 2, ...form };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      smBndMode = { ...smBndMode, [serial]: null };
      setKeyerStatusMsg(serial, 'Band added.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to add band.', 'error');
    }
  };

  const smBndSaveEdit = async (serial) => {
    const forms = smBndEditForms[serial];
    if (!forms) return;
    try {
      for (const [idStr, form] of Object.entries(forms)) {
        const refs = Object.entries(form.refs || {}).map(([rid, r]) => ({ id: Number(rid), ...r }));
        const payload = {
          action: 'mod', type: 2, id: Number(idStr),
          display: form.display, low_freq: form.low_freq, high_freq: form.high_freq,
          bcd_code: form.bcd_code, pa_power: form.pa_power, keyout: form.keyout,
          bpf_seq: form.bpf_seq, refs
        };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      }
      smBndMode = { ...smBndMode, [serial]: null };
      setKeyerStatusMsg(serial, 'Bands updated.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to update bands.', 'error');
    }
  };

  const smBndRemoveSelected = async (serial) => {
    const sel = smBndSelected[serial];
    if (!sel || sel.length === 0) return;
    try {
      const refRemovals = sel.filter((s) => typeof s === 'string' && s.includes(':'));
      const bndRemovals = sel.filter((s) => typeof s === 'number');
      for (const key of refRemovals) {
        const [objId, refId] = key.split(':').map(Number);
        const payload = { action: 'rem_ref', obj_id: objId, ref_id: refId };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      }
      for (const id of bndRemovals) {
        const payload = { action: 'rem', id };
        const res = await fetch(`/api/v1/config/devices/${serial}`, {
          method: 'PATCH',
          headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
          body: JSON.stringify({ serial, sm: { obj: payload } })
        });
        if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
        const updated = await res.json();
        configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      }
      smBndSelected = { ...smBndSelected, [serial]: [] };
      setKeyerStatusMsg(serial, 'Item(s) removed.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to remove item(s).', 'error');
    }
  };

  const smBndToggleSelect = (serial, key) => {
    const sel = smBndSelected[serial] || [];
    const idx = sel.indexOf(key);
    if (idx >= 0) {
      smBndSelected = { ...smBndSelected, [serial]: sel.filter((x) => x !== key) };
    } else {
      smBndSelected = { ...smBndSelected, [serial]: [...sel, key] };
    }
  };

  const smBndStartAddRef = (serial, bndId) => {
    smBndAddRef = { ...smBndAddRef, [serial]: { bndId, dest_id: 0 } };
  };

  const smBndSaveAddRef = async (serial) => {
    const aa = smBndAddRef[serial];
    if (!aa) return;
    const bnd = smBands(serial).find((b) => b.id === aa.bndId);
    if (!bnd) return;
    const existingRefs = (bnd.refs || []).map((r) => ({ id: r.id, dest_id: r.dest_id, rxonly: r.rxonly }));
    const newRef = { id: 0, dest_id: aa.dest_id };
    const payload = { action: 'mod', type: 2, id: aa.bndId, refs: [...existingRefs, newRef] };
    try {
      const res = await fetch(`/api/v1/config/devices/${serial}`, {
        method: 'PATCH',
        headers: { 'Content-Type': 'application/json', Accept: 'application/json' },
        body: JSON.stringify({ serial, sm: { obj: payload } })
      });
      if (!res.ok) throw new Error(`config/devices/${serial} ${res.status}`);
      const updated = await res.json();
      configDevices = configDevices.map((d) => (d.serial === serial ? updated : d));
      smBndAddRef = { ...smBndAddRef, [serial]: null };
      setKeyerStatusMsg(serial, 'Antenna/group reference added.', 'success');
    } catch (err) {
      setKeyerStatusMsg(serial, err?.message || 'Failed to add reference.', 'error');
    }
  };

  const smBndCancelAddRef = (serial) => {
    smBndAddRef = { ...smBndAddRef, [serial]: null };
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
  $: activeAnts = configDevices && activeSerial ? smAntennas(activeSerial) : [];
  $: activeAntOutCols = configDevices && activeSerial ? smAntOutputColumns(activeSerial) : [];
  $: activeVrs = configDevices && activeSerial ? smVirtualRotators(activeSerial) : [];
  $: activeGrps = configDevices && activeSerial ? smGroups(activeSerial) : [];
  $: activeAllObjs = configDevices && activeSerial ? smAllObjects(activeSerial) : [];
  $: activeBnds = configDevices && activeSerial ? smBands(activeSerial) : [];
  $: activeBpfOutputs = configDevices && activeSerial ? smBpfOutputs(activeSerial) : [];
  $: activeSeqOutputs = configDevices && activeSerial ? smSequencerOutputs(activeSerial) : [];
  $: activeAntOutputs = configDevices && activeSerial ? smAntOutputs(activeSerial) : [];
  $: activeAntsAndGroups = configDevices && activeSerial ? smAntsAndGroups(activeSerial) : [];
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
  $: activeRigModeSync = activeSerial ? (rigModeSyncForm[activeSerial] || defaultRigModeSyncForm(activeSerial)) : null;
  $: activePttR1 = activeSerial ? (pttForm[activeSerial]?.r1 || defaultPttForm(activeSerial, 'r1')) : null;
  $: activePttR2 = activeSerial ? (pttForm[activeSerial]?.r2 || defaultPttForm(activeSerial, 'r2')) : null;
  $: if (activeSerial && configDevices) {
    void radioFormGen; void pttFormGen; void messageFormGen; void allParamFormGen;
    ensureRadioForm(activeSerial, 'r1');
    if (hasR2) ensureRadioForm(activeSerial, 'r2');
    if (hasAux) ensureRadioForm(activeSerial, 'aux');
    if (hasFsk1) ensureRadioForm(activeSerial, 'fsk1');
    if (hasFsk2) ensureRadioForm(activeSerial, 'fsk2');
    ensureRigModeSyncForm(activeSerial);
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

  const activeKeyerMenuTitle = () => activePageTitle || 'Keyer';
</script>

<div class="page">
  {#if !connectedToDaemon}
    <div class="connection-overlay">
      <div class="connection-message">Can't connect to mhuxd. Retry in {retrySeconds} {retrySeconds === 1 ? 'second' : 'seconds'}</div>
    </div>
  {/if}
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
        {#if tab.status !== undefined}
          <StatusDot status={tab.status} />
        {/if}
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
      <div class="breadcrumbs">{breadcrumb}</div>

      <h1>{activePageTitle}</h1>

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
                    <div class={c.status === 'failed' ? 'text-error' : ''}>{c.status || '—'}</div>
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

            <section class="section">
              <div class="section-title">RIG Mode Sync (rigctld/flrig)</div>
              <div class="panel">
                <div class="row">
                  <div class="label">Enabled:</div>
                  <div class="value">
                    <input
                      type="checkbox"
                      checked={!!activeRigModeSync?.enabled}
                      on:change={(e) => updateRigModeSyncTop(activeSerial, 'enabled', e.target.checked)}
                    />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Backend:</div>
                  <div class="value">
                    <select
                      class="select"
                      value={activeRigModeSync?.backend || 'rigctld'}
                      on:change={(e) => updateRigModeSyncTop(activeSerial, 'backend', e.target.value)}
                    >
                      <option value="rigctld">rigctld</option>
                      <option value="flrig">flrig</option>
                      <option value="flrig_xmlrpc">flrig_xmlrpc</option>
                    </select>
                  </div>
                </div>
              </div>

              <div class="panel" style="margin-top: 12px;">
                <div class="section-subtitle">Radio 1 Endpoint</div>
                <div class="row">
                  <div class="label">Enabled:</div>
                  <div class="value">
                    <input
                      type="checkbox"
                      checked={!!activeRigModeSync?.r1?.enabled}
                      on:change={(e) => updateRigModeSyncRadio(activeSerial, 'r1', 'enabled', e.target.checked)}
                    />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Host:</div>
                  <div class="value">
                    <input
                      class="input"
                      type="text"
                      value={activeRigModeSync?.r1?.host ?? '127.0.0.1'}
                      on:input={(e) => updateRigModeSyncRadio(activeSerial, 'r1', 'host', e.target.value)}
                    />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Port:</div>
                  <div class="value">
                    <input
                      class="input"
                      type="number"
                      min="1"
                      value={activeRigModeSync?.r1?.port ?? 4532}
                      on:input={(e) => updateRigModeSyncRadio(activeSerial, 'r1', 'port', Number(e.target.value))}
                    />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Connect Timeout (ms):</div>
                  <div class="value">
                    <input
                      class="input"
                      type="number"
                      min="100"
                      value={activeRigModeSync?.r1?.connect_timeout_ms ?? 1500}
                      on:input={(e) => updateRigModeSyncRadio(activeSerial, 'r1', 'connect_timeout_ms', Number(e.target.value))}
                    />
                  </div>
                </div>
                <div class="row">
                  <div class="label">I/O Timeout (ms):</div>
                  <div class="value">
                    <input
                      class="input"
                      type="number"
                      min="100"
                      value={activeRigModeSync?.r1?.io_timeout_ms ?? 1500}
                      on:input={(e) => updateRigModeSyncRadio(activeSerial, 'r1', 'io_timeout_ms', Number(e.target.value))}
                    />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Poll Interval (ms):</div>
                  <div class="value">
                    <input
                      class="input"
                      type="number"
                      min="100"
                      value={activeRigModeSync?.r1?.poll_ms ?? 500}
                      on:input={(e) => updateRigModeSyncRadio(activeSerial, 'r1', 'poll_ms', Number(e.target.value))}
                    />
                  </div>
                </div>
              </div>

              {#if hasR2}
                <div class="panel" style="margin-top: 12px;">
                  <div class="section-subtitle">Radio 2 Endpoint</div>
                  <div class="row">
                    <div class="label">Enabled:</div>
                    <div class="value">
                      <input
                        type="checkbox"
                        checked={!!activeRigModeSync?.r2?.enabled}
                        on:change={(e) => updateRigModeSyncRadio(activeSerial, 'r2', 'enabled', e.target.checked)}
                      />
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Host:</div>
                    <div class="value">
                      <input
                        class="input"
                        type="text"
                        value={activeRigModeSync?.r2?.host ?? '127.0.0.1'}
                        on:input={(e) => updateRigModeSyncRadio(activeSerial, 'r2', 'host', e.target.value)}
                      />
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Port:</div>
                    <div class="value">
                      <input
                        class="input"
                        type="number"
                        min="1"
                        value={activeRigModeSync?.r2?.port ?? 4533}
                        on:input={(e) => updateRigModeSyncRadio(activeSerial, 'r2', 'port', Number(e.target.value))}
                      />
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Connect Timeout (ms):</div>
                    <div class="value">
                      <input
                        class="input"
                        type="number"
                        min="100"
                        value={activeRigModeSync?.r2?.connect_timeout_ms ?? 1500}
                        on:input={(e) => updateRigModeSyncRadio(activeSerial, 'r2', 'connect_timeout_ms', Number(e.target.value))}
                      />
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">I/O Timeout (ms):</div>
                    <div class="value">
                      <input
                        class="input"
                        type="number"
                        min="100"
                        value={activeRigModeSync?.r2?.io_timeout_ms ?? 1500}
                        on:input={(e) => updateRigModeSyncRadio(activeSerial, 'r2', 'io_timeout_ms', Number(e.target.value))}
                      />
                    </div>
                  </div>
                  <div class="row">
                    <div class="label">Poll Interval (ms):</div>
                    <div class="value">
                      <input
                        class="input"
                        type="number"
                        min="100"
                        value={activeRigModeSync?.r2?.poll_ms ?? 500}
                        on:input={(e) => updateRigModeSyncRadio(activeSerial, 'r2', 'poll_ms', Number(e.target.value))}
                      />
                    </div>
                  </div>
                </div>
              {/if}

              <div class="button-row">
                <button class="btn" type="button" on:click={() => applyRigModeSync(activeSerial)}>Apply</button>
              </div>
              {#if rigModeSyncStatus[activeSerial]}
                <div class={`inline-status ${rigModeSyncStatus[activeSerial].kind === 'error' ? 'error' : ''}`}>
                  {rigModeSyncStatus[activeSerial].text}
                </div>
              {/if}
            </section>
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
          {:else if activeMenuId === 'sm_antsw_load_store'}
            <section class="section">
              <div class="section-title">Load &amp; Store Antenna Switching Settings</div>
              <div class="panel">
                <div class="row">
                  <div class="label" style="text-align: left;">
                    <button
                      class="btn"
                      type="button"
                      disabled={smActionBusy[activeSerial]}
                      on:click={() => smAction(activeSerial, 'sm_load')}
                    >Load</button>
                  </div>
                  <div class="value">Retrieve antenna switching settings from Station Master</div>
                </div>
                <div class="row">
                  <div class="label" style="text-align: left;">
                    <button
                      class="btn"
                      type="button"
                      disabled={smActionBusy[activeSerial]}
                      on:click={() => smAction(activeSerial, 'sm_store')}
                    >Store</button>
                  </div>
                  <div class="value">Send antenna switching settings to Station Master</div>
                </div>
                {#if smActionStatus[activeSerial]}
                  <div class={`inline-status ${smActionStatus[activeSerial].kind === 'error' ? 'error' : ''}`}>
                    {smActionStatus[activeSerial].text}
                  </div>
                {/if}
              </div>
            </section>
          {:else if activeMenuId === 'sm_antsw_settings'}
            <section class="section">
              <div class="section-title">Settings</div>
              <div class="panel">
                <div class="row">
                  <div class="label">PA connector CI-V Function:</div>
                  <div class="value">
                    <select class="select" value={smFixed(activeSerial, 'civFunc')}
                      on:change={(e) => applySmFixed(activeSerial, 'civFunc', e.target.value)}>
                      {#each smCivFuncOptions as opt}
                        <option value={opt.value}>{opt.label}</option>
                      {/each}
                    </select>
                  </div>
                </div>
                <div class="row">
                  <div class="label">CI-V Baud Rate:</div>
                  <div class="value">
                    <select class="select" value={smFixed(activeSerial, 'civBaudRate')}
                      on:change={(e) => applySmFixed(activeSerial, 'civBaudRate', e.target.value)}>
                      {#each smCivBaudOptions as opt}
                        <option value={opt.value}>{opt.label}</option>
                      {/each}
                    </select>
                  </div>
                </div>
                <div class="row">
                  <div class="label">CI-V Address:</div>
                  <div class="value">
                    <input class="input" type="number" min="0" max="255"
                      value={smFixed(activeSerial, 'civAddress')}
                      on:change={(e) => applySmFixed(activeSerial, 'civAddress', e.target.value)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">D9 Serial Port Function:</div>
                  <div class="value">
                    <select class="select" value={smFixed(activeSerial, 'extSerFunc')}
                      on:change={(e) => applySmFixed(activeSerial, 'extSerFunc', e.target.value)}>
                      {#each smExtSerFuncOptions as opt}
                        <option value={opt.value}>{opt.label}</option>
                      {/each}
                    </select>
                  </div>
                </div>
                <div class="row">
                  <div class="label">D9 Serial Port Baud Rate:</div>
                  <div class="value">
                    <select class="select" value={smFixed(activeSerial, 'extSerBaudRate')}
                      on:change={(e) => applySmFixed(activeSerial, 'extSerBaudRate', e.target.value)}>
                      {#each smCivBaudOptions as opt}
                        <option value={opt.value}>{opt.label}</option>
                      {/each}
                    </select>
                  </div>
                </div>
                <div class="row">
                  <div class="label">Switch Delay (ms):</div>
                  <div class="value">
                    <input class="input" type="number" min="0"
                      value={smFixed(activeSerial, 'antSwDelay')}
                      on:change={(e) => applySmFixed(activeSerial, 'antSwDelay', e.target.value)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Break before make delay (ms):</div>
                  <div class="value">
                    <input class="input" type="number" min="0"
                      value={smFixed(activeSerial, 'bbmDelay')}
                      on:change={(e) => applySmFixed(activeSerial, 'bbmDelay', e.target.value)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Inhibit lead time (ms):</div>
                  <div class="value">
                    <input class="input" type="number" min="0"
                      value={smFixed(activeSerial, 'inhibitLead')}
                      on:change={(e) => applySmFixed(activeSerial, 'inhibitLead', e.target.value)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Use Key In:</div>
                  <div class="value">
                    <input type="checkbox"
                      checked={smFixed(activeSerial, 'useKeyIn') === 1}
                      on:change={(e) => applySmFixed(activeSerial, 'useKeyIn', e.target.checked ? 1 : 0)} />
                  </div>
                </div>
                <div class="row">
                  <div class="label">Invert Key In:</div>
                  <div class="value">
                    <input type="checkbox"
                      checked={smFixed(activeSerial, 'invertKeyIn') === 1}
                      on:change={(e) => applySmFixed(activeSerial, 'invertKeyIn', e.target.checked ? 1 : 0)} />
                  </div>
                </div>
                {#if keyerStatus[activeSerial]}
                  <div class={`inline-status ${keyerStatus[activeSerial].kind === 'error' ? 'error' : ''}`}>
                    {keyerStatus[activeSerial].text}
                  </div>
                {/if}
              </div>
            </section>

            {#if smSequencerOutputs(activeSerial).length > 0}
              <section class="section">
                <div class="section-title">Sequencer</div>
                <div class="panel">
                  <div class="row" style="font-weight: 700;">
                    <div class="label">&nbsp;</div>
                    <div class="value" style="display: flex; gap: 2em;">
                      <span style="width: 80px;">Lead</span>
                      <span style="width: 80px;">Tail</span>
                    </div>
                  </div>
                  {#each smSequencerOutputs(activeSerial) as output}
                    <div class="row">
                      <div class="label">{output}:</div>
                      <div class="value" style="display: flex; gap: 2em;">
                        <input class="input" style="width: 80px;" type="number" min="0"
                          value={smSequencerVal(activeSerial, 'lead', output)}
                          on:change={(e) => applySmSequencer(activeSerial, 'lead', output, e.target.value)} />
                        <input class="input" style="width: 80px;" type="number" min="0"
                          value={smSequencerVal(activeSerial, 'tail', output)}
                          on:change={(e) => applySmSequencer(activeSerial, 'tail', output, e.target.value)} />
                      </div>
                    </div>
                  {/each}
                </div>
              </section>
            {/if}
          {:else if activeMenuId === 'sm_antsw_outputs'}
            <section class="section">
              <div class="section-title">Outputs</div>
              <div class="panel">
                {#each smOutputNames as output}
                  <div class="row">
                    <div class="label">{output}:</div>
                    <div class="value">
                      <select class="select" value={smOutputVal(activeSerial, output)}
                        on:change={(e) => applySmOutput(activeSerial, output, e.target.value)}>
                        {#each smOutputClassOptions as opt}
                          <option value={opt.value}>{opt.label}</option>
                        {/each}
                      </select>
                    </div>
                  </div>
                {/each}
                {#if keyerStatus[activeSerial]}
                  <div class={`inline-status ${keyerStatus[activeSerial].kind === 'error' ? 'error' : ''}`}>
                    {keyerStatus[activeSerial].text}
                  </div>
                {/if}
              </div>
            </section>
          {:else if activeMenuId === 'sm_antsw_ant_list'}
            {@const ants = activeAnts}
            {@const antOutCols = activeAntOutCols}
            {@const mode = smAntMode[activeSerial]}
            <section class="section">
              <div class="section-title">Antenna List</div>
              <div class="panel" style="overflow-x: auto;">
                <table class="ant-table">
                  <thead>
                    <tr>
                      <th></th>
                      <th>ID</th>
                      <th>Label</th>
                      <th>Name</th>
                      <th>SteppIR</th>
                      <th>RX-Only</th>
                      <th>PA Ant Nr</th>
                      <th>Rotator</th>
                      <th>Rot. Offset</th>
                      {#each antOutCols as oc}
                        <th>{oc}</th>
                      {/each}
                    </tr>
                  </thead>
                  <tbody>
                    {#each ants as ant (ant.id)}
                      <tr>
                        {#if mode === 'edit'}
                          <td></td>
                        {:else}
                          <td><input type="checkbox"
                              checked={(smAntSelected[activeSerial] || []).includes(ant.id)}
                              on:change={() => smAntToggleSelect(activeSerial, ant.id)} /></td>
                        {/if}
                        <td>{ant.id}</td>
                        {#if mode === 'edit'}
                          {@const ef = smAntEditForms[activeSerial]?.[ant.id] || {}}
                          <td><input class="input" type="text" size="5" value={ef.label}
                              on:input={(e) => { smAntEditForms[activeSerial][ant.id].label = e.target.value; smAntEditForms = smAntEditForms; }} /></td>
                          <td><input class="input" type="text" size="10" value={ef.display}
                              on:input={(e) => { smAntEditForms[activeSerial][ant.id].display = e.target.value; smAntEditForms = smAntEditForms; }} /></td>
                          <td><select class="select" value={ef.steppir}
                              on:change={(e) => { smAntEditForms[activeSerial][ant.id].steppir = Number(e.target.value); smAntEditForms = smAntEditForms; }}>
                              <option value={0}>No</option><option value={1}>Yes</option></select></td>
                          <td><select class="select" value={ef.rxonly}
                              on:change={(e) => { smAntEditForms[activeSerial][ant.id].rxonly = Number(e.target.value); smAntEditForms = smAntEditForms; }}>
                              <option value={0}>No</option><option value={1}>Yes</option></select></td>
                          <td><input class="input" type="number" style="width:60px" value={ef.pa_ant_number}
                              on:input={(e) => { smAntEditForms[activeSerial][ant.id].pa_ant_number = Number(e.target.value); smAntEditForms = smAntEditForms; }} /></td>
                          <td><select class="select" value={ef.rotator}
                              on:change={(e) => { smAntEditForms[activeSerial][ant.id].rotator = Number(e.target.value); smAntEditForms = smAntEditForms; }}>
                              <option value={0}>No</option><option value={1}>Yes</option></select></td>
                          <td><input class="input" type="number" style="width:70px" value={ef.rotator_offset}
                              on:input={(e) => { smAntEditForms[activeSerial][ant.id].rotator_offset = Number(e.target.value); smAntEditForms = smAntEditForms; }} /></td>
                          {#each antOutCols as oc}
                            <td><select class="select" value={ef.output?.[oc] ?? 0}
                                on:change={(e) => { smAntEditForms[activeSerial][ant.id].output[oc] = Number(e.target.value); smAntEditForms = smAntEditForms; }}>
                                <option value={0}>No</option><option value={1}>Yes</option></select></td>
                          {/each}
                        {:else}
                          <td>{ant.label || ''}</td>
                          <td>{ant.display || ''}</td>
                          <td>{ant.steppir ? 'Yes' : 'No'}</td>
                          <td>{ant.rxonly ? 'Yes' : 'No'}</td>
                          <td>{ant.pa_ant_number || 0}</td>
                          <td>{ant.rotator ? 'Yes' : 'No'}</td>
                          <td>{ant.rotator_offset || 0}</td>
                          {#each antOutCols as oc}
                            <td>{ant.output?.[oc] ? 'Yes' : 'No'}</td>
                          {/each}
                        {/if}
                      </tr>
                    {/each}
                    {#if mode === 'add'}
                      {@const af = smAntAddForm[activeSerial] || smAntNewForm()}
                      <tr class="add-row">
                        <td></td>
                        <td></td>
                        <td><input class="input" type="text" size="5" bind:value={smAntAddForm[activeSerial].label} /></td>
                        <td><input class="input" type="text" size="10" bind:value={smAntAddForm[activeSerial].display} /></td>
                        <td><select class="select" bind:value={smAntAddForm[activeSerial].steppir}>
                            <option value={0}>No</option><option value={1}>Yes</option></select></td>
                        <td><select class="select" bind:value={smAntAddForm[activeSerial].rxonly}>
                            <option value={0}>No</option><option value={1}>Yes</option></select></td>
                        <td><input class="input" type="number" style="width:60px" bind:value={smAntAddForm[activeSerial].pa_ant_number} /></td>
                        <td><select class="select" bind:value={smAntAddForm[activeSerial].rotator}>
                            <option value={0}>No</option><option value={1}>Yes</option></select></td>
                        <td><input class="input" type="number" style="width:70px" bind:value={smAntAddForm[activeSerial].rotator_offset} /></td>
                        {#each antOutCols as oc}
                          <td><select class="select" bind:value={smAntAddForm[activeSerial].output[oc]}>
                              <option value={0}>No</option><option value={1}>Yes</option></select></td>
                        {/each}
                      </tr>
                    {/if}
                  </tbody>
                </table>
                <div class="button-row" style="margin-top: 8px;">
                  {#if mode === 'add'}
                    <button class="btn" type="button" on:click={() => smAntSaveAdd(activeSerial)}>Save</button>
                    <button class="btn" type="button" on:click={() => smAntCancel(activeSerial)}>Cancel</button>
                  {:else if mode === 'edit'}
                    <button class="btn" type="button" on:click={() => smAntSaveEdit(activeSerial)}>Save</button>
                    <button class="btn" type="button" on:click={() => smAntCancel(activeSerial)}>Cancel</button>
                  {:else}
                    <button class="btn" type="button" on:click={() => smAntStartAdd(activeSerial)}>Add</button>
                    <button class="btn" type="button" on:click={() => smAntStartEdit(activeSerial)}>Edit</button>
                    <button class="btn" type="button" on:click={() => smAntRemoveSelected(activeSerial)}>Remove</button>
                  {/if}
                </div>
                {#if keyerStatus[activeSerial]}
                  <div class={`inline-status ${keyerStatus[activeSerial].kind === 'error' ? 'error' : ''}`}>
                    {keyerStatus[activeSerial].text}
                  </div>
                {/if}
              </div>
            </section>
          {:else if activeMenuId === 'sm_antsw_vr_list'}
            {@const vrs = activeVrs}
            {@const vrMode = smVrMode[activeSerial]}
            {@const vrAddAntState = smVrAddAnt[activeSerial]}
            {@const allObjs = activeAllObjs}
            <section class="section">
              <div class="section-title">Virtual Rotator List</div>
              <div class="panel" style="overflow-x: auto;">
                <table class="ant-table">
                  <thead>
                    <tr>
                      <th></th>
                      <th>ID</th>
                      <th>Label</th>
                      <th>Name</th>
                      <th>RX-Only</th>
                      <th>Azimuth Min</th>
                      <th>Azimuth Max</th>
                      <th>Antenna</th>
                    </tr>
                  </thead>
                  <tbody>
                    {#each vrs as vr (vr.id)}
                      <!-- VR main row -->
                      <tr class="vr-main-row">
                        {#if vrMode === 'edit'}
                          <td></td>
                        {:else}
                          <td><input type="checkbox"
                              checked={(smVrSelected[activeSerial] || []).includes(vr.id)}
                              on:change={() => smVrToggleSelect(activeSerial, vr.id)} /></td>
                        {/if}
                        <td>{vr.id}</td>
                        {#if vrMode === 'edit'}
                          {@const ef = smVrEditForms[activeSerial]?.[vr.id] || {}}
                          <td><input class="input" type="text" size="5" value={ef.label}
                              on:input={(e) => { smVrEditForms[activeSerial][vr.id].label = e.target.value; smVrEditForms = smVrEditForms; }} /></td>
                          <td><input class="input" type="text" size="10" value={ef.display}
                              on:input={(e) => { smVrEditForms[activeSerial][vr.id].display = e.target.value; smVrEditForms = smVrEditForms; }} /></td>
                        {:else}
                          <td>{vr.label || ''}</td>
                          <td>{vr.display || ''}</td>
                        {/if}
                        <td></td>
                        <td></td>
                        <td></td>
                        <td>
                          {#if !vrMode && !vrAddAntState}
                            <button class="btn" type="button" on:click={() => smVrStartAddAnt(activeSerial, vr.id)}>Add Antenna</button>
                          {/if}
                        </td>
                      </tr>
                      <!-- Ref rows for this VR -->
                      {#each (vr.refs || []) as ref (ref.id)}
                        <tr>
                          {#if vrMode === 'edit'}
                            <td></td>
                          {:else}
                            <td><input type="checkbox"
                                checked={(smVrSelected[activeSerial] || []).includes(`${vr.id}:${ref.id}`)}
                                on:change={() => smVrToggleSelect(activeSerial, `${vr.id}:${ref.id}`)} /></td>
                          {/if}
                          <td></td>
                          <td></td>
                          <td></td>
                          <td>{smObjRxOnly(activeSerial, ref.dest_id)}</td>
                          {#if vrMode === 'edit'}
                            {@const rf = smVrEditForms[activeSerial]?.[vr.id]?.refs?.[ref.id] || {}}
                            <td><input class="input" type="number" style="width:70px" value={rf.min_azimuth}
                                on:input={(e) => { smVrEditForms[activeSerial][vr.id].refs[ref.id].min_azimuth = Number(e.target.value); smVrEditForms = smVrEditForms; }} /></td>
                            <td><input class="input" type="number" style="width:70px" value={rf.max_azimuth}
                                on:input={(e) => { smVrEditForms[activeSerial][vr.id].refs[ref.id].max_azimuth = Number(e.target.value); smVrEditForms = smVrEditForms; }} /></td>
                            <td>
                              <select class="select" value={rf.dest_id}
                                  on:change={(e) => { smVrEditForms[activeSerial][vr.id].refs[ref.id].dest_id = Number(e.target.value); smVrEditForms = smVrEditForms; }}>
                                {#each allObjs as obj}
                                  <option value={obj.id}>{obj.display || obj.label || `#${obj.id}`}</option>
                                {/each}
                              </select>
                            </td>
                          {:else}
                            <td>{ref.min_azimuth || 0}</td>
                            <td>{ref.max_azimuth || 0}</td>
                            <td>{smObjDisplay(activeSerial, ref.dest_id)}</td>
                          {/if}
                        </tr>
                      {/each}
                      <!-- Add Antenna row for this VR -->
                      {#if vrAddAntState && vrAddAntState.vrId === vr.id}
                        <tr class="add-row">
                          <td></td>
                          <td></td>
                          <td></td>
                          <td></td>
                          <td></td>
                          <td><input class="input" type="number" style="width:70px" bind:value={smVrAddAnt[activeSerial].min_azimuth} /></td>
                          <td><input class="input" type="number" style="width:70px" bind:value={smVrAddAnt[activeSerial].max_azimuth} /></td>
                          <td>
                            <select class="select" bind:value={smVrAddAnt[activeSerial].dest_id}>
                              {#each allObjs as obj}
                                <option value={obj.id}>{obj.display || obj.label || `#${obj.id}`}</option>
                              {/each}
                            </select>
                          </td>
                        </tr>
                      {/if}
                    {/each}
                    <!-- Add new VR row -->
                    {#if vrMode === 'add'}
                      <tr class="add-row">
                        <td></td>
                        <td></td>
                        <td><input class="input" type="text" size="5" bind:value={smVrAddForm[activeSerial].label} /></td>
                        <td><input class="input" type="text" size="10" bind:value={smVrAddForm[activeSerial].display} /></td>
                        <td></td>
                        <td></td>
                        <td></td>
                        <td></td>
                      </tr>
                    {/if}
                  </tbody>
                </table>
                <div class="button-row" style="margin-top: 8px;">
                  {#if vrAddAntState}
                    <button class="btn" type="button" on:click={() => smVrSaveAddAnt(activeSerial)}>Save</button>
                    <button class="btn" type="button" on:click={() => smVrCancelAddAnt(activeSerial)}>Cancel</button>
                  {:else if vrMode === 'add'}
                    <button class="btn" type="button" on:click={() => smVrSaveAdd(activeSerial)}>Save</button>
                    <button class="btn" type="button" on:click={() => smVrCancel(activeSerial)}>Cancel</button>
                  {:else if vrMode === 'edit'}
                    <button class="btn" type="button" on:click={() => smVrSaveEdit(activeSerial)}>Save</button>
                    <button class="btn" type="button" on:click={() => smVrCancel(activeSerial)}>Cancel</button>
                  {:else}
                    <button class="btn" type="button" on:click={() => smVrStartAdd(activeSerial)}>Add</button>
                    <button class="btn" type="button" on:click={() => smVrStartEdit(activeSerial)}>Edit</button>
                    <button class="btn" type="button" on:click={() => smVrRemoveSelected(activeSerial)}>Remove</button>
                  {/if}
                </div>
                {#if keyerStatus[activeSerial]}
                  <div class={`inline-status ${keyerStatus[activeSerial].kind === 'error' ? 'error' : ''}`}>
                    {keyerStatus[activeSerial].text}
                  </div>
                {/if}
              </div>
            </section>
          {:else if activeMenuId === 'sm_antsw_grp_list'}
            {@const grps = activeGrps}
            {@const grpMode = smGrpMode[activeSerial]}
            {@const grpAddAntState = smGrpAddAnt[activeSerial]}
            {@const allObjs = activeAllObjs}
            <section class="section">
              <div class="section-title">Antenna Groups List</div>
              <div class="panel" style="overflow-x: auto;">
                <table class="ant-table">
                  <thead>
                    <tr>
                      <th></th>
                      <th>ID</th>
                      <th>Label</th>
                      <th>Name</th>
                      <th>RX-Only</th>
                      <th>Antenna</th>
                    </tr>
                  </thead>
                  <tbody>
                    {#each grps as grp (grp.id)}
                      <!-- Group main row -->
                      <tr class="vr-main-row">
                        {#if grpMode === 'edit'}
                          <td></td>
                        {:else}
                          <td><input type="checkbox"
                              checked={(smGrpSelected[activeSerial] || []).includes(grp.id)}
                              on:change={() => smGrpToggleSelect(activeSerial, grp.id)} /></td>
                        {/if}
                        <td>{grp.id}</td>
                        {#if grpMode === 'edit'}
                          {@const ef = smGrpEditForms[activeSerial]?.[grp.id] || {}}
                          <td><input class="input" type="text" size="5" value={ef.label}
                              on:input={(e) => { smGrpEditForms[activeSerial][grp.id].label = e.target.value; smGrpEditForms = smGrpEditForms; }} /></td>
                          <td><input class="input" type="text" size="10" value={ef.display}
                              on:input={(e) => { smGrpEditForms[activeSerial][grp.id].display = e.target.value; smGrpEditForms = smGrpEditForms; }} /></td>
                        {:else}
                          <td>{grp.label || ''}</td>
                          <td>{grp.display || ''}</td>
                        {/if}
                        <td></td>
                        <td>
                          {#if !grpMode && !grpAddAntState}
                            <button class="btn" type="button" on:click={() => smGrpStartAddAnt(activeSerial, grp.id)}>Add Antenna</button>
                          {/if}
                        </td>
                      </tr>
                      <!-- Ref rows for this group -->
                      {#each (grp.refs || []) as ref (ref.id)}
                        <tr>
                          {#if grpMode === 'edit'}
                            <td></td>
                          {:else}
                            <td><input type="checkbox"
                                checked={(smGrpSelected[activeSerial] || []).includes(`${grp.id}:${ref.id}`)}
                                on:change={() => smGrpToggleSelect(activeSerial, `${grp.id}:${ref.id}`)} /></td>
                          {/if}
                          <td></td>
                          <td></td>
                          <td></td>
                          <td>{smObjRxOnly(activeSerial, ref.dest_id)}</td>
                          {#if grpMode === 'edit'}
                            {@const rf = smGrpEditForms[activeSerial]?.[grp.id]?.refs?.[ref.id] || {}}
                            <td>
                              <select class="select" value={rf.dest_id}
                                  on:change={(e) => { smGrpEditForms[activeSerial][grp.id].refs[ref.id].dest_id = Number(e.target.value); smGrpEditForms = smGrpEditForms; }}>
                                {#each allObjs as obj}
                                  <option value={obj.id}>{obj.display || obj.label || `#${obj.id}`}</option>
                                {/each}
                              </select>
                            </td>
                          {:else}
                            <td>{smObjDisplay(activeSerial, ref.dest_id)}</td>
                          {/if}
                        </tr>
                      {/each}
                      <!-- Add Antenna row for this group -->
                      {#if grpAddAntState && grpAddAntState.grpId === grp.id}
                        <tr class="add-row">
                          <td></td>
                          <td></td>
                          <td></td>
                          <td></td>
                          <td></td>
                          <td>
                            <select class="select" bind:value={smGrpAddAnt[activeSerial].dest_id}>
                              {#each allObjs as obj}
                                <option value={obj.id}>{obj.display || obj.label || `#${obj.id}`}</option>
                              {/each}
                            </select>
                          </td>
                        </tr>
                      {/if}
                    {/each}
                    <!-- Add new group row -->
                    {#if grpMode === 'add'}
                      <tr class="add-row">
                        <td></td>
                        <td></td>
                        <td><input class="input" type="text" size="5" bind:value={smGrpAddForm[activeSerial].label} /></td>
                        <td><input class="input" type="text" size="10" bind:value={smGrpAddForm[activeSerial].display} /></td>
                        <td></td>
                        <td></td>
                      </tr>
                    {/if}
                  </tbody>
                </table>
                <div class="button-row" style="margin-top: 8px;">
                  {#if grpAddAntState}
                    <button class="btn" type="button" on:click={() => smGrpSaveAddAnt(activeSerial)}>Save</button>
                    <button class="btn" type="button" on:click={() => smGrpCancelAddAnt(activeSerial)}>Cancel</button>
                  {:else if grpMode === 'add'}
                    <button class="btn" type="button" on:click={() => smGrpSaveAdd(activeSerial)}>Save</button>
                    <button class="btn" type="button" on:click={() => smGrpCancel(activeSerial)}>Cancel</button>
                  {:else if grpMode === 'edit'}
                    <button class="btn" type="button" on:click={() => smGrpSaveEdit(activeSerial)}>Save</button>
                    <button class="btn" type="button" on:click={() => smGrpCancel(activeSerial)}>Cancel</button>
                  {:else}
                    <button class="btn" type="button" on:click={() => smGrpStartAdd(activeSerial)}>Add</button>
                    <button class="btn" type="button" on:click={() => smGrpStartEdit(activeSerial)}>Edit</button>
                    <button class="btn" type="button" on:click={() => smGrpRemoveSelected(activeSerial)}>Remove</button>
                  {/if}
                </div>
                {#if keyerStatus[activeSerial]}
                  <div class={`inline-status ${keyerStatus[activeSerial].kind === 'error' ? 'error' : ''}`}>
                    {keyerStatus[activeSerial].text}
                  </div>
                {/if}
              </div>
            </section>
          {:else if activeMenuId === 'sm_antsw_band_list'}
            {@const bnds = activeBnds}
            {@const bndMode = smBndMode[activeSerial]}
            {@const bndAddRefState = smBndAddRef[activeSerial]}
            {@const bpfOuts = activeBpfOutputs}
            {@const seqOuts = activeSeqOutputs}
            {@const antOuts = activeAntOutputs}
            {@const antGrpObjs = activeAntsAndGroups}
            <section class="section">
              <div class="section-title">Band List</div>
              <div class="panel" style="overflow-x: auto;">
                <table class="ant-table">
                  <thead>
                    <tr>
                      <th></th>
                      <th>ID</th>
                      <th>Name</th>
                      <th colspan="2">Frequency Range</th>
                      <th>Code</th>
                      <th>PA</th>
                      <th>KeyOut</th>
                      <th>Ant</th>
                      {#if bpfOuts.length > 0}<th>BPF</th>{/if}
                      {#if seqOuts.length > 0}<th>SEQ</th>{/if}
                    </tr>
                  </thead>
                  <tbody>
                    {#each bnds as bnd (bnd.id)}
                      <!-- Band main row -->
                      <tr class="vr-main-row">
                        {#if bndMode === 'edit'}
                          <td></td>
                        {:else}
                          <td><input type="checkbox"
                              checked={(smBndSelected[activeSerial] || []).includes(bnd.id)}
                              on:change={() => smBndToggleSelect(activeSerial, bnd.id)} /></td>
                        {/if}
                        <td>{bnd.id}</td>
                        {#if bndMode === 'edit'}
                          {@const ef = smBndEditForms[activeSerial]?.[bnd.id] || {}}
                          <td><input class="input" type="text" size="10" value={ef.display}
                              on:input={(e) => { smBndEditForms[activeSerial][bnd.id].display = e.target.value; smBndEditForms = smBndEditForms; }} /></td>
                          <td><input class="input" type="number" style="width:90px" value={ef.low_freq}
                              on:input={(e) => { smBndEditForms[activeSerial][bnd.id].low_freq = Number(e.target.value); smBndEditForms = smBndEditForms; }} /></td>
                          <td><input class="input" type="number" style="width:90px" value={ef.high_freq}
                              on:input={(e) => { smBndEditForms[activeSerial][bnd.id].high_freq = Number(e.target.value); smBndEditForms = smBndEditForms; }} /></td>
                          <td><input class="input" type="number" style="width:40px" value={ef.bcd_code}
                              on:input={(e) => { smBndEditForms[activeSerial][bnd.id].bcd_code = Number(e.target.value); smBndEditForms = smBndEditForms; }} /></td>
                          <td><input type="checkbox" checked={ef.pa_power === 1}
                              on:change={(e) => { smBndEditForms[activeSerial][bnd.id].pa_power = e.target.checked ? 1 : 0; smBndEditForms = smBndEditForms; }} /></td>
                          <td><input type="checkbox" checked={ef.keyout === 1}
                              on:change={(e) => { smBndEditForms[activeSerial][bnd.id].keyout = e.target.checked ? 1 : 0; smBndEditForms = smBndEditForms; }} /></td>
                          <td></td>
                          {#if bpfOuts.length > 0}
                            <td>
                              <table class="ant-table" style="margin:0">
                                <tr>{#each bpfOuts as bo}<td style="padding:1px 4px;font-size:0.85em">{bo}</td>{/each}</tr>
                                <tr>{#each bpfOuts as bo}
                                  <td style="padding:1px 4px"><input type="checkbox" checked={(ef.bpf_seq?.[bo] || 0) === 1}
                                    on:change={(e) => { if(!smBndEditForms[activeSerial][bnd.id].bpf_seq) smBndEditForms[activeSerial][bnd.id].bpf_seq = {}; smBndEditForms[activeSerial][bnd.id].bpf_seq[bo] = e.target.checked ? 1 : 0; smBndEditForms = smBndEditForms; }} /></td>
                                {/each}</tr>
                              </table>
                            </td>
                          {/if}
                          {#if seqOuts.length > 0}
                            <td>
                              <table class="ant-table" style="margin:0">
                                <tr>{#each seqOuts as so}<td style="padding:1px 4px;font-size:0.85em">{so}</td>{/each}</tr>
                                <tr>{#each seqOuts as so}
                                  <td style="padding:1px 4px"><input type="checkbox" checked={(ef.bpf_seq?.[so] || 0) === 1}
                                    on:change={(e) => { if(!smBndEditForms[activeSerial][bnd.id].bpf_seq) smBndEditForms[activeSerial][bnd.id].bpf_seq = {}; smBndEditForms[activeSerial][bnd.id].bpf_seq[so] = e.target.checked ? 1 : 0; smBndEditForms = smBndEditForms; }} /></td>
                                {/each}</tr>
                              </table>
                            </td>
                          {/if}
                        {:else}
                          <td>{bnd.display || ''}</td>
                          <td>{bnd.low_freq || 0}</td>
                          <td>{bnd.high_freq || 0}</td>
                          <td>{bnd.bcd_code || 0}</td>
                          <td>{bnd.pa_power ? 'Yes' : 'No'}</td>
                          <td>{bnd.keyout ? 'Yes' : 'No'}</td>
                          <td></td>
                          {#if bpfOuts.length > 0}
                            <td>
                              <table class="ant-table" style="margin:0">
                                <tr>{#each bpfOuts as bo}<td style="padding:1px 4px;font-size:0.85em">{bo}</td>{/each}</tr>
                                <tr>{#each bpfOuts as bo}<td style="padding:1px 4px">{(bnd.bpf_seq?.[bo] || 0) ? 'Yes' : 'No'}</td>{/each}</tr>
                              </table>
                            </td>
                          {/if}
                          {#if seqOuts.length > 0}
                            <td>
                              <table class="ant-table" style="margin:0">
                                <tr>{#each seqOuts as so}<td style="padding:1px 4px;font-size:0.85em">{so}</td>{/each}</tr>
                                <tr>{#each seqOuts as so}<td style="padding:1px 4px">{(bnd.bpf_seq?.[so] || 0) ? 'Yes' : 'No'}</td>{/each}</tr>
                              </table>
                            </td>
                          {/if}
                        {/if}
                      </tr>
                      <!-- Ref rows for this band -->
                      {#each (bnd.refs || []) as ref (ref.id)}
                        {@const destObj = activeAllObjs.find((o) => o.id === ref.dest_id)}
                        <tr>
                          {#if bndMode === 'edit'}
                            <td></td>
                          {:else}
                            <td><input type="checkbox"
                                checked={(smBndSelected[activeSerial] || []).includes(`${bnd.id}:${ref.id}`)}
                                on:change={() => smBndToggleSelect(activeSerial, `${bnd.id}:${ref.id}`)} /></td>
                          {/if}
                          <td></td>
                          {#if bndMode === 'edit'}
                            {@const rf = smBndEditForms[activeSerial]?.[bnd.id]?.refs?.[ref.id] || {}}
                            <td colspan="3">
                              <select class="select" value={rf.dest_id}
                                  on:change={(e) => { smBndEditForms[activeSerial][bnd.id].refs[ref.id].dest_id = Number(e.target.value); smBndEditForms = smBndEditForms; }}>
                                {#each antGrpObjs as obj}
                                  <option value={obj.id}>{obj.type === 0 ? 'ANT' : obj.type === 1 && obj.virtual_rotator ? 'VIR' : 'GRP'}: {obj.display || obj.label || `#${obj.id}`}</option>
                                {/each}
                              </select>
                            </td>
                            <td>
                              RX only <input type="checkbox" checked={(rf.rxonly || 0) === 1}
                                on:change={(e) => { smBndEditForms[activeSerial][bnd.id].refs[ref.id].rxonly = e.target.checked ? 1 : 0; smBndEditForms = smBndEditForms; }} />
                            </td>
                          {:else}
                            <td colspan="3">{smObjTypedDisplay(activeSerial, ref.dest_id)}</td>
                            <td>RX only {ref.rxonly ? 'Yes' : 'No'}</td>
                          {/if}
                          <td></td>
                          <td>
                            {#if destObj && destObj.type === 0}
                              <table class="ant-table" style="margin:0">
                                <tr>{#each antOuts as ao}<td style="padding:1px 4px;font-size:0.85em">{ao}</td>{/each}</tr>
                                <tr>{#each antOuts as ao}<td style="padding:1px 4px">{(destObj.output?.[ao] || 0) ? 'Yes' : 'No'}</td>{/each}</tr>
                              </table>
                            {/if}
                          </td>
                          {#if bpfOuts.length > 0}<td></td>{/if}
                          {#if seqOuts.length > 0}<td></td>{/if}
                        </tr>
                      {/each}
                      <!-- Add ref row -->
                      <tr>
                        <td></td>
                        <td></td>
                        {#if bndAddRefState && bndAddRefState.bndId === bnd.id}
                          <td colspan="3">
                            <select class="select" bind:value={smBndAddRef[activeSerial].dest_id}>
                              {#each antGrpObjs as obj}
                                <option value={obj.id}>{obj.type === 0 ? 'ANT' : obj.type === 1 && obj.virtual_rotator ? 'VIR' : 'GRP'}: {obj.display || obj.label || `#${obj.id}`}</option>
                              {/each}
                            </select>
                          </td>
                          <td></td>
                          <td></td>
                          <td>
                            <button class="btn" type="button" on:click={() => smBndSaveAddRef(activeSerial)}>Save</button>
                          </td>
                          <td>
                            <button class="btn" type="button" on:click={() => smBndCancelAddRef(activeSerial)}>Cancel</button>
                          </td>
                          {#if seqOuts.length > 0}<td></td>{/if}
                        {:else}
                          <td colspan="5"></td>
                          {#if bpfOuts.length > 0}<td></td>{/if}
                          {#if seqOuts.length > 0}<td></td>{/if}
                          <td>
                            {#if !bndMode && !bndAddRefState}
                              <button class="btn" type="button" on:click={() => smBndStartAddRef(activeSerial, bnd.id)}>Add</button>
                            {/if}
                          </td>
                        {/if}
                      </tr>
                    {/each}
                    <!-- Add new band row -->
                    {#if bndMode === 'add'}
                      <tr class="add-row">
                        <td></td>
                        <td></td>
                        <td><input class="input" type="text" size="10" bind:value={smBndAddForm[activeSerial].display} /></td>
                        <td><input class="input" type="number" style="width:90px" bind:value={smBndAddForm[activeSerial].low_freq} /></td>
                        <td><input class="input" type="number" style="width:90px" bind:value={smBndAddForm[activeSerial].high_freq} /></td>
                        <td><input class="input" type="number" style="width:40px" bind:value={smBndAddForm[activeSerial].bcd_code} /></td>
                        <td><input type="checkbox" bind:checked={smBndAddForm[activeSerial].pa_power} /></td>
                        <td><input type="checkbox" bind:checked={smBndAddForm[activeSerial].keyout} /></td>
                        <td></td>
                        {#if bpfOuts.length > 0}<td></td>{/if}
                        {#if seqOuts.length > 0}<td></td>{/if}
                      </tr>
                    {/if}
                  </tbody>
                </table>
                <div class="button-row" style="margin-top: 8px;">
                  {#if bndAddRefState}
                    <!-- save/cancel handled inline -->
                  {:else if bndMode === 'add'}
                    <button class="btn" type="button" on:click={() => smBndSaveAdd(activeSerial)}>Save</button>
                    <button class="btn" type="button" on:click={() => smBndCancel(activeSerial)}>Cancel</button>
                  {:else if bndMode === 'edit'}
                    <button class="btn" type="button" on:click={() => smBndSaveEdit(activeSerial)}>Save</button>
                    <button class="btn" type="button" on:click={() => smBndCancel(activeSerial)}>Cancel</button>
                  {:else}
                    <button class="btn" type="button" on:click={() => smBndStartAdd(activeSerial)}>Add</button>
                    <button class="btn" type="button" on:click={() => smBndStartEdit(activeSerial)}>Edit</button>
                    <button class="btn" type="button" on:click={() => smBndRemoveSelected(activeSerial)}>Remove</button>
                  {/if}
                </div>
                {#if keyerStatus[activeSerial]}
                  <div class={`inline-status ${keyerStatus[activeSerial].kind === 'error' ? 'error' : ''}`}>
                    {keyerStatus[activeSerial].text}
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
