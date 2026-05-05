# Wii U X11 Controller

A small Linux/X11 input-master window for controlling a real Wii U through a patched [`rnconrad/WiimoteEmulator`](https://github.com/rnconrad/WiimoteEmulator) socket backend.

This was made for a local Super Mario 3D World setup: the PC impersonates a Wii Remote over Bluetooth, while this project provides a real X11 window that tracks `KeyPress`/`KeyRelease` state so multiple physical keys can be held at once.

## What is included

- `xwiictl.c` — X11 controller window; tracks real key down/up state and sends events to the emulator Unix datagram socket.
- `wiictl.c` — older alternate-screen terminal controller; useful as a fallback, but true key-release behavior is terminal-limited.
- `start-wiimote-emulator.sh` — starts patched BlueZ + WiimoteEmulator in socket mode.
- `restart-socket-controller.sh` — reconnects the emulator to a known Wii U Bluetooth address and recreates the socket.
- `stop-wiimote-emulator.sh` — stops the custom Bluetooth stack and restores normal Bluetooth.
- `wiiu-controller` — one-command launcher for the local setup.

## Not included / license note

This repository is MIT-licensed for the code in this repo. It does **not** vendor `WiimoteEmulator` or BlueZ. Those are third-party projects with their own licenses. The scripts expect a patched `WiimoteEmulator` checkout at:

```text
./WiimoteEmulator
```

The current local setup uses a patched WiimoteEmulator to support a MediaTek Bluetooth adapter and modern Linux build quirks. The local patch used during development is included as `patches/wiimoteemulator-local.patch`; it is provided for convenience and remains subject to the upstream WiimoteEmulator/BlueZ licensing context.

## Build

Dependencies:

- Linux/X11
- C compiler
- `pkg-config`
- Xlib development headers (`libx11-dev` on Debian/Ubuntu, `libx11` on Arch)
- `gh` is only needed for publishing, not runtime

Build:

```bash
make
```

Install into `~/.local/bin`:

```bash
make install
```

## Run

For the local paired setup:

```bash
./wiiu-controller
```

or, after `make install`:

```bash
wiiu-controller
```

To stop the fake Wiimote setup and restore normal Bluetooth:

```bash
./stop-wiimote-emulator.sh
```

## Current Super Mario 3D World mapping

Focus the **WiiCtl X11 Input Master** window before playing.

- `a` = Up
- `s` = Left
- `d` = Down
- `w` = Right
- `space` = Jump / Wii Remote 2
- `Shift` or `y` = Run / dash / Wii Remote 1 in Wiimote mode
- `j` = Ground pound / crouch / Wii Remote B
- `2` or `k` = Wii Remote A
- `r` = release all held buttons
- `q` / `Esc` = close the controller window
- `m` = toggle Classic Controller extension mode

The X11 controller releases all held inputs automatically when the window loses focus.

## Safety notes

This setup temporarily takes over the PC Bluetooth adapter with a custom BlueZ daemon. Use `stop-wiimote-emulator.sh` when done to restore normal Bluetooth.

Plain USB-A-to-USB-A between a Wii U and a PC is not used or recommended.
