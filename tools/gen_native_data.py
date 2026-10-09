#!/usr/bin/env python3
"""Native build: define the original's globals that the reconstructed code references, as typed-size C++ storage with their
initial values taken from REBEXE.EXE (docs/native_linux.md).

The Windows builds copy REBEXE.EXE's .data/.bss as one block at the original 32-bit layout (tools/gen_data_image.py). On LP64 every
global that holds a pointer (or `long`, or a class with a vtable) is bigger than it was, so the block cannot work. Instead each
global the objects reference but do not define becomes its own variable:

  * name, size and alignment come from the compiler (libclang parses every translation unit with the flags of the build and reports
    the declared type of each `extern` global);
  * the initial bytes come from the EXE (symbols.csv gives the address). Globals without pointers are copied verbatim; arrays
    of `const char*` are rebuilt with pointers to string literals (their targets are read from the EXE); a global that holds pointers
    and has a non-zero initial value in the EXE is reported as unsupported (the constructors of the static-initializer table fill
    those at run time);
  * globals that overlap in the original (aliases, arrays declared larger than the distance to the next symbol) are merged into one
    block at the original layout, with each name a label inside it.

Runs in the rebellion-native container (clang.cindex from python3-clang-19, pefile). Outputs, in --out: native_data.cpp,
native_data.json (the layout of every global: the static-initializer generator maps the original's stores through it).
"""
from __future__ import annotations

import argparse
import bisect
import csv
import json
import multiprocessing
import re
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

import pefile
from clang import cindex

sys.path.insert(0, str(Path(__file__).resolve().parent))
import own_data  # noqa: E402

RDATA = own_data.RDATA_RANGE


class Exe:
    def __init__(self, path: Path):
        self.pe = pefile.PE(str(path))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.sections = [(self.base + s.VirtualAddress, max(s.Misc_VirtualSize, s.SizeOfRawData), s.get_data()[: s.SizeOfRawData])
                         for s in self.pe.sections]

    def read(self, va: int, n: int) -> bytes:
        for lo, size, raw in self.sections:
            if lo <= va < lo + size:
                chunk = raw[va - lo: va - lo + n]
                return chunk + bytes(max(0, min(n, lo + size - va) - len(chunk)))
        raise ValueError(f"{va:#x} is outside the image")

    def cstring(self, va: int, limit: int = 4096) -> bytes | None:
        raw = self.read(va, limit)
        end = raw.find(b"\0")
        return raw[:end] if end >= 0 else None

    def in_image(self, va: int) -> bool:
        return any(lo <= va < lo + size for lo, size, _ in self.sections)


# ---- symbols of the objects -------------------------------------------------------------------------------------------------


def nm_symbols(objects: list[Path]) -> tuple[set[str], set[str]]:
    undefined: set[str] = set()
    defined: set[str] = set()
    for i in range(0, len(objects), 400):
        out = subprocess.run(["llvm-nm-19", "-P", *map(str, objects[i:i + 400])], check=True, capture_output=True, text=True).stdout
        for line in out.splitlines():
            parts = line.split()
            if len(parts) < 2 or line.endswith(":"):
                continue
            (undefined if parts[1] in ("U", "w", "v") else defined).add(parts[0])
    return undefined, defined


def demangle(names: list[str]) -> dict[str, str]:
    if not names:
        return {}
    out = subprocess.run(["llvm-cxxfilt-19"], input="\n".join(names) + "\n", capture_output=True, text=True, check=True).stdout
    return dict(zip(names, out.splitlines()))


# ---- types from libclang ---------------------------------------------------------------------------------------------------

POINTER_KINDS = None


def pointer_like(t: "cindex.Type", depth: int = 0) -> bool:
    """True when the type's size or layout differs between ILP32 and LP64: pointers, references, long, polymorphic classes."""
    global POINTER_KINDS
    K = cindex.TypeKind
    if POINTER_KINDS is None:
        POINTER_KINDS = {K.POINTER, K.MEMBERPOINTER, K.LVALUEREFERENCE, K.RVALUEREFERENCE, K.LONG, K.ULONG, K.BLOCKPOINTER}
    t = t.get_canonical()
    if t.kind in POINTER_KINDS:
        return True
    if t.kind in (K.CONSTANTARRAY, K.INCOMPLETEARRAY, K.VARIABLEARRAY):
        return pointer_like(t.element_type, depth + 1)
    if t.kind == K.RECORD and depth < 12:
        decl = t.get_declaration()
        for child in decl.get_children():
            if child.kind == cindex.CursorKind.FIELD_DECL and pointer_like(child.type, depth + 1):
                return True
            if child.kind == cindex.CursorKind.CXX_BASE_SPECIFIER and pointer_like(child.type, depth + 1):
                return True
            if child.kind in (cindex.CursorKind.CXX_METHOD, cindex.CursorKind.DESTRUCTOR) and child.is_virtual_method():
                return True
    return False


