#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pefile", "capstone>=5"]
# ///
"""REB_OWN_DATA: emit the assembly of the DLL's data image from REBEXE.EXE (see docs/phase2_plan.md, "M5").

The image holds
  * the game-owned .data/.bss (tools/own_data.py) as one block at the original relative layout, with a label for every
    global that the reconstructed code references (from gen_link_symbols --own-data); pointers inside it are
    relocated references: code -> the reconstructed function symbol when hooked, else the original address; .rdata
    strings -> copies in the image; game data -> block labels; CRT data -> the original address;
  * copies of the .rdata globals the reconstructed code reads (float tables, rects, GUIDs);
  * `reb_code_refs`: the address of every absolute reference to the block that remains in the EXE (immediates and
    displacements of its machine code, pointers in its other data), which the runtime shifts by the block's
    displacement at load time, so original code that has not been reconstructed uses the DLL's copy.

Runs on the host (needs the original EXE; the build container does not see it).
"""
from __future__ import annotations

import argparse
import bisect
import csv
import json
import re
import struct
import sys
from collections import Counter
from pathlib import Path

import capstone
import pefile
from capstone.x86 import X86_OP_IMM, X86_OP_MEM

sys.path.insert(0, str(Path(__file__).resolve().parent))
import own_data  # noqa: E402

STRING_RUN = re.compile(rb"[\x20-\x7e\t\r\n]{3,}\0")
MAX_STRING = 1024
MAX_NAMED_RDATA = 0x100
SUSPICIOUS_IMM_MNEMONICS = {"add", "sub", "and", "or", "xor", "imul", "test", "adc", "sbb", "shl", "shr", "sar"}


class Exe:
    def __init__(self, path: Path):
        self.pe = pefile.PE(str(path))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.sections = {s.Name.rstrip(b"\0").decode(): s for s in self.pe.sections}

    def bytes_at(self, va: int, n: int) -> bytes:
        """Initialised bytes at `va`; the part beyond a section's raw data (bss) reads as zeros."""
        for s in self.pe.sections:
            lo = self.base + s.VirtualAddress
            hi = lo + max(s.Misc_VirtualSize, s.SizeOfRawData)
            if lo <= va < hi:
                raw = s.get_data()[: s.SizeOfRawData]
                start = va - lo
                chunk = raw[start:start + n]
                return chunk + bytes(min(n, hi - va) - len(chunk))
        raise ValueError(f"{va:#x} is outside the image")

    def section_range(self, name: str) -> tuple[int, int]:
        s = self.sections[name]
        lo = self.base + s.VirtualAddress
        return lo, lo + max(s.Misc_VirtualSize, s.SizeOfRawData)


def string_mask(data: bytes) -> bytearray:
    """1 for bytes inside printable NUL-terminated runs, which are text and never pointers."""
    mask = bytearray(len(data))
    for m in STRING_RUN.finditer(data):
        mask[m.start():m.end()] = b"\x01" * (m.end() - m.start())
    return mask


def quote(name: str) -> str:
    return '"' + name.replace('"', '\\"') + '"'


# ---- scanning original code for references to the block -------------------------------------------------------


class CodeScan:
    def __init__(self, exe: Exe, function_starts: list[int]):
        lo, hi = exe.section_range(".text")
        self.text_lo, self.text_hi = lo, min(hi, own_data.TEXT_RANGE[1] + 0x1000)
        code = exe.bytes_at(lo, hi - lo)
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        md.detail = True
        self.sites: list[tuple[int, int, str, str]] = []  # (patch address, value, operand kind, mnemonic)
        self.near_misses: list[tuple[int, int]] = []
        bounds = sorted({s for s in function_starts if lo <= s < hi} | {lo, hi})
        for start, end in zip(bounds, bounds[1:]):
            self._sweep(md, code, lo, start, end)

    def _sweep(self, md: "capstone.Cs", code: bytes, lo: int, start: int, end: int) -> None:
        offset = start - lo
        stop = end - lo
        while offset < stop:
            progressed = False
            for insn in md.disasm(code[offset:stop], lo + offset):
                progressed = True
                self._visit(insn, code, lo)
                offset = insn.address + insn.size - lo
            if not progressed:
                offset += 1
            elif offset >= stop:
                break

    def _visit(self, insn: "capstone.CsInsn", code: bytes, lo: int) -> None:
        for op in insn.operands:
            if op.type == X86_OP_MEM and insn.disp_offset:
                value, kind, field = op.mem.disp & 0xFFFFFFFF, "disp", insn.disp_offset
            elif op.type == X86_OP_IMM and insn.imm_offset and insn.imm_size == 4:
                value, kind, field = op.imm & 0xFFFFFFFF, "imm", insn.imm_offset
            else:
                continue
            at = insn.address + field
            if own_data.in_segment(value):
                if struct.unpack_from("<I", code, at - lo)[0] != value:
                    raise RuntimeError(f"operand decode mismatch at {insn.address:#x}")
                self.sites.append((at, value, kind, insn.mnemonic))
            elif own_data.BLOCK_BASE - 0x40 <= value < own_data.BLOCK_BASE or own_data.BLOCK_END <= value < own_data.BLOCK_END + 0x40:
                self.near_misses.append((at, value))


