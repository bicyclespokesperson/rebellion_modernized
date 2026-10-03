#!/usr/bin/env bash
# Run REBEXE.EXE (from work/game) under Wine+Xvfb in the rebellion-dev container, driven by a scenario file.
# Usage: run_game.sh <scenario.txt> [extra docker args...]   Output: work/shots/
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
scenario="$(realpath "$1")"; shift
mkdir -p "$root/work/shots"
docker run --rm -e HOST_UID="$(id -u)" -e HOST_GID="$(id -g)" -e WINEDEBUG \
  -v "$root/work/game:/game" -v "$root/work/shots:/shots" -v "$root/tools/container:/tools:ro" \
  -v "$scenario:/scenario.txt:ro" "$@" rebellion-dev bash /tools/run_in_container.sh /scenario.txt
