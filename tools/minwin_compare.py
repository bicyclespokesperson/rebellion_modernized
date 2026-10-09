#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# ///
"""Compare the declarations of the minwin headers with the real SDK's: the API surface, as the compiler sees it.

For every function, typedef and record that the minwin headers declare, the SDK's declaration of the same name must have the
same signature: mangled name (so calling convention and argument size), parameter and return type spellings, dllimport,
noreturn; typedef targets; record kind, base classes, field names and types in order, COM methods in vtable order with
their signatures. Sizes, offsets and constant values are tools/minwin_layout_gen.py's and layout_check.cpp's job.

  tools/minwin_compare.py [--reuse]       (--reuse: keep build/minwin/cmp_*.json from a previous run)
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import minwin_inventory as inv  # noqa: E402
import minwin_layout_gen as gen  # noqa: E402

ROOT = inv.ROOT
WORK = inv.WORK
# Functions the SDK has only as an intrinsic macro; the game imports them from the DLL (audio/interlocked_import.h).
MINWIN_ONLY = {("function", "InterlockedExchange")}
IGNORED_ATTRS = re.compile(r"\s*__attribute__\(\((?:nothrow|cdecl)\)\)")


def dump(define: str, name: str) -> Path:
    source = WORK / "cmp_tu.cpp"
    source.write_text('#include "platform/win32.h"\n#include "platform/smacker.h"\n')
    out = WORK / f"cmp_{name}.json"
    gen.clang(["-fsyntax-only", "-Xclang", "-ast-dump=json", *([define] if define else []), *gen.FLAGS, "-D_ACRTIMP=", "-D_CRTIMP=",
               "-D_DCRTIMP=", "-DWIN32_LEAN_AND_MEAN", "-DNOMINMAX", str(source.relative_to(ROOT))], out)
    return out


SCALAR_ALIASES = {"DWORD": "ULONG", "unsigned long": "ULONG", "long": "LONG", "CHAR": "char", "BYTE": "unsigned char",
                  "UCHAR": "unsigned char", "WORD": "unsigned short", "USHORT": "unsigned short", "INT": "int", "UINT": "unsigned int"}
ALIAS_PATTERN = re.compile(r"\b(" + "|".join(sorted(map(re.escape, SCALAR_ALIASES), key=len, reverse=True)) + r")\b")
ANONYMOUS_LOCATION = re.compile(r"\((anonymous|unnamed) (\w+) at [^)]*\)")


def norm_type(text: str) -> str:
    text = ANONYMOUS_LOCATION.sub(r"(\1 \2)", IGNORED_ATTRS.sub("", text).replace(" __attribute__((noreturn))", ""))
    text = ALIAS_PATTERN.sub(lambda m: SCALAR_ALIASES[m.group(1)], text)
    return re.sub(r"\s+", " ", text).strip()


def signature(node: dict) -> dict:
    kind = node.get("kind")
    out: dict = {"kind": kind}
    if kind == "FunctionDecl" or kind == "CXXMethodDecl":
        out["mangled"] = node.get("mangledName") if kind == "FunctionDecl" else None
        out["type"] = norm_type(node["type"]["qualType"])
        out["params"] = [(p.get("name", ""), norm_type(p["type"]["qualType"])) for p in node.get("inner", []) if p.get("kind") == "ParmVarDecl"]
        out["noreturn"] = "noreturn" in node["type"]["qualType"] or any("NoReturn" in i.get("kind", "") for i in node.get("inner", []))
        out["virtual"] = bool(node.get("virtual"))
        out["pure"] = bool(node.get("pure"))
    elif kind in ("TypedefDecl", "TypeAliasDecl"):
        out["type"] = norm_type(node["type"]["qualType"])
    elif kind in ("CXXRecordDecl", "RecordDecl"):
        out["tag"] = node.get("tagUsed")
        out["bases"] = [b["type"]["qualType"] for b in node.get("bases", [])]
        out["members"] = record_members(node)
    return out


def record_members(node: dict) -> list:
    members: list = []
    for inner in node.get("inner", []):
        kind = inner.get("kind")
        if kind == "FieldDecl":
            members.append(("field", inner.get("name", ""), norm_type(inner["type"]["qualType"]), bool(inner.get("isBitfield"))))
        elif kind == "CXXMethodDecl" and not inner.get("name", "").startswith("operator") and not inner.get("isImplicit"):
            sig = signature(inner)
            members.append(("method", inner["name"], sig["type"], tuple(p for _, p in sig["params"]), sig["virtual"], sig["pure"]))
        elif kind in ("CXXRecordDecl", "RecordDecl") and not inner.get("name"):
            members.append(("nested", node.get("tagUsed"), record_members(inner)))
    return members


def collect(path: Path, minwin_only: bool) -> dict[tuple[str, str], dict]:
    tu = json.loads(path.read_text())
    state = {"file": ""}
    found: dict[tuple[str, str], dict] = {}
    for node in gen.flat(tu["inner"]):
        inv.walk_locations(node.get("loc", {}), state)
        wanted = "port/minwin/" in state["file"] or "platform/" in state["file"] if minwin_only else True
        kind = node.get("kind")
        name = node.get("name")
        if wanted and name and kind in ("FunctionDecl", "TypedefDecl", "TypeAliasDecl", "CXXRecordDecl", "RecordDecl"):
            if kind in ("CXXRecordDecl", "RecordDecl") and not node.get("completeDefinition"):
                inv.walk_locations(node, state)
                continue
            key = ("record" if "Record" in kind else "typedef" if "Typedef" in kind or "Alias" in kind else "function", name)
            sig = signature(node)
            imported = any(i.get("kind") == "DLLImportAttr" for i in node.get("inner", []))
            if key in found:
                if key[0] == "function":
                    found[key]["dllimport"] = found[key].get("dllimport", False) or imported
                continue
            if key[0] == "function":
                sig["dllimport"] = imported
            found[key] = sig
        inv.walk_locations(node, state)
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--reuse", action="store_true")
    args = parser.parse_args()
    WORK.mkdir(parents=True, exist_ok=True)
    sdk_path, mw_path = WORK / "cmp_sdk.json", WORK / "cmp_minwin.json"
    if not args.reuse or not sdk_path.exists():
        dump("", "sdk")
        dump("-DREB_MINWIN=1", "minwin")
    sdk = collect(sdk_path, False)
    mine = collect(mw_path, True)
    problems = 0
    counts = {"function": 0, "typedef": 0, "record": 0}
    for key, sig in sorted(mine.items()):
        counts[key[0]] += 1
        theirs = sdk.get(key)
        if theirs is None and key in MINWIN_ONLY:
            continue
        if theirs is None:
            print(f"only in minwin: {key[0]} {key[1]}")
            problems += 1
            continue
        for field in sorted(set(sig) | set(theirs)):
            if field == "params" and sig.get("params") and theirs.get("params"):
                a = [p for _, p in sig["params"]]
                b = [p for _, p in theirs["params"]]
                if a != b:
                    print(f"{key[0]} {key[1]}: parameter types differ\n   minwin {a}\n   sdk    {b}")
                    problems += 1
                continue
            if field == "params":
                continue
            if sig.get(field) != theirs.get(field):
                print(f"{key[0]} {key[1]}: {field} differs\n   minwin {sig.get(field)}\n   sdk    {theirs.get(field)}")
                problems += 1
    print(f"compared {counts['function']} functions, {counts['typedef']} typedefs, {counts['record']} records: {problems} differences")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
