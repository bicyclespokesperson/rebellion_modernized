#!/usr/bin/env bash
# Configure the native build in build/native (or $REB_NATIVE_DIR). Run once, and again after changing build options;
# ./launch_game.sh then compiles incrementally before every launch. Prerequisites are listed in README.md.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

usage() {
  cat <<EOF
Usage: $0 [--build-type TYPE] [--game-exe PATH] [-- EXTRA_CMAKE_ARGS...]

  --build-type TYPE  CMake build type (default: \$REB_BUILD_TYPE or RelWithDebInfo)
  --game-exe PATH    the installed game's REBEXE.EXE, read for startup artwork
                     (default: \$REB_GAME_EXE or orig_game_files/Star Wars - Rebellion/REBEXE.EXE)

Environment: REB_NATIVE_DIR (build directory, default build/native), CC/CXX (Linux compilers, default clang/clang++).
EOF
}

fail() {
  printf 'create_build: %s\n' "$1" >&2
  exit 1
}

BUILD_TYPE="${REB_BUILD_TYPE:-RelWithDebInfo}"
GAME_EXE="${REB_GAME_EXE:-}"
BUILD_DIR="${REB_NATIVE_DIR:-build/native}"
EXTRA_CMAKE_ARGS=()
while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-type) BUILD_TYPE="${2:?--build-type needs a value}"; shift 2 ;;
    --game-exe) GAME_EXE="${2:?--game-exe needs a value}"; shift 2 ;;
    -h|--help) usage; exit 0 ;;
    --) shift; EXTRA_CMAKE_ARGS=("$@"); break ;;
    *) usage >&2; exit 2 ;;
  esac
done

find_game_exe() {
  local candidate
  for candidate in "orig_game_files/Star Wars - Rebellion"/[Rr][Ee][Bb][Ee][Xx][Ee].[Ee][Xx][Ee]; do
    [[ -f "$candidate" ]] && { printf '%s\n' "$candidate"; return; }
  done
}

require_tools() {
  local missing=() tool
  for tool in "$@"; do
    command -v "$tool" > /dev/null 2>&1 || missing+=("$tool")
  done
  [[ ${#missing[@]} -eq 0 ]] && return
  local hint="Install them (see README.md)"
  command -v mise > /dev/null 2>&1 && hint="Run 'mise install cmake ninja uv', then 'mise exec -- $0', or install them (see README.md)"
  fail "missing ${missing[*]}. $hint."
}

require_linux_dev_packages() {
  require_tools pkg-config
  local missing=() entry
  for entry in libdecor-0:libdecor-0-dev xscrnsaver:libxss-dev; do
    pkg-config --exists "${entry%%:*}" || missing+=("${entry##*:}")
  done
  [[ ${#missing[@]} -eq 0 ]] && return
  fail "missing development packages: ${missing[*]}. Without libdecor-0-dev the window has no title bar or close
button on GNOME/Wayland. Install them (see README.md), e.g. 'sudo apt install ${missing[*]} libdecor-0-plugin-1-gtk'."
}

COMPILER_ARGS=()
case "$(uname -s)" in
  Darwin)
    PLATFORM=macos
    require_tools cmake ninja uv clang
    ;;
  Linux)
    PLATFORM=linux
    : "${CC:=clang}" "${CXX:=clang++}"
    require_tools cmake ninja uv "$CC" "$CXX"
    require_linux_dev_packages
    COMPILER_ARGS=("-DCMAKE_C_COMPILER=$CC" "-DCMAKE_CXX_COMPILER=$CXX")
    ;;
  MINGW*|MSYS*|CYGWIN*)
    PLATFORM=windows
    require_tools cmake ninja uv clang-cl
    [[ -n "${INCLUDE:-}" && -n "${LIB:-}" ]] || fail "the MSVC environment is not set up. Start Git Bash from a
Visual Studio Developer PowerShell (x64 tools and Windows SDK), for example by running 'bash' there."
    COMPILER_ARGS=(-DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl)
    ;;
  *) fail "unsupported platform $(uname -s); see README.md." ;;
esac

[[ -n "$GAME_EXE" ]] || GAME_EXE="$(find_game_exe)"
[[ -n "$GAME_EXE" && -f "$GAME_EXE" ]] || fail "the installed game was not found. Copy it to
  $ROOT/orig_game_files/Star Wars - Rebellion/
or pass --game-exe /path/to/REBEXE.EXE."

tools/fetch_deps.sh freetype liberation sdl3
mkdir -p "$BUILD_DIR"
uv run --quiet tools/extract_startup_assets.py --exe "$GAME_EXE" --out "$BUILD_DIR/startup-assets.bin"
if [[ $PLATFORM == macos ]]; then
  uv run --quiet tools/extract_macos_icon.py --exe "$GAME_EXE" --out "$BUILD_DIR/Rebellion.icns"
fi

cmake -S . -B "$BUILD_DIR" -G Ninja -DREB_NATIVE=ON "-DCMAKE_BUILD_TYPE=$BUILD_TYPE" \
  ${COMPILER_ARGS[@]+"${COMPILER_ARGS[@]}"} ${EXTRA_CMAKE_ARGS[@]+"${EXTRA_CMAKE_ARGS[@]}"}

printf '\nConfigured %s (%s, %s). Run ./launch_game.sh to compile and play.\n' "$BUILD_DIR" "$PLATFORM" "$BUILD_TYPE"
