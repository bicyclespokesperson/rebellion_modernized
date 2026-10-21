#!/usr/bin/env bash
# Configure build/native (or $REB_NATIVE_DIR) with create_build.sh if needed, then compile the game.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REB_NATIVE_DIR:-$ROOT/build/native}"
[[ -f "$BUILD_DIR/CMakeCache.txt" ]] || "$ROOT/create_build.sh"
cmake --build "$BUILD_DIR" --target rebellion
