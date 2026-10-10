#!/usr/bin/env python3
"""List the floating-point to integer conversions in src/ (game and port) (explicit casts and implicit conversions).

x86 and arm64 disagree on what C leaves undefined here: an out-of-range or NaN double gives 0x80000000 on x86 (cvttsd2si, the original's
_ftol) but saturates on arm64, and a negative value converted to an unsigned type wraps on x86-64 and gives 0 on arm64. The sites listed
are the ones that need `reb_ftol` (core/reb_x87.h) if their operand can leave the destination's range. Pair with a run of the
sanitizer build `REB_NATIVE_SANITIZE=address,undefined,float-cast-overflow` (docs/native.md), which reports the ones that do.

Runs in the rebellion-native container, against a configured build directory (compile_commands.json):
  tools/container/native_run.sh 'python3 tools/find_float_casts.py --build build/native [--unsigned-only] [-j 16]'
"""
from __future__ import annotations

import argparse
import json
import multiprocessing
import shlex
from pathlib import Path

from clang import cindex

K = cindex.TypeKind
FLOATS = {K.FLOAT, K.DOUBLE, K.LONGDOUBLE}
INTEGERS = {K.CHAR_S, K.SCHAR, K.UCHAR, K.SHORT, K.USHORT, K.INT, K.UINT, K.LONG, K.ULONG, K.LONGLONG, K.ULONGLONG, K.ENUM}
UNSIGNED_SPELLINGS = {"unsigned int", "unsigned short", "unsigned char", "unsigned long", "unsigned long long"}
CAST_KINDS = {
    cindex.CursorKind.CSTYLE_CAST_EXPR,
    cindex.CursorKind.CXX_STATIC_CAST_EXPR,
    cindex.CursorKind.CXX_FUNCTIONAL_CAST_EXPR,
    cindex.CursorKind.UNEXPOSED_EXPR,
}


def canonical_kind(t: "cindex.Type") -> "cindex.TypeKind":
    return t.get_canonical().kind


def float_conversions(cursor, root: Path, found: set[tuple[str, int, str, str]]) -> None:
    for node in cursor.walk_preorder():
        if node.kind not in CAST_KINDS | {cindex.CursorKind.COMPOUND_ASSIGNMENT_OPERATOR} or not node.location.file:
            continue
        path = Path(node.location.file.name)
        if root not in path.parents:
            continue
        if node.kind == cindex.CursorKind.COMPOUND_ASSIGNMENT_OPERATOR:
            operands = list(node.get_children())
            if len(operands) == 2 and canonical_kind(operands[0].type) in INTEGERS and canonical_kind(operands[1].type) in FLOATS:
                found.add((str(path.relative_to(root)), node.location.line, operands[0].type.get_canonical().spelling,
                           operands[1].type.spelling + " (compound assignment)"))
            continue
        if canonical_kind(node.type) not in INTEGERS:
            continue
        children = list(node.get_children())
        if len(children) != 1 or canonical_kind(children[0].type) not in FLOATS:
            continue
        found.add((str(path.relative_to(root)), node.location.line, node.type.get_canonical().spelling, children[0].type.spelling))


def scan(entry: dict) -> list[tuple[str, int, str, str]]:
    root = Path(entry["root"])
    args = shlex.split(entry["command"])[1:]
    args = [a for a in args if a not in ("-c", "-fPIE") and not a.startswith("-o")]
    args = [a for a in args if a != entry["file"]]
    index = cindex.Index.create()
    tu = index.parse(entry["file"], args=args)
    found: set[tuple[str, int, str, str]] = set()
    float_conversions(tu.cursor, root / "src", found)
    return sorted(found)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--build", type=Path, default=Path("build/native"))
    parser.add_argument("--unsigned-only", action="store_true", help="only conversions to unsigned types")
    parser.add_argument("-j", type=int, default=multiprocessing.cpu_count())
    options = parser.parse_args()
    root = Path.cwd().resolve()
    commands = json.loads((options.build / "compile_commands.json").read_text())
    entries = [dict(c, root=str(root)) for c in commands if "/src/" in c["file"] and c["file"].endswith(".cpp")]
    with multiprocessing.Pool(options.j) as pool:
        results = pool.map(scan, entries)
    sites = sorted({site for batch in results for site in batch})
    for path, line, dest, source in sites:
        if options.unsigned_only and dest not in UNSIGNED_SPELLINGS:
            continue
        print(f"{path}:{line}: {source} -> {dest}")
    print(f"{len(sites)} float to integer conversions in {len(entries)} translation units")


if __name__ == "__main__":
    main()
