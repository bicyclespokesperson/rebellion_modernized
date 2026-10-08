#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["pefile", "capstone>=5"]
# ///
"""Check that every hooked reconstruction pops the same number of argument bytes as the original (`RET n`).

A mismatch means a wrong calling convention or parameter count: e.g. a free function declared __cdecl whose
original is __stdcall (RET 4) leaves the caller's stack 4 bytes off. That corrupts the caller in a live run,
while unit difftest of the function alone can still pass.

  check_callee_pop.py            # all hooks of the last build
"""
from __future__ import annotations

import argparse
import bisect
import csv
import re
import struct
import sys
from pathlib import Path

import capstone
import pefile

ROOT = Path(__file__).resolve().parent.parent
EXE = ROOT / "orig_game_files" / "Star Wars - Rebellion" / "REBEXE.EXE"
DLL = ROOT / "build" / "hybrid" / "smackw32.dll"
MAP = ROOT / "build" / "hybrid" / "smackw32.map"
MAP_SYMBOL = re.compile(r"^\s*0001:[0-9a-f]+\s+(\S+)\s+([0-9a-f]{8,16})\s", re.M)


class Image:
    def __init__(self, path: Path):
        self.pe = pefile.PE(str(path), fast_load=True)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.disasm = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)

    def read(self, va: int, size: int) -> bytes:
        return self.pe.get_data(va - self.base, size)

    def u32(self, va: int) -> int:
        return struct.unpack("<I", self.read(va, 4))[0]

    def callee_pops(self, start: int, end: int) -> set[int]:
        """Argument bytes popped by the RET instructions of [start, end) (linear sweep)."""
        pops = set()
        for insn in self.disasm.disasm(self.read(start, end - start), start):
            if insn.mnemonic == "ret":
                pops.add(int(insn.op_str, 0) if insn.op_str else 0)
        return pops


def original_extents() -> dict[int, int]:
    """Function start -> contiguous end. functions.csv sizes count Ghidra's whole body, which can include chunks
    elsewhere, so a function also ends where the next one starts."""
    with open(ROOT / "symbols" / "functions.csv", newline="") as fh:
        rows = sorted((int(r["address"], 16), int(r["size"])) for r in csv.DictReader(fh))
    return {a: min(a + size, nxt) for (a, size), (nxt, _) in zip(rows, rows[1:] + [(1 << 32, 0)])}


def hook_entries(dll: Image) -> list[tuple[int, int, str]]:
    """(original address, our address, name) from the exported hook table."""
    dll.pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_EXPORT"]])
    table = next(e.address for e in dll.pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name == b"reb_hook_table") + dll.base
    out = []
    while (orig := dll.u32(table)) != 0:
        name_va = dll.u32(table + 8)
        name = dll.read(name_va, 256).split(b"\0")[0].decode(errors="replace") if name_va else ""
        out.append((orig, dll.u32(table + 4), name))
        table += 16
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.parse_args()
    exe, dll = Image(EXE), Image(DLL)
    ends = original_extents()
    ours = sorted(int(m[2], 16) for m in MAP_SYMBOL.finditer(MAP.read_text()))
    mismatches = 0
    for orig, repl, name in hook_entries(dll):
        if orig not in ends:
            continue
        i = bisect.bisect_right(ours, repl)
        end = ours[i] if i < len(ours) else repl + 0x1000
        theirs, mine = exe.callee_pops(orig, ends[orig]), dll.callee_pops(repl, end)
        if theirs and mine and theirs != mine:
            mismatches += 1
            print(f"{orig:#010x} {name}: original pops {sorted(theirs)}, reconstruction pops {sorted(mine)}")
    print(f"{mismatches} calling-convention mismatches")
    return 1 if mismatches else 0


if __name__ == "__main__":
    sys.exit(main())
