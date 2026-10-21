#!/usr/bin/env -S uv run
"""Exercise a campaign Take Command battle, save/reload, and immediate result with SDL dummy drivers."""
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--load", type=Path, help="optional campaign save; otherwise start a small galaxy")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--timeout", type=float, default=120)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    destination = args.output_dir.resolve() / "live-smoke.sav"
    source = f"load:{args.load.resolve()}" if args.load else f"new:1,seed:{args.seed}"
    if (args.load and "," in str(args.load.resolve())) or "," in str(destination):
        parser.error("REB_SIM save and load paths cannot contain commas")
    environment = os.environ.copy()
    environment.update({
        "SDL_VIDEODRIVER": "dummy",
        "SDL_AUDIODRIVER": "dummy",
        "REBELLION_CONFIG": str(args.config.resolve()),
        "REB_DETERMINISTIC": "1",
        "REB_LIVE_BATTLE_SMOKE": "1",
        "REB_SIM": f"{source},ticks:0,save:{destination}",
    })
    with (args.output_dir / "process.log").open("w") as output:
        subprocess.run([str(args.exe.resolve())], env=environment, stdout=output, stderr=subprocess.STDOUT,
                       timeout=args.timeout, check=True)
    log = destination.with_suffix(".sav.log").read_text()
    if "Live battle smoke passed" not in log:
        raise RuntimeError(f"Live smoke success marker missing; see {destination}.log")
    for suffix in ("", ".live.sav"):
        path = Path(str(destination) + suffix)
        if not path.is_file() or path.stat().st_size == 0:
            raise RuntimeError(f"Live smoke did not write {path}")
    print(f"Live battle smoke passed; saves, log, and review captures: {args.output_dir.resolve()}")


if __name__ == "__main__":
    main()
