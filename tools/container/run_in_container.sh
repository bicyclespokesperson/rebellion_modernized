#!/usr/bin/env bash
# In-container driver. $1 = scenario file (lines: "<sec> shot <name>" | "<sec> key <xdotool key>" |
# "<sec> click <x> <y>" | "<sec> end"), times are seconds since launch.
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
WINEDEBUG="${WINEDEBUG:-err+all}" wine explorer /desktop=reb,640x480 REBEXE.EXE > /shots/wine.log 2>&1 &
start=$(date +%s.%N)
while read -r at action a1 a2; do
  [[ -z "${at:-}" || "$at" == \#* ]] && continue
  python3 -c "import time,sys; d=$start+$at-time.time(); d>0 and time.sleep(d)"
  case "$action" in
    shot) import -window root "/shots/$a1.png" 2>/dev/null ;;
    key) xdotool key "$a1" ;;
    click) xdotool mousemove "$a1" "$a2" click 1 ;;
    rclick) xdotool mousemove "$a1" "$a2" click 3 ;;
    move) xdotool mousemove "$a1" "$a2" ;;
    end) break ;;
  esac
done < "$scenario"
wineserver -k
chown -R "${HOST_UID:-0}:${HOST_GID:-0}" /shots /game/SaveGame 2>/dev/null
