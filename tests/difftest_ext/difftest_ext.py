#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["unicorn>=2.1", "pefile", "capstone>=5"]
# ///
"""tools/difftest.py with the extensions needed to verify gfx/timer code (the stock tester is left untouched).

Extensions over the stock tester:
  * The callee-popped argument slots belong to the callee: functions that adjust their stack parameters in place
    (MSVC does, clang keeps them in registers) are not reported as having different memory effects.
  * HeapAlloc/VirtualAlloc hand out memory from a deterministic bump heap, so allocating functions run instead of
    SKIPping; the bytes written to that heap are compared as well.
  * _lopen/_lread/_lclose read the real game files by base name; FindResourceA/LoadResource/LockResource return a
    synthetic bitmap resource (--lock-hex); QueryPerformanceCounter/Frequency and timeGetTime can be given values.
  * --gdi-handles: GDI calls return non-zero handles instead of 0, so the paths behind CreateDIBitmap, SelectObject...
    are exercised.
  * --poke: overwrite memory (after the snapshot) before running both sides, e.g. "ecx+0xc=10;[ecx]+0xe=1800;esp+8=fbffffff":
    each item is <address expr>=<hex bytes> or <address expr>=@<expr> (little-endian dword). An expression is a
    register, a number, or [expr] (dword load), joined with + and -.
  * --entry: run another function of the same signature on the capture's registers and stack.
"""
from __future__ import annotations

import argparse
import importlib.util
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[2] / "tools"
_spec = importlib.util.spec_from_file_location("difftest", TOOLS / "difftest.py")
d = importlib.util.module_from_spec(_spec)
sys.modules["difftest"] = d
_spec.loader.exec_module(d)

from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP  # noqa: E402

HEAP_BASE = 0x70000000
HEAP_SIZE = 0x4000000
GDI_HANDLE_BASE = 0x00A10000
RESOURCE_HANDLES = {"FindResourceA": 0x4D0001, "LoadResource": 0x4D0002}
REGISTERS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp")


@dataclass
class Config:
    pokes: str = ""
    gdi_handles: bool = False
    qpc: int | None = None
    qpf: int | None = None
    time_get_time: int | None = None
    lock_hex: str = ""
    entry: int | None = None


CFG = Config()


