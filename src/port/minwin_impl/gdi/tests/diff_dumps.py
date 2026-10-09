#!/usr/bin/env python3
"""Compare surface dumps (gdi_ops format). The first file is the reference; prints, per scenario, whether each other file matches
and for a mismatch the number of differing pixels and an ASCII picture of the first differences (reference/other)."""
import argparse
from pathlib import Path


def read(path: Path) -> dict[str, tuple[int, int, list[list[int]]]]:
    tokens = path.read_text().split()
    out: dict[str, tuple[int, int, list[list[int]]]] = {}
    i = 0
    while i < len(tokens):
        name, w, h = tokens[i], int(tokens[i + 1]), int(tokens[i + 2])
        rows = [[int(tokens[i + 3 + y][2 * x : 2 * x + 2], 16) for x in range(w)] for y in range(h)]
        out[name] = (w, h, rows)
        i += 3 + h
    return out


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("reference", type=Path)
    parser.add_argument("others", type=Path, nargs="+")
    parser.add_argument("--picture", action="store_true", help="draw differing pixels")
    args = parser.parse_args()
    ref = read(args.reference)
    failures = 0
    for other_path in args.others:
        other = read(other_path)
        for name, (w, h, rows) in ref.items():
            if name not in other:
                print(f"MISSING {other_path.name}: {name}")
                failures += 1
                continue
            rows2 = other[name][2]
            diffs = [(x, y) for y in range(h) for x in range(w) if rows[y][x] != rows2[y][x]]
            if not diffs:
                print(f"match   {other_path.name:12} {name}")
                continue
            failures += 1
            print(f"DIFFER  {other_path.name:12} {name}: {len(diffs)} of {w * h} pixels, first {diffs[:6]}")
            for x, y in diffs[:4]:
                print(f"          ({x},{y}) reference {rows[y][x]:02x} other {rows2[y][x]:02x}")
            if args.picture:
                for y in range(h):
                    print("".join("#" if rows[y][x] != rows2[y][x] else "." for x in range(w)))
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
