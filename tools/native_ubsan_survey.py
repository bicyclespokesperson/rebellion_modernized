#!/usr/bin/env -S uv run
"""Exercise loaded and fresh campaigns under a native UBSan build."""
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path


REPORT_PATTERN = re.compile(r"^(.+?:\d+:\d+): runtime error: (.+)$", re.MULTILINE)
TICK_PATTERN = re.compile(r"(?:started new game:|loaded .*:|saved) tick (\d+)")
ROOT = str(Path(__file__).resolve().parent.parent) + "/"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--load", type=Path, required=True, help="existing single-player save")
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--ticks", type=int, default=1000, help="ticks in each fresh campaign")
    parser.add_argument("--timeout", type=float, default=180, help="seconds allowed per process")
    return parser.parse_args()


def run_case(
    name: str,
    source: str,
    ticks: int,
    executable: Path,
    config: Path,
    output_dir: Path,
    timeout: float,
    ui: bool = False,
) -> dict[str, object]:
    destination = output_dir / f"{name}.sav"
    process_log = output_dir / f"{name}.process.log"
    game_log = Path(f"{destination}.log")
    destination.unlink(missing_ok=True)
    game_log.unlink(missing_ok=True)
    environment = os.environ.copy()
    environment.update(
        REBELLION_CONFIG=str(config),
        REB_DETERMINISTIC="1",
        REB_SIM=f"{source},ticks:{ticks},every:{max(1, ticks // 4)},save:{destination}",
        SDL_AUDIODRIVER="dummy",
        SDL_VIDEODRIVER="dummy",
        UBSAN_OPTIONS="halt_on_error=0:abort_on_error=0:print_stacktrace=1",
    )
    environment.pop("REB_UI_SMOKE", None)
    environment.pop("REB_SWITCH_SIDES_SMOKE", None)
    if ui:
        environment["REB_UI_SMOKE"] = "1"
        environment["REB_SWITCH_SIDES_SMOKE"] = "1"
    try:
        with process_log.open("w") as output:
            result = subprocess.run([str(executable)], env=environment, stdout=output, stderr=subprocess.STDOUT,
                                    timeout=timeout, check=False)
        exit_code = result.returncode
    except subprocess.TimeoutExpired:
        exit_code = -1
    output_text = process_log.read_text(errors="replace")
    reports = [f"{location.removeprefix(ROOT)}  {re.sub(r'0x[0-9a-f]+', 'ADDR', message)}"
               for location, message in REPORT_PATTERN.findall(output_text)]
    log_text = game_log.read_text(errors="replace") if game_log.is_file() else ""
    found_ticks = [int(value) for value in TICK_PATTERN.findall(log_text)]
    progressed = len(found_ticks) >= 2 and found_ticks[-1] >= found_ticks[0] + ticks
    gameplay_passed = (exit_code == 0 and destination.is_file() and destination.stat().st_size > 0
                       and progressed and "references ok" in log_text and "returning for normal teardown" in log_text
                       and (not ui or ("UI visibility checks passed" in log_text
                                       and "Switch sides checks passed" in log_text)))
    passed = gameplay_passed and not reports
    print(f"{name}: {'passed' if passed else 'failed'}; gameplay {'passed' if gameplay_passed else 'failed'}; "
          f"{len(reports)} UBSan reports; exit {exit_code}", flush=True)
    return {"name": name, "sim": environment["REB_SIM"], "ui_smoke": ui,
            "passed": passed, "gameplay_passed": gameplay_passed, "exit_code": exit_code,
            "start_tick": found_ticks[0] if found_ticks else None,
            "end_tick": found_ticks[-1] if found_ticks else None, "reports": sorted(set(reports)),
            "report_count": len(reports), "save": str(destination), "process_log": str(process_log),
            "game_log": str(game_log)}


def main() -> int:
    args = parse_args()
    if args.ticks < 1 or args.timeout <= 0:
        raise SystemExit("--ticks and --timeout must be positive")
    executable = args.exe.resolve(strict=True)
    executable_hash = hashlib.sha256(executable.read_bytes()).hexdigest()
    config = args.config.resolve(strict=True)
    loaded = args.load.resolve(strict=True)
    output_dir = args.output_dir.resolve()
    if "," in str(output_dir) or "," in str(loaded):
        raise SystemExit("REB_SIM paths cannot contain commas")
    output_dir.mkdir(parents=True, exist_ok=True)
    short_ticks = min(args.ticks, 50)
    cases = [
        ("loaded_ui", f"load:{loaded}", short_ticks, True),
        ("reloaded_ui", f"load:{output_dir / 'loaded_ui.sav'}", short_ticks, True),
        ("new_small", "new:1,seed:17,difficulty:7,ai1:codex,ai2:classic", args.ticks, False),
        ("new_medium", "new:2,seed:23,difficulty:7,ai1:classic,ai2:codex", args.ticks, False),
        ("new_large", "new:3,seed:31,difficulty:7,ai1:classic,ai2:classic", args.ticks, False),
        ("reloaded_medium", f"load:{output_dir / 'new_medium.sav'},ai1:classic,ai2:codex",
         short_ticks, False),
    ]
    results: list[dict[str, object]] = []
    for name, source, ticks, ui in cases:
        results.append(run_case(name, source, ticks, executable, config, output_dir, args.timeout, ui))
    report_locations = sorted({report for result in results for report in result["reports"]})
    unchanged_binary = hashlib.sha256(executable.read_bytes()).hexdigest() == executable_hash
    summary = {"exe": str(executable), "exe_sha256": executable_hash, "unchanged_binary": unchanged_binary,
               "config": str(config), "load": str(loaded),
               "distinct_reports": report_locations, "cases": results}
    summary_path = output_dir / "summary.json"
    summary_path.write_text(json.dumps(summary, indent=2) + "\n")
    print(f"UBSan: {len(report_locations)} distinct locations across {len(results)} cases; {summary_path}")
    return 1 if report_locations or not unchanged_binary or any(not result["passed"] for result in results) else 0


if __name__ == "__main__":
    raise SystemExit(main())
