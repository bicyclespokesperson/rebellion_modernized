#!/usr/bin/env bash
# In-container driver of the headless simulation harness (REB_SIM, src/port/sim_harness.cpp): Xvfb for the display check,
# the registry values the game reads, then the standalone exe in the foreground until it exits.
set -uo pipefail
Xvfb :1 -screen 0 640x480x24 >/dev/null 2>&1 &
export DISPLAY=:1
sleep 1
key='HKLM\Software\LucasArts Entertainment Company LLC\Star Wars Rebellion'
for v in DataLocation DllLocation InstalledLocation TooBigLocation; do wine reg add "$key" /v "$v" /d 'C:\game' /f >/dev/null 2>&1; done
wine reg add "$key" /v EncyLocation /d 'C:\game\EDATA' /f >/dev/null 2>&1
wine reg add "$key" /v EXEPath /d 'C:\game\REBEXE.EXE' /f >/dev/null 2>&1
ln -sfn /game "$WINEPREFIX/drive_c/game"
cd /game
WINEDEBUG="${WINEDEBUG:-err+all}" timeout "${REB_SIM_TIMEOUT:-1800}" wine "${REB_EXE:-REBEXE.EXE}" > /out/wine.log 2>&1
echo "exit $?" >> /out/wine.log
wineserver -k
chown -R "${HOST_UID:-0}:${HOST_GID:-0}" /out 2>/dev/null
