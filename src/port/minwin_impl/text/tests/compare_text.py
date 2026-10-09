#!/usr/bin/env python3
"""Compare text_dump outputs: <dir>/real.{lines,pixels}.txt is the reference (real gdi32 under Wine); every other prefix given
is compared with it. Prints per kind of line the number of differing lines with examples, and per rendered surface the
number of differing pixels."""
import argparse
import re
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "gdi" / "tests"))
from diff_dumps import read  # noqa: E402


def kind(line: str) -> str:
    return line.split()[1]


def compare_lines(ref: list[str], other: list[str], verbose: int) -> int:
    by_kind: dict[str, list[tuple[str, str]]] = defaultdict(list)
    total: dict[str, int] = defaultdict(int)
    for a, b in zip(ref, other):
        total[kind(a)] += 1
        if a != b:
            by_kind[kind(a)].append((a, b))
    if len(ref) != len(other):
        print(f"  line count differs: {len(ref)} vs {len(other)}")
    bad = 0
    for k in sorted(total):
        diffs = by_kind.get(k, [])
        bad += len(diffs)
        print(f"  {k:16} {total[k] - len(diffs):5}/{total[k]:5} identical")
        for a, b in diffs[:verbose]:
            print(f"      ref:  {a[:200]}\n      ours: {b[:200]}")
    return bad


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory", type=Path)
    parser.add_argument("others", nargs="+")
    parser.add_argument("-v", "--verbose", type=int, default=2, help="examples per kind")
    args = parser.parse_args()
    ref_lines = (args.directory / "real.lines.txt").read_text().splitlines()
    ref_pixels = read(args.directory / "real.pixels.txt")
    failures = 0
    for name in args.others:
        print(f"== {name}")
        failures += compare_lines(ref_lines, (args.directory / f"{name}.lines.txt").read_text().splitlines(), args.verbose)
        other = read(args.directory / f"{name}.pixels.txt")
        for surface, (w, h, rows) in ref_pixels.items():
            if surface not in other:
                print(f"  MISSING {surface}")
                failures += 1
                continue
            rows2 = other[surface][2]
            n = sum(1 for y in range(h) for x in range(w) if rows[y][x] != rows2[y][x])
            if n:
                failures += 1
                print(f"  DIFFER  {surface}: {n} of {w * h} pixels")
            else:
                print(f"  match   {surface}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
