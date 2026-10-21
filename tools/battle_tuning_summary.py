#!/usr/bin/env -S uv run
"""Summarize simulator-strength gates and fighter/ion/stall telemetry from battle-log batches."""
# /// script
# dependencies = []
# ///
from __future__ import annotations

import argparse
import json
import statistics
from pathlib import Path
from typing import Any


def summarize(runs: list[dict[str, Any]]) -> dict[str, Any]:
    lopsided = [run for run in runs if max(run['initial_strength']) > 0
                and max(run['initial_strength']) >= 2 * min(run['initial_strength'])]
    wins = sum(run['winner'] == (1 if run['initial_strength'][0] > run['initial_strength'][1] else 2)
               for run in lopsided)
    fighters = sum(run[side]['fighters'] for run in runs for side in ('alliance', 'empire'))
    seconds = sorted(run['steps'] / 20 for run in runs)
    return {
        'runs': len(runs),
        'finished': sum(run['finished'] for run in runs),
        'lopsided_runs': len(lopsided),
        'stronger_wins': wins,
        'stronger_win_percent': 100 * wins / len(lopsided) if lopsided else None,
        'initial_fighters': fighters,
        'fighter_loss_percent_60': 100 * sum(run['fighters_lost_60'] for run in runs) / fighters if fighters else None,
        'fighter_loss_percent_120': 100 * sum(run['fighters_lost_120'] for run in runs) / fighters if fighters else None,
        'fighter_loss_percent_end': 100 * sum(run[side]['fighters_lost'] for run in runs
                                              for side in ('alliance', 'empire')) / fighters if fighters else None,
        'ion_spool_resets': sum(run['ion_spool_resets'] for run in runs),
        'stalemates': sum(run['end_reason'] == 2 for run in runs),
        'time_limits': sum(run['end_reason'] == 3 for run in runs),
        'seconds_median': statistics.median(seconds),
        'seconds_p90': seconds[min(len(seconds) - 1, int(.9 * len(seconds)))],
        'seconds_max': max(seconds),
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
