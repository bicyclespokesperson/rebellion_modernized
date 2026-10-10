#!/usr/bin/env bash
# Runs the text scenarios under Wine with the real gdi32 and writes build/minwin_impl/text/real.{lines,pixels}.txt.
# With "ours" as first argument the minwin_impl/text build (i686 under Wine) and the native build are run as well and diffed.
#   run_wine_text.sh [ours] [font-filter]
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../../.." && pwd)"
source "$root/tools/paths.sh"
mode=""
if [[ "${1:-}" == ours ]]; then mode=ours; shift; fi
filter="${1:-}"
install="$MAIN/orig_game_files/Star Wars - Rebellion"
out="$root/build/minwin_impl/text"
mkdir -p "$out"
[[ -f "$root/build/minwin_impl/text_samples.txt" ]] || "$root/src/port/minwin_impl/text/tests/gen_samples.py" "$install" "$root/build/minwin_impl/text_samples.txt"
targets="text_dump_real"
[[ $mode == ours ]] && targets="text_dump_real text_dump_ours"
"$root/src/port/minwin_impl/run_tests.sh" i686 $targets >/dev/null
[[ $mode == ours ]] && "$root/src/port/minwin_impl/run_tests.sh" native text_dump_ours >/dev/null
bin="$root/build/minwin_impl/i686"
# Wine has no Arial here: map "Arial" to the metric-compatible Liberation Sans, the font minwin_impl/text falls back to as well.
setup="wine reg add 'HKCU\\Software\\Wine\\Fonts\\Replacements' /v Arial /d 'Liberation Sans' /f >/dev/null 2>&1; wineserver -w"
cmd="$setup; export MW_FONT_DIR='Z:\\usr\\share\\fonts\\truetype\\liberation'; cd $out; xvfb-run -a wine $bin/text_dump_real.exe $root/build/minwin_impl/text_samples.txt real $filter >/dev/null 2>&1"
[[ $mode == ours ]] && cmd="$cmd; xvfb-run -a wine $bin/text_dump_ours.exe $root/build/minwin_impl/text_samples.txt ours32 $filter >/dev/null 2>&1"
docker run --rm -v /usr/share/fonts/truetype/liberation:/usr/share/fonts/truetype/liberation:ro -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" -v "$root:$root" rebellion-dev bash -c "$cmd; chown -R \$HOST_UID:\$HOST_GID $out"
if [[ $mode == ours ]]; then
  (cd "$out" && "$root/build/minwin_impl/native/text_dump_ours" "$root/build/minwin_impl/text_samples.txt" ours64 $filter >/dev/null)
  python3 "$root/src/port/minwin_impl/text/tests/compare_text.py" "$out" ours32 ours64
fi
