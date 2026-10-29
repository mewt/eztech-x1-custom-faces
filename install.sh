#!/usr/bin/env bash
# Drop this board and its patch into a fresh DOIT_AI (xiaozhi v1.8.2) clone.
#
#   ./install.sh                 clone upstream into ./firmware and patch it
#   ./install.sh /path/to/clone  patch an existing clone instead
#
# Safe to re-run: the patch is checked with --check first and skipped if it is
# already applied.
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
FW="${1:-$HERE/firmware}"
UPSTREAM="https://github.com/SmartArduino/DOIT_AI.git"
PATCH="$HERE/patches/xiaozhi-v1.8.2-mz01.patch"

if [ ! -d "$FW/.git" ]; then
    echo "Cloning $UPSTREAM into $FW (this is ~800 MB, it takes a while)"
    git clone --depth 1 "$UPSTREAM" "$FW"
fi

echo "Board      -> $FW/main/boards/mz01-c3-lcd"
mkdir -p "$FW/main/boards"
cp -R "$HERE/board/mz01-c3-lcd" "$FW/main/boards/"

echo "sdkconfig  -> $FW/sdkconfig.mz01"
cp "$HERE/sdkconfig.mz01" "$FW/"

cd "$FW"
if git apply --check --reverse "$PATCH" 2>/dev/null; then
    echo "Patch      -> already applied, skipping"
else
    echo "Patch      -> $(basename "$PATCH")"
    git apply "$PATCH"
fi

echo "Faces      -> rendering $(ls "$HERE"/faces/*.gif | wc -l | tr -d ' ') GIFs to RGB565"
python3 "$HERE/tools/build_faces.py"

cat <<'TXT'

Done. Next:
  tools/build.sh      build (needs ESP-IDF v5.4.2 in ~/esp/esp-idf)
  tools/backup.sh     SAVE A FULL FLASH BACKUP BEFORE YOU FLASH ANYTHING
  tools/flash.sh      write the app to ota_0, leaving NVS and pairing alone
  tools/monitor.sh    serial log
TXT
