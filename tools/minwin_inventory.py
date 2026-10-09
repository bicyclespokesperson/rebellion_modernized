#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# ///
"""Inventory of the Win32 / COM / DirectX / DirectPlay / WinMM / AVIFile / Smacker identifiers the game sources use.

The "minwin" headers (src/port/minwin, docs/phase3_plan.md P0c) must declare exactly this subset. The list is mechanical:
(1) the real SDK headers (the xwin SDK in the dev container) plus platform/win32.h are preprocessed (-dD, for macros) and
dumped as a clang AST (JSON, for functions, types, enumerators, variables and COM methods) -- tools/minwin_cc.sh; (2) every
identifier token of the game sources (src/game/**, comments and strings removed) is looked up among the names those headers
declare. A name that is declared by an SDK header and appears as a token is "used". It is an over-approximation by name
(a game method called `Open` also marks DirectPlay's `Open`); the COM method lists say so.

  tools/minwin_inventory.py [--out tools/minwin_inventory.txt] [--reuse]   (--reuse keeps build/minwin/master.{i,json})
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WORK = ROOT / "build" / "minwin"
CLANG_FLAGS = ["-Isrc/game", "-Isrc", "-D_ACRTIMP=", "-D_CRTIMP=", "-D_DCRTIMP=", "-DWIN32_LEAN_AND_MEAN", "-DNOMINMAX",
               "-D_CRT_SECURE_NO_WARNINGS", "-DREB_HYBRID=1"]
MASTER_SOURCE = '#include "platform/win32.h"\n#include "platform/smacker.h"\n'

HEADER_GROUPS = {
    "base": ["windef", "winnt", "basetsd", "minwindef", "guiddef", "wtypes", "wtypesbase", "winerror", "ntstatus", "specstrings",
             "sal", "driverspecs", "apiset", "stralign", "poppack", "pshpack1", "pshpack2", "pshpack4", "pshpack8", "winapifamily",
             "sdkddkver", "excpt", "ctype", "winnls", "rpc", "rpcndr", "rpcnsip", "rpcsal", "cguid", "wincon", "winnt.h"],
    "kernel32": ["winbase", "fileapi", "processthreadsapi", "synchapi", "libloaderapi", "handleapi", "errhandlingapi", "memoryapi",
                 "sysinfoapi", "timezoneapi", "heapapi", "stringapiset", "namedpipeapi", "ioapiset", "minwinbase", "profileapi",
                 "processenv", "winbase.h", "debugapi", "interlockedapi", "realtimeapiset", "threadpoollegacyapiset", "datetimeapi"],
    "user32": ["winuser", "winuser.h"],
    "gdi32": ["wingdi", "wingdi.h"],
    "winmm": ["mmsystem", "mmreg", "mmiscapi", "mmiscapi2", "playsoundapi", "timeapi", "joystickapi", "mciapi", "mmddk", "mmsyscom"],
    "avifil32/vfw": ["vfw", "aviriff", "mmreg"],
    "ddraw": ["ddraw"],
    "dsound": ["dsound"],
    "comctl32": ["commctrl", "prsht"],
    "version": ["winver", "verrsrc"],
    "advapi32": ["winreg", "winsvc", "securitybaseapi", "wincrypt"],
    "ole/com": ["objbase", "combaseapi", "unknwn", "unknwnbase", "objidl", "objidlbase", "oaidl", "ole2", "oleauto", "urlmon", "ole2.h",
                "servprov", "wtypes"],
    "dplayx": ["dplay"],
    "smackw32": ["smacker"],
}
HEADER_TO_GROUP = {h: g for g, hs in HEADER_GROUPS.items() for h in hs}
OWN_HEADERS = {"dplay", "smacker"}
CRT_DIR_MARKERS = ("/crt/include/", "/sdk/include/ucrt/", "/include/c++/")
EXCLUDED_NAMES = {"main", "WinMain", "std", "size_t", "ptrdiff_t", "wchar_t", "uintptr_t", "intptr_t", "NULL", "true", "false",
                  "nullptr", "offsetof", "va_list", "va_start", "va_end", "va_arg", "errno", "EOF", "assert", "FILE", "tm", "time_t",
                  "int8_t", "int16_t", "int32_t", "int64_t", "uint8_t", "uint16_t", "uint32_t", "uint64_t", "setlocale", "sprintf",
                  "min", "max"}


def run_clang(args: list[str], out: Path) -> None:
    with out.open("wb") as f:
        r = subprocess.run([str(ROOT / "tools" / "minwin_cc.sh"), *args], stdout=f, stderr=subprocess.PIPE, cwd=ROOT)
    if r.returncode != 0:
        sys.exit(f"clang failed ({r.returncode}): {r.stderr.decode()[-2000:]}")


def generate_master() -> None:
    WORK.mkdir(parents=True, exist_ok=True)
    (WORK / "master.cpp").write_text(MASTER_SOURCE)
    source = str((WORK / "master.cpp").relative_to(ROOT))
    run_clang(["-E", "-Xclang", "-dD", *CLANG_FLAGS, source], WORK / "master.i")
    run_clang(["-fsyntax-only", "-Xclang", "-ast-dump=json", *CLANG_FLAGS, source], WORK / "master.json")


def group_of_file(path: str) -> str | None:
    if any(m in path for m in CRT_DIR_MARKERS):
        return None
    stem = Path(path).name.rsplit(".", 1)[0].lower()
    if stem in OWN_HEADERS:
        return HEADER_TO_GROUP[stem]
    if "/sdk/include/" not in path and "platform/" not in path:
        return None
    return HEADER_TO_GROUP.get(stem, f"other:{stem}")


class Declarations:
    """name -> list of (kind, group); interface -> ordered method list."""

    def __init__(self) -> None:
        self.by_name: dict[str, list[tuple[str, str]]] = defaultdict(list)
        self.interface_methods: dict[str, list[str]] = {}
        self.enum_of: dict[str, str] = {}

    def add(self, name: str | None, kind: str, group: str | None) -> None:
        if name and group and (kind, group) not in self.by_name[name]:
            self.by_name[name].append((kind, group))


def collect_macros(path: Path, decls: Declarations) -> dict[str, str]:
    macros: dict[str, str] = {}
    current = ""
    marker = re.compile(r'^# \d+ "([^"]+)"')
    define = re.compile(r"^#define\s+([A-Za-z_]\w*)(\(?)")
    for line in path.read_text(errors="replace").splitlines():
        if line.startswith("# "):
            m = marker.match(line)
            if m:
                current = m.group(1)
            continue
        m = define.match(line)
        if m:
            name = m.group(1)
            group = group_of_file(current)
            if group and not name.startswith("_"):
                kind = "macro-function" if m.group(2) else "macro"
                decls.add(name, kind, group)
                macros[name] = line
        elif line.startswith("#undef"):
            name = line.split()[1]
            decls.by_name.pop(name, None)
    return macros


def walk_locations(node: object, state: dict[str, str]) -> None:
    """Follows clang's JSON writer: a location carries "file" only when it differs from the previous one printed."""
    if isinstance(node, dict):
        if "file" in node:
            state["file"] = node["file"]
        for key, value in node.items():
            if key != "includedFrom" and isinstance(value, (dict, list)):
                walk_locations(value, state)
    elif isinstance(node, list):
        for item in node:
            walk_locations(item, state)


