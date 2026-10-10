#!/usr/bin/env bash
# start|stop|restart|status the long-lived Ghidra query server (used by tools/gq).
set -uo pipefail
source "$(dirname "$0")/paths.sh"
root="$MAIN"
pidfile="$root/work/ghidra_server.pid"
sock="$root/work/ghidra.sock"
running() { [[ -f "$pidfile" ]] && kill -0 "$(cat "$pidfile")" 2>/dev/null; }
stop() {
  if running; then
    "$root/tools/gq" --quit >/dev/null 2>&1
    for _ in $(seq 20); do running || break; sleep 1; done
    running && kill "$(cat "$pidfile")"
  fi
  rm -f "$pidfile" "$sock"
}
start() {
  running && { echo "already running (pid $(cat "$pidfile"))"; return; }
  rm -f "$root"/ghidra_proj/Rebellion.lock*
  nohup "$root/tools/ghidra_py" "$root/tools/py/ghidra_server.py" > "$root/logs/ghidra_server.log" 2>&1 &
  echo $! > "$pidfile"
  for _ in $(seq 90); do [[ -S "$sock" ]] && grep -q "ready" "$root/logs/ghidra_server.log" && { echo "ready"; return; }; sleep 1; done
  echo "server did not become ready; see logs/ghidra_server.log"; return 1
}
case "${1:-status}" in
  start) start ;;
  stop) stop ;;
  restart) stop; start ;;
  status) running && echo "running (pid $(cat "$pidfile"))" || echo "stopped" ;;
esac
