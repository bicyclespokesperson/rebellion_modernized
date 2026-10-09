#!/usr/bin/env bash
# Build the native Linux x86-64 executable (build/native/rebellion) of this checkout in the rebellion-native container
# (docker/Dockerfile.native; docs/native_linux.md).
#   tools/build_native.sh [--image]      --image: (re)build the container image first
# Environment: REB_NATIVE_DIR (build directory, default build/native), REB_BUILD_TYPE (default RelWithDebInfo),
#   REB_CMAKE_ARGS (e.g. "-DREB_NATIVE_SANITIZE=address;undefined"), REB_TARGET, REB_TAIL (lines of build log shown).
set -euo pipefail
source "$(dirname "$0")/paths.sh"
if [[ "${1:-}" == "--image" ]]; then
  DOCKER_BUILDKIT=0 docker build -t rebellion-native -f "$ROOT/docker/Dockerfile.native" "$ROOT/docker"
fi
bdir="${REB_NATIVE_DIR:-build/native}"
mkdir -p "$ROOT/$bdir/gen"
container() {
  docker run --rm -u "$(id -u):$(id -g)" -v "$ROOT:$ROOT" -v "$MAIN:$MAIN:ro" -w "$ROOT" rebellion-native bash -c "$1"
}
configure="cmake -S . -B $bdir -G Ninja -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 -DCMAKE_BUILD_TYPE=${REB_BUILD_TYPE:-RelWithDebInfo} -DREB_NATIVE=ON ${REB_CMAKE_ARGS:-}"
container "$configure > $bdir/gen/configure.log 2>&1 || { tail -20 $bdir/gen/configure.log; exit 1; }; cmake --version >/dev/null && cmake --build $bdir ${REB_TARGET:+--target $REB_TARGET} -- ${REB_NINJA_ARGS:-} > $bdir/gen/build.log 2>&1; rc=\$?; tail -${REB_TAIL:-30} $bdir/gen/build.log; exit \$rc"
