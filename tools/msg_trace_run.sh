#!/usr/bin/env bash
# Message-sequence comparison driver: the scenarios on the real-user32 standalone with the trace shim (build/ref_trace) and on
# minwin_impl (build/impl), both with REB_MSG_TRACE, then tools/msg_trace_compare.py. Real-time clocks, 8-bit Xvfb, Liberation fonts
# (the same conditions as tools/pixel_run.sh).
#   build the reference once: REB_MINWIN=ON REB_STANDALONE_DIR=build/ref_trace REB_CMAKE_ARGS=-DREB_MSG_TRACE_SHIM=ON tools/build_standalone.sh
#   tools/msg_trace_run.sh [-o TAG] [-R] [--no-run] [scenario...]   (default: smoke ui_tour load_game)
# Output: work/run/mt_<TAG>/{ref,impl}_<scenario>/msgtrace.log; the comparison summary on stdout. -R reruns the reference.
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
[[ ${#scenarios[@]} -eq 0 ]] && scenarios=(smoke ui_tour load_game)
base="$ROOT/work/run/mt_$tag"
refs="$ROOT/work/run/mt_ref"
mkdir -p "$base" "$refs"
export REB_XVFB_DEPTH=8 REB_WINE_FONTS=liberation REB_MSG_TRACE='Z:\out\msgtrace.log'
if [[ $run == 1 ]]; then
  for s in "${scenarios[@]}"; do
    if [[ $rerun_ref == 1 || ! -s "$refs/ref_$s/msgtrace.log" ]]; then
      REB_STANDALONE_DIR=build/ref_trace "$ROOT/tools/run_game.sh" --standalone --out "$refs/ref_$s" "$ROOT/tests/scenarios/$s.txt" > "$refs/ref_$s.log" 2>&1 &
    fi
    REB_STANDALONE_DIR=build/impl "$ROOT/tools/run_game.sh" --standalone --out "$base/impl_$s" "$ROOT/tests/scenarios/$s.txt" > "$base/impl_$s.log" 2>&1 &
    wait
  done
fi
for s in "${scenarios[@]}"; do
  echo "## $s"
  "$ROOT/tools/msg_trace_compare.py" "$refs/ref_$s/msgtrace.log" "$base/impl_$s/msgtrace.log" | tail -n 25
done
