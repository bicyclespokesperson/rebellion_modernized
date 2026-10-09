#!/usr/bin/env python3
"""List calls (and constructor calls) whose arguments contain two or more calls: the evaluation order of those arguments is unspecified
in C++. MSVC (and clang for the Microsoft ABI) evaluates them right to left, clang for the Itanium ABI left to right, so every such site
whose calls have side effects (random draws, id allocation, stream reads) behaves differently in the native build.

Runs in the rebellion-native container (clang.cindex), after the objects were built (compile_commands.json).
  python3 tools/find_arg_order.py build/native [--all] > findings.txt
"""
from __future__ import annotations

import argparse
import multiprocessing
from pathlib import Path

from clang import cindex

CK = cindex.CursorKind
CALLS = (CK.CALL_EXPR,)
SKIP_NAMES = {"static_cast", "reinterpret_cast", "const_cast", "operator[]", "operator->", "operator*", "operator&", "sizeof", "strlen"}


def calls_inside(cursor: "cindex.Cursor") -> list["cindex.Cursor"]:
    found = []
    for c in cursor.walk_preorder():
        if c.kind == CK.CALL_EXPR and c.spelling not in SKIP_NAMES and c.referenced is not None and not c.referenced.is_const_method():
            found.append(c)
    return found


def scan(job: tuple[str, list[str], str]):
    filename, arguments, resource = job
    index = cindex.Index.create()
    tu = index.parse(None, args=arguments + ["-resource-dir", resource, "-Wno-everything"])
    out = []
    for c in tu.cursor.walk_preorder():
        if c.kind != CK.CALL_EXPR or not c.location.file or "src/game" not in c.location.file.name:
            continue
        if c.location.file.name != filename:
            continue
        args = list(c.get_arguments())
        busy = [a for a in args if calls_inside(a)]
        if len(busy) >= 2:
            out.append((c.location.file.name, c.location.line, c.spelling, len(busy)))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("build", type=Path)
    ap.add_argument("--resource-dir", default="/usr/lib/llvm-19/lib/clang/19")
    args = ap.parse_args()
    db = cindex.CompilationDatabase.fromDirectory(str(args.build))
    jobs = []
    for cmd in db.getAllCompileCommands():
        if "src/game/" not in cmd.filename:
            continue
        keep = []
        skip = 0
        for a in list(cmd.arguments)[1:]:
            if skip:
                skip -= 1
            elif a in ("-o", "-MT", "-MF"):
                skip = 1
            elif a not in ("-c", "-MD") and a != cmd.filename:
                keep.append(a)
        jobs.append((cmd.filename, keep + [cmd.filename], args.resource_dir))
    with multiprocessing.Pool() as pool:
        results = pool.map(scan, jobs, chunksize=4)
    seen = set()
    for r in results:
        for f, line, name, n in r:
            if (f, line) in seen:
                continue
            seen.add((f, line))
            print(f"{f}:{line}: {name} ({n} arguments with calls)")
    print(f"# {len(seen)} sites")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
