#!/usr/bin/env bash
# Checks of the minwin headers (src/port/minwin) against the real Windows SDK, all in the dev container (docs/phase3_plan.md P0c):
#   1. layout_expected.inc is what tools/minwin_layout_gen.py measures on the SDK now (nothing stale),
#   2. layout_check.cpp (sizes, offsets, constant values and types) compiles for i686, constants also for x86-64 Linux,
#   3. tools/minwin_compare.py: every function, typedef and record has the SDK's signature / members,
#   4. every TU of src/game compiles against minwin (tools/minwin_syntax.sh i686),
#   5. with --objects: the game objects built with and without REB_MINWIN are identical (tools/minwin_objdiff.sh).
# Prints one verdict line, "MINWIN OK" or "MINWIN FAIL: ...". Exit status 1 on failure.
set -uo pipefail
source "$(dirname "$0")/paths.sh"
cd "$ROOT"
fail() { echo "MINWIN FAIL: $*"; exit 1; }
log="$ROOT/build/minwin/check.log"; mkdir -p "$ROOT/build/minwin"

tools/minwin_layout_gen.py --out build/minwin/layout_expected.fresh.inc > "$log" 2>&1 || fail "layout generator (see $log)"
cmp -s build/minwin/layout_expected.fresh.inc src/port/minwin/layout_expected.inc || fail "layout_expected.inc is stale: rerun tools/minwin_layout_gen.py"

tools/minwin_cc.sh -fsyntax-only -DREB_MINWIN=1 -Isrc/game -Isrc src/port/minwin/layout_check.cpp >> "$log" 2>&1 || fail "layout_check.cpp on i686 (see $log)"
REB_CC=clang++ tools/minwin_cc.sh --target=x86_64-linux-gnu -std=c++23 -fsyntax-only -fms-extensions -Wno-everything -DREB_MINWIN=1 -Isrc/game -Isrc \
  src/port/minwin/layout_check.cpp >> "$log" 2>&1 || fail "layout_check.cpp on x86-64 (see $log)"

tools/minwin_compare.py >> "$log" 2>&1 || fail "signature differences (see $log)"
tools/minwin_syntax.sh i686 > build/minwin/syntax_i686.txt 2>&1
grep -q " 0 of .* TUs with errors" build/minwin/syntax_i686.txt || fail "game TUs do not compile (see build/minwin/syntax_i686.txt)"
if [[ "${1:-}" == --objects ]]; then
  tools/minwin_objdiff.sh > build/minwin/objdiff.txt 2>&1
  grep -q "different: 0" build/minwin/objdiff.txt || fail "objects differ (see build/minwin/objdiff.txt)"
fi
echo "MINWIN OK: layouts, constants, signatures and all game TUs match the SDK"
