#!/usr/bin/env bash
# Golden corpus of the headless simulation harness (REB_SIM, src/port/sim_harness.cpp; docs/phase3_plan.md, P0a).
# Each golden loads one of the install's shipped saves, runs N ticks with intermediate saves, and the sha256 of every save (and of the
# harness log: frame counts, message counters) is compared against tests/goldens/<name>.sha256. Only hashes and command lines are
# stored; the saves derive from the user's game data and stay under work/run/sim_goldens/.
#
# Usage: sim_goldens.sh record [-n RUNS]            run every golden RUNS times (default 5), require all runs identical, write hashes
#        sim_goldens.sh check  [-n RUNS] [-j JOBS] [-e VAR=VAL]... [-k TAG] [name...]
#                                                    run (default once) and compare with the recorded hashes
#   -e VAR=VAL  extra environment of the run, e.g. -e REB_HEAP_PROBE=100003 -e REB_HEAP_PROBE_MODE=reverse
#   -k TAG      run directory suffix (default: base); runs go to work/run/sim_goldens/<name>_<TAG>_<i>/
# Corpus definition: tests/goldens/sim_goldens.txt (name, install save file, ticks, checkpoint interval).
set -euo pipefail
source "$(dirname "$0")/paths.sh"
mode="${1:?usage: sim_goldens.sh record|check ...}"; shift
runs=0; jobs=3; tag=base; envs=()
while getopts "n:j:e:k:" opt; do
  case "$opt" in
    n) runs="$OPTARG" ;;
    j) jobs="$OPTARG" ;;
    e) envs+=(-e "$OPTARG") ;;
    k) tag="$OPTARG" ;;
    *) exit 2 ;;
  esac
done
shift $((OPTIND - 1))
[[ $runs == 0 ]] && { [[ $mode == record ]] && runs=5 || runs=1; }
corpus="$ROOT/tests/goldens/sim_goldens.txt"
saves="$MAIN/orig_game_files/Star Wars - Rebellion/SaveGame"
base="$ROOT/work/run/sim_goldens"
mkdir -p "$base"

want=("$@")
golden_lines() { grep -v '^#' "$corpus" | grep -v '^$'; }

run_one() {
  local name="$1" file="$2" ticks="$3" every="$4" i="$5"
  "$ROOT/tools/sim_run.sh" "${envs[@]+"${envs[@]}"}" "$saves/$file" "$ticks" "$base/${name}_${tag}_$i" "$every" > "$base/${name}_${tag}_$i.out" 2>&1 || true
}

hash_run() {
  (cd "$1" && sha256sum sim.sav* | sed 's/ \*/  /' | sort -k2)
}

status=0
while IFS='|' read -r name file ticks every; do
  name="$(echo "$name" | xargs)"; file="$(echo "$file" | xargs)"; ticks="$(echo "$ticks" | xargs)"; every="$(echo "$every" | xargs)"
  if [[ ${#want[@]} -gt 0 ]] && [[ ! " ${want[*]} " =~ " $name " ]]; then continue; fi
  running=0
  for i in $(seq 1 "$runs"); do
    run_one "$name" "$file" "$ticks" "$every" "$i" &
    running=$((running + 1))
    if [[ $running -ge $jobs ]]; then wait -n; running=$((running - 1)); fi
  done
  wait
  ref="$(hash_run "$base/${name}_${tag}_1")"
  if ! grep -q "^done" "$base/${name}_${tag}_1/sim.sav.log"; then echo "$name: FAIL run 1 did not finish (see $base/${name}_${tag}_1/sim.sav.log)"; status=1; continue; fi
  same=1
  for i in $(seq 2 "$runs"); do
    [[ "$(hash_run "$base/${name}_${tag}_$i")" == "$ref" ]] || { same=0; echo "$name: run $i differs from run 1:"; diff <(echo "$ref") <(hash_run "$base/${name}_${tag}_$i") || true; }
  done
  [[ $same == 1 ]] || { status=1; continue; }
  golden="$ROOT/tests/goldens/$name.sha256"
  if [[ $mode == record ]]; then
    {
      echo "# $name: tools/sim_run.sh \"<install>/SaveGame/$file\" $ticks <out> $every   (REB_DETERMINISTIC=1, standalone rebellion.exe)"
      echo "# identical across $runs runs"
      echo "$ref"
    } > "$golden"
    echo "$name: recorded ($runs identical runs, $(echo "$ref" | wc -l) files)"
  else
    if [[ "$(grep -v '^#' "$golden")" == "$ref" ]]; then echo "$name: IDENTICAL to golden ($runs runs${envs[*]:+, ${envs[*]}})"
    else echo "$name: DIFFERS from golden"; diff <(grep -v '^#' "$golden") <(echo "$ref") || true; status=1; fi
  fi
done < <(golden_lines)
exit $status
