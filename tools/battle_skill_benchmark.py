#!/usr/bin/env -S uv run
"""Compare paired commander runs using externally supplied refined-material costs.

Costs JSON maps decimal or hexadecimal snapshot type-bits to positive construction
costs. Keep licensed cost tables and all corpus outputs in ignored work/ paths.
The baseline is production Balanced/Normal on both sides. Each expert batch changes
only its measured side via --expert-side; the opponent remains Balanced/Normal.
"""
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

SIDES = ("alliance", "empire")
Run = dict[str, Any]
Key = tuple[str, int]


def load_runs(paths: list[Path]) -> dict[str, dict[Key, Run]]:
    engines: dict[str, dict[Key, Run]] = {}
    for path in paths:
        for line in path.read_text().splitlines():
            if not line.strip():
                continue
            run = json.loads(line)
            key = (run["snapshot"], run["seed_index"])
            group = engines.setdefault(run["engine"], {})
            if key in group:
                raise ValueError(f"duplicate {run['engine']} run: {key}")
            group[key] = run
    return engines


def contested(run: Run, ratio_min: float, ratio_max: float) -> bool:
    alliance, empire = run["initial_strength"]
    return (not run["death_star"] and alliance > 0 and empire > 0
            and ratio_min < alliance / empire < ratio_max
            and all(run[side]["capital_ships"] > 0 for side in SIDES))


def value(run: Run, side: str, costs: dict[int, float]) -> tuple[float, float]:
    ships = run[side]["capital_survival"]
    if len(ships) != run[side]["capital_ships"]:
        raise ValueError("incomplete capital_survival telemetry")
    start = sum(costs[ship["type_bits"]] for ship in ships)
    retained = sum(costs[ship["type_bits"]] for ship in ships if ship["survived"])
    return start, retained


def compare(baseline: dict[Key, Run], expert: dict[Key, Run], side: str,
            costs: dict[int, float]) -> dict[str, Any]:
    missing = baseline.keys() - expert.keys()
    if missing:
        raise ValueError(f"{side}: missing {len(missing)} paired expert runs")
    initial = old = new = winner_old = winner_new = 0.0
    same_wins = flips = 0
    winner = SIDES.index(side) + 1
    for key, reference in sorted(baseline.items()):
        candidate = expert[key]
        start, kept = value(reference, side, costs)
        new_start, new_kept = value(candidate, side, costs)
        if start != new_start or reference["seed"] != candidate["seed"]:
            raise ValueError(f"mismatched paired run: {key}")
        initial += start
        old += kept
        new += new_kept
        flips += reference["winner"] != candidate["winner"]
        if reference["winner"] == candidate["winner"] == winner:
            same_wins += 1
            winner_old += kept
            winner_new += new_kept
    return {"runs": len(baseline), "starting_cost": initial, "baseline_kept": old,
            "expert_kept": new, "relative_gain_percent": 100 * (new / old - 1) if old else None,
            "maximum_possible_gain_percent": 100 * (initial / old - 1) if old else None,
            "winner_flips": flips, "same_side_wins": same_wins,
            "same_wins_baseline_kept": winner_old, "same_wins_expert_kept": winner_new}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("results", nargs="+", type=Path)
    parser.add_argument("--costs", required=True, type=Path)
    parser.add_argument("--baseline", default="default")
    parser.add_argument("--expert-alliance", default="expert_alliance")
    parser.add_argument("--expert-empire", default="expert_empire")
    parser.add_argument("--ratio-min", default=0.5, type=float)
    parser.add_argument("--ratio-max", default=2.0, type=float)
    parser.add_argument("--write-subset", type=Path, help="write snapshot list for expert batch runs")
    args = parser.parse_args()
    try:
        if not 0 < args.ratio_min < 1 < args.ratio_max:
            raise ValueError("ratio bounds must contain 1 and be positive")
        costs = {int(key, 16) if key.startswith("0x") else int(key): float(cost)
                 for key, cost in json.loads(args.costs.read_text()).items()}
        if any(cost <= 0 for cost in costs.values()):
            raise ValueError("costs must be positive")
        engines = load_runs(args.results)
        baseline = engines[args.baseline]
        subset = {key: run for key, run in baseline.items() if contested(run, args.ratio_min, args.ratio_max)}
        if not subset:
            raise ValueError("no contested runs")
        if args.write_subset:
            args.write_subset.write_text("\n".join(sorted({key[0] for key in subset})) + "\n")
        report: dict[str, Any] = {"baseline_runs": len(baseline), "contested_runs": len(subset),
                                  "contested_snapshots": len({key[0] for key in subset}),
                                  "ratio_open_interval": [args.ratio_min, args.ratio_max],
                                  "measure": "refined-material cost of surviving or withdrawn capitals"}
        for side, label in zip(SIDES, (args.expert_alliance, args.expert_empire)):
            if label in engines:
                report[side] = compare(subset, engines[label], side, costs)
                side_index = SIDES.index(side)
                lopsided = {key: run for key, run in baseline.items()
                            if run["initial_strength"][1 - side_index] > 0
                            and run["initial_strength"][side_index]
                            < .5 * run["initial_strength"][1 - side_index]}
                paired = lopsided.keys() & engines[label].keys()
                flips = sum(lopsided[key]["winner"] != engines[label][key]["winner"] for key in paired)
                report[side]["outmatched_pairs"] = len(paired)
                report[side]["outmatched_pairs_missing"] = len(lopsided) - len(paired)
                report[side]["outmatched_winner_flips"] = flips
                report[side]["outmatched_flip_percent"] = 100 * flips / len(paired) if paired else None
        print(json.dumps(report, indent=2))
    except (ValueError, KeyError, OSError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
