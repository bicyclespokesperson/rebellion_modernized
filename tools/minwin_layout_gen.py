#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# ///
"""Generate src/port/minwin/layout_expected.inc: the sizes, field offsets and constant values the real SDK gives for every
struct and constant that the minwin headers declare (the facts layout_check.cpp then static_asserts, on i686, against minwin).

  1. minwin_cc.sh -E / -ast-dump=json of a TU that includes platform/win32.h with REB_MINWIN=1 -> the names minwin declares
  2. the same names probed through the real SDK (REB_MINWIN off): sizeof, offsetof, macro value and type
  3. layout_expected.inc

  tools/minwin_layout_gen.py [--out FILE]      (needs the dev container with the xwin SDK)
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import minwin_inventory as inv  # noqa: E402

ROOT = inv.ROOT
WORK = inv.WORK
FLAGS = ["-Isrc/game", "-Isrc", "-DREB_HYBRID=1"]


def clang(args: list[str], out: Path) -> None:
    with out.open("wb") as f:
        r = subprocess.run([str(ROOT / "tools" / "minwin_cc.sh"), *args], stdout=f, stderr=subprocess.PIPE, cwd=ROOT)
    if r.returncode not in (0, 1):
        sys.exit(f"clang failed: {r.stderr.decode()[-1500:]}")


def flat(nodes: list[dict]):
    for n in nodes:
        if n.get("kind") == "LinkageSpecDecl":
            yield from flat(n.get("inner", []))
        else:
            yield n


def field_names(record: dict) -> list[str]:
    names: list[str] = []
    pending: list[dict] = []
    for inner in record.get("inner", []):
        if inner.get("kind") in ("CXXRecordDecl", "RecordDecl") and not inner.get("name"):
            pending.append(inner)
        elif inner.get("kind") == "FieldDecl":
            qual = inner["type"]["qualType"]
            if "unnamed" in qual or "anonymous" in qual:
                names += field_names(pending.pop(0))
            if inner.get("name") and not inner.get("isBitfield"):
                names.append(inner["name"])
    return names


def minwin_names() -> tuple[dict[str, list[str]], list[str]]:
    """records (tag -> field names) and object-like macros declared by the minwin headers."""
    WORK.mkdir(parents=True, exist_ok=True)
    source = WORK / "mw_names.cpp"
    source.write_text('#include "platform/win32.h"\n#include "platform/smacker.h"\n')
    rel = str(source.relative_to(ROOT))
    clang(["-E", "-Xclang", "-dD", "-DREB_MINWIN=1", *FLAGS, rel], WORK / "mw_names.i")
    clang(["-fsyntax-only", "-Xclang", "-ast-dump=json", "-DREB_MINWIN=1", *FLAGS, rel], WORK / "mw_names.json")
    tu = json.loads((WORK / "mw_names.json").read_text())
    records: dict[str, list[str]] = {}
    state = {"file": ""}
    for node in flat(tu["inner"]):
        inv.walk_locations(node.get("loc", {}), state)
        if (node.get("kind") in ("CXXRecordDecl", "RecordDecl") and node.get("completeDefinition") and node.get("name")
                and "port/minwin/" in state["file"]
                and not any(i.get("kind") == "CXXMethodDecl" for i in node.get("inner", []))):
            records[node["name"]] = field_names(node)
        inv.walk_locations(node, state)
    macros: list[str] = []
    current = ""
    marker = re.compile(r'^# \d+ "([^"]+)"')
    define = re.compile(r"^#define\s+([A-Z][A-Za-z0-9_]*)\s+(\S.*)$")
    for line in (WORK / "mw_names.i").read_text(errors="replace").splitlines():
        m = marker.match(line)
        if m:
            current = m.group(1)
        elif "port/minwin/" in current:
            d = define.match(line)
            if d and not d.group(1).startswith("MINWIN_"):
                macros.append(d.group(1))
    return records, sorted(set(macros))


def sdk_facts(records: dict[str, list[str]], macros: list[str]) -> tuple[dict[str, int], dict[tuple[str, str], int], dict[str, tuple[str, int]]]:
    lines = ['#include "platform/win32.h"']
    offset_ids: dict[int, tuple[str, str]] = {}
    for tag, fields in sorted(records.items()):
        lines.append(f"enum reb_s_{tag} : unsigned long long {{ reb_sz_{tag} = sizeof({tag}) }};")
        for f in fields:
            index = len(offset_ids)
            offset_ids[index] = (tag, f)
            lines.append(f"enum reb_o_{index} : unsigned long long {{ reb_off_{index} = __builtin_offsetof({tag}, {f}) }};")
    for m in macros:
        lines.append(f"enum reb_v_{m} : long long {{ reb_val_{m} = ({m}) }};")
        lines.append(f"using reb_t_{m} = decltype(({m}));")
    probe = WORK / "layout_probe.cpp"
    probe.write_text("\n".join(lines) + "\n")
    out = WORK / "layout_probe.json"
    clang(["-fsyntax-only", "-Xclang", "-ast-dump=json", "-Xclang", "-ast-dump-filter=reb_", "-ferror-limit=0", "-Wno-everything",
           *inv.CLANG_FLAGS, str(probe.relative_to(ROOT))], out)
    text = out.read_text()
    sizes: dict[str, int] = {}
    offsets: dict[tuple[str, str], int] = {}
    values: dict[str, tuple[str, int]] = {}
    types: dict[str, str] = {}
    decoder = json.JSONDecoder()
    pos = 0
    while pos < len(text):
        while pos < len(text) and text[pos] in " \n\r\t":
            pos += 1
        if pos >= len(text):
            break
        node, pos = decoder.raw_decode(text, pos)
        visit(node, sizes, offsets, values, types, offset_ids)
    typed = {m: (types.get(m, "int"), v[1]) for m, v in values.items()}
    return sizes, offsets, typed


def visit(node: dict, sizes: dict, offsets: dict, values: dict, types: dict, offset_ids: dict) -> None:
    kind = node.get("kind")
    name = node.get("name", "")
    if kind == "TypeAliasDecl" and name.startswith("reb_t_"):
        types[name[6:]] = node["type"].get("desugaredQualType") or node["type"]["qualType"]
    elif kind == "EnumConstantDecl":
        value = find_value(node.get("inner", []))
        if value is None:
            return
        if name.startswith("reb_sz_"):
            sizes[name[7:]] = int(value)
        elif name.startswith("reb_off_"):
            offsets[offset_ids[int(name[8:])]] = int(value)
        elif name.startswith("reb_val_"):
            values[name[8:]] = ("", int(value))
    for inner in node.get("inner", []):
        visit(inner, sizes, offsets, values, types, offset_ids)


def find_value(nodes: list[dict]) -> str | None:
    for n in nodes:
        if n.get("kind") == "ConstantExpr" and "value" in n:
            return str(n["value"])
        deeper = find_value(n.get("inner", []))
        if deeper is not None:
            return deeper
    return None


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=ROOT / "src" / "port" / "minwin" / "layout_expected.inc")
    args = parser.parse_args()
    records, macros = minwin_names()
    sizes, offsets, values = sdk_facts(records, macros)
    out = ["// Generated by tools/minwin_layout_gen.py from the real Windows SDK (i686): sizes, field offsets and constant values of",
           "// everything the minwin headers declare. Checked by layout_check.cpp. Do not edit.", ""]
    missing_records = [t for t in records if t not in sizes]
    for tag in sorted(sizes):
        out.append(f"MINWIN_LAYOUT_SIZE({tag}, {sizes[tag]})")
    for (tag, field), off in sorted(offsets.items()):
        out.append(f"MINWIN_LAYOUT_OFFSET({tag}, {field}, {off})")
    for name in sorted(values):
        typ, value = values[name]
        out.append(f'MINWIN_LAYOUT_VALUE({name}, {value}LL, {typ.replace("const ", "")})')
    out.append("")
    args.out.write_text("\n".join(out))
    unprobed = [m for m in macros if m not in values]
    print(f"{len(sizes)} struct sizes, {len(offsets)} offsets, {len(values)} constant values -> {args.out}")
    if missing_records:
        print("records the SDK does not define (minwin-only):", missing_records)
    if unprobed:
        print("macros without a constant value (function-like, pointer-valued, or minwin-only):", unprobed)


if __name__ == "__main__":
    main()
