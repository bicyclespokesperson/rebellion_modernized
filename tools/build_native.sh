#!/usr/bin/env bash
# Build the native Linux x86-64 executable (build/native/rebellion) of this checkout in the rebellion-native container
# (docker/Dockerfile.native; docs/native_linux.md). Stages, like tools/build_standalone.sh:
#   1. objects: the game, minwin_impl (SDL3, FreeType) and the POSIX port layer;
#   2. generators (tools/gen_native_data.py, tools/gen_native_init.py) read REBEXE.EXE and the objects and write build/native/gen/*.cpp;
#   3. link.
#   tools/build_native.sh [--image] [--objects-only]
# Environment: REB_NATIVE_DIR (build directory, default build/native), REB_BUILD_TYPE (default RelWithDebInfo),
#   REB_CMAKE_ARGS (e.g. -DREB_NATIVE_SANITIZE=address,undefined; REB_SANITIZE=1 is shorthand for that with REB_NATIVE_DIR=build/native_asan), REB_NINJA_ARGS, REB_TAIL (build log lines shown).
set -euo pipefail
source "$(dirname "$0")/paths.sh"
objects_only=0
for arg in "$@"; do
  case "$arg" in
    --image) DOCKER_BUILDKIT=0 docker build -t rebellion-native -f "$ROOT/docker/Dockerfile.native" "$ROOT/docker" ;;
    --objects-only) objects_only=1 ;;
    *) echo "usage: $0 [--image] [--objects-only]" >&2; exit 2 ;;
  esac
done
if [[ "${REB_SANITIZE:-}" == 1 ]]; then
  REB_NATIVE_DIR="${REB_NATIVE_DIR:-build/native_asan}"
  REB_CMAKE_ARGS="-DREB_NATIVE_SANITIZE=address,undefined ${REB_CMAKE_ARGS:-}"
  REB_BUILD_TYPE="${REB_BUILD_TYPE:-RelWithDebInfo}"
fi
bdir="${REB_NATIVE_DIR:-build/native}"
mkdir -p "$ROOT/$bdir/gen"
container() {
  docker run --rm -u "$(id -u):$(id -g)" "${REB_MOUNTS[@]}" -w "$ROOT" rebellion-native bash -c "$1"
}
exe="$MAIN/orig_game_files/Star Wars - Rebellion/REBEXE.EXE"
configure="cmake -S . -B $bdir -G Ninja -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 -DCMAKE_BUILD_TYPE=${REB_BUILD_TYPE:-RelWithDebInfo} -DREB_NATIVE=ON ${REB_CMAKE_ARGS:-}"
build() {  # build <target> <gen: ON|OFF>
  container "$configure -DREB_NATIVE_GEN=$2 > $bdir/gen/configure.log 2>&1 || { tail -20 $bdir/gen/configure.log; exit 1; }; cmake --build $bdir --target $1 -- ${REB_NINJA_ARGS:-} > $bdir/gen/build.log 2>&1; rc=\$?; grep -E 'error|undefined reference' $bdir/gen/build.log | sort -u | head -${REB_TAIL:-30}; exit \$rc"
}
build reb_native_objects OFF
[[ $objects_only == 1 ]] && exit 0
container "python3 tools/gen_native_data.py --build $bdir --exe '$exe' --symbols symbols --out $bdir/gen"
container "python3 tools/gen_native_init.py --build $bdir --exe '$exe' --symbols symbols --out $bdir/gen"
build rebellion ON
echo "built $ROOT/$bdir/rebellion"
