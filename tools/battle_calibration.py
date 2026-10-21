#!/usr/bin/env -S uv run
"""Report the battle calibration metrics (docs/design/tactical_combat.md section 8.3) from reb_battle_runner output.

Reads one or more JSON-lines files written by `reb_battle_runner` (one line per battle and seed, any mix of engines)
and the snapshots they name, then reports per engine:

* winner against strength ratio, and how often a side with at least twice the other's strength wins;
* agreement with a recorded legacy baseline (the retired engine's corpus results, kept in work/) on the battles the
  legacy engine decides without its defects (stalemate endings and Death Star battles are left out,
  tactical_combat.md section 8.4);
* loss distributions per side: capital ships destroyed and withdrawn, hull lost, fighter craft lost.

Strength estimate (Lanchester square law, made linear in fleet size): a side's strength is sqrt(F * D), where F is its
anti-capital firepower and D its durability. A capital ship contributes F = the sum over all four arcs of turbolaser +
ion + laser / 6 (the AI's own capital-ship weighting) and D = current hull + maximum shield; a squadron contributes, per
remaining craft, F = turbolaser + ion + laser / 6 + torpedoes and D = 8 hull (the legacy per-craft hull) + shield. The
Death Star is left out. By the square law a side with twice the other's strength should win decisively.
"""
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
from __future__ import annotations

import argparse
import json
import math
import statistics
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path

SIDES = ("alliance", "empire")
WINNER_NAMES = {0: "none", 1: "alliance", 2: "empire"}
RATIO_BINS = (0.25, 0.5, 0.8, 1.25, 2.0, 4.0)
LOPSIDED = 2.0
AGREEMENT_TARGET = 0.85
LOPSIDED_TARGET = 0.90
LEGACY_CRAFT_HULL = 8


@dataclass
class SideStrength:
    firepower: float = 0.0
    durability: float = 0.0
    capital_ships: int = 0
    squadrons: int = 0

    @property
    def strength(self) -> float:
        return math.sqrt(self.firepower * self.durability)


@dataclass
class BattleInfo:
    path: str
    sides: dict[str, SideStrength] = field(default_factory=lambda: {side: SideStrength() for side in SIDES})

    @property
    def ratio(self) -> float:
        """Alliance strength over Empire strength (inf when the Empire has none)."""
        alliance, empire = self.sides["alliance"].strength, self.sides["empire"].strength
        if empire == 0:
            return math.inf if alliance > 0 else 1.0
        return alliance / empire


def split_fields(text: str) -> dict[str, str]:
    """Parse the `key=value` fields of one snapshot line (values may be quoted with backslash escapes)."""
    fields: dict[str, str] = {}
    index = 0
    while index < len(text):
        equals = text.index("=", index)
        key = text[index:equals]
        index = equals + 1
        if index < len(text) and text[index] == '"':
            value, index = read_quoted(text, index + 1)
        else:
            end = text.find(" ", index)
            end = len(text) if end < 0 else end
            value, index = text[index:end], end
        fields[key] = value
        index += 1
    return fields


def read_quoted(text: str, index: int) -> tuple[str, int]:
    out: list[str] = []
    while text[index] != '"':
        if text[index] == "\\":
            if text[index + 1] == "x":
                out.append(chr(int(text[index + 2 : index + 4], 16)))
                index += 4
                continue
            index += 1
        out.append(text[index])
        index += 1
    return "".join(out), index + 1


def anti_capital(fields: dict[str, str]) -> float:
    total = 0.0
    for arc in ("fore", "aft", "port", "starboard"):
        turbolaser, ion, laser = (int(value) for value in fields[arc].split("/"))
        total += turbolaser + ion + laser / 6
    return total


def read_snapshot(path: Path) -> BattleInfo:
    info = BattleInfo(str(path))
    for line in path.read_text(encoding="latin-1").splitlines()[4:-1]:
        keyword, _, rest = line.partition(" ")
        fields = split_fields(rest)
        if keyword == "ship":
            side = info.sides[fields["side"]]
            side.capital_ships += 1
            side.firepower += anti_capital(fields)
            side.durability += int(fields["hull"]) + int(fields["shield-max"])
        elif keyword == "squadron":
            side = info.sides[fields["side"]]
            side.squadrons += 1
            craft = int(fields["current-size"])
            side.firepower += craft * (anti_capital(fields) + int(fields["torpedoes"]))
            side.durability += craft * (LEGACY_CRAFT_HULL + int(fields["shield"]))
    return info


@dataclass
class Run:
    """One runner line: a battle under one engine and seed."""

    battle: str
    engine: str
    seed_index: int
    record: dict

    @property
    def winner(self) -> int:
        return int(self.record["winner"])


