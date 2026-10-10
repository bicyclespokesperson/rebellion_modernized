#!/usr/bin/env bash
# The simulation goldens (tests/goldens/sim_goldens.txt, tools/sim_goldens.sh) on the native executable: REB_SIM runs with the virtual
# clocks (REB_DETERMINISTIC=1) and a comparison of the saves' sha256 with the recorded i686 hashes. A native build uses the host's
# floating point, so a difference is expected to be float drift (docs/native_linux.md); the script reports the size and the number of
# differing bytes of each save that differs from the reference run in work/run/sim_goldens/<name>_base_1/ (when that exists).
#   tools/native_sim.sh [-x EXE] [-k TAG] [-a arm64] [name...]
# -a arm64 runs build/native_arm64/rebellion (tools/build_native.sh --arch arm64) under qemu-user with SDL's dummy video driver
# (needs a container image with qemu-user: docker/Dockerfile.native; REB_NATIVE_IMAGE selects the image).
set -uo pipefail
source "$(dirname "$0")/paths.sh"
exe="$ROOT/build/native/rebellion"; tag=native; runner=""; display_setup="Xvfb :79 -screen 0 640x480x24 >/dev/null 2>&1 & export DISPLAY=:79; sleep 1;"
while getopts "x:k:a:" opt; do
  case "$opt" in
    x) exe="$(realpath "$OPTARG")" ;;
    k) tag="$OPTARG" ;;
    a) [[ "$OPTARG" == arm64 ]] || { echo "unknown -a $OPTARG (arm64)" >&2; exit 2; }
       exe="$ROOT/build/native_arm64/rebellion"; tag=arm64; runner="qemu-aarch64 -L /usr/aarch64-linux-gnu"
       display_setup="export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy;" ;;
    *) exit 2 ;;
  esac
done
shift $((OPTIND - 1))
want=("$@")
saves="$MAIN/orig_game_files/Star Wars - Rebellion/SaveGame"
base="$ROOT/work/run_native/sim"
mkdir -p "$base"
status=0
while IFS='|' read -r name file ticks every; do
  name="$(echo "$name" | xargs)"; file="$(echo "$file" | xargs)"; ticks="$(echo "$ticks" | xargs)"; every="$(echo "$every" | xargs)"
  [[ -z "$name" || "$name" == \#* ]] && continue
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
  same=0; diffs=0
  while read -r hash file_name; do
    [[ "$hash" == \#* || -z "$hash" ]] && continue
    if [[ "$file_name" == sim.sav.log ]]; then
      # the log's first line names the loaded file (a host path here, Z:\in\load.sav in the golden run): compare the rest
      ref_log="$ROOT/work/run/sim_goldens/${name}_base_1/sim.sav.log"
      if [[ -f "$ref_log" ]] && diff <(tail -n +2 "$out/sim.sav.log" | sed 's|-> .*||') <(tr -d '\r' < "$ref_log" | tail -n +2 | sed 's|-> .*||') > /dev/null; then same=$((same + 1)); continue; fi
    fi
    mine="$(cd "$out" && sha256sum "$file_name" 2>/dev/null | cut -d' ' -f1)"
    if [[ "$mine" == "$hash" ]]; then same=$((same + 1)); else
      diffs=$((diffs + 1))
      ref="$ROOT/work/run/sim_goldens/${name}_base_1/$file_name"
      if [[ -f "$ref" && -f "$out/$file_name" ]]; then echo "  $name/$file_name: $(python3 "$ROOT/tools/save_diff.py" "$ref" "$out/$file_name" | head -2 | tr '\n' ' ')"; else echo "  $name/$file_name differs (no reference save to compare)"; fi
    fi
  done < "$ROOT/tests/goldens/$name.sha256"
  echo "$name: $same of $((same + diffs)) files identical to the i686 goldens"
  (( diffs == 0 )) || status=1
done < <(grep -v "^#" "$ROOT/tests/goldens/sim_goldens.txt" | grep -v "^$")
exit $status
