#!/usr/bin/env python3
"""Find the reimplemented function behind a difference between a save written by the original code and one written by the hybrid.

First the scenario runs with every hook disabled (the original code) to get the reference save, then with all hooks enabled to
check the difference reproduces. Each round then splits the suspect hooks into K chunks and runs the scenario K times in
parallel, each with one chunk disabled (REB_DISABLE_HOOKS); the chunk whose disabling makes the compared byte range equal the
reference again holds the culprit. Repeats until one function is left (same machinery as bisect_crash.py).

The scenario must be deterministic up to the compared range: use the long-run switches (tools/long_compare.sh documents
them) and compare a range that does not hold UI noise, e.g. the game manager's tick/day/serial counters at 0x6a..0x7a:

  REB_DETERMINISTIC=1 REB_QPC_FAST_UNTIL_TICK=153862 REB_QPC_START_KEY=145 REB_QPC_HOLD_UNTIL_KEY=145 REB_CLOCK_FROM_QPC=1 \\
  REB_NO_TIMERS=1 REB_VIRTUAL_SOUND=1 tools/bisect_save.py --range 0x6a:0x10 tests/scenarios/long_battle.txt

Every round prints what each chunk's run produced ("ref" = the original's bytes, "all" = the all-hooks bytes, "none" = no save,
usually a crash of that mix of original and reconstructed code). When disabling any one chunk keeps the all-hooks bytes but one
chunk's run writes no save, the culprit is most likely in that chunk: continue inside it with --within START:END (hex addresses).
--reference and --all-hooks (hex bytes printed by an earlier run) skip the two baseline runs.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def hooks() -> dict[int, str]:
    report = json.loads((ROOT / "build" / "hybrid" / "gen" / "report.json").read_text())
    return {int(a, 16): name for a, name in report["reimplemented"].items()}


def run(scenario: Path, disabled: str, tag: str, save: str, start: int, length: int, enable_only: bool = False) -> bytes | None:
    out = ROOT / "work" / "run" / f"bisect_save_{tag}"
    env = dict(os.environ, **({"REB_ENABLE_HOOKS": disabled} if enable_only else {"REB_DISABLE_HOOKS": disabled}))
    subprocess.run([str(ROOT / "tools" / "run_game.sh"), "--out", str(out), str(scenario)], env=env,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    path = out / "SaveGame" / save
    return path.read_bytes()[start:start + length] if path.exists() else None


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("scenario", type=Path)
    ap.add_argument("--save", default="SAVEGAME.005", help="save file the scenario writes")
    ap.add_argument("--range", default="0x6a:0x10", help="START:LENGTH of the bytes that must match the original code's")
    ap.add_argument("-k", type=int, default=6, help="chunks (parallel runs) per round")
    ap.add_argument("--enable-only", action="store_true",
                    help="each run installs only one chunk's hooks (REB_ENABLE_HOOKS); a chunk whose run differs from the "
                         "original holds a function that alone causes the difference (for several independent culprits, "
                         "where disabling any one chunk changes nothing)")
    ap.add_argument("--within", help="START:END: only hooks in this address range are suspects (the rest stay enabled)")
    ap.add_argument("--reference", help="hex bytes of the original code's range (skips the reference run)")
    ap.add_argument("--all-hooks", help="hex bytes of the all-hooks range (skips the baseline run)")
    args = ap.parse_args()
    start, length = (int(x, 0) for x in args.range.split(":"))
    names = hooks()
    suspects = sorted(names)
    if args.within:
        low, high = (int(x, 16) for x in args.within.split(":"))
        suspects = [a for a in suspects if low <= a <= high]

    if args.reference:
        reference = bytes.fromhex(args.reference)
    else:
        reference = run(args.scenario, "all", "reference", args.save, start, length)
    if reference is None:
        print("the original-code run wrote no save")
        return 1
    print(f"reference ({args.range}): {reference.hex()}", flush=True)
    everything = bytes.fromhex(args.all_hooks) if args.all_hooks else run(args.scenario, "0", "baseline", args.save, start, length)
    if everything == reference:
        print("the hybrid with all hooks matches the original here — nothing to bisect")
        return 1
    print(f"all hooks:  {everything.hex() if everything else None}", flush=True)

    rnd = 0
    while len(suspects) > 1:
        rnd += 1
        size = -(-len(suspects) // args.k)
        chunks = [suspects[i:i + size] for i in range(0, len(suspects), size)]
        with ThreadPoolExecutor(len(chunks)) as pool:
            results = list(pool.map(lambda ic: run(args.scenario, ",".join(f"{a:x}" for a in ic[1]), f"r{rnd}_{ic[0]}",
                                                   args.save, start, length, args.enable_only), enumerate(chunks)))
        if args.enable_only:
            culprits = [c for c, got in zip(chunks, results) if got is not None and got != reference][:1]
        else:
            culprits = [c for c, got in zip(chunks, results) if got == reference]
        for i, (c, got) in enumerate(zip(chunks, results)):
            label = "ref" if got == reference else "all" if got == everything else "none" if got is None else got.hex()
            print(f"  chunk {i} {c[0]:#x}..{c[-1]:#x} ({len(c)}): {label}", flush=True)
        what = "installing only" if args.enable_only else "disabling"
        print(f"round {rnd}: {len(suspects)} suspects in {len(chunks)} chunks; matches the original when {what} "
              f"chunk(s) {[i for i, r in enumerate(results) if r == reference]}", flush=True)
        if len(culprits) != 1:
            print("ambiguous (0 or several chunks matter: flaky run or several culprits); stopping with:")
            for c in chunks:
                print(f"  {c[0]:#x}..{c[-1]:#x} ({len(c)})")
            return 2
        suspects = culprits[0]
    print(f"culprit: {suspects[0]:#010x} {names[suspects[0]]}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