def load_runs(paths: list[Path]) -> list[Run]:
    runs: list[Run] = []
    for path in paths:
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if not line.strip():
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError as error:
                sys.exit(f"{path}:{number}: {error}")
            runs.append(Run(record["snapshot"], record["engine"], int(record["seed_index"]), record))
    return runs


def ratio_bin(ratio: float) -> str:
    edges = (0.0, *RATIO_BINS, math.inf)
    for low, high in zip(edges, edges[1:]):
        if ratio < high:
            return f"{low:g}-{high:g}" if high != math.inf else f">{low:g}"
    return f">{RATIO_BINS[-1]:g}"


def percent(part: int, whole: int) -> str:
    return f"{100 * part / whole:5.1f}%" if whole else "    -"


def stronger_side(ratio: float) -> int | None:
    if ratio >= LOPSIDED:
        return 1
    if ratio <= 1 / LOPSIDED:
        return 2
    return None


def is_defect_battle(runs: list[Run]) -> bool:
    return any(run.record["stalemate"] or run.record["death_star"] for run in runs)


def majority_winner(runs: list[Run]) -> tuple[int, float]:
    counts = Counter(run.winner for run in runs)
    winner, votes = max(sorted(counts.items()), key=lambda item: item[1])
    return winner, votes / len(runs)


def describe(values: list[float]) -> str:
    if not values:
        return "-"
    ordered = sorted(values)
    p90 = ordered[min(len(ordered) - 1, int(0.9 * len(ordered)))]
    return f"mean {statistics.fmean(values):7.2f}  median {statistics.median(values):7.2f}  p90 {p90:7.2f}"


def report_corpus(info: dict[str, BattleInfo]) -> list[str]:
    battles = list(info.values())
    ships = [sum(b.sides[side].capital_ships for side in SIDES) for b in battles]
    squadrons = [sum(b.sides[side].squadrons for side in SIDES) for b in battles]
    no_ships = sum(any(b.sides[side].capital_ships == 0 for side in SIDES) for b in battles)
    return [
        f"Corpus: {len(battles)} battles",
        f"  capital ships per battle  {describe(ships)}  max {max(ships, default=0)}",
        f"  squadrons per battle      {describe(squadrons)}  max {max(squadrons, default=0)}",
        f"  battles where a side has no capital ships: {no_ships}",
    ]


def report_strength(engine: str, runs: list[Run], info: dict[str, BattleInfo]) -> list[str]:
    lines = [f"Winner against strength ratio (alliance / empire), {engine}:"]
    lines.append(f"  {'ratio':>10} {'runs':>6} {'alliance':>9} {'empire':>8} {'none':>8}")
    by_bin: dict[str, Counter[int]] = defaultdict(Counter)
    for run in runs:
        by_bin[ratio_bin(info[run.battle].ratio)][run.winner] += 1
    edges = (0.0, *RATIO_BINS, math.inf)
    for low, high in zip(edges, edges[1:]):
        name = f"{low:g}-{high:g}" if high != math.inf else f">{low:g}"
        counts = by_bin.get(name)
        if not counts:
            continue
        total = sum(counts.values())
        lines.append(
            f"  {name:>10} {total:6} {percent(counts[1], total):>9} {percent(counts[2], total):>8}"
            f" {percent(counts[0], total):>8}"
        )
    lopsided = [run for run in runs if stronger_side(info[run.battle].ratio) is not None]
    wins = sum(run.winner == stronger_side(info[run.battle].ratio) for run in lopsided)
    verdict = "meets" if lopsided and wins / len(lopsided) >= LOPSIDED_TARGET else "misses"
    lines.append(
        f"  stronger side (ratio >= {LOPSIDED:g}) wins {wins}/{len(lopsided)} = {percent(wins, len(lopsided)).strip()}"
        f" ({verdict} the {LOPSIDED_TARGET:.0%} target)"
    )
    return lines


def report_losses(engine: str, runs: list[Run]) -> list[str]:
    lines = [f"Losses per battle, {engine} (mean / median / 90th percentile):"]
    for side in SIDES:
        tallies = [run.record[side] for run in runs if run.record[side]["capital_ships"] + run.record[side]["squadrons"]]
        if not tallies:
            continue
        hull_fraction = [t["hull_lost"] / t["hull_start"] for t in tallies if t["hull_start"]]
        fighter_fraction = [t["fighters_lost"] / t["fighters"] for t in tallies if t["fighters"]]
        lines.append(f"  {side} ({len(tallies)} battles with forces)")
        lines.append(f"    capital ships destroyed  {describe([t['destroyed'] for t in tallies])}")
        lines.append(f"    capital ships withdrawn  {describe([t['withdrawn'] for t in tallies])}")
        lines.append(f"    capital ships damaged    {describe([t['damaged'] for t in tallies])}")
        lines.append(f"    hull lost (fraction)     {describe(hull_fraction)}")
        lines.append(f"    fighter craft lost       {describe([t['fighters_lost'] for t in tallies])}")
        lines.append(f"    fighter craft lost (fr.) {describe(fighter_fraction)}")
    return lines


