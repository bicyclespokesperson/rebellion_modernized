#!/usr/bin/env -S uv run
"""Summarize capital attrition and per-ship withdrawal telemetry from battle runner JSONL."""
# /// script
# dependencies = []
# ///

from __future__ import annotations

import argparse
import json
import statistics
from pathlib import Path
from typing import Any


def mean(values: list[float]) -> float | None:
    return statistics.mean(values) if values else None


def summarize(rows: list[dict[str, Any]]) -> dict[str, Any]:
    contested = [row for row in rows if all(row[side]['capital_ships'] for side in ('alliance', 'empire'))]
    capitals = sum(row[side]['capital_ships'] for row in contested for side in ('alliance', 'empire'))
    losers = [row['alliance' if row['winner'] == 2 else 'empire'] for row in contested if row['winner'] in (1, 2)]
    loser_capitals = sum(side['capital_ships'] for side in losers)
    attempts = [sample for row in contested for sample in row.get('withdrawals', [])
                if row['winner'] in (1, 2) and sample['side'] == (0 if row['winner'] == 2 else 1)]
    escaped = [sample for sample in attempts if sample['escaped']]
    stronger = [(row, 1 if row['initial_strength'][0] >= 2 * row['initial_strength'][1] else 2)
                for row in rows if 'initial_strength' in row and min(row['initial_strength']) > 0
                and max(row['initial_strength']) >= 2 * min(row['initial_strength'])]
    return {
        'runs': len(rows),
        'unfinished': sum(not row['finished'] for row in rows),
        'contested': len(contested),
        'loser_capitals_destroyed_percent': 100 * sum(side['destroyed'] for side in losers) / max(1, loser_capitals),
        'loser_withdrew_percent': 100 * sum(side['withdrawn'] > 0 for side in losers) / max(1, len(contested)),
        'all_capitals_destroyed_percent': 100 * sum(row[side]['destroyed'] for row in contested
                                                  for side in ('alliance', 'empire')) / max(1, capitals),
        'all_capitals_withdrawn_percent': 100 * sum(row[side]['withdrawn'] for row in contested
                                                  for side in ('alliance', 'empire')) / max(1, capitals),
        'stronger_wins': sum(row['winner'] == winner for row, winner in stronger),
        'stronger_runs': len(stronger),
        'loser_withdrawal_attempts': len(attempts),
        'loser_escaped_ships': len(escaped),
        'escape_seconds_mean': mean([(sample['end'] - sample['start']) / 20 for sample in escaped]),
        'escape_seconds_median': statistics.median([(sample['end'] - sample['start']) / 20 for sample in escaped])
                                 if escaped else None,
        'escape_start_seconds_median': statistics.median([sample['start'] / 20 for sample in escaped])
                                      if escaped else None,
        'escape_hull_fraction_mean': mean([sample['hull_start'] / sample['hull_max'] for sample in escaped]),
        'escape_hull_damage_fraction_mean': mean([(sample['hull_start'] - sample['hull_end']) / sample['hull_max']
                                                 for sample in escaped]),
        'escape_never_targeted': sum(sample['targeted_ticks'] == 0 for sample in escaped),
        'escape_never_tractored': sum(sample['tractored_ticks'] == 0 for sample in escaped),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inputs', type=Path, nargs='+')
    parser.add_argument('--all-seeds', action='store_true', help='Default: compare seed index zero only.')
    args = parser.parse_args()
    for path in args.inputs:
        rows = [json.loads(line) for line in path.read_text().splitlines() if line]
        if not args.all_seeds:
            rows = [row for row in rows if row.get('seed_index', 0) == 0]
        print(json.dumps({'path': str(path), **summarize(rows)}, sort_keys=True))


if __name__ == '__main__':
    main()
