#!/usr/bin/env -S uv run --script
# /// script
# dependencies = ["pefile"]
# ///
"""Reference dump of a PE's first-language resources with pefile, in res_dump's format (type name lang size fnv1a64)."""
import argparse
from pathlib import Path

import pefile


def fnv1a64(data: bytes) -> int:
    h = 1469598103934665603
    for b in data:
        h = ((h ^ b) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def label(entry: "pefile.ResourceDirEntryData") -> str:
    return f"'{entry.name}'" if entry.name else str(entry.id)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path)
    args = parser.parse_args()
    pe = pefile.PE(str(args.path))
    if not hasattr(pe, "DIRECTORY_ENTRY_RESOURCE"):
        return
    for type_entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        for name_entry in type_entry.directory.entries:
            lang = name_entry.directory.entries[0]
            data = pe.get_data(lang.data.struct.OffsetToData, lang.data.struct.Size)
            print(f"{label(type_entry)} {label(name_entry)} {lang.id or 0} {len(data)} {fnv1a64(data):016x}")


if __name__ == "__main__":
    main()
