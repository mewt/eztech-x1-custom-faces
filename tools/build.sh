#!/usr/bin/env bash
# Build the MZ01-C3-LCD firmware (ESP32-C3). Usage: tools/build.sh [idf.py args...]
set -euo pipefail

PROJECT="$(cd "$(dirname "$0")/../firmware" && pwd)"
IDF_PATH_DIR="${IDF_PATH_DIR:-$HOME/esp/esp-idf}"
PYSHIM="$(cd "$(dirname "$0")" && pwd)/pyshim"

# ESP-IDF picks its venv from the `python3` on PATH, and Homebrew's
# python3.13/3.14 cannot create one (ensurepip fails). Force Apple's 3.9.
export PATH="$PYSHIM:$PATH"
# shellcheck disable=SC1091
. "$IDF_PATH_DIR/export.sh" > /dev/null

cd "$PROJECT"
DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.esp32c3;sdkconfig.mz01"

if [ ! -f sdkconfig ] || ! grep -q "^CONFIG_BOARD_TYPE_MZ01_C3_LCD=y" sdkconfig; then
    rm -f sdkconfig
    idf.py -D SDKCONFIG_DEFAULTS="$DEFAULTS" set-target esp32c3
fi

idf.py -D SDKCONFIG_DEFAULTS="$DEFAULTS" "${@:-build}"
