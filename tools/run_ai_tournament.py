#!/usr/bin/env -S uv run
"""Run paired AI matches with each seed played from both sides."""

# /// script
# dependencies = []
# ///

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import signal
import subprocess
import sys
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parent.parent
RUNNER = ROOT / "tools" / "run_ai_match.sh"


def executable_path(selected: Path | None) -> Path:
    if selected is not None:
        return selected.resolve()
    if sys.platform == "darwin":
        return (ROOT / "build" / "native" / "Rebellion.app" / "Contents" / "MacOS" / "Rebellion").resolve()
    return (ROOT / "build" / "native" / "rebellion").resolve()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=ROOT / "work" / "ai_tournament")
    parser.add_argument("--seed-start", type=int, default=1)
    parser.add_argument("--seed-count", type=int, default=10)
    parser.add_argument("--ai-a", default="codex")
    parser.add_argument("--ai-b", default="classic")
    parser.add_argument("--galaxy", type=int, choices=(1, 2, 3), default=1)
    parser.add_argument("--difficulty", type=int, choices=range(8), default=7)
    parser.add_argument("--victory", type=int, choices=(0, 1), default=0)
    parser.add_argument("--ticks", type=int, default=100000)
    parser.add_argument("--every", type=int, default=500)
    parser.add_argument("--timeout-seconds", type=float, help="Maximum wall time for each match")
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--resume", action="store_true", help="Reuse completed runs in summary.json")
    args = parser.parse_args()
    if args.seed_start < 0 or args.seed_count < 1 or args.ticks < 1 or args.every < 1:
        parser.error("seed start must be nonnegative; seed count, ticks, and every must be positive")
    if args.seed_start + args.seed_count - 1 > 0xFFFFFFFF or args.ticks > 0xFFFFFFFF or args.every > 0xFFFFFFFF:
        parser.error("seed range, ticks, and every must fit in 32 bits")
    if args.timeout_seconds is not None and (not math.isfinite(args.timeout_seconds) or args.timeout_seconds <= 0):
        parser.error("--timeout-seconds must be positive")
    if args.ai_a == args.ai_b:
        parser.error("--ai-a and --ai-b must name different controllers")
    return args


def write_json(path: Path, data: dict[str, Any]) -> None:
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    temporary.replace(path)


def classify(returncode: int, result: dict[str, Any] | None, ai1: str, ai2: str, seed: int) -> str:
    if returncode < 0 or returncode >= 128:
        return "crash"
    if result is None or result.get("ai1") != ai1 or result.get("ai2") != ai2 or result.get("seed") != seed:
        return "failure"
    if returncode == 0 and result.get("outcome") == "victory" and result.get("winner") in (1, 2):
        return "win"
    if returncode == 7 and result.get("outcome") == "tick_limit" and result.get("winner") == 0:
        return "tick_limit"
    return "failure"


def run_match(args: argparse.Namespace, seed: int, ai1: str, ai2: str, label: str) -> dict[str, Any]:
    save = args.out / f"seed_{seed}_{label}.sav"
    command = [
        str(RUNNER), "--out", str(save), "--seed", str(seed),
        "--galaxy", str(args.galaxy), "--difficulty", str(args.difficulty),
        "--victory", str(args.victory), "--ticks", str(args.ticks),
        "--every", str(args.every), "--ai1", ai1, "--ai2", ai2,
    ]
    if args.exe is not None:
        command.extend(("--exe", str(args.exe)))
    timed_out = False
    try:
        with (args.out / f"seed_{seed}_{label}.runner.log").open("w", encoding="utf-8") as log:
            with subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True) as process:
                try:
                    returncode = process.wait(timeout=args.timeout_seconds)
                except subprocess.TimeoutExpired:
                    timed_out = True
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    process.wait()
                    returncode = 124
    except OSError as error:
        returncode = 127
        (args.out / f"seed_{seed}_{label}.runner.log").write_text(f"{error}\n", encoding="utf-8")
    result_path = Path(f"{save}.result.json")
    try:
        result = json.loads(result_path.read_text(encoding="utf-8"))
        if not isinstance(result, dict):
            result = None
    except (OSError, json.JSONDecodeError):
        result = None
    return {
        "seed": seed, "ai1": ai1, "ai2": ai2, "save": str(save),
        "returncode": returncode, "status": "timeout" if timed_out else classify(returncode, result, ai1, ai2, seed),
        "result": result,
    }


