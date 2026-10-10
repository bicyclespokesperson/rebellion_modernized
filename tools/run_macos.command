#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export REBELLION_CONFIG="${REBELLION_CONFIG:-$ROOT/work/macos/config.ini}"
exec "$ROOT/build/native/Rebellion.app/Contents/MacOS/Rebellion" "$@"
