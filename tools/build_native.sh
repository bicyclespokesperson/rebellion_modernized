#!/usr/bin/env bash
# Build the native Linux x86-64 executable (build/native/rebellion) of this checkout in the rebellion-native container
# (docker/Dockerfile.native; docs/native.md). Stages:
#   1. objects: the game, minwin_impl (SDL3, FreeType) and the POSIX port layer;
#   2. extract versioned text assets from the local installation (no symbols or object layouts);
#   3. link.
#   tools/build_native.sh [--image] [--objects-only] [--arch arm64]
#   --arch arm64 cross-builds for aarch64 Linux (cmake/toolchain-linux-aarch64.cmake, SDL3 with only its dummy drivers, build/native_arm64);
#   run it with tools/native_sim.sh -a arm64 (qemu-user). docs/native.md.
# Environment: REB_NATIVE_IMAGE (container image, default rebellion-native), REB_NATIVE_DIR (build directory, default build/native), REB_BUILD_TYPE (default RelWithDebInfo),
#   REB_CMAKE_ARGS (e.g. -DREB_NATIVE_SANITIZE=address,undefined; REB_SANITIZE=1 is shorthand for that with REB_NATIVE_DIR=build/native_asan), REB_NINJA_ARGS, REB_TAIL (build log lines shown).
set -euo pipefail
source "$(dirname "$0")/paths.sh"
OBJECTS_ONLY=0
ARCH=x86_64
ARGS=("$@")
for ((ARG_INDEX = 0; ARG_INDEX < ${#ARGS[@]}; ARG_INDEX++)); do
  ARG="${ARGS[$ARG_INDEX]}"
  case "$ARG" in
    --image) DOCKER_BUILDKIT=0 docker build -t rebellion-native -f "$ROOT/docker/Dockerfile.native" "$ROOT/docker" ;;
    --objects-only) OBJECTS_ONLY=1 ;;
    --arch) ARCH="${ARGS[$((++ARG_INDEX))]}" ;;
    *) echo "usage: $0 [--image] [--objects-only] [--arch arm64]" >&2; exit 2 ;;
  esac
done
if [[ "${REB_SANITIZE:-}" == 1 ]]; then
  REB_NATIVE_DIR="${REB_NATIVE_DIR:-build/native_asan}"
  REB_CMAKE_ARGS="-DREB_NATIVE_SANITIZE=address,undefined ${REB_CMAKE_ARGS:-}"
  REB_BUILD_TYPE="${REB_BUILD_TYPE:-RelWithDebInfo}"
fi
IMAGE="${REB_NATIVE_IMAGE:-rebellion-native}"
CROSS_ARGS=""
case "$ARCH" in
  x86_64) DEFAULT_DIR=build/native ;;
  arm64) DEFAULT_DIR=build/native_arm64
         CROSS_ARGS="-DCMAKE_TOOLCHAIN_FILE=$ROOT/cmake/toolchain-linux-aarch64.cmake -DREB_SDL_HEADLESS=ON" ;;
  *) echo "unknown --arch $ARCH (x86_64, arm64)" >&2; exit 2 ;;
esac
BUILD_DIR="${REB_NATIVE_DIR:-$DEFAULT_DIR}"
mkdir -p "$ROOT/$BUILD_DIR/gen"
container() {
  docker run --rm -u "$(id -u):$(id -g)" "${REB_MOUNTS[@]}" -w "$ROOT" "$IMAGE" bash -c "$1"
}
EXE="$MAIN/orig_game_files/Star Wars - Rebellion/REBEXE.EXE"
CONFIGURE="cmake -S . -B $BUILD_DIR -G Ninja -DCMAKE_C_COMPILER=clang-19 -DCMAKE_CXX_COMPILER=clang++-19 $CROSS_ARGS -DCMAKE_BUILD_TYPE=${REB_BUILD_TYPE:-RelWithDebInfo} -DREB_NATIVE=ON ${REB_CMAKE_ARGS:-}"
build() {  # build <target>
  container "$CONFIGURE > $BUILD_DIR/gen/configure.log 2>&1 || { tail -20 $BUILD_DIR/gen/configure.log; exit 1; }; cmake --build $BUILD_DIR --target $1 -- ${REB_NINJA_ARGS:-} > $BUILD_DIR/gen/build.log 2>&1; RC=\$?; grep -E 'error|undefined reference' $BUILD_DIR/gen/build.log | sort -u | head -${REB_TAIL:-30}; exit \$RC"
}
if [[ $OBJECTS_ONLY == 1 ]]; then
  build reb_native_objects
  exit 0
fi
container "python3 tools/extract_startup_assets.py --exe '$EXE' --out $BUILD_DIR/startup-assets.bin"
build rebellion
echo "built $ROOT/$BUILD_DIR/rebellion"
