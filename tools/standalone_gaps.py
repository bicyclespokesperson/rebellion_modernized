#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# ///
"""Phase 2 gap report: what the reconstructed code still references in the original EXE.

Reads the "gaps" section that tools/gen_link_symbols.py writes into build/hybrid/gen/report.json.
Without arguments prints the summary table; `--list CATEGORY` lists the entries of one category.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

DESCRIPTIONS = {
    "game_function": "unreconstructed non-tactical game functions",
    "tactical_function": "unreconstructed tactical functions",
    "stale_declaration": "reconstructed functions still reached through FUN_/stale declarations",
    "alias_standin": "aliases.csv stand-in names for original addresses",
    "win32_import": "Win32 imports (game IAT slots)",
    "libci": "LIBCI iostream functions",
    "crt": "CRT / EH / C++ runtime functions",
    "data_bss": "globals in .bss",
    "data_data": "globals in .data",
    "data_rdata": "globals in .rdata",
    "unresolved_other": "unresolved externals (import libs, compiler intrinsics)",
}


def load_gaps(path: Path) -> dict:
    report = json.loads(path.read_text())
    if "gaps" not in report:
        sys.exit(f"{path} has no gap data; rebuild with tools/build_hybrid.sh")
    return report["gaps"]


def unique_addresses(entries: list[dict]) -> int:
    return len({e["address"] for e in entries if "address" in e})


def print_summary(gaps: dict) -> None:
    categories = gaps["categories"]
    print(f"{'category':20} {'symbols':>8} {'addresses':>10} {'bytes':>9}  description")
    for name, entries in categories.items():
        size = sum(e.get("size", 0) for e in {e["address"]: e for e in entries if "size" in e}.values())
        print(f"{name:20} {len(entries):8} {unique_addresses(entries):10} {size or '':>9}  {DESCRIPTIONS[name]}")
    print(f"{'multi_typed_globals':20} {len(gaps['multi_typed_globals']):8} {'':10} {'':>9}  "
          "data addresses declared under several types (also counted above)")
    total = sum(len(e) for e in categories.values())
    print(f"{'TOTAL':20} {total:8}")


def print_category(gaps: dict, name: str) -> None:
    if name == "multi_typed_globals":
        for e in gaps["multi_typed_globals"]:
            print(e["address"], " | ".join(e["symbols"]))
        return
    for e in gaps["categories"][name]:
        size = f" {e['size']:6}" if "size" in e else ""
        print(f"{e.get('address', ''):10}{size} {e.get('name', '')}  [{e['symbol']}]")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--report", type=Path, default=ROOT / "build" / "hybrid" / "gen" / "report.json")
    ap.add_argument("--list", metavar="CATEGORY", help="list one category instead of the summary table")
    args = ap.parse_args()
    gaps = load_gaps(args.report)
    valid = [*gaps["categories"], "multi_typed_globals"]
    if args.list is None:
        print_summary(gaps)
    elif args.list in valid:
        print_category(gaps, args.list)
    else:
        sys.exit(f"unknown category {args.list!r}; choose from: {', '.join(valid)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
