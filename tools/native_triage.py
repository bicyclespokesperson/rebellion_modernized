#!/usr/bin/env python3
"""Summarise the sanitizer reports in a native run's game.log: one line per distinct location, with counts.

  tools/native_triage.py work/run_native/<scenario>_asan/game.log [--all]
"""
from __future__ import annotations

import argparse
import re
from collections import Counter
from pathlib import Path

ROOT = str(Path(__file__).resolve().parent.parent) + "/"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("log", type=Path)
    ap.add_argument("--all", action="store_true", help="also list the full ASan stack of the first report of each kind")
    args = ap.parse_args()
    text = args.log.read_text(errors="replace").replace(ROOT, "")
    ubsan: Counter[str] = Counter()
    for m in re.finditer(r"^(\S+?:\d+:\d+): runtime error: (.*)$", text, re.M):
        message = re.sub(r"0x[0-9a-f]+", "ADDR", m.group(2))[:170]
        ubsan[f"{m.group(1)}  {message}"] += 1
    asan: Counter[str] = Counter()
    for m in re.finditer(r"ERROR: AddressSanitizer: (\S+).*?\n(?:.*?\n)*?\s+#0 \S+ in (.*?) (\S+:\d+)", text):
        asan[f"{m.group(1)}  {m.group(2)[:90]}  {m.group(3)}"] += 1
    print(f"UBSan: {len(ubsan)} distinct, {sum(ubsan.values())} reports")
    for k, n in ubsan.most_common():
        print(f"  {n:5d}  {k}")
    print(f"ASan: {len(asan)} distinct, {sum(asan.values())} reports")
    for k, n in asan.most_common():
        print(f"  {n:5d}  {k}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
