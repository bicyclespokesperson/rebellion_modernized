#!/usr/bin/env -S uv run
"""Run the modern native save/reload smoke in two separate processes."""
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
from __future__ import annotations

import argparse
import os
import re
import subprocess
from pathlib import Path


TICK_PATTERN = re.compile(r"(?:loaded .*:|saved) tick (\d+)")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True, help="native Rebellion executable")
    parser.add_argument("--config", type=Path, required=True, help="config.ini for the game installation")
    parser.add_argument("--load", type=Path, required=True, help="save to load in the first process")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--ticks", type=int, default=10, help="ticks to advance in each process")
    parser.add_argument("--timeout", type=float, default=60, help="maximum seconds allowed for each process")
    return parser.parse_args()


def run_phase(
    executable: Path, config: Path, source: Path, destination: Path, ticks: int, timeout: float
) -> tuple[int, int]:
    if "," in str(source.resolve()) or "," in str(destination.resolve()):
        raise ValueError("REB_SIM does not support commas in load or save paths")
    log = destination.with_suffix(destination.suffix + ".log")
    destination.unlink(missing_ok=True)
    log.unlink(missing_ok=True)
    environment = os.environ.copy()
    environment.update(
        {
            "REBELLION_CONFIG": str(config.resolve()),
            "REB_DETERMINISTIC": "1",
            "REB_SIM": f"load:{source.resolve()},ticks:{ticks},save:{destination.resolve()}",
            "SDL_AUDIODRIVER": "dummy",
            "SDL_VIDEODRIVER": "dummy",
        }
    )
    try:
        result = subprocess.run([str(executable.resolve())], env=environment, check=False, timeout=timeout)
    except subprocess.TimeoutExpired as error:
        raise RuntimeError(f"{executable} exceeded the {timeout:g}-second timeout; see {log}") from error
    if result.returncode != 0:
        raise RuntimeError(f"{executable} exited with {result.returncode}; see {log}")
    if not destination.is_file() or destination.stat().st_size == 0:
        raise RuntimeError(f"smoke did not write {destination}")
    text = log.read_text()
    if "references ok" not in text or "returning for normal teardown" not in text:
        raise RuntimeError(f"smoke did not complete validation and teardown; see {log}")
    ticks_found = [int(value) for value in TICK_PATTERN.findall(text)]
    if len(ticks_found) < 2 or ticks_found[-1] < ticks_found[0] + ticks:
        raise RuntimeError(f"smoke did not advance {ticks} ticks; see {log}")
    return ticks_found[0], ticks_found[-1]


def main() -> None:
    args = parse_args()
    if args.ticks < 1:
        raise ValueError("--ticks must be positive")
    if args.timeout <= 0:
        raise ValueError("--timeout must be positive")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    first_save = args.output_dir / "first.sav"
    second_save = args.output_dir / "second.sav"
    first_ticks = run_phase(args.exe, args.config, args.load, first_save, args.ticks, args.timeout)
    second_ticks = run_phase(args.exe, args.config, first_save, second_save, args.ticks, args.timeout)
    if second_ticks[0] != first_ticks[1]:
        raise RuntimeError(f"reload started at tick {second_ticks[0]}, expected {first_ticks[1]}")
    print(f"modern smoke passed: {first_ticks[0]} -> {first_ticks[1]} -> {second_ticks[1]}")


if __name__ == "__main__":
    main()
