#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# ///
"""Draft minwin declarations for SDK names, from the facts in the SDK's clang AST (names, types, field lists, values).

The headers under src/port/minwin are hand-maintained; this tool is how a missing name is added. It prints, per minwin
header, the declarations needed for the given names and everything they depend on (a type used only through a pointer
gets a forward declaration, a type used by value a full definition). Output is a draft: review it, check the "MANUAL"
markers, paste it into the right header, and let tools/minwin_check.sh compare it with the SDK.

  tools/minwin_scaffold.py [--all-inventory] [--outdir DIR] [NAME ...]

Needs build/minwin/master.{i,json} (tools/minwin_inventory.py creates them). Names already declared by the minwin headers
(src/port/minwin/*.h, matched by a simple scan) are skipped.
"""
from __future__ import annotations

import argparse
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import minwin_inventory as inv  # noqa: E402

ROOT = inv.ROOT
WORK = inv.WORK
MINWIN_DIR = ROOT / "src" / "port" / "minwin"

GROUP_FILE = {
    "base": "base", "ole/com": "base", "kernel32": "kernel32", "gdi32": "gdi32", "user32": "user32", "winmm": "winmm",
    "avifil32": "vfw", "avifil32/vfw": "vfw", "comctl32": "comctl32", "advapi32": "advapi32", "version": "version",
    "ddraw": "ddraw", "dsound": "dsound", "other:mmeapi": "winmm", "other:mmreg": "winmm",
}
FILE_INCLUDES = {"base": ["core"], "kernel32": ["base"], "gdi32": ["base"], "user32": ["base", "gdi32"], "winmm": ["base"],
                 "vfw": ["base", "winmm"], "comctl32": ["base", "user32"], "advapi32": ["base"], "version": ["base"],
                 "ddraw": ["base", "gdi32"], "dsound": ["base", "winmm"]}
DLL_GROUP = {"kernel32": "kernel32", "user32": "user32", "gdi32": "gdi32", "winmm": "winmm", "advapi32": "advapi32",
             "version": "version", "comctl32": "comctl32", "avifil32": "avifil32", "ddraw": "ddraw", "dsound": "dsound"}
CC_MACRO = {"stdcall": "REB_STDCALL", "cdecl": "REB_CDECL", "thiscall": "REB_THISCALL"}
ATTR = re.compile(r"\s*__attribute__\(\((\w+)\)\)")
IDENT = re.compile(r"[A-Za-z_]\w*")
KEYWORDS = {"struct", "union", "enum", "class", "const", "volatile", "unsigned", "signed", "long", "short", "int", "char", "void",
            "float", "double", "__int64", "_Bool", "bool", "wchar_t", "restrict", "__attribute__", "stdcall", "cdecl", "thiscall"}


@dataclass
class Decl:
    key: tuple[str, str]
    node: dict
    group: str | None
    order: int
    extra: dict = field(default_factory=dict)


class Ast:
    def __init__(self) -> None:
        self.decls: dict[tuple[str, str], Decl] = {}
        self.by_name: dict[str, list[Decl]] = {}
        self.records_by_id: dict[str, Decl] = {}
        self.macros: dict[str, tuple[str, str, int]] = {}
        self.dllimport: set[str] = set()

    def add(self, kind: str, node: dict, group: str | None, order: int) -> None:
        name = node.get("name")
        if not name:
            return
        key = (kind, name)
        old = self.decls.get(key)
        if old and (old.node.get("completeDefinition") or not node.get("completeDefinition")) and kind == "record":
            return
        if old and kind != "record":
            return
        decl = Decl(key, node, group, order)
        self.decls[key] = decl
        lst = self.by_name.setdefault(name, [])
        lst[:] = [d for d in lst if d.key != key] + [decl]
        if kind == "record":
            self.records_by_id[node["id"]] = decl

    def lookup(self, name: str) -> Decl | None:
        for kind in ("typedef", "record", "enumconst", "function", "var"):
            d = self.decls.get((kind, name))
            if d:
                return d
        return None