def record_methods(node: dict) -> list[str]:
    return [i["name"] for i in node.get("inner", []) if i.get("kind") == "CXXMethodDecl" and "name" in i and not i["name"].startswith("operator")]


def declare(node: dict, group: str | None, decls: Declarations) -> None:
    kind = node.get("kind")
    name = node.get("name")
    if kind == "FunctionDecl":
        decls.add(name, "function", group)
    elif kind in ("TypedefDecl", "TypeAliasDecl"):
        decls.add(name, "type", group)
    elif kind in ("CXXRecordDecl", "RecordDecl"):
        if node.get("completeDefinition"):
            decls.add(name, "struct", group)
            methods = record_methods(node)
            if methods and group:
                decls.interface_methods[name] = methods
    elif kind == "EnumDecl":
        decls.add(name, "enum", group)
        for constant in node.get("inner", []):
            if constant.get("kind") == "EnumConstantDecl":
                decls.add(constant["name"], "constant", group)
    elif kind == "VarDecl":
        decls.add(name, "variable", group)


def collect_scope(nodes: list[dict], state: dict[str, str], decls: Declarations) -> None:
    for node in nodes:
        walk_locations(node.get("loc", {}), state)
        if node.get("kind") == "LinkageSpecDecl":
            walk_locations(node.get("range", {}), state)
            collect_scope(node.get("inner", []), state, decls)
            continue
        if node.get("kind") != "NamespaceDecl":
            declare(node, group_of_file(state["file"]), decls)
        walk_locations(node, state)


