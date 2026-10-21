#!/usr/bin/env bash
# Compile (incrementally) and launch the native game. Run ./create_build.sh once first; see README.md.
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

native_path() {
  if [[ $PLATFORM == windows ]]; then cygpath -m "$1"; else printf '%s\n' "$1"; fi
}

usage() {
  cat <<EOF
Usage: $0 [--no-hd] [--no-build]

  --no-hd     use only the original artwork: no HD pack generation, HD icons or rendered battle sprites
  --no-build  launch the existing executable without compiling first

Set REBELLION_CONFIG to use a custom config.ini.
EOF
}

prepare_hd_pack() {
  HD_PACK="$ROOT/work/hd/gameplay-pack"
  [[ -z "${REB_HD_ASSET_DIR+x}" ]] || return 0
  if config_value hd_asset_dir > /dev/null; then
    return 0
  fi
  # uv, and so install_dir, is only needed to build something: the gameplay pack when it is missing, or the optional icons
  # when they are neither built nor declined.
  if [[ -d "$HD_PACK" ]] && { icons_built || [[ -z "${REB_HD_ICONS+x}" && -f "$ROOT/work/hd/.icons-declined" ]]; }; then
    export REB_HD_ASSET_DIR="$(native_path "$HD_PACK")"
    return 0
  fi
  if ! command -v uv > /dev/null 2>&1; then
    if [[ -d "$HD_PACK" ]]; then
      printf 'HD icons are not built: install uv (https://docs.astral.sh/uv/) to build them. Using the HD pack as it is.\n' >&2
      export REB_HD_ASSET_DIR="$(native_path "$HD_PACK")"
      return 0
    fi
    fail "uv is required to build the HD artwork but was not found on PATH.
Install it (https://docs.astral.sh/uv/), or pass --no-hd to play with the original artwork."
  fi
  INSTALL_DIR="$(config_value install_dir || true)"
  [[ -n "$INSTALL_DIR" ]] || fail "install_dir is not set in $CONFIG, so the HD artwork cannot be built.
Set it, or pass --no-hd to play with the original artwork."
  [[ -d "$INSTALL_DIR" ]] || fail "install_dir does not exist: $INSTALL_DIR
(from $CONFIG). Fix it, or pass --no-hd to play with the original artwork."
  if [[ ! -d "$HD_PACK" ]]; then
    STRATEGY_MODULE=""
    for MODULE in "$INSTALL_DIR"/[Ss][Tt][Rr][Aa][Tt][Ee][Gg][Yy].[Dd][Ll][Ll]; do
      if [[ -f "$MODULE" ]]; then
        STRATEGY_MODULE="$MODULE"
        break
      fi
    done
    [[ -n "$STRATEGY_MODULE" ]] || fail "STRATEGY.DLL was not found in $INSTALL_DIR, so the HD artwork cannot be built.
Check install_dir in $CONFIG, or pass --no-hd to play with the original artwork."
    printf 'Generating HD artwork from %s\n' "$STRATEGY_MODULE"
    if ! uv run "$ROOT/tools/build_hd_pack.py" --recipe "$ROOT/tools/hd_gameplay_recipe.json" \
        --module "$(native_path "$STRATEGY_MODULE")" --out "$(native_path "$HD_PACK")"; then
      rm -rf "$HD_PACK"
      printf 'HD pack generation failed; launching with original artwork. Generation will retry next launch.\n' >&2
      return 0
    fi
  fi
  prepare_hd_icons
  export REB_HD_ASSET_DIR="$(native_path "$HD_PACK")"
}

icons_built() {
  grep -q '"encyclopedia_extraction"' "$HD_PACK/manifest.json" 2> /dev/null
}

confirm_icon_download() {
  case "${REB_HD_ICONS:-}" in
    yes|y|1) return 0 ;;
    no|n|0) return 1 ;;
  esac
  if [[ ! -t 0 || ! -t 1 ]]; then
    printf 'HD ship, troop and facility icons are not built. Launch interactively, or set REB_HD_ICONS=yes, to build them.\n' >&2
    return 1
  fi
  cat >&2 << 'MESSAGE'

The HD ship, troop, facility and character icons have not been generated yet.

Building them cuts each encyclopedia picture out of its background with a local image-processing model (rembg isnet-general-use).
  - Download: about 170 MiB, once; the file is checksum-verified and cached. Nothing is uploaded anywhere.
  - Time: a few minutes on CPU. Later launches skip this step.
  - Declining is fine: the game runs with the original icons and you will not be asked again
    (delete work/hd/.icons-declined, or launch with REB_HD_ICONS=yes, to change your mind).

