#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# ///
"""Compare two PE files section by section, ignoring the link timestamp, the checksum and the debug directory.

Used for "same code, different headers" checks (docs/phase3_plan.md P0c: the standalone built with and without
REB_MINWIN must be the same executable). Prints one line per section and exits 1 if any section's bytes differ.

  tools/pe_compare.py A.exe B.exe
"""
from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Section:
    name: str
    virtual_size: int
    virtual_address: int
    raw_size: int
    raw_offset: int
    characteristics: int


@dataclass
class Pe:
    data: bytes
    sections: list[Section]
    debug_directory: tuple[int, int]
    image_base: int
    entry: int


def parse(path: Path) -> Pe:
    data = path.read_bytes()
    pe_offset = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[pe_offset:pe_offset + 4] == b"PE\0\0", "not a PE file"
    _, section_count, _, _, _, optional_size, _ = struct.unpack_from("<HHIIIHH", data, pe_offset + 4)
    optional = pe_offset + 24
    magic = struct.unpack_from("<H", data, optional)[0]
    assert magic == 0x10B, "PE32 only"
    entry = struct.unpack_from("<I", data, optional + 16)[0]
    image_base = struct.unpack_from("<I", data, optional + 28)[0]
    debug = struct.unpack_from("<II", data, optional + 96 + 6 * 8)
    sections = []
    table = optional + optional_size
    for index in range(section_count):
        raw = data[table + index * 40: table + (index + 1) * 40]
        name = raw[:8].rstrip(b"\0").decode("ascii", "replace")
        vsize, vaddr, rsize, roffset = struct.unpack_from("<IIII", raw, 8)
        characteristics = struct.unpack_from("<I", raw, 36)[0]
        sections.append(Section(name, vsize, vaddr, rsize, roffset, characteristics))
    return Pe(data, sections, debug, image_base, entry)


def section_bytes(pe: Pe, section: Section) -> bytearray:
    chunk = bytearray(pe.data[section.raw_offset: section.raw_offset + section.raw_size])
    start, size = pe.debug_directory
    if size and section.virtual_address <= start < section.virtual_address + section.raw_size:
        offset = start - section.virtual_address
        for entry in range(0, size, 28):
            chunk[offset + entry + 4: offset + entry + 8] = b"\0\0\0\0"
            raw_data_va = struct.unpack_from("<I", chunk, offset + entry + 20)[0]
            data_size = struct.unpack_from("<I", chunk, offset + entry + 16)[0]
            if section.virtual_address <= raw_data_va < section.virtual_address + section.raw_size:
                inner = raw_data_va - section.virtual_address
                chunk[inner: inner + data_size] = b"\0" * min(data_size, len(chunk) - inner)
    return chunk


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("a", type=Path)
    parser.add_argument("b", type=Path)
    args = parser.parse_args()
    a, b = parse(args.a), parse(args.b)
    different = False
    if [s.name for s in a.sections] != [s.name for s in b.sections]:
        print("section lists differ:", [s.name for s in a.sections], [s.name for s in b.sections])
        return 1
    print(f"entry {a.entry:#x} vs {b.entry:#x}, image base {a.image_base:#x} vs {b.image_base:#x}")
    for sa, sb in zip(a.sections, b.sections):
        if (sa.virtual_size, sa.virtual_address, sa.raw_size) != (sb.virtual_size, sb.virtual_address, sb.raw_size):
            print(f"{sa.name:8} layout differs: size {sa.raw_size:#x} vs {sb.raw_size:#x}, va {sa.virtual_address:#x} vs {sb.virtual_address:#x}")
            different = True
            continue
        ba, bb = section_bytes(a, sa), section_bytes(b, sb)
        count = sum(1 for x, y in zip(ba, bb) if x != y)
        first = next((i for i, (x, y) in enumerate(zip(ba, bb)) if x != y), None)
        print(f"{sa.name:8} {sa.raw_size:9} bytes  {'identical' if count == 0 else f'{count} bytes differ (first at +{first:#x}, va {sa.virtual_address + first:#x})'}")
        different |= count != 0
    return 1 if different else 0


if __name__ == "__main__":
    sys.exit(main())
