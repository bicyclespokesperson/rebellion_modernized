#!/usr/bin/env bash
# Run the native executable (build/native/rebellion) on a scenario of tests/scenarios in the rebellion-native container
# (Xvfb + xdotool, see tools/container/native_driver.sh). Output (screenshots, game.log, driver.log, the SaveGame directory the run
# used) goes to work/run_native/<scenario>/.
#   tools/run_native.sh [--out DIR] [--exe PATH] <scenario.txt>
# The game directory is a tree of symbolic links to the install (the original game files in the primary checkout) with its own copy of
# SaveGame, so runs cannot disturb each other. Environment passed on: REB_* (e.g. REB_MINWIN_SCALE, REB_MINWIN_TRACE, REB_MSG_TRACE),
# ASAN_OPTIONS, UBSAN_OPTIONS, REB_GAME_ARGS (command line of the game).
set -euo pipefail
source "$(dirname "$0")/paths.sh"
out=""; exe="$ROOT/${REB_NATIVE_DIR:-build/native}/rebellion"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --out) out="$(realpath -m "$2")"; shift 2 ;;
    --exe) exe="$(realpath "$2")"; shift 2 ;;
    *) break ;;
  esac
done
scenario="$(realpath "$1")"
out="${out:-$ROOT/work/run_native/$(basename "$scenario" .txt)}"
install="$MAIN/orig_game_files/Star Wars - Rebellion"
rm -rf "$out"; mkdir -p "$out/game"
for entry in "$install"/*; do
  name="$(basename "$entry")"
  [[ "$name" == SaveGame ]] && continue
  ln -s "$entry" "$out/game/$name"
done
cp -r "$install/SaveGame" "$out/game/SaveGame"
[[ -n "${REB_SAVE_OVERLAY:-}" ]] && cp -r "$REB_SAVE_OVERLAY"/. "$out/game/SaveGame/"
printf 'install_dir=%s\nfont_path=%s\n' "$out/game" "$MAIN/third_party/liberation-fonts-ttf-2.1.5/LiberationSans-Regular.ttf" > "$out/config.ini"
env_args="-e REBELLION_CONFIG=$out/config.ini -e HOME=$out"
for v in $(compgen -e | grep -E '^(REB_|ASAN_OPTIONS|UBSAN_OPTIONS|SDL_)'); do env_args+=" -e $v"; done
REB_DOCKER_ARGS="$env_args" "$ROOT/tools/container/native_run.sh" "tools/container/native_driver.sh '$scenario' '$exe' '$out'"
echo "output in $out"
cat "$out/driver.log" 2>/dev/null || true