def collect_ast(path: Path, decls: Declarations) -> None:
    tu = json.loads(path.read_text())
    collect_scope(tu["inner"], {"file": ""}, decls)


STRING_OR_COMMENT = re.compile(r'//[^\n]*|/\*.*?\*/|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.S)
IDENT = re.compile(r"[A-Za-z_]\w*")


def game_tokens() -> dict[str, int]:
    counts: dict[str, int] = defaultdict(int)
    for path in sorted((ROOT / "src" / "game").rglob("*")):
        if path.suffix not in (".cpp", ".h") or path.parent.name == "platform":
            continue
        text = STRING_OR_COMMENT.sub(" ", path.read_text(errors="replace"))
        text = re.sub(r"^\s*#\s*include.*$", "", text, flags=re.M)
        for token in IDENT.findall(text):
            counts[token] += 1
    return counts


def load_import_dlls() -> dict[str, str]:
    with (ROOT / "symbols" / "imports.csv").open() as f:
        return {row["name"]: row["dll"].lower().removesuffix(".dll") for row in csv.DictReader(f)}


def format_names(names: list[tuple[str, int]]) -> list[str]:
    lines: list[str] = []
    current = "  "
    for name, count in names:
        item = f"{name}({count}) "
        if len(current) + len(item) > 118:
            lines.append(current.rstrip())
            current = "  "
        current += item
    if current.strip():
        lines.append(current.rstrip())
    return lines


def build_report(decls: Declarations, tokens: dict[str, int], imports: dict[str, str]) -> str:
    used: dict[str, dict[str, list[tuple[str, int]]]] = defaultdict(lambda: defaultdict(list))
    for name, entries in decls.by_name.items():
        if name in EXCLUDED_NAMES or name not in tokens:
            continue
        kind, group = entries[0]
        if kind == "function" and name in imports:
            group = imports[name]
        elif kind == "function" and name not in imports and group in ("kernel32", "user32", "gdi32", "winmm"):
            pass
        used[group][kind].append((name, tokens[name]))
    interfaces = {n for n, e in decls.by_name.items() if n in tokens and n in decls.interface_methods and e[0][0] == "struct"
                  and n.startswith(("I", "_I"))}
    out: list[str] = ["# minwin inventory: SDK / platform names the game sources (src/game) use. Generated by"
                      " tools/minwin_inventory.py -- do not edit.", "# name(n): n = identifier occurrences in game sources.",
                      "# Over-approximation by name; see the script's docstring.", ""]
    out.append("## Summary (distinct names by group)")
    for group in sorted(used, key=lambda g: -sum(len(v) for v in used[g].values())):
        kinds = ", ".join(f"{k} {len(v)}" for k, v in sorted(used[group].items()))
        out.append(f"  {group:16} {sum(len(v) for v in used[group].values()):5}   ({kinds})")
    out.append(f"  {'TOTAL':16} {sum(len(v) for g in used.values() for v in g.values()):5}")
    out.append("")
    for group in sorted(used):
        out.append(f"## {group}")
        for kind in ("function", "struct", "type", "enum", "constant", "macro", "macro-function", "variable"):
            names = sorted(used[group].get(kind, []))
            if names:
                out.append(f" [{kind}] {len(names)}")
                out.extend(format_names(names))
        out.append("")
    out.append("## COM interfaces (methods in vtable order; * = name appears in game sources)")
    for iface in sorted(interfaces):
        methods = decls.interface_methods[iface]
        marked = " ".join(f"{m}{'*' if m in tokens else ''}" for m in methods)
        out.append(f"  {iface}: {marked}")
    return "\n".join(out) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--out", type=Path, default=ROOT / "tools" / "minwin_inventory.txt")
    parser.add_argument("--reuse", action="store_true", help="reuse build/minwin/master.{i,json}")
    args = parser.parse_args()
    if not (args.reuse and (WORK / "master.json").exists()):
        generate_master()
    decls = Declarations()
    collect_macros(WORK / "master.i", decls)
    collect_ast(WORK / "master.json", decls)
    report = build_report(decls, game_tokens(), load_import_dlls())
    args.out.write_text(report)
    print(report.split("\n\n")[1] if "\n\n" in report else report)


if __name__ == "__main__":
    main()