# ---- classification of the owners of those references ---------------------------------------------------------


class Owners:
    """Who executes a reference site: reconstructed (hooked) code is dead once hooked, tactical code stays original."""

    def __init__(self, symbols_dir: Path, hooked: set[int]):
        self.functions = sorted(int(r["address"], 16) for r in csv.DictReader(open(symbols_dir / "functions.csv")))
        self.hooked = hooked
        self.modules = sorted((int(r["start"], 16), int(r["end"], 16), r["subsystem"])
                              for r in csv.DictReader(open(symbols_dir / "modules.csv")))
        self.library = {int(r["address"], 16) for r in csv.DictReader(open(symbols_dir / "library_functions.csv"))}
        self.function_set = set(self.functions)

    def subsystem(self, address: int) -> str:
        return next((n for s, e, n in self.modules if s <= address < e), "")

    def classify(self, site: int) -> str:
        if site >= 0x629090:
            return "unwind funclet/stub"
        function = self.functions[bisect.bisect_right(self.functions, site) - 1]
        if function in self.hooked:
            return "reconstructed (dead once hooked)"
        if 0x6153B0 <= function < 0x628E10:
            return "library (CRT/LIBCI, not reconstructed)"
        return "tactical (original)" if self.subsystem(function) == "tactical" else "other non-tactical (original)"


# ---- the image ------------------------------------------------------------------------------------------------


