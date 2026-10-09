#!/usr/bin/env python3
"""Look up addresses in the standalone build's lld map (build/standalone/rebellion.map): the function that contains
each address, demangled. Wine prints crash addresses (`... at address 004507F3`) and backtraces; paste them here.

  tools/map_lookup.py 0x4507f3 0x450a10
  tools/map_lookup.py --log work/run/smoke/wine.log     # every address the log mentions
"""
from __future__ import annotations

import argparse
import bisect
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LINE = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8,16})\s+(?:f\s+)?(?:i\s+)?(\S.*)$")


def load_map(path: Path) -> tuple[list[int], list[tuple[str, str]]]:
    entries = []
    in_publics = False
    for line in path.read_text(errors="replace").splitlines():
        if "Publics by Value" in line:
            in_publics = True
            continue
        if not in_publics:
            continue
        m = LINE.match(line)
        if m and int(m.group(2), 16):
            entries.append((int(m.group(2), 16), m.group(1), m.group(3).strip()))
    entries.sort()
    return [e[0] for e in entries], [(e[1], e[2]) for e in entries]


def demangle(names: list[str]) -> list[str]:
    try:
        out = subprocess.run(["llvm-undname-19" if Path("/usr/bin/llvm-undname-19").exists() else "llvm-undname"],
                             input="\n".join(names) + "\n", capture_output=True, text=True).stdout.splitlines()
        pairs = [l for l in out if l.strip()]
        return [pairs[i + 1] if i + 1 < len(pairs) else names[i // 2] for i in range(0, len(pairs), 2)]
    except FileNotFoundError:
        return names


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("addresses", nargs="*")
    ap.add_argument("--map", type=Path, default=ROOT / "build" / "standalone" / "rebellion.map")
    ap.add_argument("--log", type=Path)
    args = ap.parse_args()
    addresses = [int(a, 16) for a in args.addresses]
    if args.log:
        addresses += [int(a, 16) for a in re.findall(r"\b(00[4-6][0-9a-fA-F]{5})\b", args.log.read_text(errors="replace"))]
    starts, names = load_map(args.map)
    for address in dict.fromkeys(addresses):
        i = bisect.bisect_right(starts, address) - 1
        if i < 0:
            print(f"{address:08x}  ?")
            continue
        symbol, obj = names[i]
        print(f"{address:08x}  {symbol}+{address - starts[i]:#x}  [{obj}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
