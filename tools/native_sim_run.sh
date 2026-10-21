#!/usr/bin/env bash
# One headless simulation run of the native executable (REB_SIM, src/port/sim_harness.cpp).
#   tools/native_sim_run.sh [-x EXE] <loadfile> <ticks> <outdir> [every]
# REB_DETERMINISTIC=1 (virtual clocks). Output: <outdir>/sim.sav, sim.sav.t<tick>, sim.sav.log, game.log. Extra environment: REB_SIM_ENV
# (space separated VAR=VAL pairs without spaces, e.g. "ASAN_OPTIONS=detect_leaks=0"); REB_SIM_PREFIX: a command to run the executable under
# (e.g. "valgrind --track-origins=yes --log-file=<dir>/vg.log").
set -uo pipefail
source "$(dirname "$0")/paths.sh"
exe="$ROOT/build/native/rebellion"
while getopts "x:" opt; do case "$opt" in x) exe="$(realpath "$OPTARG")" ;; *) exit 2 ;; esac; done
shift $((OPTIND - 1))
load="$(realpath "$1")"; ticks="$2"; out="$(realpath -m "$3")"; every="${4:-}"
rm -rf "$out"; mkdir -p "$out/game/SaveGame"
for entry in "$MAIN/orig_game_files/Star Wars - Rebellion"/*; do
  [[ "$(basename "$entry")" == SaveGame ]] || ln -s "$entry" "$out/game/$(basename "$entry")"
done
printf 'install_dir=%s\n' "$out/game" > "$out/config.ini"
cp "$load" "$out/in.sav"
spec="load:$out/in.sav,ticks:$ticks,save:$out/sim.sav"
[[ -n "$every" ]] && spec+=",every:$every"
extra=""
for kv in ${REB_SIM_ENV:-}; do extra+=" -e $kv"; done
REB_DOCKER_ARGS="-e REBELLION_CONFIG=$out/config.ini -e HOME=$out -e REB_DETERMINISTIC=1 -e REB_SIM=$spec$extra" "$ROOT/tools/container/native_run.sh" \
  "Xvfb :79 -screen 0 640x480x24 >/dev/null 2>&1 & export DISPLAY=:79; sleep 1; cd '$out/game' && SDL_RENDER_DRIVER=software timeout \${REB_SIM_TIMEOUT:-3000} ${REB_SIM_PREFIX:-} '$exe' > '$out/game.log' 2>&1; echo exit \$? >> '$out/game.log'"
tail -2 "$out/sim.sav.log" 2>/dev/null