def load_ast(imports: dict[str, str]) -> Ast:
    ast = Ast()
    tu = json.loads((WORK / "master.json").read_text())
    state = {"file": ""}
    counter = [0]

    def scope(nodes: list[dict]) -> None:
        for node in nodes:
            inv.walk_locations(node.get("loc", {}), state)
            kind = node.get("kind")
            if kind == "LinkageSpecDecl":
                inv.walk_locations(node.get("range", {}), state)
                scope(node.get("inner", []))
                continue
            counter[0] += 1
            group = inv.group_of_file(state["file"])
            if kind == "FunctionDecl":
                name = node.get("name", "")
                if name in imports and imports[name] in DLL_GROUP:
                    group = DLL_GROUP[imports[name]]
                if any(i.get("kind") == "DLLImportAttr" for i in node.get("inner", [])):
                    ast.dllimport.add(name)
                ast.add("function", node, group, counter[0])
            elif kind in ("TypedefDecl", "TypeAliasDecl"):
                ast.add("typedef", node, group, counter[0])
            elif kind in ("CXXRecordDecl", "RecordDecl"):
                ast.add("record", node, group, counter[0])
            elif kind == "EnumDecl":
                ast.add("enum", node, group, counter[0])
                for c in node.get("inner", []):
                    if c.get("kind") == "EnumConstantDecl":
                        d = Decl(("enumconst", c["name"]), c, group, counter[0], {"enum": node})
                        ast.decls[d.key] = d
            elif kind == "VarDecl":
                ast.add("var", node, group, counter[0])
            if kind != "NamespaceDecl":
                inv.walk_locations(node, state)

    scope(tu["inner"])
    return ast


def load_macros() -> dict[str, tuple[str, str, int]]:
    """name -> (group, body, line) for object-like and function-like macros of SDK headers."""
    macros: dict[str, tuple[str, str, int]] = {}
    current = ""
    marker = re.compile(r'^# \d+ "([^"]+)"')
    define = re.compile(r"^#define\s+([A-Za-z_]\w*)(\(([^)]*)\))?\s*(.*)$")
    line_no = 0
    for line in (WORK / "master.i").read_text(errors="replace").splitlines():
        line_no += 1
        if line.startswith("# "):
            m = marker.match(line)
            if m:
                current = m.group(1)
            continue
        m = define.match(line)
        if m:
            group = inv.group_of_file(current)
            if group:
                prefix = "F" if m.group(2) is not None else "O"
                macros[m.group(1)] = (group, prefix + m.group(4), line_no)
        elif line.startswith("#undef"):
            macros.pop(line.split()[1], None)
    return macros


def type_names(qual: str) -> list[str]:
    return [t for t in IDENT.findall(ATTR.sub("", qual)) if t not in KEYWORDS]


def is_pointer_type(type_obj: dict) -> bool:
    desugared = type_obj.get("desugaredQualType") or type_obj.get("qualType", "")
    return desugared.rstrip().endswith("*") or "(*)" in desugared or desugared.rstrip().endswith("&")


def split_cc(qual: str) -> tuple[str, str]:
    found = [CC_MACRO[a] for a in ATTR.findall(qual) if a in CC_MACRO]
    return (ATTR.sub("", qual), found[0] if found else "")


def declare(qual: str, name: str) -> str:
    qual, cc = split_cc(qual)
    m = re.fullmatch(r"(.*?)\s*\(\*\)\s*\((.*)\)", qual)
    if m:
        return f"{m.group(1)} ({cc + ' ' if cc else ''}*{name})({m.group(2)})"
    m = re.fullmatch(r"([^()]+?)\s*\((.*)\)", qual)
    if m and name:
        return f"{m.group(1)} {cc + ' ' if cc else ''}{name}({m.group(2)})"
    m = re.fullmatch(r"(.*?)\s*((?:\[[^\]]*\])+)", qual)
    if m:
        return f"{m.group(1)} {name}{m.group(2)}"
    return f"{qual}{'' if qual.endswith('*') else ' '}{name}"


