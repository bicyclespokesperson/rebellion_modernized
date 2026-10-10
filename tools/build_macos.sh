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
uv pip install --python "$VENV/bin/python" pefile capstone unicorn libclang==18.1.1 ninja
cmake -S . -B "$BUILD_DIR" -G Ninja -DREB_NATIVE=ON -DREB_NATIVE_GEN=OFF \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_MAKE_PROGRAM="$VENV/bin/ninja"
cmake --build "$BUILD_DIR" --target reb_native_objects --parallel
"$VENV/bin/python" tools/gen_native_data.py --build "$BUILD_DIR" --exe "$EXE" --symbols symbols --out "$BUILD_DIR/gen"
"$VENV/bin/python" tools/gen_native_init.py --build "$BUILD_DIR" --exe "$EXE" --symbols symbols --out "$BUILD_DIR/gen"
cmake -S . -B "$BUILD_DIR" -DREB_NATIVE_GEN=ON
cmake --build "$BUILD_DIR" --target rebellion --parallel
echo "Built $BUILD_DIR/Rebellion.app"