class ExtEmulator(d.Emulator):
    def setup(self) -> None:
        super().setup()
        self.frame_hi = self.cap.regs["esp"] + 4 + (self.infer_ret_imm(self.cap.function) or 0)
        self.heap_next = HEAP_BASE
        self.heap_mapped = False
        self.files: dict[int, list] = {}
        self.gdi_counter = 0
        self.apply_pokes()

    def run(self, entry: int):
        res = super().run(entry)
        if self.heap_mapped:
            used = bytes(self.uc.mem_read(HEAP_BASE, self.heap_next - HEAP_BASE))
            res.memory.update({HEAP_BASE + i: b for i, b in enumerate(used) if b})
        return res

    def call_stub(self, address: int) -> None:
        name, _, arg_bytes = self.imports.by_stub.get(address, ("", "unknown", -1))
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        count = arg_bytes // 4 if arg_bytes >= 0 else 0
        args = struct.unpack(f"<{count}I", bytes(self.uc.mem_read(esp + 4, 4 * count))) if count else ()
        result = self.fake_api(name, args)
        super().call_stub(address)
        if result is not None:
            self.uc.reg_write(UC_X86_REG_EAX, result & 0xFFFFFFFF)

    def fake_api(self, name: str, args: tuple) -> int | None:
        dll, _, function = name.partition("!")
        if function == "HeapAlloc":
            return self.heap_alloc(args[2])
        if function == "VirtualAlloc":
            return self.heap_alloc(args[1])
        if function == "HeapFree":
            return 1
        if function == "_lopen":
            return self.host_open(args[0])
        if function == "_lread":
            return self.host_read(*args)
        if function == "_lclose":
            return 0
        if function == "QueryPerformanceCounter" and CFG.qpc is not None:
            self.uc.mem_write(args[0], CFG.qpc.to_bytes(8, "little"))
            return 1
        if function == "QueryPerformanceFrequency" and CFG.qpf is not None:
            self.uc.mem_write(args[0], CFG.qpf.to_bytes(8, "little"))
            return 1
        if function == "timeGetTime" and CFG.time_get_time is not None:
            return CFG.time_get_time
        if CFG.lock_hex and function in RESOURCE_HANDLES:
            return RESOURCE_HANDLES[function]
        if CFG.lock_hex and function == "LockResource":
            blob = bytes.fromhex(CFG.lock_hex)
            block = self.heap_alloc(len(blob))
            self.uc.mem_write(block, blob)
            return block
        if CFG.gdi_handles and dll == "GDI32.DLL":
            return self.fake_gdi(function, args)
        return None

    def heap_alloc(self, size: int) -> int:
        if not self.heap_mapped:
            self.uc.mem_map(HEAP_BASE, HEAP_SIZE, d.UC_PROT_ALL)
            self.heap_mapped = True
        block = self.heap_next
        self.heap_next += (size + 15) & ~15
        return block

    def host_open(self, path_pointer: int) -> int:
        raw = bytearray()
        while (b := self.uc.mem_read(path_pointer + len(raw), 1)[0]) != 0:
            raw.append(b)
        base = raw.decode().replace("\\", "/").rsplit("/", 1)[-1].lower()
        for candidate in (d.MAIN / "orig_game_files" / "Star Wars - Rebellion").rglob("*"):
            if candidate.is_file() and candidate.name.lower() == base:
                handle = 100 + len(self.files)
                self.files[handle] = [candidate.read_bytes(), 0]
                return handle
        return 0xFFFFFFFF

    def host_read(self, handle: int, buffer: int, count: int) -> int:
        data, position = self.files[handle]
        chunk = data[position:position + count]
        self.files[handle][1] = position + len(chunk)
        self.uc.mem_write(buffer, chunk)
        return len(chunk)

    def fake_gdi(self, function: str, args: tuple) -> int:
        self.gdi_counter += 1
        handle = GDI_HANDLE_BASE + 4 * self.gdi_counter
        if function in ("CreateCompatibleDC", "CreateDIBitmap", "CreatePalette", "SelectObject", "SelectPalette"):
            return handle
        if function == "CreateDIBSection":
            info, bits_out = args[1], args[3]
            width, height, _, bit_count = struct.unpack("<iiHH", bytes(self.uc.mem_read(info + 4, 12)))
            size = (((bit_count * width + 31) >> 3) & ~3) * abs(height)
            self.uc.mem_write(bits_out, struct.pack("<I", self.heap_alloc(size)))
            return handle
        return 1

    def eval_expression(self, text: str) -> int:
        text = text.strip()

        def term(i: int) -> tuple[int, int]:
            if text[i] == "[":
                value, j = total(i + 1)
                assert text[j] == "]", text
                self.ensure_mapped(value)
                return struct.unpack("<I", bytes(self.uc.mem_read(value, 4)))[0], j + 1
            j = i
            while j < len(text) and (text[j].isalnum() or text[j] == "x"):
                j += 1
            token = text[i:j]
            return (self.cap.regs[token] if token in REGISTERS else int(token, 0)), j

        def total(i: int) -> tuple[int, int]:
            value, i = term(i)
            while i < len(text) and text[i] in "+-":
                sign = 1 if text[i] == "+" else -1
                part, i = term(i + 1)
                value += sign * part
            return value & 0xFFFFFFFF, i

        value, end = total(0)
        assert end == len(text), text
        return value

    def ensure_mapped(self, address: int) -> None:
        page = address & ~(d.PAGE - 1)
        if page not in self.original:
            self.map_page(page)

    def apply_pokes(self) -> None:
        for item in filter(None, CFG.pokes.split(";")):
            where, data = item.rsplit("=", 1)
            address = self.eval_expression(where)
            value = self.eval_expression(data[1:]).to_bytes(4, "little") if data.startswith("@") else bytes.fromhex(data)
            for i, byte in enumerate(value):
                self.ensure_mapped(address + i)
                page = (address + i) & ~(d.PAGE - 1)
                self.uc.mem_write(address + i, bytes([byte]))
                self.original[page] = bytearray(self.original[page])
                self.original[page][address + i - page] = byte


d.Emulator = ExtEmulator
_stock_load = d.Capture.load


def _load_with_entry(path):
    cap = _stock_load(path)
    if CFG.entry is not None:
        cap.function = CFG.entry
    return cap


d.Capture.load = staticmethod(_load_with_entry)


def run_captures(captures: list[Path], dll: Path = d.DLL_PATH, integration: bool = False, orig_only: bool = False,
                 max_insns: int = 20_000_000) -> list[bool]:
    args = argparse.Namespace(dll=dll, integration=integration, orig_only=orig_only, max_insns=max_insns)
    exe = d.Image.load(d.EXE_PATH, sections=(".text", ".rdata"))
    imports = d.Imports.load()
    return [d.run_capture(path, args, exe, imports) for path in captures]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("captures", nargs="+", type=Path)
    ap.add_argument("--dll", type=Path, default=d.DLL_PATH)
    ap.add_argument("--integration", action="store_true")
    ap.add_argument("--orig-only", action="store_true")
    ap.add_argument("--max-insns", type=int, default=20_000_000)
    ap.add_argument("--poke", default="")
    ap.add_argument("--entry", type=lambda s: int(s, 0))
    ap.add_argument("--gdi-handles", action="store_true")
    ap.add_argument("--qpc", type=lambda s: int(s, 0))
    ap.add_argument("--qpf", type=lambda s: int(s, 0))
    ap.add_argument("--time-get-time", type=lambda s: int(s, 0))
    ap.add_argument("--lock-hex", default="")
    args = ap.parse_args()
    CFG.pokes, CFG.gdi_handles, CFG.qpc, CFG.qpf = args.poke, args.gdi_handles, args.qpc, args.qpf
    CFG.time_get_time, CFG.lock_hex, CFG.entry = args.time_get_time, args.lock_hex, args.entry
    results = run_captures(args.captures, args.dll, args.integration, args.orig_only, args.max_insns)
    print(f"{sum(results)}/{len(results)} passed or skipped")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