def normalize_builtin(text: str, group_file: str) -> str:
    if group_file == "core":
        return text
    text = re.sub(r"\bunsigned long\b(?! long)", "ULONG", text)
    text = re.sub(r"(?<!unsigned )\blong\b(?! long)", "LONG", text)
    return text


class Scaffold:
    def __init__(self, ast: Ast, macros: dict[str, tuple[str, str, int]], skip: set[str]) -> None:
        self.ast = ast
        self.macros = macros
        self.skip = skip
        self.full: set[tuple[str, str]] = set()
        self.fwd: set[tuple[str, str]] = set()
        self.need_macros: set[str] = set()
        self.valued: set[tuple[str, str]] = set()
        self.manual: list[str] = []

    def need(self, name: str, by_value: bool = True) -> None:
        if name in self.skip:
            return
        if name in self.macros and not self.ast.lookup(name):
            if name not in self.need_macros:
                self.need_macros.add(name)
                for t in IDENT.findall(self.macros[name][1][1:]):
                    self.need(t)
            return
        decl = self.ast.lookup(name)
        if not decl:
            return
        self.need_decl(decl, by_value)

    def need_decl(self, decl: Decl, by_value: bool) -> None:
        kind = decl.key[0]
        if kind == "record":
            if decl.key in self.full or (not by_value and decl.key in self.fwd):
                return
            if by_value and decl.node.get("completeDefinition"):
                self.full.add(decl.key)
                self.fwd.discard(decl.key)
                self.visit_record(decl)
            else:
                self.fwd.add(decl.key)
            return
        if decl.key in self.full and not (by_value and decl.key not in self.valued):
            return
        if by_value:
            self.valued.add(decl.key)
        if kind == "enumconst":
            self.need_decl(self.ast.decls[("enum", decl.extra["enum"]["name"])], True) if decl.extra["enum"].get("name") else None
            self.full.add(decl.key)
            return
        self.full.add(decl.key)
        node = decl.node
        if kind == "typedef":
            underlying = node["type"]
            ptr = is_pointer_type(underlying)
            for t in type_names(underlying["qualType"]):
                self.need(t, by_value and not ptr)
            for owned in self.owned_records(node):
                self.need_decl(owned, by_value and not ptr)
        elif kind == "function":
            self.visit_function(node)
        elif kind == "var":
            self.need_type_obj(node["type"], True)
        elif kind == "enum":
            return

    def need_pointed_interface(self, name: str) -> None:
        """A used pointer typedef of a COM interface (LPDIRECTDRAWSURFACE): the game calls through it, so define the interface."""
        decl = self.ast.decls.get(("typedef", name))
        if not decl or not is_pointer_type(decl.node["type"]):
            return
        for t in type_names(decl.node["type"]["qualType"]):
            record = self.ast.decls.get(("record", t))
            if record and any(i.get("kind") == "CXXMethodDecl" for i in record.node.get("inner", [])):
                self.need_decl(record, True)

    def owned_records(self, node: dict) -> list[Decl]:
        found: list[Decl] = []
        for inner in node.get("inner", []):
            owned = inner.get("ownedTagDecl")
            if owned:
                current = self.ast.decls.get(("record", owned.get("name", ""))) or self.ast.records_by_id.get(owned["id"])
                if current:
                    found.append(current)
        return found

    def need_type_obj(self, type_obj: dict, by_value: bool) -> None:
        ptr = is_pointer_type(type_obj)
        for t in type_names(type_obj["qualType"]):
            self.need(t, by_value and not ptr)

    def visit_function(self, node: dict) -> None:
        for inner in node.get("inner", []):
            if inner.get("kind") == "ParmVarDecl":
                self.need_type_obj(inner["type"], True)
        ret = re.match(r"(.*?)\s*\(", node["type"]["qualType"])
        if ret:
            for t in type_names(ret.group(1)):
                self.need(t, not ret.group(1).rstrip().endswith("*"))

    def visit_record(self, decl: Decl) -> None:
        for base in decl.node.get("bases", []):
            for t in type_names(base["type"]["qualType"]):
                self.need(t, True)
        for inner in decl.node.get("inner", []):
            kind = inner.get("kind")
            if kind == "FieldDecl":
                self.need_type_obj(inner["type"], True)
            elif kind == "CXXMethodDecl" and not inner.get("name", "").startswith("operator"):
                self.visit_function(inner)

    # ---- emission -------------------------------------------------------------------------------------------------

    def file_of(self, decl: Decl) -> str:
        return GROUP_FILE.get(decl.group or "", "base")

    def emit_record(self, decl: Decl, fname: str) -> list[str]:
        node = decl.node
        head = f"{node.get('tagUsed', 'struct')} {node.get('name', '')}"
        bases = node.get("bases", [])
        if bases:
            head += " : public " + ", public ".join(b["type"]["qualType"] for b in bases)
        return [head + " {", *self.record_body(node, fname, "    "), "};"]

    def record_body(self, node: dict, fname: str, indent: str) -> list[str]:
        lines: list[str] = []
        pending: list[dict] = []
        for inner in node.get("inner", []):
            kind = inner.get("kind")
            if kind in ("CXXRecordDecl", "RecordDecl") and not inner.get("name"):
                pending.append(inner)
            elif kind == "FieldDecl":
                qual = inner["type"]["qualType"]
                name = inner.get("name", "")
                if "unnamed" in qual or "anonymous" in qual:
                    nested = pending.pop(0)
                    tag = nested.get("tagUsed", "struct")
                    lines.append(f"{indent}{tag} {{")
                    lines.extend(self.record_body(nested, fname, indent + "    "))
                    lines.append(f"{indent}}}{' ' + name if name else ''};")
                    continue
                bits = f" : {inner['inner'][0]['value']}" if inner.get("isBitfield") else ""
                lines.append(f"{indent}{declare(normalize_builtin(qual, fname), name)}{bits};")
            elif kind == "CXXMethodDecl" and not inner.get("name", "").startswith("operator") and not inner.get("isImplicit"):
                lines.append(indent + self.method(inner, fname))
        return lines

    def method(self, node: dict, fname: str) -> str:
        qual, cc = split_cc(node["type"]["qualType"])
        ret = re.match(r"(.*?)\s*\(", qual).group(1)
        params = [declare(normalize_builtin(p["type"]["qualType"], fname), p.get("name", "")).strip()
                  for p in node.get("inner", []) if p.get("kind") == "ParmVarDecl"]
        if qual.rstrip().endswith("...)"):
            params.append("...")
        virtual = "virtual " if node.get("virtual") else ""
        pure = " = 0" if node.get("pure") else ""
        const = " const" if re.search(r"\)\s*const$", qual) else ""
        return f"{virtual}{ret} {cc + ' ' if cc else ''}{node['name']}({', '.join(params)}){const}{pure};"

    def emit_typedef(self, decl: Decl, fname: str) -> list[str]:
        node = decl.node
        qual = node["type"]["qualType"]
        name = node["name"]
        base = re.sub(r"^(struct|union|enum|class)\s+", "", qual)
        if base == name:
            return []
        if "unnamed" in qual or "anonymous" in qual:
            self.manual.append(f"typedef {name}")
            return [f"// MANUAL: typedef {name} of an unnamed type"]
        return ["typedef " + declare(normalize_builtin(qual, fname), name) + ";"]

    def emit_function(self, decl: Decl, fname: str) -> str:
        node = decl.node
        qual, cc = split_cc(node["type"]["qualType"])
        ret = re.match(r"(.*?)\s*\(", qual).group(1)
        params = [declare(normalize_builtin(p["type"]["qualType"], fname), p.get("name", "")).strip()
                  for p in node.get("inner", []) if p.get("kind") == "ParmVarDecl"]
        if qual.rstrip().endswith("...)"):
            params.append("...")
        if not params:
            params = ["void"]
        imp = ("[[noreturn]] " if "noreturn" in node["type"]["qualType"] else "") + ("MINWIN_IMPORT " if decl.key[1] in self.ast.dllimport or decl.group in DLL_GROUP.values() else "")
        return f"{imp}{normalize_builtin(ret, fname)} {cc + ' ' if cc else ''}{node['name']}({', '.join(params)});"

    def emit_enum(self, decl: Decl) -> list[str]:
        node = decl.node
        constants = []
        for c in node.get("inner", []):
            if c.get("kind") != "EnumConstantDecl":
                continue
            value = None
            for e in c.get("inner", []):
                value = e.get("value", value)
            constants.append(f"    {c['name']}" + (f" = {value}" if value is not None else ""))
        return [f"enum {node.get('name', '')} {{", ",\n".join(constants), "};"]

    def emit_macro(self, name: str, values: dict[str, tuple[str, str]]) -> str:
        group, body, _ = self.macros[name]
        if body[0] == "F":
            self.manual.append(f"macro {name}")
            return f"// MANUAL: function-like macro {name}"
        text = body[1:].strip()
        if name in values:
            typ, val = values[name]
            return f"#define {name} {format_constant(typ, val)}"
        if re.fullmatch(r"[A-Za-z_]\w*", text) and self.ast.lookup(text):
            return f"#define {name} {text}"
        self.manual.append(f"macro {name}")
        return f"// MANUAL: #define {name} {text}"


