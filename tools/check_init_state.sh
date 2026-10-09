#!/usr/bin/env bash
# Is the standalone's game data, once its generated static-initializer table has run, equal to what the original code
# leaves at WinMain's first instruction? Reference: original game (all hooks disabled) captured at 0x4165b0; standalone:
# REB_DUMP_INIT_STATE. Compared by tools/compare_init_state.py. Needs build/standalone/rebellion.exe and build/hybrid.
set -euo pipefail
source "$(dirname "$0")/paths.sh"
scenario="$ROOT/tests/scenarios/smoke.txt"
base="$ROOT/work/run/init_state"
mkdir -p "$base"
REB_CAPTURE=0x4165b0:1 REB_DISABLE_HOOKS=all "$ROOT/tools/run_game.sh" --out "$base/reference" "$scenario" > "$base/reference.log" 2>&1
REB_DUMP_INIT_STATE='Z:\out\init_state.bin' "$ROOT/tools/run_game.sh" --standalone --out "$base/standalone" "$scenario" > "$base/standalone.log" 2>&1
"$ROOT/tools/compare_init_state.py" "$base/reference/captures" "$base/standalone/init_state.bin" "$@"
