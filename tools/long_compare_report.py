#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow"]
# ///
"""Report of tools/long_compare.sh: pairwise comparison of the saves and screenshots of every run in a directory.

  tools/long_compare_report.py work/run/long/alliance [--save SAVEGAME.005]

Run directories are <config>_<i>. The save matrix shows, per pair, "=" for byte-identical files, "~N" for N differing bytes at
equal size (a handful: UI state in the save such as message timestamps; thousands: the simulations diverged), "size" for
different sizes. Exit status 1 when any pair of runs differs by more than --noise bytes.
"""
from __future__ import annotations

import argparse
from itertools import combinations
from pathlib import Path

from PIL import Image, ImageChops


def differing_bytes(a: bytes, b: bytes) -> int | None:
    if len(a) != len(b):
        return None
    return sum(x != y for x, y in zip(a, b))


def differing_pixels(a: Path, b: Path) -> int | None:
    first, second = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    if first.size != second.size:
        return None
    diff = ImageChops.difference(first, second).convert("L").point(lambda v: 255 if v else 0)
    return diff.histogram()[255]


def verdict(count: int | None) -> str:
    return "size" if count is None else "=" if count == 0 else f"~{count}"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("directory", type=Path)
    ap.add_argument("--save", default="SAVEGAME.005")
    ap.add_argument("--noise", type=int, default=0, help="differing bytes tolerated for the exit status (default 0)")
    args = ap.parse_args()
    runs = sorted(p for p in args.directory.iterdir() if (p / "SaveGame" / args.save).exists())
    if not runs:
        print("no run wrote", args.save)
        return 1
    saves = {p.name: (p / "SaveGame" / args.save).read_bytes() for p in runs}
    names = list(saves)
    width = max(len(n) for n in names) + 1
    print(f"== {args.save}: sizes ==")
    for n in names:
        print(f"  {n:<{width}} {len(saves[n])}")
    print(f"\n== {args.save}: pairwise (= identical, ~N bytes differ at equal size, size = different sizes) ==")
    print(" " * (width + 2) + "".join(f"{n:>{width + 2}}" for n in names))
    worst = 0
    for a in names:
        row = []
        for b in names:
            count = 0 if a == b else differing_bytes(saves[a], saves[b])
            row.append(f"{verdict(count):>{width + 2}}")
            if a != b and (count is None or count > args.noise):
                worst = max(worst, 1 << 30 if count is None else count)
        print(f"  {a:<{width}}" + "".join(row))

    reference = runs[0]
    print(f"\n== screenshots, differing pixels against {reference.name} (only differences listed) ==")
    for run in runs[1:]:
        lines = []
        for shot in sorted(reference.glob("*.png")):
            other = run / shot.name
            if not other.exists():
                lines.append(f"    {shot.name}: missing")
                continue
            count = differing_pixels(shot, other)
            if count != 0:
                lines.append(f"    {shot.name}: {'size differs' if count is None else count}")
        print(f"  {run.name}: " + ("identical" if not lines else ""))
        print("\n".join(lines))
    return 1 if worst else 0


if __name__ == "__main__":
    raise SystemExit(main())