def report_endings(engine: str, runs: list[Run]) -> list[str]:
    total = len(runs)
    stalemates = [run for run in runs if run.record["stalemate"]]
    wiped = sum(
        any(run.record[side]["capital_ships"] and run.record[side]["destroyed"] == run.record[side]["capital_ships"]
            for side in SIDES)
        for run in stalemates
    )
    unfinished = sum(not run.record["finished"] for run in runs)
    withdrawals = sum(
        run.record["outcome"]["alliance_withdrew"] or run.record["outcome"]["empire_withdrew"] for run in runs
    )
    steps = [run.record["steps"] for run in runs]
    return [
        f"Endings, {engine}: {total} runs, {withdrawals} with a side withdrawing, {len(stalemates)} by stalemate"
        f" ({wiped} of them with a side's capital ships all destroyed),"
        f" {sum(run.record['death_star'] for run in runs)} with a Death Star, {unfinished} unfinished",
        f"  steps {describe(steps)}",
    ]


def report_agreement(engine: str, runs: list[Run], baseline: list[Run], info: dict[str, BattleInfo]) -> list[str]:
    by_battle: dict[str, list[Run]] = defaultdict(list)
    for run in runs:
        by_battle[run.battle].append(run)
    base_by_battle: dict[str, list[Run]] = defaultdict(list)
    for run in baseline:
        base_by_battle[run.battle].append(run)
    compared = agreed = excluded = inconsistent = 0
    divergences: list[str] = []
    for battle, base_runs in sorted(base_by_battle.items()):
        if battle not in by_battle:
            continue
        if is_defect_battle(base_runs):
            excluded += 1
            continue
        base_winner, base_share = majority_winner(base_runs)
        if base_share < 1.0:
            inconsistent += 1
            continue
        winner, share = majority_winner(by_battle[battle])
        compared += 1
        if winner == base_winner:
            agreed += 1
        else:
            divergences.append(
                f"    {battle}: ratio {info[battle].ratio:.2f}, legacy {WINNER_NAMES[base_winner]},"
                f" {engine} {WINNER_NAMES[winner]} ({share:.0%} of seeds)"
            )
    verdict = "meets" if compared and agreed / compared >= AGREEMENT_TARGET else "misses"
    lines = [
        f"Agreement with the legacy baseline, {engine}: {agreed}/{compared} = {percent(agreed, compared).strip()}"
        f" ({verdict} the {AGREEMENT_TARGET:.0%} target); {excluded} defect battles left out,"
        f" {inconsistent} inconsistent in the baseline"
    ]
    if divergences:
        lines.append("  divergences:")
        lines.extend(divergences)
    return lines


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("results", nargs="+", type=Path, help="reb_battle_runner JSON-lines files")
    parser.add_argument("--baseline", default="legacy", help="engine to compare the others against (default legacy)")
    parser.add_argument(
        "--snapshot-root", type=Path, default=Path.cwd(), help="directory the runner's snapshot paths are relative to"
    )
    args = parser.parse_args()

    runs = load_runs(args.results)
    if not runs:
        sys.exit("no runner results")
    info: dict[str, BattleInfo] = {}
    for battle in sorted({run.battle for run in runs}):
        path = Path(battle) if Path(battle).is_absolute() else args.snapshot_root / battle
        info[battle] = read_snapshot(path)

    by_engine: dict[str, list[Run]] = defaultdict(list)
    for run in runs:
        by_engine[run.engine].append(run)
    out: list[str] = [f"{len(info)} battles, {len(runs)} runs: " + ", ".join(
        f"{engine} {len(engine_runs)}" for engine, engine_runs in sorted(by_engine.items()))]
    out.extend(report_corpus(info))
    for engine, engine_runs in sorted(by_engine.items()):
        out.append("")
        out.extend(report_endings(engine, engine_runs))
        out.extend(report_strength(engine, engine_runs, info))
        out.extend(report_losses(engine, engine_runs))
        if engine != args.baseline and args.baseline in by_engine:
            out.extend(report_agreement(engine, engine_runs, by_engine[args.baseline], info))
    print("\n".join(out))


if __name__ == "__main__":
    main()
