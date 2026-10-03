#!/usr/bin/env bash
# Build the hybrid proxy DLL (build/hybrid/smackw32.dll) of this checkout in the dev container.
set -euo pipefail
source "$(dirname "$0")/paths.sh"
docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:$ROOT" -w "$ROOT" rebellion-dev bash -c '
  cmake -S . -B build/hybrid -G Ninja -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-win32-clangcl.cmake -DCMAKE_BUILD_TYPE=Release >/dev/null &&
  cmake --build build/hybrid'
