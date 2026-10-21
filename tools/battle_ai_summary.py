#!/usr/bin/env -S uv run
"""Summarize commander calibration telemetry from reb_space_battle_log --batch."""
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
from __future__ import annotations

import argparse
import json
import statistics
from collections import defaultdict
from pathlib import Path
from typing import Any


def stronger_side(record: dict[str, Any]) -> int:
    alliance, empire = record["initial_strength"]
    if alliance > 0 and alliance >= 2 * empire:
        return 1
    if empire > 0 and empire >= 2 * alliance:
        return 2
    return 0


def percent(numerator: float, denominator: float) -> str:
    return f"{100 * numerator / denominator:.1f}%" if denominator else "n/a"


def summarize(engine: str, records: list[dict[str, Any]]) -> None:
    lopsided = [record for record in records if stronger_side(record)]
    wins = sum(record["winner"] == stronger_side(record) for record in lopsided)
    durations = sorted(record["steps"] / 20 for record in records)
    fighters = sum(record[side]["fighters"] for record in records for side in ("alliance", "empire"))
    lost = sum(record[side]["fighters_lost"] for record in records for side in ("alliance", "empire"))
    lost60 = sum(record["fighters_lost_60"] for record in records)
    lost120 = sum(record["fighters_lost_120"] for record in records)
    resets = sum(record["ion_spool_resets"] for record in records)
    reset_runs = sum(record["ion_spool_resets"] > 0 for record in records)
    ships = sum(record[side]["capital_ships"] for record in records for side in ("alliance", "empire"))
    destroyed = sum(record[side]["destroyed"] for record in records for side in ("alliance", "empire"))
    start_hull = sum(record[side]["hull_start"] for record in records for side in ("alliance", "empire"))
    hull_lost = sum(record[side]["hull_lost"] for record in records for side in ("alliance", "empire"))
    print(f"{engine}: {len(records)} runs")
    print(f"  Simulator initial-strength >=2 wins: {wins}/{len(lopsided)} ({percent(wins, len(lopsided))})")
    print(f"  Duration seconds mean {statistics.fmean(durations):.1f}, median {statistics.median(durations):.1f},"
          f" p90 {durations[min(len(durations) - 1, int(.9 * len(durations)))]:.1f}, max {max(durations):.1f}")
    print(f"  Runs >=600 seconds: {sum(duration >= 600 for duration in durations)}")
    print(f"  Fighter losses: {lost}/{fighters} ({percent(lost, fighters)})")
    print(f"  Losses by 60s: {lost60} ({percent(lost60, lost)} of all losses);"
          f" by 120s: {lost120} ({percent(lost120, lost)} of all losses)")
    print(f"  Capital ships destroyed: {destroyed}/{ships} ({percent(destroyed, ships)})")
    print(f"  Capital hull retained: {start_hull - hull_lost}/{start_hull}"
          f" ({percent(start_hull - hull_lost, start_hull)})")
    print(f"  Ion engine-disruption spool resets: {resets} in {reset_runs} runs")


def compare(
    baseline: list[dict[str, Any]], records: list[dict[str, Any]], side: str, engine: str
) -> None:
    reference = {(record["snapshot"], record["seed_index"]): record for record in baseline}
    pairs = [(reference[(record["snapshot"], record["seed_index"])], record) for record in records
             if (record["snapshot"], record["seed_index"]) in reference]
    if not pairs:
        return
    lopsided = [(old, new) for old, new in pairs if stronger_side(old)]
    flips = sum(old["winner"] != new["winner"] for old, new in lopsided)
    side_number = 1 if side == "alliance" else 2
    weaker = [(old, new) for old, new in pairs if stronger_side(old) not in (0, side_number)]
    weaker_flips = sum(old["winner"] != new["winner"] for old, new in weaker)
    print(f"Paired {engine}, measured {side}: {len(pairs)} matched runs")
    if all("capital_value_start" in old[side] for old, _ in pairs):
        start_value = sum(old[side]["capital_value_start"] for old, _ in pairs)
        baseline_value = sum(old[side]["capital_value_retained"] for old, _ in pairs)
        ceiling = 100 * (start_value / baseline_value - 1) if baseline_value else 0
        print(f"  Maximum possible capital-proxy improvement if every ship survived: {ceiling:.2f}%")
    for metric, label in (("capital_value_retained", "Surviving capital max-hull proxy"),
                          ("hull_lost", "Remaining capital hull")):
        if metric == "capital_value_retained" and any(metric not in record[side] for pair in pairs for record in pair):
            continue
        def retained(record: dict[str, Any], which_side: str) -> int:
            tally = record[which_side]
            return tally["hull_start"] - tally["hull_lost"] if metric == "hull_lost" else tally[metric]
        old_value = sum(retained(old, side) for old, _ in pairs)
        new_value = sum(retained(new, side) for _, new in pairs)
        improvement = 100 * (new_value / old_value - 1) if old_value else 0
        print(f"  {label}: baseline {old_value}, new {new_value}, relative change {improvement:+.2f}%")
        same_winner = [(old, new) for old, new in pairs if old["winner"] == new["winner"] == side_number]
        old_winner_value = sum(retained(old, side) for old, _ in same_winner)
        new_winner_value = sum(retained(new, side) for _, new in same_winner)
        winner_improvement = 100 * (new_winner_value / old_winner_value - 1) if old_winner_value else 0
        print(f"    Same measured-side wins ({len(same_winner)}): {old_winner_value} -> {new_winner_value}"
              f", {winner_improvement:+.2f}%")
    print(f"  Winner flips in any lopsided matchup: {flips}/{len(lopsided)} ({percent(flips, len(lopsided))})")
    print(f"  Winner flips when measured side outmatched >=2: {weaker_flips}/{len(weaker)}"
          f" ({percent(weaker_flips, len(weaker))})")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", nargs="+", type=Path)
    parser.add_argument("--baseline", default="default", help="engine label of the frozen baseline")
    parser.add_argument("--side", choices=("alliance", "empire"), help="side measured in a paired comparison")
    args = parser.parse_args()
    by_engine: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for path in args.results:
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if not line.strip():
                continue
            try:
                record = json.loads(line)
                if "initial_strength" not in record:
                    raise ValueError("requires reb_space_battle_log batch telemetry")
                by_engine[record["engine"]].append(record)
            except (ValueError, KeyError) as error:
                parser.error(f"{path}:{number}: {error}")
    if not by_engine:
        parser.error("no battle runs")
    for engine, records in sorted(by_engine.items()):
        summarize(engine, records)
        if args.side and args.baseline in by_engine and engine != args.baseline:
            compare(by_engine[args.baseline], records, args.side, engine)


if __name__ == "__main__":
    main()
