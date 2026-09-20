# Making mhuxd VSPs show up in application port lists

Investigated 2026-09-20/21 against fldigi 4.2.11, flrig, QLog and Hamlib (git
working copies), with four live VSPs on the machine. Results below were
confirmed on a running system.

## The short answer

Renaming the VSPs cannot fix this on its own. The applications do not filter by
name, they enumerate a **subsystem**, and a VSP is a CUSE character device:

```
$ udevadm info -q all -n /dev/mhuxd/cat1
P: /devices/virtual/cuse/mhuxd!cat1
N: mhuxd/cat1
E: DEVNAME=/dev/mhuxd/cat1
E: SUBSYSTEM=cuse
```

The `cuse` class is assigned by the kernel and cannot be changed from userspace.
There is no `/sys/class/tty` entry, no parent device, no `ID_BUS`. Naming a VSP
`ttyMH0` would produce `/dev/mhuxd/ttyMH0` with `SUBSYSTEM=cuse` - still
invisible to anything that enumerates the `tty` subsystem.

Two hooks exist that do not depend on the subsystem:

* `/dev/serial/by-id/` - a plain directory of symlinks that anyone can add to
  via udev. fldigi, flrig and Hamlib all glob it.
* `/sys/class/cuse/` - scanned directly by flrig.

## fldigi 4.2.11

Consistent: one function feeds every port widget.

* `src/misc/configuration.cxx:1057` `test_Linux_COMports()` - the only Linux
  enumerator, called from `testCommPorts()` at `src/misc/configuration.cxx:1102`.
* `src/misc/configuration.cxx:891` `update_COM_controls()` - adds the path to all
  12 combo boxes at once: `inpTTYdev` (PTT), `inpRIGdev` (Hamlib device),
  `inpXmlRigDevice` (RigCAT device), `select_nanoIO_CommPort`,
  `select_nanoCW_CommPort`, `select_CW_KEYLINE_CommPort`, `select_FSK_CommPort`,
  `select_USN_FSK_port`, `select_Nav_config_port`, `select_WK_CommPort`,
  `select_WKFSK_CommPort`.

What it scans:

1. `glob("/dev/serial/by-id/*")`, accepting anything that `stat()`s as a
   character device and whose name does not contain `"modem"`.
2. `/dev/ttyS%u`, `/dev/ttyUSB%u`, `/dev/usb/ttyUSB%u`, `/dev/ttyACM%u`,
   `/dev/usb/ttyACM%u`, `/dev/rfcomm%u`, `/opt/vttyS%u`, each for index 0..7.

Step 1 is the opening: `stat()` follows symlinks, so a by-id symlink pointing at
a CUSE device passes the filter. **Confirmed on the running system - fldigi
lists all four VSPs once the udev rule below is installed. No fldigi patch is
needed.**

There is no mhuxd awareness in fldigi. Its only virtual-port special case is the
microHAM uH Router FIFO, added to `inpTTYdev` only and not to the other widgets
(`src/misc/configuration.cxx:1091`) - but that is `#if HAVE_UHROUTER`, which is
macOS-only (`src/include/ptt.h:48`), so it never applies on Linux.

## flrig

flrig is the one that knows about mhuxd, and it does it properly - it reads
`/sys/class/cuse/` directly rather than relying on a symlink:

* `src/support/dialogs.cxx:157` `init_port_combos()` (Linux version).
* `src/support/dialogs.cxx:245` onwards: `chdir("/sys/class/cuse")`, then for
  each entry whose target contains `/devices/virtual/` and whose name starts
  with `"mhuxd"` (`:258`), it rewrites the first `!` back into a `/` and offers
  `/dev/mhuxd/<name>`.
* `src/support/dialogs.cxx:88` `add_combos()` - feeds all five combos
  (`selectCommPort`, `selectAuxPort`, `selectSepPTTPort`, `select_cwioPORT`,
  `select_fskioPORT`), so this is consistent too.

flrig additionally globs `/dev/pts/*`, `/dev/serial/by-id/*`, `/dev/tty*` and
`/dev/tnt*`, and scans `/sys/class/tty`. **Confirmed: with the udev rule
installed, flrig lists each VSP twice** - once as `/dev/serial/by-id/mhuxd-cat1`
(new) and once as `/dev/mhuxd/cat1` (its own cuse scan). Cosmetic; both paths
open the same device.

