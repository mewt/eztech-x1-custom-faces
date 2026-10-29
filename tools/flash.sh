#!/usr/bin/env bash
# Flash ONLY the application into ota_0 on the MZ01-C3-LCD.
#
# The built partition table (partitions/v1/8m.csv) is byte-identical to the
# factory one, so the bootloader, the partition table and NVS are left alone:
# Wi-Fi credentials and the device's identity on xiaozhi.me survive.
#
#   tools/flash.sh              # flash build/xiaozhi.bin to 0x100000
#   PORT=/dev/cu.usbmodemXXX tools/flash.sh
set -euo pipefail

PORT="${PORT:-/dev/cu.usbmodem101}"
APP="$(cd "$(dirname "$0")/.." && pwd)/firmware/build/xiaozhi.bin"
ESPTOOL="${ESPTOOL:-$HOME/.local/bin/esptool.py}"

[ -f "$APP" ] || { echo "no app image at $APP - run tools/build.sh first" >&2; exit 1; }
[ -e "$PORT" ] || { echo "no device at $PORT" >&2; exit 1; }

echo "Device : $PORT"
echo "App    : $APP ($(du -h "$APP" | cut -f1))"
echo "Target : ota_0 @ 0x100000   (bootloader / partition table / NVS untouched)"
echo

"$ESPTOOL" --port "$PORT" --chip esp32c3 write-flash 0x100000 "$APP"

echo
echo "Done. Watch it boot with:  tools/monitor.sh"
