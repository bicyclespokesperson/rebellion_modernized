#!/usr/bin/env bash
# Launch the native desktop game; see README.md for build and installation setup.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

fail() {
  printf 'Cannot launch Rebellion: %s\n' "$1" >&2
  exit 1
}

if [[ $# -ne 0 ]]; then
  printf 'Usage: %s (no arguments)\nSet REBELLION_CONFIG to use a custom config.ini.\n' "$0" >&2
  exit 2
fi

case "$(uname -s)" in
  Darwin)
    EXECUTABLE="$ROOT/build/native/Rebellion.app/Contents/MacOS/Rebellion"
    USER_CONFIG="${HOME:-}/Library/Application Support/Rebellion/config.ini"
    LOCAL_CONFIG="$ROOT/work/macos/config.ini"
    BUILD_HELP='tools/build_macos.sh (requires Xcode command-line tools, CMake and uv)'
    ;;
  Linux)
    EXECUTABLE="$ROOT/build/native/rebellion"
    USER_CONFIG="${XDG_CONFIG_HOME:-${HOME:-}/.config}/rebellion/config.ini"
    LOCAL_CONFIG="$ROOT/work/launch/config.ini"
    BUILD_HELP=$'tools/fetch_deps.sh freetype liberation sdl3\n  tools/build_native.sh --image\n  tools/build_native.sh (requires Docker)'
    ;;
  *) fail 'this launcher supports macOS and Linux native builds only.' ;;
esac

[[ -f "$EXECUTABLE" ]] || fail "the native game has not been built:
  $EXECUTABLE
From the repo root, run:
  $BUILD_HELP
The build requires your installed game at:
  $ROOT/orig_game_files/Star Wars - Rebellion
See docs/native.md for details."
[[ -x "$EXECUTABLE" ]] || fail "the game binary is not executable:
  $EXECUTABLE
Restore its permission with chmod +x, or rebuild it."

cd "$ROOT"
if [[ -n "${REBELLION_CONFIG:-}" ]]; then
  CONFIG="$REBELLION_CONFIG"
elif [[ -f "$USER_CONFIG" ]]; then
  CONFIG="$USER_CONFIG"
elif [[ -f "$LOCAL_CONFIG" ]]; then
  CONFIG="$LOCAL_CONFIG"
else
  INSTALL_DIR="$ROOT/orig_game_files/Star Wars - Rebellion"
  [[ -d "$INSTALL_DIR" ]] || fail "no configuration or installed game was found.
Copy your complete installed game into:
  $INSTALL_DIR
Or create a config.ini containing:
  install_dir=/absolute/path/to/Star Wars - Rebellion
Then select it with REBELLION_CONFIG=/absolute/path/to/config.ini ./launch_game.sh"
  CONFIG="$ROOT/work/launch/config.ini"
  mkdir -p "$(dirname "$CONFIG")" || fail "could not create the config directory for $CONFIG"
  printf 'install_dir=%s\n' "$INSTALL_DIR" > "$CONFIG" || fail "could not write $CONFIG"
  printf 'Created configuration: %s\n' "$CONFIG"
fi

[[ -f "$CONFIG" && -r "$CONFIG" ]] || fail "the configuration file is missing or unreadable:
  $CONFIG
Create a plain text file containing install_dir=/absolute/path/to/Star Wars - Rebellion."
export REBELLION_CONFIG="$CONFIG"
printf 'Launching Rebellion\nConfig: %s\n' "$CONFIG"
# The game checks install_dir, resource files and SaveGame before starting.
exec "$EXECUTABLE"
