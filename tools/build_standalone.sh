#!/usr/bin/env bash
# Build the standalone rebellion.exe (build/standalone/rebellion.exe) of this checkout in the dev container
# (docs/phase2_plan.md, M6). Stages, like build_hybrid.sh --own-data: (1) container: objects + the list of original
# globals they reference; (2) host: tools/gen_data_image.py --standalone (reads the original EXE) writes the data image;
# (3) container: link. The link fails loudly when symbols are unresolved; tools/standalone_link_report.py then groups
# them by category (build/standalone/gen/link_report.txt).
#   REB_ALLOW_UNRESOLVED=ON: link with /FORCE:UNRESOLVED (the unresolved calls crash when reached) for bring-up.
set -euo pipefail
source "$(dirname "$0")/paths.sh"
out="$ROOT/build/standalone"
rm -f "$out/rebellion.exe"
mkdir -p "$out/gen"
container() {
  docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:$ROOT" -w "$ROOT" rebellion-dev bash -c "$1"
}
configure="cmake -S . -B build/standalone -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-win32-clangcl.cmake -DCMAKE_BUILD_TYPE=Release -DREB_STANDALONE=ON -DREB_ALLOW_UNRESOLVED=${REB_ALLOW_UNRESOLVED:-OFF}"
if ! container "$configure -DREB_DATA_IMAGE=OFF >/dev/null && cmake --build build/standalone --target reb_standalone_refs reb_import_libs > build/standalone/gen/build.log 2>&1"; then
  tail -20 "$out/gen/build.log"
  exit 1
fi
"$ROOT/tools/gen_data_image.py" --standalone --exe "$MAIN/orig_game_files/Star Wars - Rebellion/REBEXE.EXE" \
  --symbols "$ROOT/symbols" --refs "$out/gen/data_refs.json" \
  --out-asm "$out/gen/data_image.s" --out-report "$out/gen/data_image_report.json"
if ! container "$configure -DREB_DATA_IMAGE=ON >/dev/null && cmake --build build/standalone > build/standalone/gen/build.log 2>&1"; then
  tail -5 "$out/gen/build.log"
  container "python3 tools/standalone_link_report.py --log build/standalone/gen/build.log --out build/standalone/gen/link_report.txt" || true
  exit 1
fi
echo "built $out/rebellion.exe"
