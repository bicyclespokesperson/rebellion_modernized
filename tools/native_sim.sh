#!/usr/bin/env bash
# The headless simulation corpus on the native executable: each entry loads one of the install's shipped saves and runs N
# ticks under REB_SIM with the virtual clocks (REB_DETERMINISTIC=1), saving every k ticks. With -c TAG every save of a
# run is compared byte for byte with the same save of an earlier run tagged TAG (e.g. an arm64 run against the x86-64 one,
# or two builds); tools/save_diff.py summarises each save that differs. Runs go to work/run_native/sim/<name>_<tag>/.
#   tools/native_sim.sh [-x EXE] [-k TAG] [-c TAG] [-a arm64] [name...]
# -a arm64 runs build/native_arm64/rebellion (tools/build_native.sh --arch arm64) under qemu-user with SDL's dummy video driver
# (needs a container image with qemu-user: docker/Dockerfile.native; REB_NATIVE_IMAGE selects the image).
set -uo pipefail
source "$(dirname "$0")/paths.sh"
exe="$ROOT/build/native/rebellion"; tag=native; compare=""; runner=""; display_setup="Xvfb :79 -screen 0 640x480x24 >/dev/null 2>&1 & export DISPLAY=:79; sleep 1;"
while getopts "x:k:c:a:" opt; do
  case "$opt" in
    x) exe="$(realpath "$OPTARG")" ;;
    k) tag="$OPTARG" ;;
    c) compare="$OPTARG" ;;
    a) [[ "$OPTARG" == arm64 ]] || { echo "unknown -a $OPTARG (arm64)" >&2; exit 2; }
       exe="$ROOT/build/native_arm64/rebellion"; tag=arm64; runner="qemu-aarch64 -L /usr/aarch64-linux-gnu"
       display_setup="export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy;" ;;
    *) exit 2 ;;
  esac
done
shift $((OPTIND - 1))
want=("$@")
# name | save file in the install's SaveGame directory | ticks | checkpoint interval (ticks)
#   battle1: Empire game, auto-resolved space battle at about tick +2250; alliance1, empire1: AI activity only
corpus=(
  "battle1|SAVEGAME.001|3000|500"
  "alliance1|SAVEGAME.002|2000|500"
  "empire1|SAVEGAME.004|2000|500"
)
saves="$MAIN/orig_game_files/Star Wars - Rebellion/SaveGame"
base="$ROOT/work/run_native/sim"
mkdir -p "$base"
status=0
for corpus_entry in "${corpus[@]}"; do
  IFS='|' read -r name file ticks every <<<"$corpus_entry"
  if [[ ${#want[@]} -gt 0 ]] && [[ ! " ${want[*]} " =~ " $name " ]]; then continue; fi
  out="$base/${name}_$tag"
  rm -rf "$out"; mkdir -p "$out/game"
  for entry in "$MAIN/orig_game_files/Star Wars - Rebellion"/*; do
    [[ "$(basename "$entry")" == SaveGame ]] || ln -s "$entry" "$out/game/$(basename "$entry")"
  done
  mkdir -p "$out/game/SaveGame"
  printf 'install_dir=%s\n' "$out/game" > "$out/config.ini"
  cp "$saves/$file" "$out/in.sav"
  spec="load:$out/in.sav,ticks:$ticks,save:$out/sim.sav,every:$every"
  REB_DOCKER_ARGS="-e REBELLION_CONFIG=$out/config.ini -e HOME=$out -e REB_DETERMINISTIC=1 -e REB_SIM=$spec" "$ROOT/tools/container/native_run.sh" \
    "$display_setup cd '$out/game' && SDL_RENDER_DRIVER=software timeout \${REB_SIM_TIMEOUT:-1500} $runner '$exe' > '$out/game.log' 2>&1; echo exit \$? >> '$out/game.log'" || true
  if ! grep -q "^done" "$out/sim.sav.log" 2>/dev/null; then echo "$name: did not finish (see $out/game.log, $out/sim.sav.log)"; status=1; continue; fi
  if [[ -z "$compare" ]]; then echo "$name: finished"; continue; fi
  ref="$base/${name}_$compare"
  if [[ ! -d "$ref" ]]; then echo "$name: no run tagged $compare to compare with ($ref)"; status=1; continue; fi
  same=0; diffs=0
  for path in "$out"/sim.sav "$out"/sim.sav.t*; do
    [[ -f "$path" ]] || continue
    file_name="$(basename "$path")"
    if cmp -s "$path" "$ref/$file_name"; then same=$((same + 1)); else
      diffs=$((diffs + 1))
      if [[ -f "$ref/$file_name" ]]; then echo "  $name/$file_name: $(python3 "$ROOT/tools/save_diff.py" "$ref/$file_name" "$path" | head -2 | tr '\n' ' ')"; else echo "  $name/$file_name: missing in $ref"; fi
    fi
  done
  echo "$name: $same of $((same + diffs)) saves identical to the $compare run"
  (( diffs == 0 )) || status=1
done
exit $status
