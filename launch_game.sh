#!/usr/bin/env bash
# Launch the native desktop game; see README.md for build and installation setup.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

fail() {
  printf 'Cannot launch Rebellion: %s\n' "$1" >&2
  exit 1
}

config_value() {
  awk -v KEY="$1" '
    function trim(VALUE) { sub(/^[ \t\r]+/, "", VALUE); sub(/[ \t\r]+$/, "", VALUE); return VALUE }
    {
      LINE = trim($0)
      if (LINE == "" || LINE ~ /^[#;]/) next
      if (LINE ~ /^\[/) {
        sub(/^\[/, "", LINE); sub(/\].*$/, "", LINE)
        REGISTRY = tolower(trim(LINE)) == "registry"
        next
      }
      EQ = index(LINE, "=")
      if (!REGISTRY && EQ && tolower(trim(substr(LINE, 1, EQ - 1))) == KEY) {
        print trim(substr(LINE, EQ + 1))
        FOUND = 1
        exit
      }
    }
    END { if (!FOUND) exit 1 }
  ' "$CONFIG"
}

prepare_hd_pack() {
  HD_PACK="$ROOT/work/hd/gameplay-pack"
  [[ -z "${REB_HD_ASSET_DIR+x}" ]] || return 0
  if config_value hd_asset_dir > /dev/null; then
    return 0
  fi
  if [[ ! -d "$HD_PACK" ]]; then
    INSTALL_DIR="$(config_value install_dir || true)"
    [[ -n "$INSTALL_DIR" ]] || return 0
    STRATEGY_MODULE=""
    for MODULE in "$INSTALL_DIR"/[Ss][Tt][Rr][Aa][Tt][Ee][Gg][Yy].[Dd][Ll][Ll]; do
      if [[ -f "$MODULE" ]]; then
        STRATEGY_MODULE="$MODULE"
        break
      fi
    done
    [[ -n "$STRATEGY_MODULE" ]] || return 0
    if ! command -v uv > /dev/null 2>&1; then
      printf 'HD pack not generated: install uv, then relaunch to enable HD artwork.\n' >&2
      return 0
    fi
    printf 'Generating HD artwork from %s\n' "$STRATEGY_MODULE"
    if ! uv run "$ROOT/tools/build_hd_pack.py" --recipe "$ROOT/tools/hd_gameplay_recipe.json" \
        --module "$STRATEGY_MODULE" --out "$HD_PACK"; then
      rm -rf "$HD_PACK"
      printf 'HD pack generation failed; launching with original artwork. Generation will retry next launch.\n' >&2
      return 0
    fi
  fi
  export REB_HD_ASSET_DIR="$HD_PACK"
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
prepare_hd_pack
printf 'Launching Rebellion\nConfig: %s\n' "$CONFIG"
if [[ -n "${REB_HD_ASSET_DIR:-}" ]]; then
  printf 'HD asset fallback: %s\n' "$REB_HD_ASSET_DIR"
fi
# The game checks install_dir, resource files and SaveGame before starting.
exec "$EXECUTABLE"