def aggregate(runs: list[dict[str, Any]], ai_a: str, ai_b: str) -> dict[str, Any]:
    counts: dict[str, Any] = {
        "wins": {ai_a: 0, ai_b: 0},
        "wins_by_side": {"alliance": 0, "empire": 0},
        "tick_limits": 0,
        "timeouts": 0,
        "crashes": 0,
        "failures": 0,
    }
    territory: dict[str, dict[str, list[int]]] = {
        ai_a: {"alliance": [], "empire": []},
        ai_b: {"alliance": [], "empire": []},
    }
    neutral: list[int] = []
    for run in runs:
        result = run.get("result")
        if run["status"] == "win":
            winner = run["ai1"] if result["winner"] == 1 else run["ai2"]
            counts["wins"][winner] += 1
            counts["wins_by_side"]["alliance" if result["winner"] == 1 else "empire"] += 1
        elif run["status"] == "tick_limit":
            counts["tick_limits"] += 1
        elif run["status"] == "timeout":
            counts["timeouts"] += 1
        elif run["status"] == "crash":
            counts["crashes"] += 1
        else:
            counts["failures"] += 1
        if run["status"] in ("win", "tick_limit"):
            systems = result.get("systems", {})
            if all(isinstance(systems.get(side), int) for side in ("alliance", "empire", "neutral")):
                territory[run["ai1"]]["alliance"].append(systems["alliance"])
                territory[run["ai2"]]["empire"].append(systems["empire"])
                neutral.append(systems["neutral"])
    counts["completed"] = len(runs)
    counts["territory"] = {
        "mean_systems_by_ai_and_side": {
            ai: {
                side: sum(values) / len(values) if values else None
                for side, values in sides.items()
            }
            for ai, sides in territory.items()
        },
        "mean_neutral_systems": sum(neutral) / len(neutral) if neutral else None,
        "samples": len(neutral),
    }
    return counts


def main() -> int:
    args = arguments()
    args.exe = executable_path(args.exe)
    try:
        executable_hash = sha256_file(args.exe)
    except OSError as error:
        raise SystemExit(f"Cannot read game executable {args.exe}: {error}") from error
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=True)
    summary_path = args.out / "summary.json"
    config = {
        "seed_start": args.seed_start, "seed_count": args.seed_count,
        "ai_a": args.ai_a, "ai_b": args.ai_b, "galaxy": args.galaxy,
        "difficulty": args.difficulty, "victory": args.victory,
        "ticks": args.ticks, "every": args.every,
        "timeout_seconds": args.timeout_seconds,
        "exe": str(args.exe), "exe_sha256": executable_hash,
    }
    runs: list[dict[str, Any]] = []
    if args.resume and summary_path.exists():
        previous = json.loads(summary_path.read_text(encoding="utf-8"))
        if previous.get("config") != config:
            raise SystemExit("Cannot resume: summary.json configuration differs from this invocation")
        runs = previous["runs"]
    finished = {(run["seed"], run["ai1"], run["ai2"]) for run in runs}
    for seed in range(args.seed_start, args.seed_start + args.seed_count):
        for ai1, ai2, label in ((args.ai_a, args.ai_b, "a_alliance"), (args.ai_b, args.ai_a, "b_alliance")):
            if (seed, ai1, ai2) in finished:
                continue
            try:
                current_hash = sha256_file(args.exe)
            except OSError as error:
                raise SystemExit(f"Cannot recheck game executable {args.exe}: {error}") from error
            if current_hash != executable_hash:
                raise SystemExit(
                    f"Game executable changed during tournament: {args.exe}. "
                    f"Completed results remain in {summary_path}; start a new output directory."
                )
            print(f"seed {seed}: Alliance {ai1}, Empire {ai2}", flush=True)
            run = run_match(args, seed, ai1, ai2, label)
            runs.append(run)
            totals = aggregate(runs, args.ai_a, args.ai_b)
            write_json(summary_path, {"config": config, "totals": totals, "runs": runs})
            outcome = run["status"]
            if outcome == "win":
                winner = run["result"]["winner"]
                faction = "Alliance" if winner == 1 else "Empire"
                outcome = f"winner: {run['ai1'] if winner == 1 else run['ai2']} ({faction})"
            print(f"  {outcome} (exit {run['returncode']}); summary: {summary_path}", flush=True)
    totals = aggregate(runs, args.ai_a, args.ai_b)
    print(json.dumps(totals, indent=2), flush=True)
    return 1 if totals["failures"] or totals["crashes"] or totals["timeouts"] else 0


if __name__ == "__main__":
    sys.exit(main())