def format_constant(typ: str, value: str) -> str:
    n = int(value)
    typ = typ.replace("const ", "")
    hexed = f"0x{n:x}" if n >= 10 else str(n)
    if typ == "int":
        if n < -0x7fffffff:
            return f"((int)0x{n & 0xffffffff:x})"
        return str(n) if n < 10 else hexed
    if typ == "unsigned int":
        return f"{hexed}u"
    if typ == "unsigned long":
        return f"((DWORD){hexed})"
    if typ == "long":
        return f"((LONG){hexed})" if n >= 0 else f"((LONG)0x{n & 0xffffffff:x})"
    if typ == "unsigned short":
        return f"((WORD){hexed})"
    return f"(({typ}){n})"


def probe_values(names: list[str]) -> dict[str, tuple[str, str]]:
    """(type, value) of each object-like macro: the value from an enumerator initialiser, the type from decltype."""
    probe = WORK / "probe.cpp"
    lines = ['#include "platform/win32.h"']
    lines += [f"enum : long long {{ reb_value_{n} = ({n}) }};\nusing reb_type_{n} = decltype(({n}));" for n in names]
    probe.write_text("\n".join(lines) + "\n")
    out = WORK / "probe.json"
    import subprocess
    with out.open("wb") as f:
        subprocess.run([str(ROOT / "tools" / "minwin_cc.sh"), "-fsyntax-only", "-Xclang", "-ast-dump=json", "-ferror-limit=0",
                        "-Wno-everything", *inv.CLANG_FLAGS, str(probe.relative_to(ROOT))], stdout=f, stderr=subprocess.DEVNULL, cwd=ROOT)
    values: dict[str, tuple[str, str]] = {}
    types: dict[str, str] = {}
    tu = json.loads(out.read_text())
    for node in tu["inner"]:
        name = node.get("name", "")
        if node.get("kind") == "TypeAliasDecl" and name.startswith("reb_type_"):
            types[name[len("reb_type_"):]] = node["type"].get("desugaredQualType") or node["type"]["qualType"]
        elif node.get("kind") == "EnumDecl":
            for c in node.get("inner", []):
                if c.get("kind") == "EnumConstantDecl" and c["name"].startswith("reb_value_"):
                    found = find_value(c.get("inner", []))
                    if found is not None:
                        values[c["name"][len("reb_value_"):]] = ("", found)
    return {n: (types.get(n, "int"), v[1]) for n, v in values.items()}


