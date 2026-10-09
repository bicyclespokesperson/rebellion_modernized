#!/usr/bin/env python3
"""Group the unresolved symbols of a failed standalone link by category (the categories of standalone_gaps.py).

Reads the lld-link output (`undefined symbol: X` lines), maps each symbol back to the original function/global it names
through symbols/symbols.csv exactly like gen_link_symbols.py does, and prints, per category, address, size (for
functions) and name. Symbols that name nothing in the original are listed under unresolved_other.

Runs inside the rebellion-dev container (needs llvm-undname-19).
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_link_symbols as g  # noqa: E402

UNDEFINED = re.compile(r"undefined symbol: (.+?)\s*$")
REFERENCED_BY = re.compile(r">>> referenced by (\S+)")
IMPORT_STUB = re.compile(r"^__imp_")


def parse_log(text: str) -> dict[str, str]:
    """symbol -> first referencing object/file."""
    out: dict[str, str] = {}
    current = None
    for line in text.splitlines():
        m = UNDEFINED.search(line)
        if m:
            current = m.group(1)
            out.setdefault(current, "")
            continue
        m = REFERENCED_BY.match(line.strip())
        if m and current and not out[current]:
            out[current] = m.group(1)
    return out


def classify(symbols: dict[str, str], symbols_dir: Path) -> dict[str, list[dict]]:
    table, ambiguous = g.load_csv(symbols_dir / "symbols.csv")
    imports = g.load_game_imports(symbols_dir / "imports.csv")
    classifier = g.GapClassifier.load(symbols_dir)
    hooked: set[int] = set()
    objs = [g.ObjSymbol(raw, "U", False) for raw in symbols]
    demangled = g.demangle_all(list(symbols))
    for s in objs:
        if (data_key := g.data_key_from_mangled(s.raw)) is not None:
            s.key = s.signature_key = data_key
        elif s.raw in demangled:
            s.key, s.signature_key = g.keys_from_demangled(demangled[s.raw])
        else:
            s.key = s.signature_key = g.c_symbol_key(s.raw)
    refs: list[tuple[str, str, g.OriginalSymbol]] = []
    other: list[dict] = []
    for s in objs:
        m = g.IMPORT_REF.match(s.raw)
        if m and m.group(1) in imports:
            refs.append((s.raw, "import", g.OriginalSymbol(imports[m.group(1)], "data", m.group(1))))
            continue
        try:
            orig = g.resolve(s, table, ambiguous)
        except LookupError as e:
            other.append({"symbol": s.raw, "note": str(e), "referenced_by": symbols[s.raw]})
            continue
        if orig is None:
            other.append({"symbol": s.raw, "demangled": demangled.get(s.raw, ""), "referenced_by": symbols[s.raw]})
        else:
            refs.append((s.raw, "csv", orig))
    gaps = g.build_gap_report(refs, hooked, classifier)["categories"]
    gaps["unresolved_other"] += other
    return gaps


def format_report(gaps: dict[str, list[dict]]) -> str:
    lines = []
    for category, entries in gaps.items():
        if not entries:
            continue
        size = sum(e.get("size", 0) for e in entries)
        lines.append(f"== {category}: {len(entries)} symbols" + (f", {size} bytes" if size else ""))
        for e in entries:
            size_text = f" {e['size']:6}" if "size" in e else ""
            name = e.get("name") or e.get("demangled") or e.get("note", "")
            lines.append(f"  {e.get('address', ''):10}{size_text} {name}  [{e['symbol']}]"
                         + (f"  <- {e['referenced_by']}" if e.get("referenced_by") else ""))
    return "\n".join(lines) + "\n"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--log", type=Path, required=True)
    ap.add_argument("--symbols", type=Path, default=Path(__file__).resolve().parent.parent / "symbols")
    ap.add_argument("--out", type=Path)
    ap.add_argument("--json", type=Path)
    args = ap.parse_args()
    symbols = parse_log(args.log.read_text(errors="replace"))
    if not symbols:
        print("standalone_link_report: no unresolved symbols in the log")
        return 0
    gaps = classify(symbols, args.symbols)
    report = format_report(gaps)
    summary = ", ".join(f"{c} {len(e)}" for c, e in gaps.items() if e)
    if args.out:
        args.out.write_text(report)
    if args.json:
        args.json.write_text(json.dumps(gaps, indent=1))
    print(f"standalone_link_report: {len(symbols)} unresolved symbols: {summary}"
          + (f" (details: {args.out})" if args.out else ""))
    if not args.out:
        print(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
