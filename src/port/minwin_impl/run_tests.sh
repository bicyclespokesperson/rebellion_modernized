#!/usr/bin/env bash
# Build and run the minwin_impl unit tests in the native container (docker/Dockerfile.native).
#   run_tests.sh [target...]
# Build tree: build/minwin_impl/native. Extra cmake arguments via MW_CMAKE_ARGS. Test working directory is the build tree.
set -euo pipefail
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
targets="${*:-}"
out="build/minwin_impl/native"
image="${REB_NATIVE_IMAGE:-rebellion-native}"
mkdir -p "$root/build"

docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp -v "$root:$root" -w "$root" "$image" bash -c \
  "set -e; cmake -S src/port/minwin_impl -B $out -G Ninja -DCMAKE_BUILD_TYPE=Release ${MW_CMAKE_ARGS:-} >/dev/null; \
   cmake --build $out ${targets:+--target $targets} 2>&1 | tail -n 40"
docker run --rm -u "$(id -u):$(id -g)" -v "$root:$root" -w "$root/$out" "$image" ctest --output-on-failure 2>&1 | tail -n 80