def find_value(nodes: list[dict]) -> str | None:
    for n in nodes:
        if n.get("kind") == "ConstantExpr" and "value" in n:
            return str(n["value"])
        deeper = find_value(n.get("inner", []))
        if deeper is not None:
            return deeper
    return None


def existing_names() -> set[str]:
    names: set[str] = set()
    for path in MINWIN_DIR.glob("*.h") if MINWIN_DIR.exists() else []:
        text = path.read_text()
        names.update(re.findall(r"#define\s+(\w+)", text))
        names.update(re.findall(r"typedef[^;{]*?\b(\w+)\s*(?:\[[^\]]*\])?\s*;", text))
        names.update(re.findall(r"(?:struct|enum|class)\s+(\w+)\s*(?::[^{;]*)?\{", text))
        names.update(re.findall(r"\b(\w+)\s*\([^()]*\)\s*;", text))
    return names


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("names", nargs="*")
    parser.add_argument("--all-inventory", action="store_true", help="seed with every name of the inventory")
    parser.add_argument("--outdir", type=Path, default=WORK / "scaffold")
    parser.add_argument("--no-skip", action="store_true", help="do not skip names the minwin headers declare already")
    args = parser.parse_args()
    imports = inv.load_import_dlls()
    ast = load_ast(imports)
    macros = load_macros()
    seeds = list(args.names)
    if args.all_inventory:
        decls = inv.Declarations()
        inv.collect_macros(WORK / "master.i", decls)
        inv.collect_ast(WORK / "master.json", decls)
        tokens = inv.game_tokens()
        seeds += sorted(n for n in decls.by_name if n in tokens and n not in inv.EXCLUDED_NAMES
                        and decls.by_name[n][0][1] not in ("dplayx", "smackw32"))
    skip = set() if args.no_skip else existing_names()
    scaffold = Scaffold(ast, macros, skip)
    for seed in seeds:
        scaffold.need(seed)
        scaffold.need_pointed_interface(seed)
    values = probe_values(sorted(n for n in scaffold.need_macros if macros[n][1][0] == "O"))
    files: dict[str, list[tuple[int, str, list[str]]]] = {}
    for key in sorted(scaffold.full | scaffold.fwd, key=lambda k: ast.decls[k].order):
        decl = ast.decls[key]
        fname = scaffold.file_of(decl)
        full = key in scaffold.full
        kind = key[0]
        if kind == "record":
            if full:
                text = scaffold.emit_record(decl, fname)
            else:
                text = [f"{decl.node.get('tagUsed', 'struct')} {key[1]};"]
        elif kind == "typedef":
            text = scaffold.emit_typedef(decl, fname)
        elif kind == "function":
            text = [scaffold.emit_function(decl, fname)]
        elif kind == "var":
            text = [f"extern \"C\" const {declare(decl.node['type']['qualType'].replace('const ', ''), key[1])};"]
        elif kind == "enum":
            text = scaffold.emit_enum(decl)
        else:
            continue
        if text:
            forward = kind == "record" and not full
            files.setdefault(fname, []).append((-1 if forward else decl.order, kind, text))
    macro_text: dict[str, list[str]] = {}
    for name in sorted(scaffold.need_macros, key=lambda n: macros[n][2]):
        fname = GROUP_FILE.get(macros[name][0], "base")
        macro_text.setdefault(fname, []).append(scaffold.emit_macro(name, values))
    args.outdir.mkdir(parents=True, exist_ok=True)
    for fname in sorted(set(files) | set(macro_text)):
        out = ["// DRAFT from tools/minwin_scaffold.py", "#pragma once"]
        out += [f'#include "port/minwin/{dep}.h"' for dep in FILE_INCLUDES.get(fname, ["base"])]
        out.append("")
        functions: list[str] = []
        for _, kind, text in sorted(files.get(fname, []), key=lambda e: e[0]):
            if kind == "function":
                functions.append(text[0])
            else:
                out.extend(text)
        if functions:
            out += ["", 'extern "C" {', *functions, "}"]
        out.append("")
        out.extend(macro_text.get(fname, []))
        (args.outdir / f"{fname}.h").write_text("\n".join(out) + "\n")
    print(f"wrote {sorted(set(files) | set(macro_text))} to {args.outdir}; MANUAL items: {len(scaffold.manual)}")
    for item in scaffold.manual:
        print("  MANUAL", item)


if __name__ == "__main__":
    main()
