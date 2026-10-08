#!/usr/bin/env python3
"""Decompilation progress: reconstructed game functions/bytes overall and per module.

Reads symbols/functions.csv (all functions), symbols/modules.csv (module ranges, optional) and the hook
report of the last hybrid build (build/hybrid/gen/report.json).
"""
from __future__ import annotations

import argparse
import bisect
import csv
import json
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def load_modules(path: Path) -> list[tuple[int, int, str, str]]:
    if not path.exists():
        return []
    with open(path, newline="") as fh:
        return sorted((int(r["start"], 16), int(r["end"], 16), r["module"], r.get("subsystem", ""))
                      for r in csv.DictReader(fh))


def symbol_names(path: Path) -> dict[int, str]:
    with open(path, newline="") as fh:
        return {int(r["address"], 16): r["name"] for r in csv.DictReader(fh) if r["kind"] == "func"}


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--report", type=Path, default=ROOT / "build" / "hybrid" / "gen" / "report.json")
    ap.add_argument("--by", choices=["module", "subsystem"], default="subsystem")
    ap.add_argument("--all", action="store_true", help="also list modules with no progress")
    ap.add_argument("--missing", metavar="GROUP", help="list the functions of this module/subsystem (per --by) that "
                    "are not reconstructed yet: address, size, symbol name")
    args = ap.parse_args()

    done = {int(a, 16) for a in json.loads(args.report.read_text())["reimplemented"]} if args.report.exists() else set()
    modules = load_modules(ROOT / "symbols" / "modules.csv")
    starts = [m[0] for m in modules]
    totals: dict[str, list[int]] = defaultdict(lambda: [0, 0, 0, 0])  # funcs, bytes, done funcs, done bytes
    names = symbol_names(ROOT / "symbols" / "symbols.csv") if args.missing else {}
    with open(ROOT / "symbols" / "functions.csv", newline="") as fh:
        for row in csv.DictReader(fh):
            if row["region"] != "game":
                continue
            a, size = int(row["address"], 16), int(row["size"])
            i = bisect.bisect_right(starts, a) - 1
            group = "(unmapped)"
            if i >= 0 and a < modules[i][1]:
                group = modules[i][2] if args.by == "module" else (modules[i][3] or modules[i][2])
            if group == args.missing and a not in done:
                print(f"{a:#010x} {size:6} {names.get(a, '')}")
            for key in (group, "TOTAL"):
                t = totals[key]
                t[0] += 1
                t[1] += size
                if a in done:
                    t[2] += 1
                    t[3] += size
    if args.missing:
        return 0
    print(f"{'':32} {'functions':>16} {'bytes':>22}")
    for key in sorted(totals, key=lambda k: (k == "TOTAL", -totals[k][1])):
        f, b, df, db = totals[key]
        if not args.all and df == 0 and key != "TOTAL":
            continue
        print(f"{key[:32]:32} {df:6}/{f:<6} {100*df/f:5.1f}% {db:9}/{b:<9} {100*db/b:5.1f}%")
    return 0


if __name__ == "__main__":
    sys.exit(main())