def type_info(t: "cindex.Type") -> dict:
    K = cindex.TypeKind
    c = t.get_canonical()
    info = {"spelling": t.spelling, "pointer_like": pointer_like(t)}
    size = c.get_size()
    info["size"] = size if size >= 0 else None
    try:
        align = c.get_align()
    except Exception:
        align = -1
    info["align"] = align if align > 0 else None
    arr = c
    dims = []
    while arr.kind in (K.CONSTANTARRAY, K.INCOMPLETEARRAY):
        dims.append(arr.element_count if arr.kind == K.CONSTANTARRAY else None)
        arr = arr.element_type.get_canonical()
    info["dims"] = dims
    info["element"] = arr.spelling
    esize = arr.get_size()
    info["element_size"] = esize if esize > 0 else None
    info["element_is_pointer"] = arr.kind == K.POINTER
    if arr.kind == K.POINTER:
        pointee = arr.get_pointee().get_canonical()
        info["pointee_is_char"] = pointee.kind in (K.CHAR_S, K.CHAR_U, K.SCHAR, K.UCHAR)
    return info


def parse_unit(job: tuple[list[str], list[str], str]) -> dict[str, dict]:
    arguments, wanted_list, resource = job
    wanted = set(wanted_list)
    found: dict[str, dict] = {}
    index = cindex.Index.create()
    tu = index.parse(None, args=arguments + ["-resource-dir", resource, "-Wno-everything"],
                     options=cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)

    def visit(cursor) -> None:
        for c in cursor.get_children():
            if c.kind == cindex.CursorKind.VAR_DECL and c.spelling in wanted:
                try:
                    found.setdefault(c.spelling, type_info(c.type))
                except Exception as e:  # noqa: BLE001
                    found.setdefault(c.spelling, {"error": str(e)})
            elif c.kind in (cindex.CursorKind.NAMESPACE, cindex.CursorKind.LINKAGE_SPEC):
                visit(c)

    visit(tu.cursor)
    return found


def compile_arguments(build: Path) -> list[tuple[str, list[str]]]:
    db = cindex.CompilationDatabase.fromDirectory(str(build))
    jobs = []
    for cmd in db.getAllCompileCommands():
        args = list(cmd.arguments)[1:]
        keep: list[str] = []
        skip = 0
        for a in args:
            if skip:
                skip -= 1
            elif a in ("-o", "-MT", "-MF"):
                skip = 1
            elif a in ("-c", "-MD"):
                pass
            elif a == cmd.filename:
                pass
            else:
                keep.append(a)
        if "src/game/" in cmd.filename and cmd.filename.endswith(".cpp"):
            jobs.append((cmd.filename, keep + [cmd.filename]))
    return jobs


# ---- emission --------------------------------------------------------------------------------------------------------------


def c_string(raw: bytes) -> str:
    return '"' + "".join(f"\\x{b:02x}" if not (0x20 <= b < 0x7F) or b in (0x22, 0x5C, 0x3F) else chr(b) for b in raw) + '"'


def final_align(align: int, size: int) -> int:
    """The x86-64 ABI aligns arrays of 16 bytes or more to 16, and the compiler relies on it for externs (aligned SSE stores)."""
    return max(align, 16 if size >= 16 else 8 if size >= 8 else 1)


