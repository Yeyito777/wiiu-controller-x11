# Third-party notices

This repository is distributed under **GPL-2.0-or-later**.

## WiimoteEmulator

`WiimoteEmulator/` is a vendored, locally patched copy of:

- <https://github.com/rnconrad/WiimoteEmulator>

The upstream checkout used during development did not include a top-level license file. However, it includes BlueZ-derived code such as `bdaddr.c` with GPL-2.0-or-later notices, and it builds against/downloads BlueZ 4.101, which includes GPL/LGPL components.

Local modifications are kept in the vendored source and also summarized in `patches/wiimoteemulator-local.patch`. They include:

- MediaTek Bluetooth public address override support using vendor opcode `0xfc1a`.
- Keeping the MediaTek runtime address override live instead of resetting it away immediately.
- Build compatibility fixes for modern Linux/SDL2.
- A small `graceful_disconnect` compatibility shim.
- BlueZ plugin callback signature adjustment for the bundled BlueZ 4.101 headers.

## BlueZ

`WiimoteEmulator/build-custom.sh` downloads and builds BlueZ 4.101 at runtime/build time. Generated/downloaded `WiimoteEmulator/bluez-4.101/` is intentionally ignored by git.

BlueZ 4.101 includes GPL-2.0 and LGPL-2.1 components; see the BlueZ `COPYING` and `COPYING.LIB` files after running the build script.
