#!/usr/bin/env bash
# Put the device back on its factory firmware.
#
#   tools/restore.sh app     restore only the factory app into ota_0 (keeps the
#                            current NVS, so Wi-Fi and pairing stay as they are)
#   tools/restore.sh full    rewrite the whole 8 MB flash from the backup
#                            (also rewinds NVS to whenever you took it)
set -euo pipefail

MODE="${1:-app}"
PORT="${PORT:-/dev/cu.usbmodem101}"
BACKUP="${BACKUP:?set BACKUP=/path/to/your/flash-backup.bin (make one with tools/backup.sh)}"
ESPTOOL="${ESPTOOL:-$HOME/.local/bin/esptool.py}"
WORK="$(mktemp -d)"

[ -f "$BACKUP" ] || { echo "backup not found: $BACKUP" >&2; exit 1; }
[ -e "$PORT" ]   || { echo "no device at $PORT" >&2; exit 1; }

case "$MODE" in
  app)
    # ota_0 lives at 0x100000 and is 0x380000 long in the factory layout.
    dd if="$BACKUP" of="$WORK/factory_ota0.bin" bs=4096 skip=256 count=896 status=none
    echo "Restoring the factory app into ota_0 on $PORT"
    "$ESPTOOL" --port "$PORT" --chip esp32c3 write-flash 0x100000 "$WORK/factory_ota0.bin"
    ;;
  full)
    echo "Rewriting the ENTIRE 8 MB flash on $PORT from $BACKUP"
    "$ESPTOOL" --port "$PORT" --chip esp32c3 write-flash 0x0 "$BACKUP"
    ;;
  *)
    echo "usage: $0 [app|full]" >&2; exit 1 ;;
esac

rm -rf "$WORK"
echo "Done."
