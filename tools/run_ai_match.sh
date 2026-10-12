#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=""
LOAD=""
GALAXY=1
SEED=1
TICKS=100000
EVERY=500
VICTORY=0
DIFFICULTY=7
AI1=""
AI2=""
EXE=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --out) OUT=$2; shift 2 ;;
    --load) LOAD=$2; shift 2 ;;
    --galaxy) GALAXY=$2; shift 2 ;;
    --seed) SEED=$2; shift 2 ;;
    --ticks) TICKS=$2; shift 2 ;;
    --every) EVERY=$2; shift 2 ;;
    --victory) VICTORY=$2; shift 2 ;;
    --difficulty) DIFFICULTY=$2; shift 2 ;;
    --ai1) AI1=$2; shift 2 ;;
    --ai2) AI2=$2; shift 2 ;;
    --exe) EXE=$2; shift 2 ;;
    *) echo "Unknown option: $1" >&2; exit 2 ;;
  esac
done

OUT=${OUT:-$ROOT/work/ai_match/match.sav}
mkdir -p "$(dirname "$OUT")"
OUT=$(cd "$(dirname "$OUT")" && pwd)/$(basename "$OUT")
if [[ -n "$LOAD" ]]; then
  LOAD=$(cd "$(dirname "$LOAD")" && pwd)/$(basename "$LOAD")
  SOURCE="load:$LOAD"
else
  SOURCE="new:$GALAXY,seed:$SEED,victory:$VICTORY,difficulty:$DIFFICULTY"
fi

if [[ -z "$EXE" ]]; then
  if [[ $(uname -s) == Darwin ]]; then
    EXE="$ROOT/build/native/Rebellion.app/Contents/MacOS/Rebellion"
  else
    EXE="$ROOT/build/native/rebellion"
  fi
fi
if [[ ! -x "$EXE" ]]; then
  echo "Game executable not found: $EXE" >&2
  exit 2
fi

if [[ -z "${REBELLION_CONFIG:-}" ]]; then
  REBELLION_CONFIG="$(dirname "$OUT")/ai_match_config.ini"
  printf 'install_dir=%s\nfont_path=%s\n' \
    "$ROOT/orig_game_files/Star Wars - Rebellion" \
    "$ROOT/third_party/liberation-fonts-ttf-2.1.5/LiberationSans-Regular.ttf" > "$REBELLION_CONFIG"
  export REBELLION_CONFIG
fi

export REB_DETERMINISTIC=1
AI_OPTIONS=""
[[ -z "$AI1" ]] || AI_OPTIONS+=",ai1:$AI1"
[[ -z "$AI2" ]] || AI_OPTIONS+=",ai2:$AI2"
export REB_SIM="$SOURCE,ticks:$TICKS,until:win$AI_OPTIONS,every:$EVERY,save:$OUT"
rm -f "$OUT.result.json"
set +e
"$EXE"
STATUS=$?
set -e
if [[ -f "$OUT.log" ]]; then
  echo "Match log: $OUT.log"
fi
if [[ -f "$OUT.result.json" ]]; then
  echo "Match result: $OUT.result.json"
else
  echo "Match ended without a result file (status $STATUS)" >&2
fi
exit "$STATUS"
