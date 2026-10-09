#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow"]
# ///
"""Report of tools/long_compare.sh: pairwise comparison of the saves and screenshots of every run in a directory.

  tools/long_compare_report.py work/run/long/alliance [--save SAVEGAME.005]

Run directories are <config>_<i>. Even the original game does not reproduce its own long runs byte for byte: its droid advisor
draws from the random generator the AI breaks ties with, at moments that depend on asynchronous input (docs/phase3_plan.md), so
runs fall onto a few "trajectories". Runs are grouped into trajectories (equal size, at most --noise differing bytes: the UI
state in the save, such as 16-bit message stamps, differs by tens of bytes even on one trajectory) and the verdict asks whether
the reconstructions reach the trajectory the original code reaches most often. The save matrix shows, per pair, "=" for byte-identical files, "~N" for N differing bytes at
equal size (a handful: UI state in the save such as message timestamps; thousands: the simulations diverged), "size" for
different sizes. Exit status 1 when a configuration never reaches the original code's most common trajectory. Screenshots are
compared (pixel by pixel) across the runs of that trajectory.
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


def trajectories(saves: dict[str, bytes], noise: int) -> list[list[str]]:
    groups: list[list[str]] = []
    for name in saves:
        for group in groups:
            count = differing_bytes(saves[group[0]], saves[name])
            if count is not None and count <= noise:
                group.append(name)
                break
        else:
            groups.append([name])
    return sorted(groups, key=len, reverse=True)


def verdict_status(clusters: list[list[str]], configs: list[str]) -> tuple[int, list[str]]:
    def count(members: list[str], config: str) -> int:
        return sum(n.rsplit("_", 1)[0] == config for n in members)

    modal = max(clusters, key=lambda members: count(members, "orig"))
    if "orig" not in configs:
        return 0, modal
    print(f"\n== verdict: trajectory of the original code's most common outcome ({count(modal, 'orig')} of its runs) ==")
    status = 0
    for config in configs:
        if config == "orig":
            continue
        reached = count(modal, config)
        total = sum(1 for members in clusters for n in members if n.rsplit("_", 1)[0] == config)
        print(f"  {config}: {reached} of {total} runs on it")
        status |= reached == 0
    return status, modal


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("directory", type=Path)
    ap.add_argument("--save", default="SAVEGAME.005")
    ap.add_argument("--noise", type=int, default=100, help="differing bytes at equal size that still count as one trajectory (default 100)")
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
    for a in names:
        row = []
        for b in names:
            count = 0 if a == b else differing_bytes(saves[a], saves[b])
            row.append(f"{verdict(count):>{width + 2}}")
        print(f"  {a:<{width}}" + "".join(row))

    clusters = trajectories(saves, args.noise)
    print(f"\n== trajectories (equal size, <= {args.noise} differing bytes) ==")
    for number, members in enumerate(clusters, 1):
        by_config: dict[str, int] = {}
        for name in members:
            config = name.rsplit("_", 1)[0]
            by_config[config] = by_config.get(config, 0) + 1
        print(f"  #{number}: {len(saves[members[0]])} bytes, {len(members)} runs: " + ", ".join(f"{c} x{n}" for c, n in sorted(by_config.items())))
    configs = sorted({n.rsplit("_", 1)[0] for n in names})
    exit_status, modal = verdict_status(clusters, configs)

    reference = runs[0].parent / modal[0]
    print(f"\n== screenshots of the runs on that trajectory, differing pixels against {reference.name} (only differences listed) ==")
    for run in (reference.parent / n for n in modal[1:]):
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
    return exit_status


if __name__ == "__main__":
    raise SystemExit(main())
