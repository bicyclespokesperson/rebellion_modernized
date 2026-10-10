#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# ///
"""Rewrite the MSVC calling-convention keywords of src/game (outside platform/) to the REB_* macros of core/reb_abi.h.

Run it after merging a branch that was written before P0b: `tools/reb_abi_sweep.py` (idempotent; comments and string literals are left alone,
core/reb_abi.h is included where a file starts using a macro).
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TOKENS = {"__thiscall": "REB_THISCALL", "__stdcall": "REB_STDCALL", "__cdecl": "REB_CDECL"}
KEYWORD = re.compile(r"\b(__thiscall|__stdcall|__cdecl)\b")
SKIP_DIRS = {"platform"}
SKIP_FILES = {"crt/x87_math.cpp", "crt/floor.cpp", "audio/interlocked_import.h", "core/reb_abi.h"}
INCLUDE = '#include "core/reb_abi.h"'


def convert_line(line: str) -> str:
    out: list[str] = []
    i = 0
    quote: str | None = None
    while i < len(line):
        c = line[i]
        if quote:
            out.append(c)
            if c == "\\" and i + 1 < len(line):
                out.append(line[i + 1])
                i += 2
                continue
            if c == quote:
                quote = None
            i += 1
            continue
        if c in "\"'":
            quote = c
            out.append(c)
            i += 1
            continue
        if line.startswith("//", i):
            out.append(line[i:])
            break
        m = KEYWORD.match(line, i)
        if m and (i == 0 or not (line[i - 1].isalnum() or line[i - 1] == "_")):
            out.append(TOKENS[m.group(1)])
            i = m.end()
            continue
        out.append(c)
        i += 1
    return "".join(out)


def convert_text(text: str) -> str:
    in_block = False
    lines: list[str] = []
    for line in text.split("\n"):
        if in_block:
            lines.append(line)
            in_block = "*/" not in line
            continue
        if line.lstrip().startswith("/*") and "*/" not in line:
            in_block = True
            lines.append(line)
            continue
        lines.append(convert_line(line))
    return "\n".join(lines)


def add_include(text: str, is_header: bool) -> str:
    if INCLUDE in text:
        return text
    lines = text.split("\n")
    first_include = next((k for k, l in enumerate(lines) if l.startswith("#include")), None)
    pragma_once = next((k for k, l in enumerate(lines) if l.strip() == "#pragma once"), None)
    if is_header and pragma_once is not None:
        lines.insert(pragma_once + 1, INCLUDE)
    elif first_include is not None:
        lines.insert(first_include, INCLUDE)
    else:
        lines.insert(0, INCLUDE)
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("paths", nargs="*", type=Path, help="files to rewrite (default: every .h/.cpp under src/game)")
    ap.add_argument("--check", action="store_true", help="only list the files that would change")
    args = ap.parse_args()
    game = ROOT / "src" / "game"
    files = args.paths or sorted(p for p in game.rglob("*") if p.suffix in (".h", ".cpp"))
    changed = 0
    for path in files:
        rel = path.resolve().relative_to(game).as_posix()
        if rel.split("/")[0] in SKIP_DIRS or rel in SKIP_FILES:
            continue
        text = path.read_text()
        converted = convert_text(text)
        if converted == text:
            continue
        converted = add_include(converted, path.suffix == ".h")
        changed += 1
        print(rel)
        if not args.check:
            path.write_text(converted)
    print(f"{changed} files {'would change' if args.check else 'rewritten'}")
    return 1 if args.check and changed else 0


if __name__ == "__main__":
    raise SystemExit(main())
