# Linux audio with microHAM codecs (WSJT-X, PipeWire, ALSA)

Investigated 2026-10-06 with a DigiKeyer II on Debian 13 (kernel 6.12),
PipeWire 1.4.2, WirePlumber 0.5.8 and WSJT-X 3.1.0 improved PLUS (Qt5 build).
Measurements below were taken on the running system. Items marked *untested*
were not verified.

## The short answer

A DK2 connected to WSJT-X showed signals in the waterfall that were
"interrupted in intervals", and only a small part of them decoded. The cause
was not the audio stream itself but the codec's **mixer**:

* PipeWire picked the codec's **Microphone** port automatically, because the
  generic port recipes give Mic a higher priority (8700) than Line In (8100).
  The DK2 delivers radio RX audio on the **Line** input. WSJT-X was recording
  the unused mic preamp at +15 dB: noise plus interference bursts every 1.5 s.
* PipeWire's **Line In** port does not work either: its recipe switches off
  `Mic Capture Switch`, and on this codec that control mutes *all* capture,
  not just the mic input. Result: exact digital silence.

What works (verified for receive and transmit): the card profile **Pro Audio**, which stops
PipeWire from touching the mixer, plus the mixer set once by hand -
`PCM Capture Source` = Line and `Mic Capture Switch` on - saved with
`sudo alsactl store`. In WSJT-X select the source
`alsa_input.usb-microHAM_microHAM_CODEC-00.pro-input-0` with input channel
**Left**. See [Recommended setup: Pro Audio](#recommended-setup-pro-audio).

On Debian 13 the stored mixer state is **not** restored when the DK2 is
plugged in after boot, because of a typo in alsa-utils' udev rule. See
[Restore on plug-in is broken on Debian 13](#restore-on-plug-in-is-broken-on-debian-13).

## The layers

```
             WSJT-X  (Qt5 Multimedia: two audio plugins, one merged device list)
               |                                   |
   Qt "pulseaudio" plugin                 Qt "alsa" plugin -> libasound (ALSA library)
   lists: alsa_input.*                    lists what `arecord -L` shows
               |                        +----------+--------------------+
               |          "pulse" / "pipewire" / "default"    hw: / plughw: / sysdefault:
               |          (library plugins that forward       front: / dsnoop:
               v           into the sound server)             (straight to the driver)
   +---------------------------------------------+                    |
   | PipeWire (+ pipewire-pulse, WirePlumber)    |                    |
   | opens the hardware, resamples, mixes, lets  |                    |
   | several apps share one device, picks a      |                    |
   | profile + port per card                     |                    |
   +----------------------+----------------------+                    |
                          v                                           v
          kernel ALSA driver (snd-usb-audio), card "CODEC"
             PCM device 0    - the sample stream apps open
             mixer controls  - PCM Capture Source [Mic|Line], switches, gains
                          |
                  DK2 codec chip  <- Line in (radio RX)  /  Mic in (unused)
```

"ALSA" means two different things:

* **The kernel driver.** Every path ends there, PipeWire included. PipeWire is
  just a client of the kernel driver - normally the only one that opens the
  hardware - and applications talk to PipeWire.
* **The user-space library (libasound).** An application can use it either to
  reach the hardware directly (`hw:`, `plughw:`, `sysdefault:`, `front:`,
  `dsnoop:`) or to reach PipeWire through plugins (`pulse`, `pipewire`,
  `default`). On Debian with PipeWire, ALSA `default` is redirected to PipeWire
  by `/etc/alsa/conf.d/99-pipewire-default.conf`.

`alsa_input.usb-microHAM_microHAM_CODEC-00.analog-stereo` is **not** direct
ALSA. It is a PipeWire node name: `alsa_input` = "a source that wraps an ALSA
capture device", `usb-microHAM_microHAM_CODEC-00` = the card,
`analog-stereo` = the active profile. An application selecting it talks to
PipeWire (via `pipewire-pulse`).

## Device names in WSJT-X

WSJT-X 3.1.0 improved is a Qt5 application. With both Qt5 audio plugins
installed (`libqtaudio_alsa.so`, `libqtmedia_pulse.so` in
`/usr/lib/x86_64-linux-gnu/qt5/plugins/audio/`), Qt5 merges both device lists
into one dropdown:

| Entry                       | Path                                    | Which codec is used |
|-----------------------------|-----------------------------------------|---------------------|
| `alsa_input.usb-microHAM...`| Pulse protocol -> PipeWire -> kernel    | the one named. **Recommended.** |
| `pulse`, `pipewire`, `default` | libasound -> plugin -> PipeWire      | PipeWire's *default* source, or wherever the stream is moved in pavucontrol |
| `plughw:CARD=CODEC,DEV=0`   | libasound -> kernel, format conversion  | the one named; exclusive |
| `hw:CARD=CODEC,DEV=0`       | libasound -> kernel, no conversion      | the one named; app must match the exact hardware format |
| `dsnoop:CARD=CODEC,DEV=0`   | libasound -> kernel, capture shareable between ALSA apps | the one named |
| `alsa_output....monitor`    | PipeWire                                | not an input: captures what is played *to* that output |

Notes:

* `pulse` and `pipewire` are not devices. They mean "the server, its default".
  The codec is chosen in pavucontrol (default input, or by moving the
  application's stream).
* Direct `hw:`/`plughw:` bypasses PipeWire's processing but is exclusive. It
  only opens while PipeWire has the device idle (PipeWire suspends idle nodes
  after about 5 s), and no other program can use the codec at the same time.
  Through PipeWire, WSJT-X and fldigi can listen to the same codec at once.
* Advise users to select the explicit `alsa_input....` name. It does not depend
  on which input happens to be the system default.

## Line, Mic and ports

Line and Mic are not separate devices. The codec has **one** capture stream
(PCM device 0); a selector in the **mixer** decides which jack feeds it. That is
the `PCM Capture Source [Mic|Line]` control. The mixer is a property of the
card, set once for the whole system - not something each application should
change, or they would fight each other and PipeWire. That is why WSJT-X (and
fldigi) have no Line/Mic option.

In PipeWire terms that selection is a **port** (pavucontrol -> Input Devices ->
"Port: Microphone / Line In"). A port is a recipe of mixer settings from
`/usr/share/alsa-card-profile/mixer/paths/*.conf`, applied when the port is
selected. The recipes are written for PC sound cards and assume that a control
named "Mic ..." belongs to the mic jack. `analog-input-linein.conf` contains:

```
[Element Mic]
switch = off
volume = off
```

On the DK2 codec that turns off all capture.

The mixer and the stream are independent. Opening `plughw:` directly in WSJT-X
would **not** have fixed the DK2: PipeWire still applies its Mic recipe to the
mixer when it sets up the card, regardless of who opens the PCM later.

The **Pro Audio** profile has no ports at all: it exposes each PCM device raw,
with channels named aux0, aux1, ... and software volume only, and never
touches the mixer. pavucontrol therefore shows no port selector for it.

### Why Audacity shows Line, Mic, ...

Audacity (configured with `Host=ALSA`) uses PortMixer, which reads the card's
ALSA mixer controls and flips the capture selector itself
(`RecordingSource=...` in `~/.config/audacity/audacity.cfg`). Those entries are
mixer settings, not separate devices. Changing them in Audacity modifies the
same hardware control that PipeWire's ports manage.

## Finding an application in pavucontrol

Qt5 does not pass the application name to PipeWire. WSJT-X therefore shows up
in pavucontrol's **Recording** tab as `QtPulseAudio:<pid>` (stream name
`QtmPulseStream-...`); the number changes on every start. The Playback tab only
lists it while the output is open.

pavucontrol tabs:

* **Configuration** - the card's profile: which PCMs are opened and with which
  channel layout. "Pro Audio" exposes every PCM raw.
* **Input Devices** - port (Line/Mic), input level, and the default input
  (green check button).
* **Recording** - which source each running application reads from.

A profile chosen in the Configuration tab is stored per user in
`~/.local/state/wireplumber/default-profile` and restored from there.

The same information on the command line:

```
pactl list cards              # profiles, "Active Profile:"
pactl list sources            # ports, "Active Port:", volume, base volume
pactl list source-outputs     # which app records from which source
wpctl status                  # overview of the PipeWire graph
```

## DigiKeyer II details

USB IDs: `0403:eeef` (DIGI KEYER II, FTDI) and `074d:3556` (Micronas
"Composite USB-Device" = the codec). ALSA card id `CODEC`, one PCM device.

In PipeWire and pavucontrol the codec is called **"Composite USB-Device"** (the
USB product string of the codec chip); ALSA calls it "microHAM CODEC". Nothing
in pavucontrol says "DK2", so it is easy to change the wrong USB sound card.

Capture altsets (`/proc/asound/cardN/stream0`): S16_LE mono or stereo,
6400-48000 Hz continuous, full-speed USB, adaptive endpoint. PipeWire runs it
at S16LE 2ch 48000 Hz.

### Mixer controls

Capture side (`amixer -c CODEC contents`), as the codec actually behaves:

| Control                   | Range            | Behaviour |
|---------------------------|------------------|-----------|
| `PCM Capture Source`      | Mic / Line       | input selector; radio RX audio is on **Line** |
| `Line Capture Volume`     | 0..12 (0..+12 dB)| Line gain |
| `Line Capture Switch`     | on/off           | **no effect** - capture works with it off |
| `Mic Capture Switch`      | on/off           | **mutes all capture** when off, despite the name |
| `Mic Capture Volume`      | 0..15 (0..+15 dB)| effect on Line capture not verified |

In `alsamixer -c CODEC`:

* F4 = capture view. A red `CAPTURE` under a control means its capture switch
  is **on**; `-------` means off; the space bar toggles it. The working DK2
  state shows red `CAPTURE` under Mic; Line may show `-------`.
* The input selector is **not** in the capture view. `PCM Capture Source` is
  listed in the playback view (F3) and in "All" (F5); change it with the
  cursor up/down keys. Toggling `CAPTURE` flags in F4 does not change the
  input.

Why: alsa-lib's simple mixer only treats a control named exactly
`Capture Source` as the input selector and ties its items to the `CAPTURE`
flags - on a typical sound card, setting `CAPTURE` on Line selects Line.
`PCM Capture Source` is classified as a plain enum (`amixer -c CODEC
scontents` shows `Capabilities: enum`, not `cenum`), and alsamixer lists plain
enums in the playback view.

Measured with Source = Line, Line switch on:

| Mic Capture Switch | Line Capture Volume | Result |
|--------------------|---------------------|--------|
| off                | +12 dB              | exact digital zeros |
| on                 | +12 dB              | audio, heavily clipped (L RMS ~27000) |
| on                 | 0 dB                | clean audio (L RMS ~5700, about -14 dBFS) |

### USB topology vs. behaviour

The codec's USB Audio Class descriptors (`lsusb -v -d 074d:3556`) describe the
capture side as two parallel branches in front of a selector:

```
Mic jack  (terminal 11) -> Feature Unit 2 "Mic Capture"  [mute, volume] --+
                                                                         +--> Selector 7 "PCM Capture Source" --> USB capture
Line jack (terminal 15) -> Feature Unit 3 "Line Capture" [mute, L/R vol] -+
```

A second microphone terminal (13) feeds Feature Unit 6 ("Mic Playback") into
the playback mixer - a monitor path, not relevant for capture.

The kernel names the controls correctly from these descriptors. The chip just
does not behave as described: Feature Unit 2's mute also silences the Line
branch, and Feature Unit 3's mute does nothing. Software that trusts the
names - PipeWire's port recipes - therefore mutes everything when it selects
Line In.

### Channels: main and sub receiver

Per the DK2 documentation, main RX audio is on the left channel and sub RX
audio on the right.

* Measured with the sub receiver not in use: left RMS about 1950, right
  about 3.
* WSJT-X "Mono" gets `(L+R)/2` from PipeWire: measured exactly -6.0 dB
  against the left channel, also with Pro Audio's aux0/aux1 channels.
* With the sub receiver active, "Mono" would mix both receivers into one
  waterfall, and decodes from the sub receiver would be reported with the main
  VFO's frequency. Select **Left** for the main receiver, **Right** for the sub
  receiver.
* Two WSJT-X instances (`wsjtx --rig-name=...`), one on Left and one on Right,
  could decode both; PipeWire lets them share the codec. CAT control for the
  second instance is the open question.

### Setting the level

* The WSJT-X level meter is 20*log10(RMS) of the 16-bit samples: a recording
  measured a median of 36.7 dB on the left channel while WSJT-X showed about
  40 dB (peaks around -42 dBFS). Full scale is about 90 dB, so 80 dB means an
  RMS level only about 10 dB below clipping.
* With the keyer's level potentiometer at zero the meter drops to near 0 dB:
  the codec adds almost no noise of its own. Band noise at 30-40 dB is well
  above that floor and leaves about 50 dB of headroom for strong signals.
* Use one control for the level - the keyer's potentiometers - and leave
  `Line Capture Volume` fixed (+7 dB in the working setup). The radio's
  data/ACC output level is another option.
* For comparison: with the Analog Stereo profile at 0 dB Line gain and the
  potentiometers as found, the level was much hotter - about -14 dBFS RMS,
  peaks near -3 dBFS.

### The original symptom

With the Mic port active, the recording showed a noise floor around -68 dBFS
with 500 Hz harmonic lines, plus ~250 ms bursts (peak near 9.5 kHz) every
1.50 s - interference picked up by the unused mic preamp. The "interrupted"
waterfall came from these bursts.

### Recommended setup: Pro Audio

Verified 2026-10-06 for receive (`pro-input-0`) and transmit
(`pro-output-0`).

1. pavucontrol -> Configuration -> "Composite USB-Device" -> profile
   **Pro Audio**.
2. Set the mixer with `alsamixer -c CODEC`: `PCM Capture Source` = Line (F3
   or F5 view, cursor up/down), red `CAPTURE` under Mic (F4 view). Or with
   amixer:
   ```
   amixer -c CODEC cset name='PCM Capture Source' Line
   amixer -c CODEC cset name='Mic Capture Switch' on
   ```
3. WSJT-X: input `alsa_input.usb-microHAM_microHAM_CODEC-00.pro-input-0`,
   channel Left (Right for the sub receiver); output
   `alsa_output.usb-microHAM_microHAM_CODEC-00.pro-output-0`. Changing the
   profile renames both nodes, so both must be selected again.
4. Set the level with the keyer's potentiometers (see above) and confirm
   signals in the waterfall.
5. Only now: `sudo alsactl store`. Storing earlier risks saving a muted state,
   which every later restore brings back.

Pitfall: Pro Audio never touches the mixer, so it never *fixes* it either.
After switching profiles the mixer keeps whatever the previous profile's
recipe set. After the Line In recipe that is `Mic Capture Switch` off - silence
until it is switched on by hand.

How the mixer state is kept:

* `alsactl store` writes `/var/lib/alsa/asound.state`, one `state.<card id>`
  section per card (here `state.CODEC`).
* The udev rule `90-alsa-restore.rules` (alsa-utils) is meant to run
  `alsactl restore` for a card when it is plugged in. On Debian 13 it restores
  the wrong card - see below.
* On Debian, `alsa-restore.service` also runs `alsactl restore` at boot (all
  cards; this works) and `alsactl store` at shutdown. The state saved at
  shutdown is whatever the mixer is at that moment: with Pro Audio the state
  you set, with Analog Stereo whatever PipeWire's recipe left.

### Restore on plug-in is broken on Debian 13

Replug test 2026-10-06: USB disconnected, rig and keyer power-cycled, USB
reconnected. The codec came up with its power-on defaults - `PCM Capture
Source` = Mic, `Line Capture Volume` 0, `Mic Capture Volume` 0 - and the
waterfall showed only patterned noise, the original symptom. The stored state
in `/var/lib/alsa/asound.state` was correct.

Cause: a label typo in `/usr/lib/udev/rules.d/90-alsa-restore.rules`
(alsa-utils 1.2.14-1). Lines 18 and 22 jump to `LABEL="alsa_restore_std"`,
which does not exist; line 26, where the restore command is, is labelled
`alsa_restore_go` a second time. `udevadm verify` reports it:

```
90-alsa-restore.rules:18 GOTO="alsa_restore_std" has no matching label, ignoring.
90-alsa-restore.rules:22 GOTO="alsa_restore_std" has no matching label, ignoring.
90-alsa-restore.rules:26 style: LABEL="alsa_restore_go" is unused.
```

udev drops both jumps, so every card falls through into the section meant for
laptops with HDA plus a digital microphone. That section imports
`/run/udev/alsa-hda-analog-card` (here `ALSA_CARD_HDA_ANALOG=0`, written at
boot for the onboard Intel card) and replaces the card number with it. A
hot-plugged DK2 therefore runs `alsactl restore 0`, which restores the onboard
card, not the codec. (`udevadm test` shows `restore 3` only because test mode
skips the `cat`.) At boot `alsa-restore.service` restores all cards correctly;
only plugging in after boot is affected.

Known upstream: [ALT Linux bug 56062](https://bugzilla.altlinux.org/56062);
openmamba applied the upstream fix to 1.2.13 in November 2024
([commit](https://src.openmamba.org/rpms/alsa-utils/commit/bb57f4737aee1ab22722507d43d04c58506249ff)).
Debian's 1.2.14-1 still ships the broken rule.

Workaround after plugging in:

```
sudo alsactl restore CODEC
```

Fix: a corrected copy in `/etc/udev/rules.d/` overrides the packaged rule
(same file name). The edit was checked with `udevadm verify` on a copy;
*a replug with the fixed rule is not yet tested*:

```
sudo cp /usr/lib/udev/rules.d/90-alsa-restore.rules /etc/udev/rules.d/
sudo sed -i '26s/alsa_restore_go/alsa_restore_std/' /etc/udev/rules.d/90-alsa-restore.rules
udevadm verify /etc/udev/rules.d/90-alsa-restore.rules
sudo udevadm control --reload
```

Remove the copy once Debian ships a fixed alsa-utils, or it will mask later
changes to the rule.

### Analog Stereo profile: fixing it by hand (temporary)

First set the PipeWire port to Line In, so PipeWire does not reapply the Mic
recipe on its next route change. Selecting Line In turns `Mic Capture Switch`
off, so fix the mixer afterwards:

```
pactl set-source-port alsa_input.usb-microHAM_microHAM_CODEC-00.analog-stereo analog-input-linein
amixer -c CODEC cset name='PCM Capture Source' Line
amixer -c CODEC cset name='Line Capture Switch' on
amixer -c CODEC cset name='Line Capture Volume' 0,0
amixer -c CODEC cset name='Mic Capture Switch' on
```

This lasts until PipeWire applies the recipe again (replug, PipeWire restart,
reboot, port or profile changes in pavucontrol).

### Alternative: WirePlumber rule

Verified 2026-10-06 for receive, transmit and replug: with the rule active,
switching the port between Line In and Microphone and moving the input slider
in pavucontrol left the hardware mixer unchanged, and after unplugging and
reconnecting the DK2 the stored mixer state came back (with the fixed udev
rule, see above).

Keep the Analog Stereo profile, but tell PipeWire to leave this codec's mixer
alone. The order matters - store the mixer state **last**, once it is verified:

1. Create the rule:
   ```
   mkdir -p ~/.config/wireplumber/wireplumber.conf.d
   cat > ~/.config/wireplumber/wireplumber.conf.d/51-microham-dk2.conf <<'EOF'
   monitor.alsa.rules = [
     {
       matches = [ { device.name = "alsa_card.usb-microHAM_microHAM_CODEC-00" } ]
       actions = { update-props = { api.alsa.soft-mixer = true, api.alsa.disable-mixer-path = true } }
     }
   ]
   EOF
   ```
2. Restart WirePlumber **as the desktop user** - not root, not sudo:
   ```
   systemctl --user restart wireplumber
   ```
   `systemctl --user` acts on the calling user's session. Run as root it fails
   with "A dependency job for wireplumber.service failed" (PipeWire's units
   have `ConditionUser=!root`) and leaves the real session untouched.
3. Check that the rule is active: in `pactl list sources` the DK2 source no
   longer has `HW_VOLUME_CTRL` in its `Flags:`.
4. Set the mixer by hand (alsamixer: `PCM Capture Source` = Line on F3/F5, red
   `CAPTURE` under Mic on F4). It is most likely wrong at this point: before
   the rule was active, WirePlumber applied its Line In recipe whenever the
   card was set up, which turns `Mic Capture Switch` off.
5. In WSJT-X select the `...analog-stereo` input and output again (channel
   Left) and confirm signals in the waterfall.
6. Only now: `sudo alsactl store`.

Pitfall seen in testing: running `alsactl store` before step 4 saved the muted
state, and `alsactl restore` then faithfully restored silence. `alsactl
restore` only ever brings back what was stored - if it "does not work", look
at the `state.CODEC` section of `/var/lib/alsa/asound.state` and its
timestamp.

With the rule active:

* The Line In / Microphone port selector in pavucontrol has no effect any
  more. The mixer belongs to alsamixer and `alsactl` alone.
* The pavucontrol input slider is software volume only (base volume 100%).
  Keep it at 100% - above that it is digital gain and can clip - and set the
  level with the keyer's potentiometers.
* The same applies to the **output** (Output Devices tab). WirePlumber
  restores the volume saved earlier for each route, now as software volume:
  here the output came back at about 40% in pavucontrol (linear 0.064,
  -24 dB), and the TX audio was far too low
  until the DK2 output was set to 100%. After that the TX level matched the
  Pro Audio setup. Check both sliders after the restart.
* `api.alsa.soft-mixer` alone is not enough: per `man 7 pipewire-props` the
  hardware mixer is then still used "to mute unused audio paths".
  `api.alsa.disable-mixer-path` stops the port recipes as well.

Compared with Pro Audio this keeps left/right channel names and the
`analog-stereo` node names; otherwise the effect is the same.

## microKEYER III notes

From the saved state on the same machine (the MK3 was not connected during
this investigation):

* The MK3 codec uses **PCM device 0 for playback and PCM device 1 for
  capture**. microHAM's UCM file (`/usr/share/alsa/ucm/MKIII/LineIn`) says so
  (`PlaybackPCM "hw:MKIII,0"`, `CapturePCM "hw:MKIII,1"`), and an older WSJT-X
  configuration used `plughw:CARD=MKIII,DEV=1` / `DEV=0` directly.
* PipeWire's generic profiles only probe PCM device 0. They find a mono
  capture there (`mono-fallback`) but miss the main RX input on device 1.
  Two ways around it: the **Pro Audio** profile, which exposes device 1 as
  `pro-input-1`, or microHAM's UCM file, if it is picked up.
* WirePlumber had the MK3 profile stored as `pro-audio`
  (`~/.local/state/wireplumber/default-profile`). WirePlumber only stores a
  profile on a user-initiated change (`save` flag, see
  `/usr/share/wireplumber/scripts/device/state-profile.lua`): pavucontrol,
  desktop sound settings, `pactl set-card-profile`, `wpctl set-profile`. The
  UCM file cannot produce this entry.
* `/var/lib/alsa/asound.state` contains a `state.MKIII` section - stored
  either by hand or by `alsa-restore.service` at a shutdown while the MK3 was
  connected.
* No stored profile or node name refers to the UCM verb `LineIn`; all are the
  generic ones (`analog-stereo+input:mono-fallback`, `pro-input-0/1`). The
  file is legacy UCM v1 syntax in the directory the alsa-ucm-conf README calls
  obsolete ("new configuration files should be created in the ucm2 tree
  only"). Whether current alsa-lib/PipeWire load it was not verified. Check
  with the MK3 connected: a `LineIn` profile in `pactl list cards` means it is
  used.
* A per-user stored profile overrides system-wide configuration for that user:
  even with a working UCM file, a stored `pro-audio` would likely still be
  restored. When a user's setup behaves unexpectedly, look at
  `~/.local/state/wireplumber/default-profile` and `default-routes`.

## Triage checklist

1. Is the card there? `cat /proc/asound/cards`, `arecord -l`. Note the names:
   the DK2 codec is "Composite USB-Device" in pavucontrol.
2. Which profile and port are active? `pactl list cards` -> `Active Profile:`,
   `pactl list sources` -> `Active Port:`. With microHAM codecs, an active
   "Microphone" port is a red flag.
3. What is the hardware mixer really set to? `amixer -c CODEC contents`, or
   `alsamixer -c CODEC` (F4 = capture view, red `CAPTURE` = capture on;
   `PCM Capture Source` is in the F3/F5 views).
   Works until a replug? Compare with `state.CODEC` in
   `/var/lib/alsa/asound.state`, and run
   `udevadm verify /usr/lib/udev/rules.d/90-alsa-restore.rules`.
4. Is it running, and are there dropouts? `pw-top` shows rate, format and an
   ERR counter (xruns). `/proc/asound/cardN/stream0` shows what the hardware is
   doing.
5. What does the audio look like? Record a test file through PipeWire:
   `pw-record --target <source name> --rate 48000 --channels 2 --format s16 test.wav`
   * exact zeros - something in the mixer path is muted
   * noise, hum or bursts with no signal - wrong input selected
   * distortion or clipping - gain too high
   * gaps with a rising ERR count in `pw-top` - buffering problem
6. In WSJT-X, select the explicit `alsa_input....` source rather than
   `pulse`, `pipewire` or `default`, and the input channel that carries the
   wanted receiver (Left = main RX on the DK2). After a profile change,
   select input *and* output again - the node names change.
