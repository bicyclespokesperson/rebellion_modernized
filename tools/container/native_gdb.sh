#!/usr/bin/env bash
# Debug helper: run the native executable under gdb (batch) in a run directory made by tools/run_native.sh, and print a backtrace
# where it stops. $1 = run directory (work/run_native/<scenario>), $2.. = extra gdb -ex commands (default: run, bt).
set -uo pipefail
dir="$1"; shift
exe="${REB_EXE:-$(cd "$(dirname "$0")/../.." && pwd)/build/native/rebellion}"
Xvfb :78 -screen 0 640x480x24 >/dev/null 2>&1 &
export DISPLAY=:78
sleep 1
cd "$dir/game"
export REBELLION_CONFIG="$dir/config.ini" HOME="$dir"
cmds=("$@")
[[ ${#cmds[@]} -eq 0 ]] && cmds=("run" "bt 30")
args=()
for c in "${cmds[@]}"; do args+=(-ex "$c"); done
timeout -s INT "${REB_GDB_TIMEOUT:-60}" gdb -batch -ex "handle SIGPIPE nostop" "${args[@]}" "$exe" 2>&1 | tail -${REB_GDB_LINES:-60}
kill %1 2>/dev/null
