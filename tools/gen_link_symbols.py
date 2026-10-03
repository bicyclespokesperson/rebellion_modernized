#!/usr/bin/env python3
"""Generate the hybrid build's link-time glue from the compiled objects and symbols/symbols.csv.

For every symbol the reconstructed code *references but does not define* and that names an original
function/global (via symbols.csv, or the Ghidra default patterns FUN_xxxxxxxx / DAT_xxxxxxxx), emit an
absolute COFF symbol at the original address, so calls/loads go straight into REBEXE.EXE.

For every function the reconstructed code *defines* that names an original function, emit a hook-table
entry; the runtime patches the original to jump to the reimplementation.

Runs inside the rebellion-dev container (needs llvm-nm-19 and llvm-undname-19).
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

CALLING_CONVENTIONS = ("__cdecl", "__thiscall", "__stdcall", "__fastcall", "__vectorcall")
DEFAULT_NAME = re.compile(r"^(?:FUN|DAT|LAB|PTR_FUN|PTR_DAT)_([0-9a-fA-F]{8})$")
TEXT_TYPES = {"T", "t"}
DATA_TYPES = {"D", "d", "B", "b", "R", "r"}
# Defined by the hybrid runtime, never mapped to original addresses.
RUNTIME_SYMBOLS = {"_DllMain@12", "__fltused", "@feat.00"}


@dataclass(frozen=True)
class OriginalSymbol:
    address: int
    kind: str
    name: str


@dataclass
class ObjSymbol:
    raw: str
    nm_type: str
    defined: bool
    key: str = ""
    signature_key: str = ""


def run_nm(objs: list[Path]) -> list[ObjSymbol]:
    out = subprocess.run(["llvm-nm-19", "-P", *map(str, objs)], check=True, capture_output=True, text=True).stdout
    symbols: dict[tuple[str, bool], ObjSymbol] = {}
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 2 or line.endswith(":"):
            continue
        name, nm_type = parts[0], parts[1]
        defined = nm_type not in ("U", "w", "v")
        symbols.setdefault((name, defined), ObjSymbol(name, nm_type, defined))
    defined_names = {n for (n, d) in symbols if d}
    return [s for (n, d), s in symbols.items() if d or n not in defined_names]


def demangle_all(names: list[str]) -> dict[str, str]:
    mangled = [n for n in names if n.startswith("?")]
    if not mangled:
        return {}
    out = subprocess.run(["llvm-undname-19"], input="\n".join(mangled) + "\n", check=True, capture_output=True, text=True).stdout
    lines = [l for l in out.splitlines() if l.strip()]
    result = {}
    for i in range(0, len(lines) - 1, 2):
        result[lines[i]] = lines[i + 1]
    return result


def matching_open_paren(text: str, close_index: int) -> int:
    depth = 0
    for i in range(close_index, -1, -1):
        if text[i] == ")":
            depth += 1
        elif text[i] == "(":
            depth -= 1
            if depth == 0:
                return i
    return -1


def last_identifier(text: str) -> str:
    """Last whitespace-separated token at template depth 0, stripped of pointer/reference sigils."""
    depth = 0
    for i in range(len(text) - 1, -1, -1):
        c = text[i]
        if c == ">":
            depth += 1
        elif c == "<":
            depth -= 1
        elif depth == 0 and c in " *&":
            return text[i + 1:]
    return text


def keys_from_demangled(demangled: str) -> tuple[str, str]:
    """('Qualified::name', 'Qualified::name(params)') from an llvm-undname rendering."""
    text = re.sub(r"^(public|protected|private): ", "", demangled)
    text = re.sub(r"^(static|virtual) ", "", text)
    stripped = re.sub(r"\s*(const|volatile|__ptr64)\s*$", "", text).rstrip()
    if stripped.endswith(")") and any(cc in stripped for cc in CALLING_CONVENTIONS):
        open_i = matching_open_paren(stripped, len(stripped) - 1)
        params = stripped[open_i:]
        head = stripped[:open_i].rstrip()
        cc = max(CALLING_CONVENTIONS, key=head.rfind)
        # Everything after the calling convention is the (possibly qualified/operator) name.
        name = head[head.rindex(cc) + len(cc):].strip() if head.rfind(cc) >= 0 else last_identifier(head)
        return name, f"{name}{params.replace(' ', '')}"
    name = last_identifier(stripped)
    return name, name


def c_symbol_key(raw: str) -> str:
    if raw.startswith("@") and "@" in raw[1:]:
        return raw[1:raw.index("@", 1)]
    base = raw[1:] if raw.startswith("_") else raw
    return re.sub(r"@\d+$", "", base)


def load_csv(path: Path) -> dict[str, OriginalSymbol]:
    table: dict[str, OriginalSymbol] = {}
    with open(path, newline="") as fh:
        for row in csv.DictReader(fh):
            sym = OriginalSymbol(int(row["address"], 16), row["kind"], row["name"])
            for key in {sym.name, sym.name.replace(" ", ""), sym.name.lstrip("_") if row["kind"] == "func" else sym.name}:
                table.setdefault(key, sym)
    return table


def resolve(sym: ObjSymbol, table: dict[str, OriginalSymbol]) -> OriginalSymbol | None:
    for key in (sym.signature_key, sym.key, sym.key.lstrip("_")):
        if key in table:
            return table[key]
        m = DEFAULT_NAME.match(key)
        if m:
            kind = "func" if key.startswith(("FUN_", "LAB_")) else "data"
            return OriginalSymbol(int(m.group(1), 16), kind, key)
    return None


def quote(sym: str) -> str:
    return '"' + sym.replace('"', '\\"') + '"'


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", type=Path, required=True)
    ap.add_argument("--out-asm", type=Path, required=True)
    ap.add_argument("--out-report", type=Path, required=True)
    ap.add_argument("objects", nargs="+", type=Path)
    args = ap.parse_args()

    table = load_csv(args.csv)
    symbols = [s for s in run_nm(args.objects) if s.raw not in RUNTIME_SYMBOLS]
    demangled = demangle_all([s.raw for s in symbols])
    for s in symbols:
        if s.raw in demangled:
            s.key, s.signature_key = keys_from_demangled(demangled[s.raw])
        else:
            s.key = s.signature_key = c_symbol_key(s.raw)

    absolute: list[tuple[str, int]] = []
    hooks: list[tuple[int, str, str]] = []
    errors: list[str] = []
    hooked_by_address: dict[int, str] = {}
    for s in symbols:
        orig = resolve(s, table)
        if orig is None:
            continue
        if not s.defined:
            absolute.append((s.raw, orig.address))
        elif s.nm_type in TEXT_TYPES:
            if orig.kind != "func":
                errors.append(f"{s.key}: defined as code but {orig.name} at {orig.address:#x} is data")
            elif orig.address in hooked_by_address:
                errors.append(f"{orig.address:#x} reimplemented twice: {hooked_by_address[orig.address]} and {s.key}")
            else:
                hooked_by_address[orig.address] = s.key
                hooks.append((orig.address, s.raw, s.signature_key))
        elif s.nm_type in DATA_TYPES:
            errors.append(f"{s.key}: original global at {orig.address:#x} must be declared extern in the hybrid build")
    if errors:
        print("gen_link_symbols: errors:\n  " + "\n  ".join(errors), file=sys.stderr)
        return 1

    lines = ['.globl "@feat.00"', '"@feat.00" = 1']
    for raw, address in sorted(absolute, key=lambda x: x[1]):
        lines += [f".globl {quote(raw)}", f"{quote(raw)} = {address:#x}"]
    lines += ['.section .rdata,"dr"', ".p2align 2", ".globl _reb_hook_table", "_reb_hook_table:"]
    for i, (address, raw, _) in enumerate(sorted(hooks)):
        lines += [f".long {address:#x}", f".long {quote(raw)}", f".long reb_hook_name_{i}"]
    lines += [".long 0", ".long 0", ".long 0"]
    for i, (_, _, key) in enumerate(sorted(hooks)):
        lines.append(f'reb_hook_name_{i}: .asciz "{key}"')
    args.out_asm.parent.mkdir(parents=True, exist_ok=True)
    args.out_asm.write_text("\n".join(lines) + "\n")
    args.out_report.write_text(json.dumps({
        "reimplemented": {f"{a:#010x}": k for a, _, k in sorted(hooks)},
        "referenced_originals": len(absolute),
    }, indent=1) + "\n")
    print(f"gen_link_symbols: {len(hooks)} hooks, {len(absolute)} original references")
    return 0


if __name__ == "__main__":
    sys.exit(main())
