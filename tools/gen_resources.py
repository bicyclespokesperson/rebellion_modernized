#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pefile"]
# ///
"""Copy the resource section of the user's REBEXE.EXE into a Win32 .res file for the standalone build.

The original EXE carries its own resources (animation / message-template RCDATA read through FindResourceA on the
module handle, the icons, the cursors, the version info). They are the user's copyrighted data, so they are extracted at
build time from the install (like the data image) and never committed.

Writes the .res format (a null resource header, then one entry per resource) that lld-link links directly.
"""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

import pefile

MEMORY_FLAGS = 0x1030  # MOVEABLE | PURE | DISCARDABLE, what the resource compilers write


def identifier(value: int | str) -> bytes:
    if isinstance(value, int):
        return struct.pack("<HH", 0xFFFF, value)
    text = (value.decode("utf-16-le", "replace") if isinstance(value, bytes) else value).upper().encode("utf-16-le") + b"\0\0"
    return text


def align4(data: bytes) -> bytes:
    return data + b"\0" * (-len(data) % 4)


def entry(type_id: int | str, name_id: int | str, language: int, data: bytes) -> bytes:
    ids = align4(identifier(type_id) + identifier(name_id))
    header_size = 8 + len(ids) + 16
    header = struct.pack("<II", len(data), header_size) + ids + struct.pack("<IHHII", 0, MEMORY_FLAGS, language, 0, 0)
    return header + align4(data)


def resources(exe: Path) -> list[tuple[int | str, int | str, int, bytes]]:
    pe = pefile.PE(str(exe))
    out = []
    for type_entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        type_id = type_entry.id if type_entry.id is not None else type_entry.name.string
        for name_entry in type_entry.directory.entries:
            name_id = name_entry.id if name_entry.id is not None else name_entry.name.string
            for language_entry in name_entry.directory.entries:
                data = pe.get_data(language_entry.data.struct.OffsetToData, language_entry.data.struct.Size)
                out.append((type_id, name_id, language_entry.id or 0, data))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()
    items = resources(args.exe)
    null_header = struct.pack("<II", 0, 0x20) + identifier(0) + identifier(0) + struct.pack("<IHHII", 0, 0, 0, 0, 0)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(null_header + b"".join(entry(*item) for item in items))
    print(f"gen_resources: {len(items)} resources, {sum(len(d) for *_, d in items)} bytes -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