class Image:
    def __init__(self, exe: Exe, symbols_dir: Path, refs: dict):
        self.exe = exe
        self.data_refs: dict[str, tuple[int, str]] = {raw: (int(e["address"], 16), e["name"])
                                                      for raw, e in refs["data"].items()}
        self.hook_symbols = {int(a, 16): raw for a, raw in refs["hooks"].items()}
        self.functions = {int(r["address"], 16) for r in csv.DictReader(open(symbols_dir / "functions.csv"))}
        self.symbol_starts = sorted({int(r["address"], 16) for r in csv.DictReader(open(symbols_dir / "symbols.csv"))
                                     if r["kind"] == "data"})
        self.symbol_sizes = {int(r["address"], 16): int(r["size"]) for r in csv.DictReader(open(symbols_dir / "symbols.csv"))
                             if r["kind"] == "data" and r["size"]}
        self.fixups = Counter()
        self.block = bytearray(own_data.BLOCK_END - own_data.BLOCK_BASE)
        for lo, hi in own_data.OWN_SEGMENTS:
            self.block[lo - own_data.BLOCK_BASE:hi - own_data.BLOCK_BASE] = exe.bytes_at(lo, hi - lo)
        self.rdata_intervals: list[list[int]] = []  # [start, end) copies of .rdata
        self.rdata_unknown_targets: list[int] = []

    # -- pointers inside the block
    def pointer_slots(self) -> list[tuple[int, int, str]]:
        """(block offset, value, kind) for every aligned dword of the block that is a pointer."""
        mask = string_mask(bytes(self.block))
        out = []
        for offset in range(0, len(self.block) - 3, 4):
            address = own_data.BLOCK_BASE + offset
            if not own_data.in_segment(address) or any(mask[offset:offset + 4]):
                continue
            value = struct.unpack_from("<I", self.block, offset)[0]
            kind = self.pointer_kind(value)
            if kind:
                out.append((offset, value, kind))
        return out

    def pointer_kind(self, value: int) -> str | None:
        if not own_data.TEXT_RANGE[0] <= value < own_data.IMAGE_END:
            return None
        if value < own_data.TEXT_RANGE[1]:
            return "code" if value in self.functions else None
        if value < own_data.RDATA_RANGE[1]:
            return "rdata"
        return "game data" if own_data.in_segment(value) else "CRT data"

    # -- .rdata copies
    def named_rdata(self) -> list[tuple[str, int]]:
        return sorted((raw, a) for raw, (a, name) in self.data_refs.items() if own_data.is_copied_rdata(a, name))

    def named_extent(self, address: int) -> int:
        if address in self.symbol_sizes:
            return self.symbol_sizes[address]
        later = [a for a in self.symbol_starts if a > address]
        next_symbol = later[0] if later else address + MAX_NAMED_RDATA
        return max(4, min(next_symbol - address, MAX_NAMED_RDATA))

    def string_extent(self, address: int) -> int | None:
        raw = self.exe.bytes_at(address, MAX_STRING)
        end = raw.find(b"\0")
        if end <= 0 or any(not (0x20 <= c < 0x7F or c in (9, 10, 13)) for c in raw[:end]):
            return None
        return end + 1

    def plan_rdata(self, rdata_pointer_values: set[int]) -> None:
        wanted: dict[int, int] = {}
        for _, address in self.named_rdata():
            wanted[address] = max(wanted.get(address, 0), self.named_extent(address))
        for value in sorted(rdata_pointer_values):
            extent = self.string_extent(value)
            if extent is None:
                self.rdata_unknown_targets.append(value)
            else:
                wanted[value] = max(wanted.get(value, 0), extent)
        merged: list[list[int]] = []
        for start in sorted(wanted):
            end = start + wanted[start]
            if merged and start <= merged[-1][1]:
                merged[-1][1] = max(merged[-1][1], end)
            else:
                merged.append([start, end])
        self.rdata_intervals = merged

    def rdata_label(self, address: int) -> str | None:
        i = bisect.bisect_right([s for s, _ in self.rdata_intervals], address) - 1
        if i >= 0 and address < self.rdata_intervals[i][1]:
            start = self.rdata_intervals[i][0]
            return f"reb_rdata_{start:08x}" + (f"+{address - start}" if address != start else "")
        return None

    # -- expression for a pointer value
    def expression(self, value: int, kind: str) -> str:
        if kind == "code":
            symbol = self.hook_symbols.get(value)
            self.fixups["code -> reconstructed function symbol" if symbol else "code -> original address"] += 1
            return quote(symbol) if symbol else f"{value:#x}"
        if kind == "rdata":
            label = self.rdata_label(value)
            self.fixups["rdata -> copied into image" if label else "rdata -> original address (extent unknown)"] += 1
            return label or f"{value:#x}"
        if kind == "game data":
            self.fixups["game data -> block label"] += 1
            return f"_reb_data_block+{value - own_data.BLOCK_BASE:#x}"
        self.fixups["CRT data -> original address"] += 1
        return f"{value:#x}"

    # -- asm emission
    def emit_bytes(self, lines: list[str], data: bytes) -> None:
        for i in range(0, len(data), 24):
            lines.append("  .byte " + ",".join(str(b) for b in data[i:i + 24]))

    def emit_span(self, lines: list[str], data: bytes) -> None:
        """Raw bytes with long zero runs folded into .zero."""
        for part in re.split(rb"(\x00{16,})", data):
            if not part:
                continue
            if part[0] == 0 and len(part) >= 16:
                lines.append(f"  .zero {len(part)}")
            else:
                self.emit_bytes(lines, part)

    def block_asm(self, slots: list[tuple[int, int, str]]) -> list[str]:
        labels: dict[int, list[str]] = {}
        for raw, (address, name) in self.data_refs.items():
            if own_data.in_segment(address):
                labels.setdefault(address - own_data.BLOCK_BASE, []).append(raw)
        events = sorted({*labels, *(o for o, _, _ in slots)})
        slot_at = {o: (v, k) for o, v, k in slots}
        lines = ['.section .data,"dw"', ".p2align 4", ".globl _reb_data_block", "_reb_data_block:"]
        cursor = 0
        size = len(self.block)
        for offset in events:
            if offset < cursor:  # a label inside a pointer slot's 4 bytes: emitted after the slot
                continue
            self.emit_span(lines, bytes(self.block[cursor:offset]))
            cursor = offset
            for raw in sorted(labels.get(offset, ())):
                lines += [f".globl {quote(raw)}", f"{quote(raw)}:"]
            if offset in slot_at:
                value, kind = slot_at[offset]
                lines.append(f"  .long {self.expression(value, kind)}")
                cursor += 4
        self.emit_span(lines, bytes(self.block[cursor:size]))
        interior = [o for o in labels if any(s < o < s + 4 for s, _, _ in slots)]
        if interior:
            raise RuntimeError(f"labels inside pointer slots: {[hex(own_data.BLOCK_BASE + o) for o in interior]}")
        return lines

    def rdata_asm(self) -> list[str]:
        if not self.rdata_intervals:
            return []
        names: dict[int, list[str]] = {}
        for raw, address in self.named_rdata():
            names.setdefault(address, []).append(raw)
        lines = ['.section .rdata,"dr"', ".p2align 2"]
        for start, end in self.rdata_intervals:
            lines.append(f"reb_rdata_{start:08x}:")
            cuts = sorted({a for a in names if start <= a < end} | {start, end})
            for a, b in zip(cuts, cuts[1:]):
                for raw in sorted(names.get(a, ())):
                    lines += [f".globl {quote(raw)}", f"{quote(raw)}:"]
                self.emit_bytes(lines, self.exe.bytes_at(a, b - a))
            lines.append(".p2align 2")
        return lines


