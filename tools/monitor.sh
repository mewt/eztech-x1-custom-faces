#!/usr/bin/env bash
# Serial log from the device. Ctrl-] to quit.
set -euo pipefail
PORT="${PORT:-/dev/cu.usbmodem101}"
PYSHIM="$(cd "$(dirname "$0")" && pwd)/pyshim"
# ESP-IDF picks its venv from the `python3` on PATH, and Homebrew's
# python3.13/3.14 cannot create one (ensurepip fails). Force Apple's 3.9.
export PATH="$PYSHIM:$PATH"
. "${IDF_PATH_DIR:-$HOME/esp/esp-idf}/export.sh" > /dev/null
cd "$(dirname "$0")/../firmware"
idf.py -p "$PORT" monitor
