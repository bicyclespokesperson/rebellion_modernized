#!/usr/bin/env bash
# Plays movies through the real SMACKW32.DLL under Wine (reference) and through minwin_impl/smack (i686 under Wine and native x86-64)
# and diffs, per frame, the decoded picture, palette, changed rectangles and audio.
#   run_wine_smack.sh [--ref-only] [maxFrames] [movie-number...]     movie numbers are MDATA.nnn, default 000 101 202
# Output: build/minwin_impl/smack/{real,ours32,ours64}.txt
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
source "$root/tools/paths.sh"
refOnly=0
if [[ "${1:-}" == --ref-only ]]; then refOnly=1; shift; fi
frames="${1:-0}"
[[ $# -gt 0 ]] && shift
movies=("$@")
[[ ${#movies[@]} -eq 0 ]] && movies=(000 101 202)
install="$MAIN/orig_game_files/Star Wars - Rebellion"
out="$root/build/minwin_impl/smack"
mkdir -p "$out"
targets="smack_dump_real"
[[ $refOnly == 0 ]] && targets="smack_dump_real smack_dump_ours"
"$root/src/port/minwin_impl/run_tests.sh" i686 $targets >/dev/null
[[ $refOnly == 0 ]] && "$root/src/port/minwin_impl/run_tests.sh" native smack_dump_ours >/dev/null
bin="$root/build/minwin_impl/i686"
files=""
for m in "${movies[@]}"; do files="$files 'Z:\\game\\MDATA\\MDATA.$m'"; done
cmd="cd /game && xvfb-run -a wine $bin/smack_dump_real.exe --dll 'Z:\\game\\SMACKW32.DLL' $out/real.txt $frames $files >/dev/null 2>$out/real.err"
[[ $refOnly == 0 ]] && cmd="$cmd; xvfb-run -a wine $bin/smack_dump_ours.exe $out/ours32.txt $frames $files >/dev/null 2>$out/ours32.err"
docker run --rm -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" -v "$install:/game:ro" -v "$root:$root" rebellion-dev bash -c "$cmd; chown -R \$HOST_UID:\$HOST_GID $out"
if [[ $refOnly == 0 ]]; then
  nativeFiles=()
  for m in "${movies[@]}"; do nativeFiles+=("$install/MDATA/MDATA.$m"); done
  "$root/build/minwin_impl/native/smack_dump_ours" "$out/ours64.txt" "$frames" "${nativeFiles[@]}"
  python3 "$root/src/port/minwin_impl/smack/tests/compare_smack.py" "$out"
fi
