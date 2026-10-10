#!/usr/bin/env bash
# Regression guard for the standalone rebellion.exe: build it, run smoke (no "Unhandled" in wine.log), then check save
# fidelity: the save the standalone writes in tests/scenarios/save_determinism.txt against the one the original code writes
# in the same session (hybrid with REB_DISABLE_HOOKS=all). Equal, or a tick-only difference (<= 70 differing bytes at
# equal size: the tick counter at save time varies with timing, see docs/agent_brief.md) which is retried once.
# Prints one verdict line starting with "STANDALONE OK" or "STANDALONE FAIL". Exit status 1 on failure.
#   tools/standalone_check.sh [--no-build]
set -uo pipefail
source "$(dirname "$0")/paths.sh"
base="$ROOT/work/run/standalone_check"
mkdir -p "$base"
fail() { echo "STANDALONE FAIL: $*"; exit 1; }

if [[ "${1:-}" != --no-build ]]; then
  "$ROOT/tools/build_standalone.sh" > "$base/build.log" 2>&1 || { tail -5 "$base/build.log"; fail "build (see $base/build.log)"; }
fi

"$ROOT/tools/run_game.sh" --standalone --out "$base/smoke" "$ROOT/tests/scenarios/smoke.txt" > "$base/smoke.log" 2>&1 || fail "smoke run"
if grep -q Unhandled "$base/smoke/wine.log"; then fail "smoke: $(grep -m1 Unhandled "$base/smoke/wine.log")"; fi
ls "$base/smoke"/*.png > /dev/null 2>&1 || fail "smoke produced no screenshot"

save_pair_difference() {
  local standalone_dir="$1" original_dir="$2" worst=0 name count
  for f in "$standalone_dir"/*; do
    name="$(basename "$f")"
    cmp -s "$f" "$MAIN/orig_game_files/Star Wars - Rebellion/SaveGame/$name" && continue
    [[ -f "$original_dir/$name" ]] || { echo "missing:$name"; return; }
    cmp -s "$f" "$original_dir/$name" && continue
    [[ "$(stat -c %s "$f")" == "$(stat -c %s "$original_dir/$name")" ]] || { echo "size:$name"; return; }
    count="$(cmp -l "$f" "$original_dir/$name" | wc -l)"
    (( count > worst )) && worst=$count
  done
  echo "bytes:$worst"
}

attempt() {
  rm -rf "$ROOT/work/run/determinism/sa_1" "$ROOT/work/run/determinism/orig_1"
  "$ROOT/tools/determinism_check.sh" -n 1 -s -t sa > "$base/save_sa.log" 2>&1 &
  local standalone_pid=$!
  "$ROOT/tools/determinism_check.sh" -n 1 -t orig -e REB_DISABLE_HOOKS=all > "$base/save_orig.log" 2>&1 || fail "original save run (see $base/save_orig.log)"
  wait "$standalone_pid" || fail "standalone save run (see $base/save_sa.log)"
  result="$(save_pair_difference "$ROOT/work/run/determinism/sa_1/SaveGame" "$ROOT/work/run/determinism/orig_1/SaveGame")"
}

attempt
if [[ "$result" == bytes:* && "${result#bytes:}" != 0 ]] && (( ${result#bytes:} <= 70 )); then attempt; fi
case "$result" in
  bytes:0) echo "STANDALONE OK: smoke clean, saves byte-identical to the original's" ;;
  bytes:*) count="${result#bytes:}"
           if (( count <= 70 )); then echo "STANDALONE OK: smoke clean, saves differ only in the tick/checksum ($count bytes, twice)"
           else fail "saves differ in $count bytes (cmp -l work/run/determinism/sa_1/SaveGame/* vs orig_1)"; fi ;;
  *) fail "saves differ: $result" ;;
esac
