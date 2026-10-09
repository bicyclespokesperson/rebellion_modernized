#!/usr/bin/env bash
# In-container driver of the native executable (tools/run_native.sh). $1 = scenario file (syntax of
# tools/container/run_in_container.sh: "<sec> <action> [args]", seconds since launch), $2 = the executable, $3 = output directory.
# Xvfb 640x480, the game with its own SDL window at the origin, xdotool for input, ImageMagick `import` for screenshots.
set -uo pipefail
scenario="$1"; exe="$2"; out="$3"
display=":${REB_DISPLAY_NUMBER:-77}"
Xvfb "$display" -screen 0 640x480x24 >/dev/null 2>&1 &
xvfb=$!
export DISPLAY="$display"
export SDL_RENDER_DRIVER="${SDL_RENDER_DRIVER:-software}"
export ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0:alloc_dealloc_mismatch=0}"
sleep 1
cd "$out/game"
if [[ -n "${REB_GDB:-}" ]]; then
  # REB_GDB=1: run under gdb; a crash prints the backtrace into game.log
  gdb -batch -ex "handle SIGPIPE nostop noprint" -ex run -ex "bt 40" -ex "info registers rip" --args "$exe" ${REB_GAME_ARGS:-} > "$out/game.log" 2>&1 &
else
  "$exe" ${REB_GAME_ARGS:-} > "$out/game.log" 2>&1 &
fi
game=$!
start=$(date +%s.%N)
finished=0
while read -r at action a1 a2 a3 a4; do
  [[ -z "${at:-}" || "$at" == \#* ]] && continue
  python3 -c "import time; d=$start+$at-time.time(); d>0 and time.sleep(d)"
  if ! kill -0 "$game" 2>/dev/null; then echo "game exited early (before '$action $a1' at $at s)" >> "$out/driver.log"; break; fi
  case "$action" in
    shot) import -window root "$out/$a1.png" 2>/dev/null ;;
    key) xdotool key "$a1" ;;
    keydown) xdotool keydown "$a1" ;;
    keyup) xdotool keyup "$a1" ;;
    dblclick) xdotool mousemove "$a1" "$a2" click --repeat 2 --delay 80 1 ;;
    mousedown) xdotool mousemove "$a1" "$a2" mousedown 1 ;;
    mouseup) xdotool mousemove "$a1" "$a2" mouseup 1 ;;
    drag) xdotool mousemove "$a1" "$a2" mousedown 1 sleep 0.3 mousemove "$a3" "$a4" sleep 0.3 mouseup 1 ;;
    click) xdotool mousemove "$a1" "$a2" click 1 ;;
    rclick) xdotool mousemove "$a1" "$a2" click 3 ;;
    move) xdotool mousemove "$a1" "$a2" ;;
    end) finished=1; break ;;
  esac
done < "$scenario"
if kill -0 "$game" 2>/dev/null; then
  kill "$game" 2>/dev/null
  sleep 1
  kill -9 "$game" 2>/dev/null
  echo "game was still running at the end of the scenario" >> "$out/driver.log"
else
  wait "$game"
  echo "game exit status $?" >> "$out/driver.log"
fi
kill "$xvfb" 2>/dev/null
exit 0
