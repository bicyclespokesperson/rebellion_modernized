#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow"]
# ///
"""Differential run comparison (M6): two output directories of tools/run_game.sh, e.g. the hybrid's and the standalone's.

Per screenshot: the number of differing pixels (0 = pixel-identical). Per save file written by the run (SaveGame/SAVEGAME.* or
NETGAME.* that differ from the original saves): size and the number and offsets of differing bytes.

  tools/compare_runs.py work/run/hyb_save_game_slot work/run/save_game_slot
"""
from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageChops


def differing_pixels(a: Path, b: Path) -> int | None:
    first, second = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    if first.size != second.size:
        return None
    difference = ImageChops.difference(first, second).convert("L").point(lambda v: 255 if v else 0)
    return sum(1 for v in difference.getdata() if v)


def compare_screenshots(left: Path, right: Path) -> tuple[int, int]:
    identical = total = 0
    for shot in sorted(left.glob("*.png")):
        other = right / shot.name
        if not other.exists():
            print(f"  {shot.name}: missing on the right")
            continue
        total += 1
        count = differing_pixels(shot, other)
        identical += count == 0
        print(f"  {shot.name}: " + ("size differs" if count is None else "identical" if count == 0 else f"{count} pixels differ"))
    return identical, total


def compare_saves(left: Path, right: Path) -> None:
    for save in sorted((left / "SaveGame").glob("*")):
        other = right / "SaveGame" / save.name
        if not other.exists() or not save.is_file():
            continue
        a, b = save.read_bytes(), other.read_bytes()
        if a == b:
            continue
        offsets = [i for i in range(min(len(a), len(b))) if a[i] != b[i]]
        print(f"  {save.name}: sizes {len(a)} / {len(b)}, {len(offsets)} bytes differ" + (f" at {offsets[:40]}" if offsets else ""))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("left", type=Path)
    ap.add_argument("right", type=Path)
    args = ap.parse_args()
    print("screenshots:")
    identical, total = compare_screenshots(args.left, args.right)
    print(f"  {identical} of {total} pixel-identical")
    print("saves that differ (files unchanged from the install's saves are not listed):")
    compare_saves(args.left, args.right)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
