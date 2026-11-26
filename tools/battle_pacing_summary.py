#!/usr/bin/env -S uv run
"""Summarize battle pacing (tactical_combat.md §6.1) from reb_space_battle_log --batch output.

Reports battle length, how long destroyed capital ships (line ships of at least 1,000 hull apart from lighter ones)
and craft lasted once under fire, which sources deal hull damage to capital ships, and hit rates. Only battles that came to blows (some unit destroyed or damaged) count toward battle length.
"""
# /// script
# dependencies = []
# ///
from __future__ import annotations

import argparse
import json
import statistics
from pathlib import Path
from typing import Any


def quantiles(values: list[float]) -> dict[str, float | int | None]:
    if not values:
        return {'n': 0, 'p10': None, 'median': None, 'p90': None}
    ordered = sorted(values)

    def at(share: float) -> float:
        return round(ordered[min(len(ordered) - 1, int(share * len(ordered)))], 1)

    return {'n': len(ordered), 'p10': at(.1), 'median': round(statistics.median(ordered), 1), 'p90': at(.9)}


def rate(tally: dict[str, int]) -> float | None:
    return round(100 * tally['hits'] / tally['fired'], 1) if tally['fired'] else None


def fought(run: dict[str, Any]) -> bool:
    return any(run[side]['hull_lost'] > 0 or run[side]['fighters_lost'] > 0 for side in ('alliance', 'empire'))


HULL_SOURCES = ('ship_turbolaser', 'ship_laser', 'craft_guns', 'torpedoes', 'planetary')
LINE_SHIP_HULL = 1000


def capital_deaths(pacing: list[dict[str, Any]], key: str, line: bool) -> list[float]:
    return [seconds for p in pacing for seconds, hull in zip(p[key], p['capital_hull_max'])
            if (hull >= LINE_SHIP_HULL) == line]


def summarize(runs: list[dict[str, Any]]) -> dict[str, Any]:
    pacing = [run['pacing'] for run in runs]
    hull_damage = [sum(p['hull_damage_by_source'][i] for p in pacing) for i in range(len(HULL_SOURCES))]
    shots: dict[str, dict[str, int]] = {}
    for key in ('ship_vs_craft', 'craft_vs_craft', 'ship_vs_capital'):
        shots[key] = {'fired': sum(p[key]['fired'] for p in pacing), 'hits': sum(p[key]['hits'] for p in pacing)}
    both_capitals = [run for run in runs if fought(run)
                     and all(run[side]['capital_ships'] for side in ('alliance', 'empire'))]
    craft_lost = sum(run[side]['fighters_lost'] for run in runs for side in ('alliance', 'empire'))
    craft_total = sum(run[side]['fighters'] for run in runs for side in ('alliance', 'empire'))
    capitals_lost = sum(run[side]['destroyed'] for run in runs for side in ('alliance', 'empire'))
    capitals_total = sum(run[side]['capital_ships'] for run in runs for side in ('alliance', 'empire'))
    return {
        'runs': len(runs),
        'battle_seconds': quantiles([run['steps'] / 20 for run in runs if fought(run)]),
        'battle_seconds_capitals_both_sides': quantiles([run['steps'] / 20 for run in both_capitals]),
        'line_ship_seconds_under_fire': quantiles(capital_deaths(pacing, 'capital_seconds_under_fire', True)),
        'line_ship_hull_seconds': quantiles(capital_deaths(pacing, 'capital_hull_seconds', True)),
        'light_ship_seconds_under_fire': quantiles(capital_deaths(pacing, 'capital_seconds_under_fire', False)),
        'light_ship_hull_seconds': quantiles(capital_deaths(pacing, 'capital_hull_seconds', False)),
        'hull_damage_percent_by_source': {name: round(100 * value / max(1.0, sum(hull_damage)), 1)
                                          for name, value in zip(HULL_SOURCES, hull_damage)},
        'craft_seconds_in_space': quantiles([v for p in pacing for v in p['craft_seconds_in_space']]),
        'craft_seconds_under_fire': quantiles([v for p in pacing for v in p['craft_seconds_under_fire']]),
        'capitals_destroyed_percent': round(100 * capitals_lost / max(1, capitals_total), 1),
        'craft_lost_percent': round(100 * craft_lost / max(1, craft_total), 1),
        'hit_percent': {key: rate(tally) for key, tally in shots.items()},
        'shots': shots,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('results', nargs='+', type=Path)
    args = parser.parse_args()
    engines: dict[str, list[dict[str, Any]]] = {}
    for path in args.results:
        for line in path.read_text().splitlines():
            if line.strip():
                run = json.loads(line)
                engines.setdefault(run['engine'], []).append(run)
    print(json.dumps({engine: summarize(runs) for engine, runs in engines.items()}, indent=2))


if __name__ == '__main__':
    main()
