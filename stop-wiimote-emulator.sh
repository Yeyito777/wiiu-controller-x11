#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
set -euo pipefail
LOGDIR="$HOME/Workspace/research/wiimote/logs"
if [ -f "$LOGDIR/wmemulator.pid" ]; then
  sudo kill "$(cat "$LOGDIR/wmemulator.pid")" 2>/dev/null || true
fi
if [ -f "$LOGDIR/custom-bluetoothd.pid" ]; then
  sudo kill "$(cat "$LOGDIR/custom-bluetoothd.pid")" 2>/dev/null || true
fi
sudo pkill -f "$HOME/Workspace/research/wiimote/WiimoteEmulator/bluez-4.101/dist/sbin/bluetoothd" 2>/dev/null || true
sudo pkill -x bluetoothd 2>/dev/null || true
# MediaTek address spoofing is temporary and should reset here.
sudo hciconfig hci0 reset 2>/dev/null || true
sudo systemctl unmask bluetooth.service || true
sleep 1
sudo systemctl start bluetooth.service || true
