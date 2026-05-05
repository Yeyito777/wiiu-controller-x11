#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
MODE="${1:-socket}"
cd "$HOME/Workspace/research/wiimote/WiimoteEmulator"
LOGDIR="$HOME/Workspace/research/wiimote/logs"
mkdir -p "$LOGDIR"
BTD_LOG="$LOGDIR/custom-bluetoothd.log"
WM_LOG="$LOGDIR/wmemulator.log"
SOCK="$LOGDIR/wmemu.sock"
rm -f "$BTD_LOG" "$WM_LOG" "$SOCK"

echo "Stopping and runtime-masking system bluetooth..."
sudo systemctl mask --runtime bluetooth.service || true
sudo systemctl stop bluetooth.service || true
sudo pkill -x bluetoothd || true
sleep 2

echo "Disabling SSP mode and resetting hci0..."
sudo ./bluez-4.101/dist/sbin/hciconfig hci0 sspmode disabled || true
sudo ./bluez-4.101/dist/sbin/hciconfig hci0 reset || true
sleep 1

echo "Starting custom BlueZ; log: $BTD_LOG"
sudo env LD_LIBRARY_PATH="$PWD/bluez-4.101/dist/lib" ./bluez-4.101/dist/sbin/bluetoothd -n -d >"$BTD_LOG" 2>&1 &
echo $! > "$LOGDIR/custom-bluetoothd.pid"
sleep 3

echo "Starting Wiimote emulator; log: $WM_LOG"
echo "Press Wii U SYNC if it is not already connected."
if [ "$MODE" = "gui" ]; then
  sudo env DISPLAY="${DISPLAY:-:0}" XDG_RUNTIME_DIR="/run/user/$(id -u)" XAUTHORITY="${XAUTHORITY:-$HOME/.Xauthority}" LD_LIBRARY_PATH="$PWD/bluez-4.101/dist/lib" ./wmemulator pair gui >"$WM_LOG" 2>&1 &
else
  sudo env LD_LIBRARY_PATH="$PWD/bluez-4.101/dist/lib" ./wmemulator pair unix "$SOCK" >"$WM_LOG" 2>&1 &
fi
echo $! > "$LOGDIR/wmemulator.pid"
if [ "$MODE" != "gui" ]; then
  for _ in $(seq 1 20); do
    [ -S "$SOCK" ] && break
    sleep 0.1
  done
  sudo chmod 666 "$SOCK" 2>/dev/null || true
fi

echo "Started. PIDs: bluetoothd=$(cat "$LOGDIR/custom-bluetoothd.pid"), wmemulator=$(cat "$LOGDIR/wmemulator.pid")"
echo "Logs: $BTD_LOG and $WM_LOG"
if [ "$MODE" = "gui" ]; then
  echo "GUI controls are in the tiny SDL window."
else
  echo "Socket input: $SOCK"
  echo "Run: ~/Workspace/research/wiimote/wiictl $SOCK"
fi
