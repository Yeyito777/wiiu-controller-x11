#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
BASE="$HOME/Workspace/research/wiimote"
EMU="$BASE/WiimoteEmulator"
LOGDIR="$BASE/logs"
SOCK="$LOGDIR/wmemu.sock"
WIIU_ADDR="${1:-CC:FB:65:AB:45:5A}"
mkdir -p "$LOGDIR"
cd "$EMU"

if [ -f "$LOGDIR/wmemulator.pid" ]; then
  sudo kill "$(cat "$LOGDIR/wmemulator.pid")" 2>/dev/null || true
fi
sudo pkill -f "$EMU/wmemulator" 2>/dev/null || true
rm -f "$SOCK" "$LOGDIR/wmemulator.log"

# Keep existing custom BlueZ if it is already running; otherwise start the whole stack.
if ! pgrep -f "bluez-4.101/dist/sbin/bluetoothd" >/dev/null; then
  exec "$BASE/start-wiimote-emulator.sh" socket
fi

sudo env LD_LIBRARY_PATH="$EMU/bluez-4.101/dist/lib" ./wmemulator "$WIIU_ADDR" unix "$SOCK" >"$LOGDIR/wmemulator.log" 2>&1 &
echo $! > "$LOGDIR/wmemulator.pid"

# The socket is created by a root process; let the user TUI send datagrams to it.
for _ in $(seq 1 20); do
  [ -S "$SOCK" ] && break
  sleep 0.1
done
sudo chmod 666 "$SOCK" 2>/dev/null || true

echo "Started socket-mode emulator PID $(cat "$LOGDIR/wmemulator.pid")"
echo "Target Wii U: $WIIU_ADDR"
echo "Socket: $SOCK"
echo "Run TUI: $BASE/wiictl $SOCK"
