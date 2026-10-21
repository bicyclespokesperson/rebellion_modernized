"""Checks for paired, cost-weighted preservation measurements."""
from __future__ import annotations

import importlib.util
import sys
import unittest
from pathlib import Path
from typing import Any

TOOL = Path(__file__).resolve().parents[2] / "tools" / "battle_skill_benchmark.py"
SPEC = importlib.util.spec_from_file_location("battle_skill_benchmark", TOOL)
assert SPEC and SPEC.loader
benchmark = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = benchmark
SPEC.loader.exec_module(benchmark)


def run(survival: list[bool], winner: int = 1) -> dict[str, Any]:
    tally = {"capital_ships": 2, "capital_survival": [
        {"type_bits": index, "survived": survived} for index, survived in enumerate(survival)]}
    return {"seed": 42, "winner": winner, "initial_strength": [100, 100], "death_star": False,
            "alliance": tally, "empire": tally}


class BenchmarkTests(unittest.TestCase):
    def test_actual_cost_weights_survival_and_withdrawal_equally(self) -> None:
        key = ("battle", 0)
        result = benchmark.compare({key: run([True, False])}, {key: run([False, True])},
                                   "alliance", {0: 10, 1: 30})
        self.assertEqual(result["starting_cost"], 40)
        self.assertEqual(result["baseline_kept"], 10)
        self.assertEqual(result["expert_kept"], 30)
        self.assertEqual(result["relative_gain_percent"], 200)

    def test_contested_excludes_lopsided_boundary_and_station(self) -> None:
        record = run([True, True])
        self.assertTrue(benchmark.contested(record, .5, 2))
        record["initial_strength"] = [200, 100]
        self.assertFalse(benchmark.contested(record, .5, 2))
        record["initial_strength"] = [50, 100]
        self.assertFalse(benchmark.contested(record, .5, 2))
        record["initial_strength"] = [100, 100]
        record["death_star"] = True
        self.assertFalse(benchmark.contested(record, .5, 2))

    def test_missing_or_different_seed_pair_is_rejected(self) -> None:
        baseline = {("battle", 0): run([True, True])}
        with self.assertRaises(ValueError):
            benchmark.compare(baseline, {}, "alliance", {0: 10, 1: 30})
        candidate = run([True, True])
        candidate["seed"] += 1
        with self.assertRaises(ValueError):
            benchmark.compare(baseline, {("battle", 0): candidate}, "alliance", {0: 10, 1: 30})


if __name__ == "__main__":
    unittest.main()
