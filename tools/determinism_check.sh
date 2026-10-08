#!/usr/bin/env bash
# Run one scenario N times with the hybrid build and compare the resulting SaveGame directories byte for byte.
#
# Usage: determinism_check.sh [-n RUNS] [-t TAG] [-e VAR=VALUE]... [scenario.txt]
#   default scenario: tests/scenarios/save_determinism.txt (loads a game, saves SAVEGAME.005)
#   REB_DETERMINISTIC=1 is always set; add more with -e (e.g. -e REB_HEAP_PROBE=19070976)
#   runs go to work/run/determinism/<TAG>_<i>/; TAG defaults to "base"
# Prints, per save file that differs from run 1, the number of differing bytes and the first offsets
# (use `cmp -l a b` for all). Exit status 1 when any file differs.
set -euo pipefail
source "$(dirname "$0")/paths.sh"
runs=3; tag=base; envs=()
while getopts "n:t:e:" opt; do
  case "$opt" in
    n) runs="$OPTARG" ;;
    t) tag="$OPTARG" ;;
    e) envs+=("$OPTARG") ;;
    *) exit 2 ;;
  esac
done
shift $((OPTIND - 1))
scenario="${1:-$ROOT/tests/scenarios/save_determinism.txt}"
base="$ROOT/work/run/determinism"
mkdir -p "$base"

export REB_DETERMINISTIC=1
for kv in "${envs[@]+"${envs[@]}"}"; do export "$kv"; done

for i in $(seq 1 "$runs"); do
  "$ROOT/tools/run_game.sh" --out "$base/${tag}_$i" "$scenario" > "$base/${tag}_$i.log" 2>&1
  echo "run $i done: $(grep -c 'Unhandled' "$base/${tag}_$i/wine.log" || true) unhandled"
done

status=0
orig_saves="$MAIN/orig_game_files/Star Wars - Rebellion/SaveGame"
written=0
for f in "$base/${tag}_1/SaveGame"/*; do
  [[ -f "$orig_saves/$(basename "$f")" ]] && cmp -s "$f" "$orig_saves/$(basename "$f")" || written=$((written + 1))
done
if [[ $written == 0 ]]; then echo "FAIL: run 1 wrote no save file (an identical result would be vacuous)"; exit 1; fi
ref="$base/${tag}_1/SaveGame"
for i in $(seq 2 "$runs"); do
  other="$base/${tag}_$i/SaveGame"
  for f in "$ref"/*; do
    name="$(basename "$f")"
    if [[ ! -f "$other/$name" ]]; then echo "run $i: $name missing"; status=1; continue; fi
    if cmp -s "$f" "$other/$name"; then continue; fi
    count="$(cmp -l "$f" "$other/$name" 2>/dev/null | wc -l || true)"
    echo "run 1 vs run $i: $name differs in $count bytes (size $(stat -c %s "$f") / $(stat -c %s "$other/$name")); first offsets:"
    { cmp -l "$f" "$other/$name" 2>/dev/null || true; } | head -12 | awk '{printf "  0x%x: %s -> %s (octal)\n", $1-1, $2, $3}'
    status=1
  done
done
[[ $status == 0 ]] && echo "IDENTICAL: all save files match across $runs runs ($tag)"
exit $status
