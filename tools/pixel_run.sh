#!/usr/bin/env bash
# Pixel comparison driver: run the scenarios on the real-Win32 standalone (build/standalone) and on minwin_impl (build/impl),
# both deterministic, 8-bit Xvfb, Liberation fonts, then tools/pixel_compare.py per scenario (it also compares in palette-index space:
# the impl run dumps its frames with REB_MINWIN_DUMP).
#   tools/pixel_run.sh [-o TAG] [-R] [--no-run] [scenario...]   (names without .txt; default: smoke ui_tour load_game new_game_rebel
#                                                               space_battle_autoresolve)
#   -R: run the reference again (otherwise work/run/px_ref/ref_<scenario> is reused when present: it does not depend on the impl)
# Output: work/run/px_<TAG>/impl_<scenario>/ and cmp_<scenario>/ (diff images: reference | impl | differences); the table on stdout.
set -uo pipefail
source "$(dirname "$0")/paths.sh"
tag=cur; run=1; rerun_ref=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    -o) tag="$2"; shift 2 ;;
    -R) rerun_ref=1; shift ;;
    --no-run) run=0; shift ;;
    *) break ;;
  esac
done
scenarios=("$@")
[[ ${#scenarios[@]} -eq 0 ]] && scenarios=(smoke ui_tour load_game new_game_rebel space_battle_autoresolve)
base="$ROOT/work/run/px_$tag"
clock="${REB_PIXEL_CLOCK:-real}"
# real: wall-clock time in both builds (animations differ by their phase); virtual: REB_DETERMINISTIC=1, the clock advances per call
# (game state then depends on loop speed); frozen: virtual, and the clock stands still from the moment a game exists (what a
# screenshot of a loaded game needs to be reproducible: the animation phase is the same in both builds).
refs="$ROOT/work/run/px_ref_$clock"
mkdir -p "$base" "$refs"
case "$clock" in
  virtual) export REB_DETERMINISTIC=1 REB_VIRTUAL_SOUND=1 ;;
  frozen) export REB_DETERMINISTIC=1 REB_VIRTUAL_SOUND=1 REB_NO_TIMERS=1 REB_QPC_FAST_UNTIL_TICK=1 REB_QPC_START_KEY=145 ;;
esac
export REB_XVFB_DEPTH="${REB_XVFB_DEPTH:-8}" REB_WINE_FONTS=liberation REB_MINWIN_DUMP=Z:/out
if [[ $run == 1 ]]; then
  for s in "${scenarios[@]}"; do   # one scenario at a time: many concurrent games slow the movies enough to desynchronise the scripted input
    if [[ $rerun_ref == 1 || ! -d "$refs/ref_$s" ]]; then
      "$ROOT/tools/run_game.sh" --standalone --out "$refs/ref_$s" "$ROOT/tests/scenarios/$s.txt" > "$refs/ref_$s.log" 2>&1 &
    fi
    REB_STANDALONE_DIR=build/impl "$ROOT/tools/run_game.sh" --standalone --out "$base/impl_$s" "$ROOT/tests/scenarios/$s.txt" > "$base/impl_$s.log" 2>&1 &
    wait
  done
fi
for s in "${scenarios[@]}"; do
  echo "## $s"
  "$ROOT/tools/pixel_compare.py" "$refs/ref_$s" "$base/impl_$s" --out "$base/cmp_$s"
done
