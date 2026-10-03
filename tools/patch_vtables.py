#!/usr/bin/env python3
"""Post-link step for the hybrid DLL: make reconstructed code use the *original* addresses for vtables and
for reimplemented functions whose address is taken, then pin the image to its base.

1. Vtables: clang emits its own copy of each class's vftable and the vptr stores in our ctors/dtors point at
   it. For every vftable whose class has an original vtable in symbols.csv (`Class::vftable[{Base}]`), the
   immediate operand of each vptr-storing instruction (`mov [mem], imm32` / `mov reg, imm32` in code) that
   holds our copy's address is rewritten to the original address. Other dwords that merely happen to equal
   that address (e.g. an array end pointer) are left alone.
2. Function addresses: a relocated dword holding the entry of a reimplemented function (a function pointer
   stored or registered by our code) is rewritten to the original function's address — which jumps to our
   code — so pointer identity matches the original (registries, callbacks, comparisons). The hook table
   itself is excluded: it must point at our code.

Afterwards the relocation directory is dropped: the DLL must load at its fixed base anyway, because direct
calls into REBEXE.EXE use absolute targets.

Runs in the rebellion-dev container (needs llvm-undname-19); uses pefile if available, else a minimal parser.
"""
from __future__ import annotations

import argparse
import csv
import re
import struct
import subprocess
import sys
from pathlib import Path

VFTABLE = re.compile(r"^\s*\S+\s+(\?\?_7\S+)\s+([0-9a-f]{8,16})\s", re.M)


def load_original_vtables(csv_path: Path) -> dict[str, int]:
    out = {}
    with open(csv_path, newline="") as fh:
        for row in csv.DictReader(fh):
            if row["kind"] == "data" and "::vftable" in row["name"]:  # also Class::vftable{Base}
                out[row["name"]] = int(row["address"], 16)
    return out


def our_vtables(map_path: Path) -> dict[str, int]:
    """Map 'Class::vftable' -> address of our copy, from the lld-link /MAP file."""
    found = {m.group(1): int(m.group(2), 16) for m in VFTABLE.finditer(map_path.read_text())}
    if not found:
        return {}
    out = subprocess.run(["llvm-undname-19"], input="\n".join(found) + "\n", capture_output=True, text=True, check=True).stdout
    lines = [l for l in out.splitlines() if l.strip()]
    result = {}
    for mangled, demangled in zip(lines[0::2], lines[1::2]):
        m = re.search(r"(?:const )?([\w:<>, ]+)::`vftable'(?:\{for `(.+)'\})?", demangled)
        if not m:
            continue
        name = m.group(1).strip() + "::vftable" + (f"{{{m.group(2)}}}" if m.group(2) else "")
        result[name] = found[mangled]
    return result


