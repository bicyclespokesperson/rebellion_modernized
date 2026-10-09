#!/usr/bin/env bash
# Compare the PE resource reader with pefile on every EXE/DLL of the installation: for each resource the type, name, language,
# size and a hash of the data must be identical. Needs the native build (run_tests.sh native) and `uv`.
#   compare_pefile.sh [install_dir]
set -uo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
source "$root/tools/paths.sh"
install="${1:-$MAIN/orig_game_files/Star Wars - Rebellion}"
dump="$root/build/minwin_impl/native/res_dump"
ref="$root/src/port/minwin_impl/res/tests/pe_dump_ref.py"
fail=0
for f in "$install"/*.DLL "$install"/*.EXE; do
  [[ -f "$f" ]] || continue
  mine=$("$dump" "$f" 2>&1)
  theirs=$("$ref" "$f" 2>&1)
  n=$(printf '%s\n' "$theirs" | grep -c .)
  if [[ "$mine" == "$theirs" ]]; then
    echo "OK   $(basename "$f"): $n resources identical"
  else
    echo "DIFF $(basename "$f")"
    diff <(printf '%s\n' "$mine") <(printf '%s\n' "$theirs") | head -5
    fail=1
  fi
done
exit $fail
