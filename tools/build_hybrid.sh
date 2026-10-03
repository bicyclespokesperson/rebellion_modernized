#!/usr/bin/env bash
# Build the hybrid proxy DLL in the dev container and install it into work/game.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
docker run --rm -u "$(id -u):$(id -g)" -v "$root:/src" -w /src rebellion-dev bash -c '
  cmake -S . -B build/hybrid -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-win32-clangcl.cmake -DCMAKE_BUILD_TYPE=Release >/dev/null &&
  cmake --build build/hybrid'
game="$root/work/game"
if [[ ! -f "$game/smackorig.dll" ]]; then
  cp "$root/orig_game_files/Star Wars - Rebellion/SMACKW32.DLL" "$game/smackorig.dll"
fi
cp "$root/build/hybrid/smackw32.dll" "$game/SMACKW32.DLL"
echo "installed hybrid DLL into work/game"
