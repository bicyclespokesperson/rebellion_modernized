#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pefile"]
# ///
"""How complete are the reconstructed vtables? (M5: vtable patching can only go once they are.)

Reads the built hybrid DLL (build/hybrid/smackw32.dll), its link map and gen/report.json. Every vftable the
compiler emitted for a reconstructed class is parsed from the DLL; each slot is either a reconstructed function
(hooked: patch_vtables rewrote the slot to the original address, which jumps to the reconstruction) or an
original function the reconstruction does not replace yet. Slots are further split by what the original is:
tactical (fenced off, M3), library (CRT/LIBCI), or other game code.
"""
from __future__ import annotations

import argparse
import bisect
import csv
import json
import re
import struct
from collections import Counter
from pathlib import Path

import pefile

ROOT = Path(__file__).resolve().parent.parent
MAP_LINE = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s", re.M)
TEXT = (0x401000, 0x657400)
PURECALL = 0x618B80  # the CRT's _purecall: the slot of a pure virtual method (the compiler emits its own)


def class_name(mangled: str) -> str:
    m = re.match(r"\?\?_7(.+?)@@6B(.*)@$", mangled)
    if not m:
        return mangled
    return "::".join(reversed(m.group(1).split("@"))) + (f" (base {m.group(2)})" if m.group(2) else "")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build", type=Path, default=ROOT / "build" / "hybrid")
    ap.add_argument("--list", action="store_true", help="list every vtable with unreconstructed slots")
    args = ap.parse_args()

    pe = pefile.PE(str(args.build / "smackw32.dll"))
    base = pe.OPTIONAL_HEADER.ImageBase
    publics = sorted({int(m[2], 16) for m in MAP_LINE.finditer((args.build / "smackw32.map").read_text())})
    vtables = {m[1]: int(m[2], 16) for m in MAP_LINE.finditer((args.build / "smackw32.map").read_text())
               if m[1].startswith("??_7")}
    hooked = {int(a, 16) for a in json.loads((args.build / "gen" / "report.json").read_text())["reimplemented"]}
    functions = sorted(int(r["address"], 16) for r in csv.DictReader(open(ROOT / "symbols" / "functions.csv")))
    modules = sorted((int(r["start"], 16), int(r["end"], 16), r["subsystem"])
                     for r in csv.DictReader(open(ROOT / "symbols" / "modules.csv")))
    text_end = max(s.VirtualAddress + s.Misc_VirtualSize for s in pe.sections if s.Name.startswith(b".text")) + base

    def origin(address: int) -> str:
        if address == PURECALL:
            return "pure virtual"
        if 0x6153B0 <= address < 0x628E10:
            return "library"
        subsystem = next((n for s, e, n in modules if s <= address < e), "")
        return "tactical" if subsystem == "tactical" else "other game code"

    rows = []
    slot_kinds: Counter = Counter()
    for name, va in sorted(vtables.items(), key=lambda kv: kv[1]):
        following = publics[bisect.bisect_right(publics, va):]
        limit = (following[0] if following else va + 0x400) - va
        slots = []
        for offset in range(0, limit, 4):
            value = struct.unpack("<I", pe.get_data(va + offset - base, 4))[0]
            ours = base <= value < text_end
            original = TEXT[0] <= value < TEXT[1]
            if not (ours or original or (value == 0 and slots)):
                break
            slots.append(value)
        missing = [v for v in slots if TEXT[0] <= v < TEXT[1] and v not in hooked and v != PURECALL]
        for v in missing:
            slot_kinds[origin(v)] += 1
        rows.append((name, len(slots), missing))

    complete = sum(1 for _, _, m in rows if not m)
    only_unreachable = sum(1 for _, _, m in rows if m and all(origin(v) in ("tactical", "library") for v in m))
    print(f"reconstructed vtables: {len(rows)}; slots: {sum(n for _, n, _ in rows)}")
    print(f"  every slot reconstructed (pure virtual slots count as complete): {complete}")
    print(f"  with unreconstructed slots: {len(rows) - complete} "
          f"({sum(len(m) for _, _, m in rows)} slots, {len({v for _, _, m in rows for v in m})} distinct functions)")
    print(f"    of which only tactical/library slots: {only_unreachable}")
    print(f"  unreconstructed slots by origin: {dict(slot_kinds)}")
    if args.list:
        for name, n, missing in rows:
            if missing:
                print(f"{class_name(name)}: {len(missing)}/{n} slots: " + " ".join(f"{v:#x}[{origin(v)}]" for v in missing))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
