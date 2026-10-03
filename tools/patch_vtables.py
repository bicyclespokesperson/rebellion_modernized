#!/usr/bin/env python3
"""Post-link step for the hybrid DLL: make reconstructed constructors/destructors store the *original*
vtable addresses, then pin the image to its base.

clang emits its own copy of each class's vftable and the vptr stores in our ctors/dtors point at it. For
every vftable symbol whose class has an original vtable in symbols.csv (name `Class::vftable`), rewrite
each base-relocated dword in the image that holds our copy's address to the original address. Afterwards
the relocation directory is dropped: the DLL must load at its fixed base anyway, because direct calls into
REBEXE.EXE use absolute targets.

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
            self.sections.append((va, max(vsize, rsize), raw, rsize))

    def file_offset(self, rva: int) -> int | None:
        for va, size, raw, rsize in self.sections:
            if va <= rva < va + rsize:
                return raw + rva - va
        return None

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
    patched = 0
    for rva in pe.relocated_rvas():
        off = pe.file_offset(rva)
        if off is None:
            continue
        value = struct.unpack_from("<I", pe.data, off)[0]
        if value in redirect:
            struct.pack_into("<I", pe.data, off, redirect[value])
            patched += 1
    pe.strip_relocations()
    args.dll.write_bytes(pe.data)
    unmapped = sorted(n for n in ours if n not in originals)
    print(f"patch_vtables: {len(redirect)} vtables redirected ({patched} references)"
          + (f"; no original address for: {', '.join(unmapped)}" if unmapped else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