class Pe:
    def __init__(self, data: bytearray):
        self.data = data
        self.pe = struct.unpack_from("<I", data, 0x3C)[0]
        self.nsec = struct.unpack_from("<H", data, self.pe + 6)[0]
        opt = self.pe + 24
        self.base = struct.unpack_from("<I", data, opt + 28)[0]
        self.reloc_dir = opt + 96 + 5 * 8
        sec = opt + struct.unpack_from("<H", data, self.pe + 20)[0]
        self.sections = []
        for i in range(self.nsec):
            o = sec + 40 * i
            vsize, va, rsize, raw = struct.unpack_from("<IIII", data, o + 8)
            characteristics = struct.unpack_from("<I", data, o + 36)[0]
            self.sections.append((va, max(vsize, rsize), raw, rsize, characteristics))
        self.export_dir = opt + 96

    def file_offset(self, rva: int) -> int | None:
        for va, size, raw, rsize, _ in self.sections:
            if va <= rva < va + rsize:
                return raw + rva - va
        return None

    def is_code(self, rva: int) -> bool:
        return any(va <= rva < va + size and ch & 0x20000000 for va, size, _, _, ch in self.sections)

    def u32(self, rva: int) -> int:
        return struct.unpack_from("<I", self.data, self.file_offset(rva))[0]

    def export_rva(self, name: str) -> int | None:
        rva, _ = struct.unpack_from("<II", self.data, self.export_dir)
        if not rva:
            return None
        n_funcs, n_names, funcs, names, ordinals = (self.u32(rva + o) for o in (20, 24, 28, 32, 36))
        for i in range(n_names):
            off = self.file_offset(self.u32(names + 4 * i))
            if self.data[off:off + len(name) + 1] == name.encode() + b"\0":
                ordinal = struct.unpack_from("<H", self.data, self.file_offset(ordinals + 2 * i))[0]
                return self.u32(funcs + 4 * ordinal)
        return None

    def hook_table(self) -> tuple[dict[int, int], range]:
        """(our function VA -> original address, RVA range of the table itself)."""
        start = self.export_rva("reb_hook_table")
        if start is None:
            return {}, range(0)
        table, rva = {}, start
        while (orig := self.u32(rva)) != 0:
            table[self.u32(rva + 4)] = orig
            rva += 16
        return table, range(start, rva + 16)

    def relocated_rvas(self):
        rva, size = struct.unpack_from("<II", self.data, self.reloc_dir)
        off, end = self.file_offset(rva), None
        if off is None:
            return
        end = off + size
        while off < end:
            page, block = struct.unpack_from("<II", self.data, off)
            if block == 0:
                break
            for i in range((block - 8) // 2):
                entry = struct.unpack_from("<H", self.data, off + 8 + 2 * i)[0]
                if entry >> 12 == 3:  # IMAGE_REL_BASED_HIGHLOW
                    yield page + (entry & 0xFFF)
            off += block

    def strip_relocations(self) -> None:
        struct.pack_into("<II", self.data, self.reloc_dir, 0, 0)
        characteristics = struct.unpack_from("<H", self.data, self.pe + 22)[0]
        struct.pack_into("<H", self.data, self.pe + 22, characteristics | 0x0001)  # RELOCS_STRIPPED


def is_store_immediate(data: bytes, off: int) -> bool:
    """True if the dword at file offset `off` is the imm32 of `mov r/m32, imm32` (C7 /0) or `mov r32, imm32`."""
    if off >= 1 and 0xB8 <= data[off - 1] <= 0xBF:
        return True
    for k in range(2, 8):
        if off < k or data[off - k] != 0xC7:
            continue
        modrm = data[off - k + 1]
        mod, reg, rm = modrm >> 6, (modrm >> 3) & 7, modrm & 7
        if reg != 0 or mod == 3:
            continue
        length = 1 + (rm == 4)  # modrm [+ sib]
        base = data[off - k + 2] & 7 if rm == 4 else rm
        length += 1 if mod == 1 else 4 if mod == 2 or (mod == 0 and base == 5) else 0
        if 1 + length == k:
            return True
    return False


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dll", type=Path, required=True)
    ap.add_argument("--map", type=Path, required=True)
    ap.add_argument("--csv", type=Path, required=True)
    args = ap.parse_args()

    originals = load_original_vtables(args.csv)
    ours = our_vtables(args.map)
    redirect = {ours[n]: originals[n] for n in ours if n in originals}
    pe = Pe(bytearray(args.dll.read_bytes()))
    functions, table_rvas = pe.hook_table()
    patched_vtables = patched_functions = 0
    for rva in pe.relocated_rvas():
        off = pe.file_offset(rva)
        if off is None:
            continue
        value = struct.unpack_from("<I", pe.data, off)[0]
        if value in redirect and pe.is_code(rva) and is_store_immediate(pe.data, off):
            struct.pack_into("<I", pe.data, off, redirect[value])
            patched_vtables += 1
        elif value in functions and rva not in table_rvas:
            struct.pack_into("<I", pe.data, off, functions[value])
            patched_functions += 1
    pe.strip_relocations()
    args.dll.write_bytes(pe.data)
    unmapped = sorted(n for n in ours if n not in originals)
    print(f"patch_vtables: {len(redirect)} vtables redirected ({patched_vtables} vptr stores), "
          f"{patched_functions} function-address references redirected"
          + (f"; no original address for: {', '.join(unmapped)}" if unmapped else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
