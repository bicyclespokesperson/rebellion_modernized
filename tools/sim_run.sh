#!/usr/bin/env bash
# One headless simulation run of the standalone exe (REB_SIM harness, src/port/sim_harness.cpp) under Wine.
#
# Usage: sim_run.sh [-e VAR=VAL]... <loadfile> <ticks> <outdir> [every]
#   <loadfile>: a path on the host (copied read-only into the container)
#   <outdir>:   receives sim.sav (final save), sim.sav.t<tick> (every <every> ticks), sim.sav.log, wine.log
# REB_DETERMINISTIC=1 is always set (virtual clocks, fixed time()); -e adds more (e.g. -e REB_HEAP_PROBE=19070976).
set -euo pipefail
source "$(dirname "$0")/paths.sh"
envs=()
while getopts "e:" opt; do case "$opt" in e) envs+=(-e "$OPTARG") ;; *) exit 2 ;; esac; done
shift $((OPTIND - 1))
load="$(realpath "$1")"; ticks="$2"; out="$(realpath -m "$3")"; every="${4:-}"
exe="$ROOT/build/standalone/rebellion.exe"
[[ -f "$exe" ]] || { echo "no standalone build at $exe; run tools/build_standalone.sh" >&2; exit 1; }
rm -rf "$out"; mkdir -p "$out"
spec="load:Z:\\in\\load.sav,ticks:$ticks,save:Z:\\out\\sim.sav"
[[ -n "$every" ]] && spec+=",every:$every"
docker run --rm -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" -e WINEDEBUG -e REB_SIM_TIMEOUT \
  -e REB_DETERMINISTIC=1 -e "REB_SIM=$spec" -e REB_CLOCK_STEP_US -e REB_QPC_STEP_NS -e REB_HEAP_PROBE -e REB_HEAP_PROBE_SBH \
  -e 'REB_LOG=Z:\out\rebellion.log' -e 'REB_EXE=Z:\rebellion.exe' "${envs[@]+"${envs[@]}"}" \
  -v "$MAIN/work/game:/game" -v "$MAIN/orig_game_files/Star Wars - Rebellion/SMACKW32.DLL:/game/SMACKW32.DLL:ro" \
  -v "$exe:/rebellion.exe:ro" -v "$load:/in/load.sav:ro" -v "$out:/out" -v "$ROOT/tools/container:/tools:ro" \
  rebellion-dev bash /tools/run_sim.sh
echo "output: $out"
