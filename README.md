# mhuxd MicroHam Keyer Device Router
(C) 2012-2026 Matthias Moeller, DJ5QV


## NOTES: On this is the V0.90beta branch

- work in progress
- Web UI migrated to a JS/Svelte based version. It can already be accessed on http://localhost:5052/svelte/index.html#home/summary. 
  Old UI is still available and default, but not maintained anymore.

- Internal Clearsilver/HDF based configuration management migrated to JSON. Clearsilver still included for backward compatibility with
  the old config file. If mhuxd doesn't find a json format config file, it will attempt to load the hdf config and migrate it to json.
  (mhuxd-state.hdf -> mhuxd-state.json)
- Rest-API implemented, used by new JS WebUI to change settings or receive events via SSE.
- Can connect to rigctld to query the attached rig and update mode (CW, VOICE, etc.) and frequency information in the keyer. The keyer
  could run those queries itself, but tends to interfere with application CAT requests. This can be configured on the keyer/radio page.
  If this is used, CAT queries by keyer can and should be disabled ("Use Decoder if Connected:  off")
- TCP connector supports IPv6 now.

Additional pre-requesits to compile, needed for json support and compiling Svelte code into plain JS:
- libjansson-dev  
- npm  (package 'npm' on Debian)
- nodejs (package 'nodejs' on Debian)


---

## ABOUT

**Homepage:** [http://mhuxd.dj5qv.de](http://mhuxd.dj5qv.de)

Mhuxd is a device router for microHam keyers. Currently it runs on the Linux operating system only. Supported microHam keyers are:

- micro KEYER
- micro KEYER II
- micro KEYER III
- DIGI KEYER
- DIGI KEYER II
- MK2R & MK2R+
- CW KEYER

Starting with version 0.50 mhuxd provides a web interface. It allows to perform keyer configuration changes including Winkey configuration. Once mhuxd is running the web interfaces can be accessed at:

[http://localhost:5052](http://localhost:5052)

This can be changed by the `-w` option.

Similar to the microHam device router mhuxd can create virtual serial ports. This allows ham radio applications like CQRLOG or Fldigi to send rig commands or utilize the K1EL Winkey chip. These ports can be created using the web interface.

## PREREQUISITES

- Linux Operating system
- `cuse` kernel module
- `ftdi_sio` kernel module
- libfuse
- libev
- libudev

Mhuxd doesn't need to run as root. However it needs read/write access to `/dev/cuse` which is typically owned by root.

## INSTALLATION

### Ubuntu and related distributions (Debian, Mint)

Binary packages are available. Refer to [mhuxd.dj5qv.de](https://mhuxd.dj5qv.de/) for details.

### Compile from source code

To compile mhuxd you'll need the development packages for libudev, libfuse, libev and pkg-config.
(For Debian & Ubuntu that would be `libev-dev`, `libfuse-dev`, `libudev-dev` and `pkg-config`).

```bash
./configure --prefix=/usr/local/mhuxd
make
make install
```

This would install everything into `/usr/local/mhuxd`.

If you cloned the source code from GITHUB then you also need the packages `autoconf` and `automake` and run `./autogen.sh` first.

## ACKNOWLEDGEMENTS

- **microHam** - For providing the protocol specs and support
- **Marc Lehmann and Emanuele Giaquinta** - For libev
- **Brandon Long** - For the ClearSilver template system
- **Igor Sysoev, Joyent Inc. et al.** - For http\_parser
- **Google Inc., Filipe Almeida** - For streamhtmlparser
