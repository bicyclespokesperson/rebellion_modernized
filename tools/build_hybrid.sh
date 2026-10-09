#!/usr/bin/env bash
# Build the hybrid proxy DLL (build/hybrid/smackw32.dll) of this checkout in the dev container.
# REB_NO_TACTICAL_3D=ON builds the variant that stubs out the 3D tactical code and always auto-resolves battles
# (the setting is passed on every run, so a later plain build is the default hybrid again).
set -euo pipefail
source "$(dirname "$0")/paths.sh"
# A failed link must not leave the previous DLL behind for difftest/run_game to pick up silently.
rm -f "$ROOT/build/hybrid/smackw32.dll"
docker run --rm -u "$(id -u):$(id -g)" -e "REB_NO_TACTICAL_3D=${REB_NO_TACTICAL_3D:-OFF}" -v "$ROOT:$ROOT" -w "$ROOT" rebellion-dev bash -c '
  cmake -S . -B build/hybrid -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-win32-clangcl.cmake -DCMAKE_BUILD_TYPE=Release "-DREB_NO_TACTICAL_3D=$REB_NO_TACTICAL_3D" >/dev/null &&
  cmake --build build/hybrid'
