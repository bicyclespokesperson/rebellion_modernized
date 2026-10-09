#!/usr/bin/env bash
# Long deterministic fidelity comparison (slow, ~15 min per scenario; not part of integrate.sh): the same UI-driven scenario
# on the original logic (hybrid with REB_DISABLE_HOOKS=all), the hybrid with all hooks and the standalone exe, twice each, then
# the saves are compared byte for byte and the screenshots pixel for pixel, within and across configurations.
#
#   tools/long_compare.sh [-s alliance|battle] [-t TICK] [-n RUNS] [-c "orig hybrid standalone"] [--no-build] [--noise BYTES]
#
# Scenarios (tests/scenarios/long_<name>.txt):
#   alliance  Alliance game "Rebels" (tick 95,075) with the Imperial AI playing, run to tick 98,575 (default; ~3,500 ticks)
#   battle    Empire game "JAS1" (tick 150,326) with an auto-resolved space battle (~tick 152,576), run to tick 154,500
# How the run is made reproducible (shims in src/hybrid/determinism.cpp and src/standalone/determinism.cpp):
#  - the game's virtual clock stands still from the moment a game exists until the script presses Scroll Lock after it has
#    set the speed (REB_QPC_START_KEY), so idle frames before the start do not move game time by a wall-clock amount;
#  - then the virtual QueryPerformanceCounter advances 1 ms per call (REB_QPC_FAST_UNTIL_TICK), so ticks are cheap, and
#    timeGetTime/GetTickCount read that counter (REB_CLOCK_FROM_QPC) instead of counting calls (the droid advisor times its
#    cues with them and draws from the random generator the AI breaks ties with); multimedia timers never fire (REB_NO_TIMERS);
#  - when the game's tick counter reaches TICK the clock stands still again (REB_QPC_HOLD_UNTIL_KEY) until the script presses
#    Scroll Lock a second time, right before it opens the Saved Games screen, and runs one tick per ~3 s from then on.
# The save therefore happens at TICK + a few ticks counted from that key press, not from wall time.
# All runs execute concurrently (one container each; ~7 min for 2 runs x 3 configurations). Output: work/run/long/<scenario>/<config>_<i>/, summary on stdout; exit status 1 when anything differs.
set -uo pipefail
source "$(dirname "$0")/paths.sh"
scenario=alliance; ticks=""; runs=2; noise=0; configs="orig hybrid standalone"; build=1
while [[ $# -gt 0 ]]; do
  case "$1" in
    -s) scenario="$2"; shift 2 ;;
    -t) ticks="$2"; shift 2 ;;
    -n) runs="$2"; shift 2 ;;
    -c) configs="$2"; shift 2 ;;
    --no-build) build=0; shift ;;
    --noise) noise="$2"; shift 2 ;;
    *) echo "unknown argument $1" >&2; exit 2 ;;
  esac
done
case "$scenario" in
  alliance) default_ticks=98575 ;;
  battle) default_ticks=154500 ;;
  *) echo "unknown scenario $scenario" >&2; exit 2 ;;
esac
ticks="${ticks:-$default_ticks}"
file="$ROOT/tests/scenarios/long_$scenario.txt"
base="$ROOT/work/run/long/$scenario"
mkdir -p "$base"

if [[ $build == 1 ]]; then
  "$ROOT/tools/build_hybrid.sh" > "$base/build_hybrid.log" 2>&1 || { echo "hybrid build failed ($base/build_hybrid.log)"; exit 1; }
  [[ " $configs " == *" standalone "* ]] && { "$ROOT/tools/build_standalone.sh" > "$base/build_standalone.log" 2>&1 || { echo "standalone build failed"; exit 1; }; }
fi

export REB_DETERMINISTIC=1 REB_QPC_FAST_UNTIL_TICK="$ticks" REB_QPC_START_KEY=145 REB_QPC_HOLD_UNTIL_KEY=145 REB_CLOCK_FROM_QPC=1 REB_NO_TIMERS=1

run_one() {
  local config="$1" i="$2" flag=() hooks=()
  case "$config" in
    orig) hooks=(REB_DISABLE_HOOKS=all) ;;
    standalone) flag=(--standalone) ;;
  esac
  env "${hooks[@]+"${hooks[@]}"}" "$ROOT/tools/run_game.sh" "${flag[@]+"${flag[@]}"}" --out "$base/${config}_$i" "$file" > "$base/${config}_$i.log" 2>&1
  echo "$config run $i: exit $?, $(grep -c Unhandled "$base/${config}_$i/wine.log" 2>/dev/null || echo 0) unhandled"
}

pids=()
for config in $configs; do
  for i in $(seq 1 "$runs"); do run_one "$config" "$i" & pids+=($!); done
done
wait "${pids[@]}"

"$ROOT/tools/long_compare_report.py" "$base" --noise "$noise"
status=$?
[[ $status == 0 ]] && echo "LONG COMPARE: all runs agree (<= $noise differing bytes)" || echo "LONG COMPARE: runs differ (see the matrix above)"
exit $status
