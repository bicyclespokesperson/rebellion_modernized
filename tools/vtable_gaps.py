#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pefile", "capstone"]
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
from collections.abc import Callable
from pathlib import Path

import capstone
import pefile

ROOT = Path(__file__).resolve().parent.parent
MAP_LINE = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{16})\s", re.M)
TEXT = (0x401000, 0x657400)
PURECALL = 0x618B80  # the CRT's _purecall: the slot of a pure virtual method (the compiler emits its own)


def class_name(mangled: str) -> str:
    m = re.match(r"\?\?_7(.+?)@@6B(.*)@$", mangled)
    if not m:
        return mangled
    return "::".join(reversed(m.group(1).split("@"))) + (f" (base {m.group(2).rstrip('@')})" if m.group(2) else "")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--build", type=Path, default=ROOT / "build" / "hybrid")
    ap.add_argument("--list", action="store_true", help="list every vtable with unreconstructed slots")
    ap.add_argument("--exe", type=Path, help="the original EXE: also compare every slot with the original vtable's")
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

    originals = {r["name"]: int(r["address"], 16) for r in csv.DictReader(open(ROOT / "symbols" / "symbols.csv"))
                 if r["kind"] == "data" and "::vftable" in r["name"]}
    exe = pefile.PE(str(args.exe)) if args.exe else None
    differing: list[str] = []
    sizes = {int(r["address"], 16): int(r["size"]) for r in csv.DictReader(open(ROOT / "symbols" / "functions.csv"))}

    disassembler = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    disassembler.detail = True

    def forwards_to(thunk: int, target: int) -> bool:
        """The original function at `thunk` only hands its own arguments, unchanged and in order, to `target`
        (a derived class that overrides a slot just to call the base: `push args; call base; ret n`, or `jmp base`).
        Our vtable inherits `target` directly, which behaves the same."""
        if exe is None:
            return False
        eb = exe.OPTIONAL_HEADER.ImageBase
        loaded: dict[int, int] = {}
        pushed: list[int] = []
        called = False
        for ins in disassembler.disasm(exe.get_data(thunk - eb, 48), thunk):
            ops = ins.operands
            if ins.mnemonic == "jmp" and ops[0].type == capstone.x86.X86_OP_IMM:
                return not pushed and not called and ops[0].imm == target and not loaded
            if ins.mnemonic == "mov" and ops[1].type == capstone.x86.X86_OP_MEM and ops[1].mem.base == capstone.x86.X86_REG_ESP \
                    and ops[0].type == capstone.x86.X86_OP_REG and ops[1].mem.index == 0 and not called:
                loaded[ops[0].reg] = (ops[1].mem.disp - 4) // 4 - len(pushed)
            elif ins.mnemonic == "push" and ops[0].type == capstone.x86.X86_OP_REG and ops[0].reg in loaded and not called:
                pushed.append(loaded[ops[0].reg])
            elif ins.mnemonic == "call" and ops[0].type == capstone.x86.X86_OP_IMM and not called:
                if ops[0].imm != target:
                    return False
                called = True
            elif ins.mnemonic == "ret" and called:
                count = len(pushed)
                return pushed == list(range(count - 1, -1, -1)) and (ops[0].imm if ops else 0) == 4 * count
            else:
                return False
        return False

    def trivial_behaviour(code_at: Callable[[int], bytes], address: int, follow_jumps: bool = True) -> tuple[int | None, int] | None:
        """(the low 16 bits of the constant returned in eax or None if eax is untouched, bytes popped by ret) for a
        body that is only `[xor eax,eax | mov eax,imm | mov ax,imm] ; ret [n]`, following `jmp` thunks; None for
        anything else."""
        value: int | None = None
        for _ in range(8):
            for ins in disassembler.disasm(code_at(address), address):
                ops = ins.operands
                if ins.mnemonic == "jmp" and ops[0].type == capstone.x86.X86_OP_IMM:
                    if not follow_jumps:
                        return None
                    address = ops[0].imm
                    break
                if ins.mnemonic == "xor" and ops[0].reg == ops[1].reg and \
                        ops[0].reg in (capstone.x86.X86_REG_EAX, capstone.x86.X86_REG_AX):
                    value = 0
                elif ins.mnemonic == "mov" and ops[0].type == capstone.x86.X86_OP_REG and \
                        ops[0].reg in (capstone.x86.X86_REG_EAX, capstone.x86.X86_REG_AX) and \
                        ops[1].type == capstone.x86.X86_OP_IMM:
                    value = ops[1].imm & 0xffff
                elif ins.mnemonic == "ret":
                    return value, (ops[0].imm if ops else 0)
                else:
                    return None
            else:
                return None
        return None

    def exe_code(address: int) -> bytes:
        return exe.get_data(address - exe.OPTIONAL_HEADER.ImageBase, 16)

    def dll_code(address: int) -> bytes:
        return pe.get_data(address - base, 16)

    def own_copy_differs(ours_va: int, original_slot: int) -> bool:
        """Our own copy of a trivial stub returns something else than the original function in this slot does."""
        if exe is None:
            return False
        ours_behaviour = trivial_behaviour(dll_code, ours_va, follow_jumps=False)
        if ours_behaviour is None:
            return False
        return trivial_behaviour(exe_code, original_slot) != ours_behaviour

    def normalized_listing(start: int, size: int) -> list[str]:
        """The function's instructions with branch targets inside it made relative and those outside absolute: equal
        bytes are not enough, a relative call to another address has the same bytes at a different place."""
        eb = exe.OPTIONAL_HEADER.ImageBase
        listing = []
        for ins in disassembler.disasm(exe.get_data(start - eb, size), start):
            text = ins.op_str
            if ins.group(capstone.CS_GRP_JUMP) or ins.group(capstone.CS_GRP_CALL):
                if ins.operands and ins.operands[0].type == capstone.x86.X86_OP_IMM:
                    target = ins.operands[0].imm
                    text = f"+{target - start:#x}" if start <= target < start + size else f"{target:#x}"
            listing.append(f"{ins.mnemonic} {text}")
        return listing

    def same_machine_code(a: int, b: int) -> bool:
        """ICF folded identical functions to one address in the original; a slot that names a different function
        with different bytes is a real difference (a missing override), unless the original's function only
        forwards to ours."""
        if forwards_to(b, a):
            return True
        n = sizes.get(a, 0)
        if n == 0 or n != sizes.get(b, -1) or exe is None:
            return False
        return normalized_listing(a, n) == normalized_listing(b, n)

    all_vtable_symbols = sorted(int(r["address"], 16) for r in csv.DictReader(open(ROOT / "symbols" / "symbols.csv"))
                                if r["kind"] == "data" and "vftable" in r["name"])

    def original_slot_count(original_va: int) -> int:
        following = [a for a in all_vtable_symbols if a > original_va]
        return ((following[0] if following else original_va + 0x400) - original_va) // 4
    genuine: list[str] = []
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
        label = class_name(name)
        multi = re.match(r"(.*) \(base (.*)\)$", label)
        original_va = originals.get(f"{multi[1]}::vftable{{{multi[2]}}}") if multi else originals.get(label + "::vftable")
        if exe is not None and original_va is not None:
            exe_base = exe.OPTIONAL_HEADER.ImageBase
            original_count = original_slot_count(original_va)
            for i, ours_slot in enumerate(slots):
                if i >= original_count:
                    continue  # declared past the end of the original vtable: no original function to compare with
                theirs = struct.unpack("<I", exe.get_data(original_va + 4 * i - exe_base, 4))[0]
                if ours_slot != theirs and ours_slot != 0:
                    kind = "own copy" if ours_slot >= base else "different original function"
                    differing.append(f"{label} slot {i}: reconstruction {ours_slot:#x} ({kind}), original {theirs:#x}")
                    if ours_slot < base and not same_machine_code(ours_slot, theirs):
                        genuine.append(differing[-1])
                    if ours_slot >= base and TEXT[0] <= theirs < TEXT[1] and theirs != PURECALL and own_copy_differs(ours_slot, theirs):
                        mine = trivial_behaviour(dll_code, ours_slot, follow_jumps=False)
                        orig = trivial_behaviour(exe_code, theirs)
                        genuine.append(differing[-1] + f" [own stub body differs: ours {mine}, original {orig or 'not a trivial stub'}]")
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
    if exe is not None:
        kinds = Counter(d.split("(")[1].split(")")[0] for d in differing)
        print(f"  slots that differ from the original vtable: {len(differing)} {dict(kinds)} in "
              f"{len({d.split(' slot ')[0] for d in differing})} vtables (ICF-folded originals show up here; a missing "
              "override shows as 'different original function')")
        print(f"  of the 'different original function' slots, {len(genuine)} name a function with other machine code "
              f"(candidates for a missing override, in {len({d.split(' slot ')[0] for d in genuine})} vtables)")
        if args.list:
            print("\n".join(genuine))
    if args.list:
        for name, n, missing in rows:
            if missing:
                print(f"{class_name(name)}: {len(missing)}/{n} slots: " + " ".join(f"{v:#x}[{origin(v)}]" for v in missing))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
