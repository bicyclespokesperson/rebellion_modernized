#!/usr/bin/env bash
# Integrate an agent branch into the main checkout and verify it:
#   merge -> normalize symbols -> sync Ghidra -> build -> store its captures -> difftest all -> smoke run.
# Usage: tools/integrate.sh <branch> [worktree path holding the branch's work/run captures]
#        tools/integrate.sh --verify <branch> [worktree]   # after fixing a failed run by hand: skip the merge
set -euo pipefail
source "$(dirname "$0")/paths.sh"
cd "$MAIN"
verify_only=0
if [[ "$1" == --verify ]]; then verify_only=1; shift; fi
branch="$1"; worktree="${2:-}"
(( verify_only )) || git merge --no-ff --no-edit "$branch"
if ! tools/sym.py normalize; then
  echo "symbol name conflicts: resolve in symbols/symbols.csv, then run: tools/integrate.sh --verify $branch $worktree"; exit 1
fi
git diff --quiet symbols/symbols.csv || git commit -qm "Normalize symbols.csv after merging $branch" symbols/symbols.csv
tools/gq -f tools/py/apply_symbols.py && tools/gq --save
tools/build_hybrid.sh | tail -3
if [[ -n "$worktree" && -d "$worktree/work" ]]; then
  find "$worktree/work" -name '*.cap' -printf '%h\n' | sort -u | while read -r d; do
    tools/capture_store.py add "$d" --origin "$(basename "$(dirname "$d")")"
  done
fi
echo "== calling conventions"; tools/check_callee_pop.py | tail -20 || true
echo "== difftest (unit)"; tools/difftest.py -j "$(nproc)" $(tools/capture_store.py paths) | grep -v '^PASS' || true
echo "== difftest (integration)"; tools/difftest.py -j "$(nproc)" --integration $(tools/capture_store.py paths) | grep -v '^PASS' || true
echo "== soak (crashes that only show in long runs; bisect with tools/bisect_crash.py)"
tools/run_game.sh --out work/run/soak tests/scenarios/soak_fast.txt >/dev/null
if grep -q Unhandled work/run/soak/wine.log; then echo "SOAK CRASH: $(grep -m1 Unhandled work/run/soak/wine.log)"; else echo "soak ok"; fi
echo "== smoke"; tools/run_game.sh tests/scenarios/smoke.txt
cat work/run/smoke/rebhybrid.log
tools/progress.py
