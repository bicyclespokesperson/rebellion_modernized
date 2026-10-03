#!/usr/bin/env python3
"""Edit symbols/symbols.csv (the source of truth for original-binary names) and mirror it into Ghidra.

  sym.py set 0x5f35b0 func String::String "(char const *)"   add/rename (signature optional)
  sym.py set 0x6be668 data g_directSound
  sym.py get 0x5f35b0 | String::String                       look up by address or name
  sym.py --no-ghidra set ...                                  skip the Ghidra update (server not running)

The Ghidra copy is updated through tools/gq and saved; if the server is down, the CSV is still updated and
`tools/gq -f tools/py/apply_symbols.py` re-syncs later.
"""
from __future__ import annotations

import argparse
import csv
import fcntl
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CSV = ROOT / "symbols" / "symbols.csv"
FIELDS = ["address", "kind", "name", "signature", "size"]


def load() -> list[dict[str, str]]:
    with open(CSV, newline="") as fh:
        return list(csv.DictReader(fh))


def save(rows: list[dict[str, str]]) -> None:
    rows.sort(key=lambda r: (int(r["address"], 16), r["kind"]))
    with open(CSV, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=FIELDS, lineterminator="\n")
        w.writeheader()
        w.writerows({k: r.get(k, "") for k in FIELDS} for r in rows)


def ghidra(code: str) -> str:
    return subprocess.run([str(ROOT / "tools" / "gq"), code], capture_output=True, text=True).stdout


def cmd_set(args) -> int:
    address = int(args.address, 16)
    with open(CSV.with_suffix(".lock"), "w") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        rows = [r for r in load() if not (int(r["address"], 16) == address and r["kind"] == args.kind)]
        rows.append({"address": f"0x{address:08x}", "kind": args.kind, "name": args.name,
                     "signature": args.signature or "", "size": ""})
        save(rows)
    if not args.no_ghidra:
        fn = "name_func" if args.kind == "func" else "name_data"
        out = ghidra(f"{fn}({address}, {json.dumps(args.name)})")
        if out.strip():
            print("ghidra:", out.strip(), file=sys.stderr)
            return 1
        ghidra_save = subprocess.run([str(ROOT / "tools" / "gq"), "--save"], capture_output=True, text=True)
        if "saved" not in ghidra_save.stdout:
            print("ghidra save failed:", ghidra_save.stdout, ghidra_save.stderr, file=sys.stderr)
            return 1
    print(f"0x{address:08x} {args.kind} {args.name}{args.signature or ''}")
    return 0


def cmd_get(args) -> int:
    q = args.query
    hits = [r for r in load() if r["name"] == q or (q.startswith("0x") and int(r["address"], 16) == int(q, 16))]
    for r in hits:
        print(f'{r["address"]} {r["kind"]} {r["name"]}{r["signature"]}')
    return 0 if hits else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--no-ghidra", action="store_true")
    sub = ap.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("set")
    s.add_argument("address")
    s.add_argument("kind", choices=["func", "data"])
    s.add_argument("name")
    s.add_argument("signature", nargs="?")
    g = sub.add_parser("get")
    g.add_argument("query")
    args = ap.parse_args()
    return cmd_set(args) if args.cmd == "set" else cmd_get(args)


if __name__ == "__main__":
    sys.exit(main())
