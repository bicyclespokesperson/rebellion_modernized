#!/usr/bin/env bash
# Build the hybrid proxy DLL (build/hybrid/smackw32.dll) of this checkout in the dev container.
#   --own-data (or REB_OWN_DATA=1): the DLL owns the game's globals (docs/phase2_plan.md, M5). The data image is
#   generated from the original EXE on the host, between two container stages.
set -euo pipefail
source "$(dirname "$0")/paths.sh"
own_data="${REB_OWN_DATA:-0}"
[[ "${1:-}" == "--own-data" ]] && own_data=1
own_vtables="${REB_OWN_VTABLES:-0}"  # experiment: keep our vtable copies (see docs/phase2_plan.md, M5)
cmake_vtables=OFF
[[ "$own_vtables" == 1 ]] && cmake_vtables=ON
# A failed link must not leave the previous DLL behind for difftest/run_game to pick up silently.
rm -f "$ROOT/build/hybrid/smackw32.dll"
if [[ "$own_data" == 1 ]]; then
  cmake_option=ON
else
  cmake_option=OFF
fi
container() {
  docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:$ROOT" -w "$ROOT" rebellion-dev bash -c "$1"
}
configure="cmake -S . -B build/hybrid -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-win32-clangcl.cmake -DCMAKE_BUILD_TYPE=Release -DREB_OWN_DATA=$cmake_option -DREB_OWN_VTABLES=$cmake_vtables >/dev/null"
if [[ "$own_data" == 1 ]]; then
  container "$configure && cmake --build build/hybrid --target reb_link_symbols"
  "$ROOT/tools/gen_data_image.py" --exe "$MAIN/orig_game_files/Star Wars - Rebellion/REBEXE.EXE" \
    --symbols "$ROOT/symbols" --refs "$ROOT/build/hybrid/gen/data_refs.json" \
    --out-asm "$ROOT/build/hybrid/gen/data_image.s" --out-report "$ROOT/build/hybrid/gen/data_image_report.json"
fi
container "$configure && cmake --build build/hybrid"
