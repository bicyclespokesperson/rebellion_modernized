"""Keep attrition denominators and loser-only withdrawal samples stable."""
from __future__ import annotations

import importlib.util
import unittest
from pathlib import Path
from typing import Any

TOOL = Path(__file__).resolve().parents[2] / 'tools' / 'battle_decisiveness.py'
SPEC = importlib.util.spec_from_file_location('battle_decisiveness', TOOL)
assert SPEC and SPEC.loader
summary = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(summary)


def battle(alliance: tuple[int, int, int], empire: tuple[int, int, int], winner: int) -> dict[str, Any]:
    def side(values: tuple[int, int, int]) -> dict[str, int]:
        return dict(zip(('capital_ships', 'destroyed', 'withdrawn'), values))
    return {'alliance': side(alliance), 'empire': side(empire), 'winner': winner, 'finished': True}


class DecisivenessTests(unittest.TestCase):
    def test_aggregate_ships_and_battles_use_different_denominators(self) -> None:
        rows = [battle((10, 2, 8), (2, 1, 0), 2), battle((1, 0, 0), (2, 1, 1), 1),
                battle((0, 0, 0), (100, 0, 100), 1)]
        result = summary.summarize(rows)
        self.assertEqual(result['contested'], 2)
        self.assertEqual(result['loser_capitals_destroyed_percent'], 25)
        self.assertEqual(result['loser_withdrew_percent'], 100)
        self.assertAlmostEqual(result['all_capitals_destroyed_percent'], 100 * 4 / 15)
        self.assertAlmostEqual(result['all_capitals_withdrawn_percent'], 100 * 9 / 15)

    def test_escape_telemetry_excludes_winner_and_destroyed_attempts(self) -> None:
        row = battle((2, 1, 1), (1, 0, 0), 2)
        sample = {'side': 0, 'escaped': True, 'start': 100, 'end': 500, 'hull_start': 100,
                  'hull_end': 80, 'hull_max': 200, 'targeted_ticks': 0, 'tractored_ticks': 0}
        row['withdrawals'] = [sample, dict(sample, side=1, end=900), dict(sample, escaped=False, end=700)]
        result = summary.summarize([row])
        self.assertEqual(result['loser_withdrawal_attempts'], 2)
        self.assertEqual(result['loser_escaped_ships'], 1)
        self.assertEqual(result['escape_seconds_median'], 20)
        self.assertEqual(result['escape_hull_damage_fraction_mean'], .1)
        self.assertEqual(result['escape_never_tractored'], 1)


if __name__ == '__main__':
    unittest.main()
