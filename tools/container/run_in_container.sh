#!/usr/bin/env bash
# In-container driver. $1 = scenario file; one action per line, "<sec> <action> [args]", times in seconds since
# launch:  shot <name> | key <xdotool key> | keydown <key> | keyup <key> | click <x> <y> | rclick <x> <y> |
# dblclick <x> <y> | move <x> <y> | mousedown <x> <y> | mouseup <x> <y> | drag <x1> <y1> <x2> <y2> | end
# Modifier clicks: keydown shift / click ... / keyup shift.
set -uo pipefail
scenario="$1"
Xvfb :1 -screen 0 640x480x24 >/dev/null 2>&1 &
export DISPLAY=:1
sleep 1
key='HKLM\Software\LucasArts Entertainment Company LLC\Star Wars Rebellion'
for v in DataLocation DllLocation InstalledLocation TooBigLocation; do wine reg add "$key" /v "$v" /d 'C:\game' /f >/dev/null 2>&1; done
wine reg add "$key" /v EncyLocation /d 'C:\game\EDATA' /f >/dev/null 2>&1
wine reg add "$key" /v EXEPath /d 'C:\game\REBEXE.EXE' /f >/dev/null 2>&1
ln -sfn /game "$WINEPREFIX/drive_c/game"
cd /game
WINEDEBUG="${WINEDEBUG:-err+all}" wine explorer /desktop=reb,640x480 "${REB_EXE:-REBEXE.EXE}" > /out/wine.log 2>&1 &
start=$(date +%s.%N)
while read -r at action a1 a2 a3 a4; do
  [[ -z "${at:-}" || "$at" == \#* ]] && continue
  python3 -c "import time,sys; d=$start+$at-time.time(); d>0 and time.sleep(d)"
  case "$action" in
    shot) import -window root "/out/$a1.png" 2>/dev/null ;;
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
    end) break ;;
  esac
done < "$scenario"
wineserver -k
chown -R "${HOST_UID:-0}:${HOST_GID:-0}" /out /game/SaveGame 2>/dev/null
