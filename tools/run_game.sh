#!/usr/bin/env bash
# Run REBEXE.EXE under Wine+Xvfb in the rebellion-dev container, driven by a scenario file
# (see tools/container/run_in_container.sh for the scenario syntax).
#
# Usage: run_game.sh [--orig] [--out DIR] <scenario.txt> [extra docker args...]
#   default: the game uses this checkout's hybrid DLL (build/hybrid/smackw32.dll)
#   --orig:  the unmodified original game (no hybrid DLL)
#   --out:   output directory (default work/run/<scenario name>); receives screenshots, wine.log,
#            rebhybrid.log and captures/ (when REB_CAPTURE is set)
# Environment passed through: REB_CAPTURE, REB_DISABLE_HOOKS, WINEDEBUG, and the determinism switches
# REB_DETERMINISTIC, REB_CLOCK_STEP_US, REB_QPC_STEP_NS, REB_CLOCK_TRACE, REB_NO_TIMERS, REB_HEAP_PROBE, REB_HEAP_PROBE_SBH, REB_DATA_TRIPWIRE
# (see src/hybrid/determinism.cpp).
# When REB_CAPTURE is set, REB_DISABLE_HOOKS defaults to "all": captures then record original-code state,
# and functions that reconstructed code calls directly (bypassing the original address) are still hit.
set -euo pipefail
source "$(dirname "$0")/paths.sh"
orig=0; out=""
while [[ $# -gt 0 ]]; do
  case "$1" in
    --orig) orig=1; shift ;;
    --out) out="$(realpath -m "$2")"; shift 2 ;;
    *) break ;;
  esac
done
scenario="$(realpath "$1")"; shift
out="${out:-$ROOT/work/run/$(basename "$scenario" .txt)}"
rm -rf "$out"; mkdir -p "$out"
# Every run starts from the original saved games, so scenarios that save cannot disturb later or concurrent runs.
cp -r "$MAIN/orig_game_files/Star Wars - Rebellion/SaveGame" "$out/SaveGame"
game="$MAIN/work/game"
[[ -f "$game/smackorig.dll" ]] || cp "$MAIN/orig_game_files/Star Wars - Rebellion/SMACKW32.DLL" "$game/smackorig.dll"
if [[ $orig == 1 ]]; then
  dll="$MAIN/orig_game_files/Star Wars - Rebellion/SMACKW32.DLL"
else
  dll="$ROOT/build/hybrid/smackw32.dll"
  [[ -f "$dll" ]] || { echo "no hybrid build at $dll; run tools/build_hybrid.sh" >&2; exit 1; }
fi
if [[ -n "${REB_CAPTURE:-}" ]]; then export REB_DISABLE_HOOKS="${REB_DISABLE_HOOKS:-all}"; fi
docker run --rm -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" -e WINEDEBUG -e REB_CAPTURE -e REB_DISABLE_HOOKS -e REB_DETERMINISTIC -e REB_CLOCK_STEP_US -e REB_QPC_STEP_NS -e REB_CLOCK_TRACE -e REB_NO_TIMERS -e REB_HEAP_PROBE -e REB_HEAP_PROBE_SBH -e REB_DATA_TRIPWIRE -e REB_OWN_DATA_NO_PATCH \
  -e 'REB_LOG=Z:\out\rebhybrid.log' -e 'REB_CAPTURE_DIR=Z:\out\captures' \
  -v "$game:/game" -v "$dll:/game/SMACKW32.DLL:ro" -v "$out:/out" -v "$out/SaveGame:/game/SaveGame" -v "$ROOT/tools/container:/tools:ro" \
  -v "$scenario:/scenario.txt:ro" "$@" rebellion-dev bash /tools/run_in_container.sh /scenario.txt
echo "output: $out"