## QLog

Consistent: one widget class, used for every serial field.

* `ui/component/EditLine.cpp:99` `SerialPortEditLine::SerialPortEditLine()`.
* Used by `cwPortEdit`, `rigPortEdit`, `rigPTTPortEdit`, `rotPortEdit`
  (`ui/SettingsDialog.ui`) and dynamically in `ui/PlatformSettingsDialog.cpp:109`.

The list comes from `QSerialPortInfo::availablePorts()`, which asks udev for the
`tty` subsystem (`udev_enumerate_add_match_subsystem`, present in
`libQt6SerialPort.so.6`). `/dev/serial/by-id` is then used **for display names
only**: the loop iterates over the *ports*, and consults the symlink list purely
to relabel a port that was already found -

```cpp
for ( const QSerialPortInfo &port : ports )        // <- iterates ports
{
    QString dev = QString("/dev/%1").arg(port.portName());
    QString niceName = dev;
    for ( const QString &entry : symlinks )        // <- lookup table only
        if ( QFileInfo(...).canonicalFilePath() == dev ) { niceName = fullPath; break; }
    portNames << niceName;                         // <- one entry per port
}
```

Nothing ever iterates `symlinks` into `portNames`. A by-id link whose target is
not in `availablePorts()` is simply never reached. So:

* the keyer's FTDI port *is* a tty, so it is in `availablePorts()` as
  `ttyUSB0`, its by-id link matches, and QLog shows
  `/dev/serial/by-id/usb-microHAM_micro_KEYER_III_...` - one entry, nicer label;
* a VSP is not in `availablePorts()` at all, so `mhuxd-cat1` sits in the
  `symlinks` list unused and never becomes an entry.

That is expected behaviour, and it means the udev rule does nothing for QLog.
QLog needs a code change; see the patch below.

The widget is a `QLineEdit` with a `QCompleter`, so a path can always be typed by
hand. The completer prefix is `/dev/` on non-Windows
(`ui/component/EditLine.cpp:151`) and `QCompleter` defaults to
`MatchStartsWith`, so any `/dev/...` path that reaches the list is offered.

## Hamlib

No port enumeration for a UI, and no name-based restriction on what it will
open: `ser_open()` special-cases only the literal pathnames `uh-rig` and
`uh-ptt`, everything else goes straight to `open()` (`src/serial.c:925`).
`/dev/mhuxd/cat1` works as-is.

Worth knowing: Hamlib's own microHAM support globs
`/dev/serial/by-id/*microHAM*_MK*` and friends (`src/microham.c:255`) to find the
raw FTDI device. That is a competing user of the keyer, not of our VSPs, but it
confirms `/dev/serial/by-id/` as the de-facto discovery namespace in ham
software.

## What was changed in mhuxd

`debian/mhuxd.udev` now publishes every VSP as `/dev/serial/by-id/mhuxd-<name>`:

```
SUBSYSTEM=="cuse", KERNEL=="mhuxd*", PROGRAM="/usr/bin/basename $env{DEVNAME}", SYMLINK+="serial/by-id/mhuxd-%c"
```

`PROGRAM` reduces `DEVNAME=/dev/mhuxd/cat1` to `cat1`; `%c` is its output. The
sysfs name cannot be used directly because it is `mhuxd!cat1`.

`GROUP="mhuxd"` was split onto its own rule line on purpose: a `PROGRAM` that
exits non-zero makes the whole rule line not match, which would otherwise take
the group ownership down with it and leave the VSP root-only.

`SYMLINK+=` (rather than `RUN+=ln`) means udev removes the link again when the
connector is destroyed.

Install and verify:

```sh
sudo cp debian/mhuxd.udev /usr/lib/udev/rules.d/59-mhuxd.rules
sudo udevadm control --reload-rules
sudo udevadm trigger --subsystem-match=cuse
ls -l /dev/serial/by-id/
```

Confirmed working:

