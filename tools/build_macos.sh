#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
EXE="${REB_GAME_EXE:-$ROOT/orig_game_files/Star Wars - Rebellion/REBEXE.EXE}"
BUILD_DIR="${REB_NATIVE_DIR:-$ROOT/build/native}"
VENV="$ROOT/build/tools-venv"
if [[ ! -f "$EXE" ]]; then
  echo "Set REB_GAME_EXE to your game's REBEXE.EXE." >&2
  exit 1
fi
tools/fetch_deps.sh freetype liberation sdl3
[[ -x "$VENV/bin/python" ]] || uv venv "$VENV"
uv pip install --python "$VENV/bin/python" pefile pillow ninja
"$VENV/bin/python" tools/extract_startup_assets.py --exe "$EXE" --out "$BUILD_DIR/startup-assets.bin"
"$VENV/bin/python" tools/extract_macos_icon.py --exe "$EXE" --out "$BUILD_DIR/Rebellion.icns"
cmake -S . -B "$BUILD_DIR" -G Ninja -DREB_NATIVE=ON \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_MAKE_PROGRAM="$VENV/bin/ninja"
cmake --build "$BUILD_DIR" --target rebellion --parallel
echo "Built $BUILD_DIR/Rebellion.app"
