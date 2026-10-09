#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pefile"]
# ///
"""Validate a REB_OWN_DATA build (tools/build_hybrid.sh --own-data): every label of the data image sits at the
offset of its original address, the block's bytes equal the EXE's except for the relocated pointers, and every
runtime patch site really holds an address of the block.

Reads build/hybrid/smackw32.dll, its link map, gen/data_refs.json and the original EXE.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from pathlib import Path

import pefile

sys.path.insert(0, str(Path(__file__).resolve().parent))
import own_data  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
MAP_LINE = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s+data_image\.obj$", re.M)


class Image:
    def __init__(self, path: Path):
        self.pe = pefile.PE(str(path))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase

    def read(self, va: int, n: int) -> bytes:
        data = self.pe.get_data(va - self.base, n)
        return data + bytes(n - len(data))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build", type=Path, default=ROOT / "build" / "hybrid")
    ap.add_argument("--exe", type=Path, required=True)
    args = ap.parse_args()

    dll = Image(args.build / "smackw32.dll")
    exe = Image(args.exe)
    symbols = {m[1]: int(m[2], 16) for m in MAP_LINE.finditer((args.build / "smackw32.map").read_text())}
    refs = json.loads((args.build / "gen" / "data_refs.json").read_text())["data"]
    block = symbols["_reb_data_block"]
    errors: list[str] = []

    checked = 0
    for raw, entry in refs.items():
        address = int(entry["address"], 16)
        if raw not in symbols:
            errors.append(f"{raw}: not defined by the data image")
            continue
        if own_data.in_segment(address):
            if symbols[raw] - block != address - own_data.BLOCK_BASE:
                errors.append(f"{raw}: at block+{symbols[raw] - block:#x}, expected {address - own_data.BLOCK_BASE:#x}")
        elif dll.read(symbols[raw], 4) != exe.read(address, 4):
            errors.append(f"{raw}: rdata copy differs from {address:#x}")
        checked += 1

    differing = 0
    for lo, hi in own_data.OWN_SEGMENTS:
        mine, theirs = dll.read(block + lo - own_data.BLOCK_BASE, hi - lo), exe.read(lo, hi - lo)
        for offset in range(0, hi - lo, 4):
            if mine[offset:offset + 4] == theirs[offset:offset + 4]:
                continue
            differing += 1
            value = struct.unpack_from("<I", mine, offset)[0]
            target = struct.unpack_from("<I", theirs, offset)[0]
            expected_block = block + target - own_data.BLOCK_BASE
            if value not in (expected_block,) and not (0x20000000 <= value < 0x30000000):
                errors.append(f"block dword at {lo + offset:#x}: {value:#x} (EXE has {target:#x})")
    gap_bytes = dll.read(block + own_data.OWN_SEGMENTS[0][1] - own_data.BLOCK_BASE,
                         own_data.OWN_SEGMENTS[1][0] - own_data.OWN_SEGMENTS[0][1])
    if any(gap_bytes):
        errors.append("the CRT gap of the block is not zero")

    table = symbols["_reb_code_refs"]
    sites = 0
    while (site := struct.unpack("<I", dll.read(table + 4 * sites, 4))[0]) != 0:
        value = struct.unpack("<I", exe.read(site, 4))[0]
        if not own_data.in_segment(value):
            errors.append(f"patch site {site:#x} holds {value:#x}, which is not in the block")
        sites += 1

    print(f"check_data_image: {checked} labels, {differing} relocated dwords, {sites} patch sites, {len(errors)} errors")
    for e in errors[:40]:
        print("  " + e)
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