def non_block_pointer_sites(exe: Exe) -> list[tuple[int, str]]:
    """Dwords outside the block (all of .rdata, the CRT-owned data) whose value points into the block."""
    sites = []
    ranges = [(own_data.RDATA_RANGE[0], own_data.RDATA_RANGE[1], "rdata"),
              (0x6A6000, own_data.OWN_SEGMENTS[0][0], "init tables"),
              (own_data.OWN_SEGMENTS[0][1], own_data.OWN_SEGMENTS[1][0], "CRT .data")]
    for lo, hi, label in ranges:
        data = exe.bytes_at(lo, hi - lo)
        mask = string_mask(data)
        for offset in range(0, len(data) - 3, 4):
            value = struct.unpack_from("<I", data, offset)[0]
            if own_data.in_segment(value) and not any(mask[offset:offset + 4]):
                sites.append((lo + offset, label))
    return sites


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", type=Path, required=True)
    ap.add_argument("--symbols", type=Path, required=True, help="the symbols/ directory")
    ap.add_argument("--refs", type=Path, required=True, help="data_refs.json from gen_link_symbols.py --own-data")
    ap.add_argument("--out-asm", type=Path, required=True)
    ap.add_argument("--out-report", type=Path, required=True)
    args = ap.parse_args()

    exe = Exe(args.exe)
    refs = json.loads(args.refs.read_text())
    image = Image(exe, args.symbols, refs)
    hooked = set(image.hook_symbols)

    slots = image.pointer_slots()
    image.plan_rdata({v for _, v, k in slots if k == "rdata"})

    scan = CodeScan(exe, sorted(image.functions))
    owners = Owners(args.symbols, hooked)
    by_owner = Counter(owners.classify(site) for site, *_ in scan.sites)
    by_operand = Counter(kind for _, _, kind, _ in scan.sites)
    suspicious = [(a, v, m) for a, v, k, m in scan.sites if k == "imm" and m in SUSPICIOUS_IMM_MNEMONICS]
    data_sites = non_block_pointer_sites(exe)
    patch_sites = sorted({a for a, *_ in scan.sites} | {a for a, _ in data_sites})

    lines = ['.globl "@feat.00"', '"@feat.00" = 1', ".text", *image.block_asm(slots), *image.rdata_asm(),
             '.section .rdata,"dr"', ".p2align 2", ".globl _reb_code_refs", "_reb_code_refs:"]
    lines += [f"  .long {a:#x}" for a in patch_sites] + ["  .long 0"]
    lines += [".globl _reb_data_block_original", f"_reb_data_block_original = {own_data.BLOCK_BASE:#x}"]
    args.out_asm.parent.mkdir(parents=True, exist_ok=True)
    args.out_asm.write_text("\n".join(lines) + "\n")

    report = {
        "block": {"original": f"{own_data.BLOCK_BASE:#x}-{own_data.BLOCK_END:#x}", "bytes": len(image.block),
                  "owned_bytes": sum(hi - lo for lo, hi in own_data.OWN_SEGMENTS),
                  "labels": sum(1 for a, _ in image.data_refs.values() if own_data.in_segment(a)),
                  "distinct_label_addresses": len({a for a, _ in image.data_refs.values() if own_data.in_segment(a)})},
        "rdata_copies": {"named_labels": len(image.named_rdata()), "blocks": len(image.rdata_intervals),
                         "bytes": sum(e - s for s, e in image.rdata_intervals),
                         "pointer_targets_left_absolute": [f"{v:#x}" for v in image.rdata_unknown_targets]},
        "pointer_fixups_in_block": dict(image.fixups) | {"total slots": len(slots)},
        "code_reference_sites": {"total": len(scan.sites), "by operand": dict(by_operand), "by owner": dict(by_owner),
                                 "suspicious immediates (non-mov/push/cmp)": [f"{a:#x}: {m} {v:#x}" for a, v, m in suspicious],
                                 "near misses just outside the block": [f"{a:#x}: {v:#x}" for a, v in scan.near_misses]},
        "data_pointer_sites_outside_block": dict(Counter(label for _, label in data_sites)),
        "runtime_patch_sites": len(patch_sites),
    }
    args.out_report.write_text(json.dumps(report, indent=1) + "\n")
    print(f"gen_data_image: block {len(image.block):#x} bytes, {report['block']['labels']} labels, "
          f"{len(slots)} pointer fixups {dict(image.fixups)}, {len(image.rdata_intervals)} .rdata copies "
          f"({report['rdata_copies']['bytes']} bytes), {len(patch_sites)} runtime patch sites "
          f"({len(scan.sites)} in code, {len(data_sites)} in data)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
