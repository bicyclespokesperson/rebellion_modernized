#!/usr/bin/env python3
"""Find the reimplemented function behind a crash that only shows in long live runs (difftest can't see it).

Each round splits the suspect hooks into K chunks and runs the scenario K times in parallel, each run with one
chunk disabled (REB_DISABLE_HOOKS). A chunk whose disabling makes the crash disappear contains the culprit (this also
works when the culprit needs other reimplemented code around it). Repeats until one function is left.

  bisect_crash.py tests/scenarios/soak_fast.txt                  # crash = 'Unhandled' in wine.log
  bisect_crash.py --pattern 'page fault' -k 6 tests/scenarios/soak_fast.txt
"""
from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def hooks() -> dict[int, str]:
    report = json.loads((ROOT / "build" / "hybrid" / "gen" / "report.json").read_text())
    return {int(a, 16): name for a, name in report["reimplemented"].items()}


def crashes(scenario: Path, disabled: list[int], tag: str, pattern: re.Pattern) -> bool:
    out = ROOT / "work" / "run" / f"bisect_{tag}"
    env = dict(os.environ, REB_DISABLE_HOOKS=",".join(f"{a:x}" for a in disabled) or "0")
    subprocess.run([str(ROOT / "tools" / "run_game.sh"), "--out", str(out), str(scenario)], env=env,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    log = out / "wine.log"
    return bool(log.exists() and pattern.search(log.read_text(errors="replace")))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scenario", type=Path)
    ap.add_argument("--pattern", default="Unhandled", help="regex in wine.log that marks the crash")
    ap.add_argument("-k", type=int, default=4, help="chunks (parallel runs) per round")
    args = ap.parse_args()
    pattern = re.compile(args.pattern)
    names = hooks()
    suspects = sorted(names)
    print(f"{len(suspects)} hooks; checking the crash reproduces with all of them...", flush=True)
    if not crashes(args.scenario, [], "baseline", pattern):
        print("no crash with all hooks enabled — nothing to bisect")
        return 1
    rnd = 0
    while len(suspects) > 1:
        rnd += 1
        size = -(-len(suspects) // args.k)
        chunks = [suspects[i:i + size] for i in range(0, len(suspects), size)]
        with ThreadPoolExecutor(len(chunks)) as pool:
            results = list(pool.map(lambda ic: crashes(args.scenario, ic[1], f"r{rnd}_{ic[0]}", pattern),
                                    enumerate(chunks)))
        culprits = [c for c, crashed in zip(chunks, results) if not crashed]
        print(f"round {rnd}: {len(suspects)} suspects in {len(chunks)} chunks; crash disappears when disabling "
              f"chunk(s) {[i for i, r in enumerate(results) if not r]}", flush=True)
        if len(culprits) != 1:
            print("ambiguous (0 or several chunks matter — flaky crash or several culprits); stopping with:")
            for c in chunks:
                print(f"  {c[0]:#x}..{c[-1]:#x} ({len(c)})")
            return 2
        suspects = culprits[0]
    print(f"culprit: {suspects[0]:#010x} {names[suspects[0]]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