def byte_lines(data: bytes) -> list[str]:
    return ["    " + ",".join(str(b) for b in data[i:i + 32]) + "," for i in range(0, len(data), 32)]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", type=Path, required=True, help="the cmake build directory (objects, compile_commands.json)")
    ap.add_argument("--exe", type=Path, required=True)
    ap.add_argument("--symbols", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--resource-dir", default="/usr/lib/llvm-19/lib/clang/19")
    ap.add_argument("-j", type=int, default=multiprocessing.cpu_count())
    args = ap.parse_args()

    objects = sorted(o for o in args.build.glob("CMakeFiles/*.dir/**/*.o") if "reb_native_gen" not in o.parts[-6:-1][0:0] and "reb_native_gen.dir" not in str(o))
    undefined, defined = nm_symbols(objects)
    csv_rows = [r for r in csv.DictReader(open(args.symbols / "symbols.csv")) if r["kind"] == "data"]
    by_name = {r["name"]: r for r in csv_rows}
    mangled = [n for n in undefined - defined if n.startswith("_Z")]
    pretty = demangle(mangled)
    wanted: dict[str, dict] = {}
    unknown = []
    for name in sorted(undefined - defined):
        row = by_name.get(name) or by_name.get(pretty.get(name, ""))
        if row:
            wanted[name] = row
        elif name.startswith("g_"):
            unknown.append(name)

    exe = Exe(args.exe)
    addresses = sorted(int(r["address"], 16) for r in csv_rows)

    jobs = [(src, a, sorted(wanted), args.resource_dir) for src, a in compile_arguments(args.build)]
    print(f"gen_native_data: {len(wanted)} globals to define, parsing {len(jobs)} translation units")
    types: dict[str, dict] = {}
    with multiprocessing.Pool(args.j) as pool:
        for found in pool.imap_unordered(parse_unit, [(a, w, r) for _, a, w, r in jobs], chunksize=4):
            for name, info in found.items():
                old = types.get(name)
                if old is None or (info.get("size") or 0) > (old.get("size") or 0):
                    types[name] = info

    entries = []
    problems = []
    for name, row in sorted(wanted.items(), key=lambda kv: int(kv[1]["address"], 16)):
        address = int(row["address"], 16)
        i = bisect.bisect_right(addresses, address)
        distance = (addresses[i] - address) if i < len(addresses) else 0x1000
        if row["size"]:
            distance = int(row["size"])
        info = types.get(name)
        if not info or "error" in info:
            problems.append(f"{name} ({address:#x}): no declaration found by libclang ({info})")
            continue
        entries.append({"name": name, "address": address, "distance": distance, "info": info})

    out_lines = ['// Generated by tools/gen_native_data.py from REBEXE.EXE and the compiler\'s view of the declarations. Do not edit.',
                 "#include <cstddef>", "#include <cstdint>", ""]
    layout = {}
    counts = defaultdict(int)

    def define(entry: dict) -> None:
        name, address, info = entry["name"], entry["address"], entry["info"]
        size, align = info["size"], info["align"] or 1
        dims = info["dims"]
        incomplete = size is None
        extent32 = entry["distance"]
        raw_symbol = f'__asm__("{name}")'
        if incomplete:
            esize = info["element_size"] or 1
            count = max(1, extent32 // esize)
            size = count * esize
            align = max(align, esize if esize in (1, 2, 4, 8) else 1)
        if info["pointer_like"]:
            initial = exe.read(address, extent32) if not incomplete else exe.read(address, size)
            string_pointers = info["element_is_pointer"] and info.get("pointee_is_char") and len(dims) <= 1
            if any(initial) and not string_pointers:
                problems.append(f"{name} ({address:#x}): holds pointers (type {info['spelling']}) and has a non-zero initial value in the EXE")
            if string_pointers and any(initial):
                count = (dims[0] if dims[0] is not None else extent32 // 4) if dims else 1
                lines = [f"const char* {name.replace('::', '_')}_n[{count}] {raw_symbol} = {{"]
                for k in range(count):
                    value = int.from_bytes(exe.read(address + 4 * k, 4), "little")
                    if value == 0:
                        lines.append("    nullptr,")
                    else:
                        text = exe.cstring(value)
                        if text is None:
                            problems.append(f"{name}[{k}]: pointer {value:#x} is not a string")
                            lines.append("    nullptr,")
                        else:
                            lines.append(f"    {c_string(text)},")
                lines.append("};")
                out_lines.extend(lines)
                counts["pointer table with strings"] += 1
                layout[name] = {"address": address, "size": count * 8, "kind": "string pointers"}
                return
            out_lines.append(f"alignas({final_align(align, size)}) unsigned char {name}_n[{size}] {raw_symbol};")
            counts["zero-initialised, holds pointers"] += 1
            layout[name] = {"address": address, "size": size, "kind": "zero"}
            return
        data = exe.read(address, size)
        if any(data):
            out_lines.append(f"alignas({final_align(align, size)}) unsigned char {name}_n[{size}] {raw_symbol} = {{")
            out_lines.extend(byte_lines(data))
            out_lines.append("};")
            counts["copied from the EXE"] += 1
            layout[name] = {"address": address, "size": size, "kind": "data"}
        else:
            out_lines.append(f"alignas({final_align(align, size)}) unsigned char {name}_n[{size}] {raw_symbol};")
            counts["zero"] += 1
            layout[name] = {"address": address, "size": size, "kind": "zero"}

    for entry in entries:
        define(entry)
        layout[entry["name"]]["extent32"] = entry["distance"]

    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "native_data.cpp").write_text("\n".join(out_lines) + "\n")
    (args.out / "native_data.json").write_text(json.dumps({"globals": layout, "unknown_g_symbols": unknown, "problems": problems}, indent=1))
    print(f"gen_native_data: defined {len(layout)} globals {dict(counts)}; {len(problems)} problems, {len(unknown)} unknown g_ symbols")
    for p in problems[:60]:
        print("  PROBLEM:", p)
    for u in unknown[:20]:
        print("  UNKNOWN:", u)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