MESSAGE
  local answer
  read -r -p 'Download the model and generate the HD icons now? [Y/n] ' answer
  [[ -z "$answer" || "$answer" =~ ^[Yy] ]]
}

prepare_hd_icons() {
  icons_built && return 0
  [[ -z "${REB_HD_ICONS+x}" && -f "$ROOT/work/hd/.icons-declined" ]] && return 0
  local install_dir="$INSTALL_DIR"
  if ! confirm_icon_download; then
    [[ -z "${REB_HD_ICONS+x}" && -t 0 ]] && touch "$ROOT/work/hd/.icons-declined"
    return 0
  fi
  rm -f "$ROOT/work/hd/.icons-declined"
  printf 'Generating HD icons (the first run downloads the model)\n'
  uv run --locked --python 3.12 "$ROOT/tools/build_encyclopedia_icons.py" \
      --install-dir "$install_dir" --no-overrides --out "$(native_path "$HD_PACK")" ||
    printf 'HD icon generation failed; launching without them. It will be offered again next launch.\n' >&2
}

prepare_tactical_sprites() {
  [[ -z "${REB_TACTICAL_SPRITE_DIR+x}" ]] || return 0
  config_value tactical_sprite_dir > /dev/null && return 0
  [[ -f "$ROOT/work/hd/tactical/pack/manifest.txt" ]] && export REB_TACTICAL_SPRITE_DIR="$(native_path "$ROOT/work/hd/tactical/pack")"
  return 0
}

NO_HD=0
BUILD=1
while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-hd) NO_HD=1 ;;
    --no-build) BUILD=0 ;;
    -h|--help) usage; exit 0 ;;
    *) usage >&2; exit 2 ;;
  esac
  shift
done

BUILD_DIR="${REB_NATIVE_DIR:-$ROOT/build/native}"
case "$(uname -s)" in
  Darwin)
    PLATFORM=macos
    EXECUTABLE="$BUILD_DIR/Rebellion.app/Contents/MacOS/Rebellion"
    USER_CONFIG="${HOME:-}/Library/Application Support/Rebellion/config.ini"
    LOCAL_CONFIG="$ROOT/work/macos/config.ini"
    ;;
  Linux)
    PLATFORM=linux
    EXECUTABLE="$BUILD_DIR/rebellion"
    USER_CONFIG="${XDG_CONFIG_HOME:-${HOME:-}/.config}/rebellion/config.ini"
    LOCAL_CONFIG="$ROOT/work/launch/config.ini"
    ;;
  MINGW*|MSYS*|CYGWIN*)
    PLATFORM=windows
    EXECUTABLE="$BUILD_DIR/rebellion.exe"
    USER_CONFIG=""
    [[ -n "${APPDATA:-}" ]] && USER_CONFIG="$(cygpath -u "$APPDATA")/Rebellion/config.ini"
    LOCAL_CONFIG="$ROOT/work/launch/config.ini"
    ;;
  *) fail "unsupported platform $(uname -s); see README.md." ;;
esac

if [[ $BUILD == 1 ]]; then
  [[ -f "$BUILD_DIR/CMakeCache.txt" ]] || fail "the build is not configured yet. From the repo root, run:
  ./create_build.sh
See README.md for prerequisites."
  cmake --build "$BUILD_DIR" --target rebellion || fail "the build failed; see the errors above."
fi
[[ -f "$EXECUTABLE" ]] || fail "the game has not been built:
  $EXECUTABLE
Run ./create_build.sh, then ./launch_game.sh."

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
  printf 'install_dir=%s\n' "$(native_path "$INSTALL_DIR")" > "$CONFIG" || fail "could not write $CONFIG"
  printf 'Created configuration: %s\n' "$CONFIG"
fi

[[ -f "$CONFIG" && -r "$CONFIG" ]] || fail "the configuration file is missing or unreadable:
  $CONFIG
Create a plain text file containing install_dir=/absolute/path/to/Star Wars - Rebellion."
export REBELLION_CONFIG="$(native_path "$CONFIG")"
if [[ $NO_HD == 1 ]]; then
  export REB_NO_HD=1
else
  prepare_hd_pack
  prepare_tactical_sprites
fi
printf 'Launching Rebellion\nConfig: %s\n' "$CONFIG"
if [[ $NO_HD == 1 ]]; then
  printf 'HD artwork: off\n'
elif [[ -n "${REB_HD_ASSET_DIR:-}" ]]; then
  printf 'HD asset fallback: %s\n' "$REB_HD_ASSET_DIR"
fi
# The game checks install_dir, resource files and SaveGame before starting.
exec "$EXECUTABLE"