```
mhuxd-cat1 -> ../../mhuxd/cat1
mhuxd-fsk1 -> ../../mhuxd/fsk1
mhuxd-ptt1 -> ../../mhuxd/ptt1
mhuxd-wk   -> ../../mhuxd/wk
usb-microHAM_micro_KEYER_III_M32VFFGJ-if00-port0 -> ../../ttyUSB0
```

Result: fldigi and flrig both list the VSPs. QLog does not.

## QLog patch

`docs/patches/qlog-mhuxd-port-enumeration.patch`. Purely additive - it does not
touch the existing loop. After the port loop it appends every `/dev/serial/by-id`
entry that no port claimed, then the contents of `/dev/mhuxd` for installations
whose udev rules predate the symlinks, deduplicating on
`QFileInfo::canonicalFilePath()` so a device reachable under several names is
listed once.

Part one is vendor-neutral - it picks up any CUSE-based VSP daemon that
registers a by-id link - which should make it easier to upstream.

Compiles cleanly against the QLog working copy with its own build flags.
Behaviour was verified by running QLog's list builder, transcribed verbatim, on
this machine:

```
=== QLog as shipped ===
  /dev/serial/by-id/usb-microHAM_micro_KEYER_III_M32VFFGJ-if00-port0
  /dev/ttyS1 /dev/ttyS2 /dev/ttyS3 /dev/ttyS0
=== QLog with patch ===
  ... the same five, plus
  /dev/serial/by-id/mhuxd-cat1
  /dev/serial/by-id/mhuxd-fsk1
  /dev/serial/by-id/mhuxd-ptt1
  /dev/serial/by-id/mhuxd-wk
```

`QDir::Readable` means a user who is not in the `mhuxd` group will not see the
ports - which is correct, they could not open them either.

## VSP device name validation

`cspec->vsp.devname` used to go straight from config into `DEVNAME=mhuxd/%s`
(`src/con_vsp.c`) unchecked, so a name containing `/`, a space or `..` produced a
surprising device path - and, with the new udev rule, a surprising symlink or a
`SYMLINK+=` value that udev would split on whitespace. It also broke flrig, whose
translation rewrites only the first `!` (`src/support/dialogs.cxx:262`).

`vsp_devname_is_valid()` (`src/con_vsp.c`) now accepts 1 to `VSP_DEVNAME_MAX`
(64) characters from `[A-Za-z0-9_.-]`, first character alphanumeric - which also
rules out `.` and `..`. The buffers in `vsp_create()` would tolerate 112
characters; 64 is simply a sane device name length.

It is enforced at all three entry points:

* `conmgr_create_con_cfg()` (`src/conmgr.c`) - the funnel every path goes
  through, including the legacy HDF config (`src/cfgmgr.c:81`). Fails the
  connector with an `err()` naming the offending value, in the same style as the
  existing `serial` / `channel` / `type` checks.
* `apply_connector_from_json()` (`src/cfgmgrj.c`) - rejects before
  `conmgr_create_con_cfg()` is called, so a bad name is not written to the config
  file and the REST caller sees the request fail. Without this the connector
  would be recorded as "intent" and the API would report success.
* `vsp_create()` (`src/con_vsp.c`) - last line of defence, next to the code that
  builds the device name.

Both web UIs check the same rule before submitting: the Svelte UI in
`webui/svelte/src/lib/DaemonPorts.svelte` (`vspDevnameValid`, with a message
naming what is allowed), and the classic UI via `maxlength` / `pattern` on the
input in `webui/cs/daemon_ports.cs`. A Node cross-check confirmed the JavaScript
and C rules agree on all 28 cases.

`tests/vsp/devname_test.c` is a unit test that links the real `con_vsp.o`. It
needs no root and touches no devices:

```sh
make -C build && make -C tests/vsp && ./tests/vsp/devname_test
```

### Note on upgrades

This is a behaviour change: a VSP whose configured name does not match the rule
will no longer be created, and the log will say why. Names the web UI generates
(`cat1`, `ptt1`, `fsk1`, `wk`, `cat1_2`) are all fine; a hand-edited config with
something exotic in it is the case to watch for.

### Still open

`apply_connector_from_json()` returns `-1` for every kind of malformed input, and
`restapi.c:644` maps that to HTTP 500. A validation failure is really a 400.
Changing that means giving the function a richer error contract, so it was left
alone - the web UIs now catch these before they are sent.
