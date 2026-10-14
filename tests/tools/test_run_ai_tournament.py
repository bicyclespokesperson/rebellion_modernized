from __future__ import annotations

import json
import hashlib
import subprocess
import sys
import tempfile
import unittest
from argparse import Namespace
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.run_ai_tournament import aggregate, classify, executable_path, run_match, sha256_file


SCRIPT = Path(__file__).resolve().parents[2] / "tools" / "run_ai_tournament.py"


class TournamentTests(unittest.TestCase):
    def test_results_distinguish_victory_limit_crash_and_error(self) -> None:
        result = {
            "seed": 5, "ai1": "codex", "ai2": "classic", "winner": 1,
            "outcome": "victory", "systems": {"alliance": 12, "empire": 8, "neutral": 4},
        }
        self.assertEqual(classify(0, result, "codex", "classic", 5), "win")
        self.assertEqual(classify(0, {**result, "winner": 0}, "codex", "classic", 5), "failure")
        self.assertEqual(classify(0, result, "codex", "classic", 6), "failure")
        limit = {**result, "winner": 0, "outcome": "tick_limit"}
        self.assertEqual(classify(7, limit, "codex", "classic", 5), "tick_limit")
        self.assertEqual(classify(139, None, "codex", "classic", 5), "crash")
        self.assertEqual(classify(4, None, "codex", "classic", 5), "failure")
        runs = [
            {"seed": 5, "ai1": "codex", "ai2": "classic", "status": "win", "result": result},
            {"seed": 5, "ai1": "classic", "ai2": "codex", "status": "tick_limit", "result": limit},
            {"seed": 6, "ai1": "codex", "ai2": "classic", "status": "crash", "result": None},
            {"seed": 6, "ai1": "classic", "ai2": "codex", "status": "failure", "result": None},
        ]
        totals = aggregate(runs, "codex", "classic")
        self.assertEqual(totals["wins"], {"codex": 1, "classic": 0})
        self.assertEqual((totals["tick_limits"], totals["crashes"], totals["failures"]), (1, 1, 1))
        self.assertEqual(totals["territory"]["samples"], 2)

    def test_resume_rejects_mismatched_configuration_without_running(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            executable = output / "fake-executable"
            executable.write_bytes(b"fake executable")
            summary = output / "summary.json"
            original = {"config": {"seed_count": 99}, "runs": []}
            summary.write_text(json.dumps(original), encoding="utf-8")
            completed = subprocess.run(
                [sys.executable, str(SCRIPT), "--out", str(output), "--resume", "--seed-count", "1",
                 "--exe", str(executable)],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("configuration differs", completed.stderr)
            self.assertEqual(json.loads(summary.read_text(encoding="utf-8")), original)

    def test_invalid_limits_are_rejected(self) -> None:
        for option, value in (("--ticks", "-1"), ("--timeout-seconds", "-1"), ("--seed-count", "0")):
            with self.subTest(option=option):
                completed = subprocess.run(
                    [sys.executable, str(SCRIPT), option, value],
                    capture_output=True, text=True, check=False,
                )
                self.assertEqual(completed.returncode, 2)

    def test_wall_timeout_is_recorded_separately(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            runner = output / "slow-runner"
            runner.write_text("#!/usr/bin/env bash\nsleep 2\n", encoding="utf-8")
            runner.chmod(0o755)
            args = Namespace(
                out=output, galaxy=1, difficulty=7, victory=0, ticks=10,
                every=10, exe=None, timeout_seconds=0.02,
            )
            with patch("tools.run_ai_tournament.RUNNER", runner):
                run = run_match(args, 1, "codex", "classic", "a_alliance")
            self.assertEqual((run["status"], run["returncode"]), ("timeout", 124))
            self.assertEqual(aggregate([run], "codex", "classic")["timeouts"], 1)

    def test_runner_launch_error_is_failure(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            args = Namespace(
                out=output, galaxy=1, difficulty=7, victory=0, ticks=10,
                every=10, exe=None, timeout_seconds=None,
            )
            with patch("tools.run_ai_tournament.RUNNER", output / "missing-runner"):
                run = run_match(args, 1, "codex", "classic", "a_alliance")
            self.assertEqual((run["status"], run["returncode"]), ("failure", 127))

    def test_resume_rejects_changed_executable(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            executable = output / "fake-executable"
            executable.write_text("#!/usr/bin/env bash\nexit 2\n", encoding="utf-8")
            executable.chmod(0o755)
            command = [
                sys.executable, str(SCRIPT), "--out", str(output / "tournament"),
                "--seed-count", "1", "--exe", str(executable),
            ]
            first = subprocess.run(command, capture_output=True, text=True, check=False)
            self.assertEqual(first.returncode, 1)
            summary = output / "tournament" / "summary.json"
            config = json.loads(summary.read_text(encoding="utf-8"))["config"]
            self.assertEqual(config["exe"], str(executable_path(executable)))
            self.assertEqual(config["exe_sha256"], hashlib.sha256(executable.read_bytes()).hexdigest())
            self.assertEqual(config["exe_sha256"], sha256_file(executable))
            executable.write_text("#!/usr/bin/env bash\nexit 3\n", encoding="utf-8")
            resumed = subprocess.run(command + ["--resume"], capture_output=True, text=True, check=False)
            self.assertNotEqual(resumed.returncode, 0)
            self.assertIn("configuration differs", resumed.stderr)
            self.assertEqual(json.loads(summary.read_text(encoding="utf-8"))["config"], config)

    def test_binary_change_between_matches_preserves_completed_summary(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            executable = output / "self-modifying-executable"
            executable.write_text(
                "#!/usr/bin/env bash\nprintf '\\n# changed\\n' >> \"$0\"\nexit 2\n",
                encoding="utf-8",
            )
            executable.chmod(0o755)
            original_hash = sha256_file(executable)
            tournament = output / "tournament"
            completed = subprocess.run(
                [sys.executable, str(SCRIPT), "--out", str(tournament), "--seed-count", "1",
                 "--exe", str(executable)],
                capture_output=True, text=True, check=False,
            )
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("executable changed during tournament", completed.stderr)
            summary = json.loads((tournament / "summary.json").read_text(encoding="utf-8"))
            self.assertEqual(summary["config"]["exe_sha256"], original_hash)
            self.assertEqual(len(summary["runs"]), 1)
            self.assertNotEqual(sha256_file(executable), original_hash)


if __name__ == "__main__":
    unittest.main()
