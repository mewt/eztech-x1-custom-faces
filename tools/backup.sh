#!/usr/bin/env bash
# Dump the whole 8 MB flash before you touch anything. Do this first.
#
# The dump is your only way back to the factory firmware, and tools/restore.sh
# needs it. It also contains your Wi-Fi passwords and the device's MQTT
# credential in cleartext, so keep it off shared drives and out of git.
#
#   tools/backup.sh [outfile]
set -euo pipefail

PORT="${PORT:-/dev/cu.usbmodem101}"
OUT="${1:-xiaozhi-flash-backup-$(date +%Y%m%d-%H%M%S).bin}"
ESPTOOL="${ESPTOOL:-esptool.py}"

[ -e "$PORT" ] || { echo "no device at $PORT" >&2; exit 1; }

echo "Reading 8 MB from $PORT -> $OUT (a few minutes)"
"$ESPTOOL" --port "$PORT" --chip esp32c3 read-flash 0x0 0x800000 "$OUT"
echo
echo "sha256: $(shasum -a 256 "$OUT" | cut -d' ' -f1)"
echo "Keep this file. restore.sh needs it:  BACKUP=$OUT tools/restore.sh app"
