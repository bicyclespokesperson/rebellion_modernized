#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["unicorn>=2.1", "pefile", "capstone>=5"]
# ///
"""Differential tester: replay a captured call of an original REBEXE.EXE function in an emulator, once with
the original machine code and once with the reimplementation from the hybrid DLL, and compare the effects.

A capture (see src/hybrid/capture.cpp) holds the CPU state at function entry plus a snapshot of all process
memory. Both runs start from that identical state. Compared:
  * return registers (EAX; EDX and ST0 reported when they differ), ESP after return (calling convention)
    and callee-saved registers (EBX/ESI/EDI/EBP)
  * every byte of memory written by the call, outside the callee's own stack frame
  * the sequence of Win32 API calls made. Imports are stubbed: they pop their arguments and return 0, except
    heap allocation (a deterministic bump heap whose contents are compared too), reads of game files
    (_lopen/_lread/CreateFileA/ReadFile... served from the original install) and the opt-in overrides below

Usage:
  difftest.py work/game/captures/00611330_*.cap            # unit mode: only the captured function replaced
  difftest.py --integration work/game/captures/*.cap       # every reimplemented function replaced
  difftest.py --orig-only CAP                              # just emulate the original and print its effects
"""
from __future__ import annotations

import argparse
import csv
import re
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

import capstone
import pefile
from unicorn import (UC_ARCH_X86, UC_HOOK_BLOCK, UC_HOOK_CODE, UC_HOOK_MEM_UNMAPPED, UC_HOOK_MEM_WRITE,
                     UC_MEM_FETCH_UNMAPPED, UC_MODE_32, UC_PROT_ALL, Uc, UcError)
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ECX,
                               UC_X86_REG_EDI, UC_X86_REG_EDX, UC_X86_REG_EFLAGS, UC_X86_REG_EIP,
                               UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_FPCW, UC_X86_REG_FPSW,
                               UC_X86_REG_FP0, UC_X86_REG_FS, UC_X86_REG_GDTR,
                               UC_X86_REG_SS, UC_X86_REG_DS, UC_X86_REG_ES)

sys.path.insert(0, str(Path(__file__).resolve().parent))
from paths import MAIN, ROOT  # noqa: E402

PAGE = 0x1000
EXE_PATH = MAIN / "orig_game_files" / "Star Wars - Rebellion" / "REBEXE.EXE"
DLL_PATH = ROOT / "build" / "hybrid" / "smackw32.dll"
IMPORTS_CSV = ROOT / "symbols" / "imports.csv"
STUB_BASE = 0xF0000000
SENTINEL = 0xF0FF0000
GDT_BASE = 0xF1000000
HEADER = struct.Struct("<4I I 10I 3I 80s I")
PAGEREF = struct.Struct("<IIQ")
CALLEE_SAVED = {"ebx": UC_X86_REG_EBX, "esi": UC_X86_REG_ESI, "edi": UC_X86_REG_EDI, "ebp": UC_X86_REG_EBP}


@dataclass
class Capture:
    path: Path
    function: int
    teb: int
    regs: dict[str, int]
    fpu_control: int
    pages: dict[int, tuple[int, int]]  # page address -> (protect, pack offset)

    @staticmethod
    def load(path: Path) -> "Capture":
        data = path.read_bytes()
        f = HEADER.unpack_from(data, 0)
        magic, version, function, _thread, teb = f[0:5]
        if magic != 0x50414352 or version != 1:
            raise ValueError(f"{path}: not a v1 capture")
        names = ["eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp", "eip", "eflags"]
        regs = dict(zip(names, f[5:15]))
        count = f[19]
        pages = {}
        for i in range(count):
            a, prot, off = PAGEREF.unpack_from(data, HEADER.size + i * PAGEREF.size)
            pages[a] = (prot, off)
        return Capture(path, function, teb, regs, f[15], pages)


@dataclass
class Image:
    """A PE image as it should appear in memory: page address -> bytes."""
    pages: dict[int, bytearray]
    pe: pefile.PE

    @staticmethod
    def load(path: Path, sections: tuple[str, ...] | None = None) -> "Image":
        """Map a PE as the loader would; `sections` limits which sections' pages are taken from the file."""
        pe = pefile.PE(str(path))
        mem = pe.get_memory_mapped_image()
        base = pe.OPTIONAL_HEADER.ImageBase
        size = (pe.OPTIONAL_HEADER.SizeOfImage + PAGE - 1) & ~(PAGE - 1)
        mem = mem.ljust(size, b"\0")
        if sections is None:
            ranges = [(0, size)]
        else:
            ranges = [(sec.VirtualAddress, sec.VirtualAddress + sec.Misc_VirtualSize) for sec in pe.sections
                      if sec.Name.rstrip(b"\0").decode() in sections]
        pages = {}
        for lo, hi in ranges:
            for o in range(lo & ~(PAGE - 1), hi, PAGE):
                pages[base + o] = bytearray(mem[o:o + PAGE])
        return Image(pages, pe)

    def read(self, address: int, n: int) -> bytes:
        return bytes(self.pages[(address + i) & ~(PAGE - 1)][(address + i) & (PAGE - 1)] for i in range(n))

    def read_cstr(self, address: int) -> str:
        out = bytearray()
        while (b := self.read(address + len(out), 1)[0]) != 0:
            out.append(b)
        return out.decode()

    def write(self, address: int, data: bytes) -> None:
        for i, b in enumerate(data):
            self.pages[(address + i) & ~(PAGE - 1)][(address + i) & (PAGE - 1)] = b


@dataclass
class Imports:
    by_name: dict[tuple[str, str], tuple[int, str, int]] = field(default_factory=dict)  # -> (stub, conv, bytes)
    by_stub: dict[int, tuple[str, str, int]] = field(default_factory=dict)
    exe_iat: dict[int, int] = field(default_factory=dict)  # IAT slot -> stub

    @staticmethod
    def load() -> "Imports":
        imp = Imports()
        for i, row in enumerate(csv.DictReader(open(IMPORTS_CSV))):
            stub = STUB_BASE + 16 * i
            key = (row["dll"].upper(), row["name"])
            imp.by_name[key] = (stub, row["convention"], int(row["arg_bytes"] or 0))
            imp.by_stub[stub] = (f'{row["dll"]}!{row["name"]}', row["convention"], int(row["arg_bytes"] or 0))
            imp.exe_iat[int(row["iat"], 16)] = stub
        return imp

    def stub_for(self, dll: str, name: str) -> int:
        key = (dll.upper(), name)
        if key not in self.by_name:
            stub = STUB_BASE + 16 * (len(self.by_stub) + 1)
            self.by_name[key] = (stub, "unknown", -1)
            self.by_stub[stub] = (f"{dll}!{name}", "unknown", -1)
        return self.by_name[key][0]


@dataclass
class RunResult:
    ok: bool
    error: str = ""
    regs: dict[str, int] = field(default_factory=dict)
    st0: bytes = b""
    fpu_top: int = 0
    memory: dict[int, int] = field(default_factory=dict)  # changed bytes outside the callee frame
    calls: list[tuple[str, tuple]] = field(default_factory=list)
    instructions: int = 0


RETURN_VOID, RETURN_X87, RETURN_EDX_EAX = 0, 0x10, 8


@dataclass(frozen=True)
class Hook:
    original: int
    replacement: int
    name: str
    return_kind: int


def hook_table(dll: Image) -> list[Hook]:
    exports = {e.name.decode(): e.address for e in dll.pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
    base = dll.pe.OPTIONAL_HEADER.ImageBase
    table = base + exports["reb_hook_table"]
    out = []
    while True:
        orig, repl, name_ptr, ret = struct.unpack("<4I", dll.read(table, 16))
        if orig == 0:
            return out
        out.append(Hook(orig, repl, dll.read_cstr(name_ptr), ret))
        table += 16


def bind_dll_imports(dll: Image, imports: Imports) -> None:
    for entry in getattr(dll.pe, "DIRECTORY_ENTRY_IMPORT", []):
        for imp in entry.imports:
            name = imp.name.decode() if imp.name else f"Ordinal_{imp.ordinal}"
            dll.write(imp.address, struct.pack("<I", imports.stub_for(entry.dll.decode(), name)))


def jump(src: int, dst: int) -> bytes:
    return b"\xE9" + struct.pack("<i", dst - (src + 5))


HEAP_BASE = 0x70000000
HEAP_SIZE = 0x4000000
FILE_HANDLE_BASE = 0x100
GDI_HANDLE_BASE = 0x00A10000
RESOURCE_HANDLES = {"FindResourceA": 0x4D0001, "LoadResource": 0x4D0002}
REGISTERS = ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp")
GAME_DIR = MAIN / "orig_game_files" / "Star Wars - Rebellion"


@dataclass
class Config:
    """Optional API behaviour and state overrides (see --help); the defaults suit ordinary captures."""
    pokes: str = ""
    gdi_handles: bool = False
    qpc: int | None = None
    qpf: int | None = None
    time_get_time: int | None = None
    lock_hex: str = ""
    entry: int | None = None


CFG = Config()

EIGHT_BYTE_PARAMS = ("double", "__int64")


def stack_arg_bytes(signature_key: str) -> int:
    """Bytes of stack arguments from a hook name like 'Foo::Bar(int,double)' ('this' travels in ECX)."""
    m = re.search(r"\((.*)\)$", signature_key)
    if not m or m.group(1) in ("", "void"):
        return 0
    depth, params, cur = 0, [], ""
    for c in m.group(1):
        depth += c in "<(" ; depth -= c in ">)"
        if c == "," and depth == 0:
            params.append(cur); cur = ""
        else:
            cur += c
    params.append(cur)
    # Varargs ("...") have no static size; only the named parameters are counted.
    return sum(8 if p.endswith(EIGHT_BYTE_PARAMS) else 4 for p in params if p != "...")


class Emulator:
    def __init__(self, cap: Capture, pack: Path, exe: Image, dll: Image, imports: Imports, max_insns: int,
                 arg_bytes: int = 0):
        self.cap, self.exe, self.dll, self.imports, self.max_insns = cap, exe, dll, imports, max_insns
        self.pack = open(pack, "rb")
        self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        self.writes: set[int] = set()
        self.original: dict[int, bytes] = {}
        self.calls: list[tuple[str, tuple]] = []
        self.error = ""
        self.done = False
        self.count = 0
        esp = cap.regs["esp"]
        self.stack_lo = (esp - 0x100000) & ~(PAGE - 1)
        # The return-address slot, everything below it, and the incoming argument slots (MSVC reuses them as
        # scratch) belong to the callee.
        self.frame_hi = esp + 4 + arg_bytes
        self.heap_next = HEAP_BASE
        self.heap_mapped = False
        self.files: dict[int, list] = {}
        self.gdi_counter = 0

    def page_bytes(self, page: int) -> bytes | None:
        if page in self.exe.pages:
            return bytes(self.exe.pages[page])
        if page in self.dll.pages:
            return bytes(self.dll.pages[page])
        if page in self.cap.pages:
            self.pack.seek(self.cap.pages[page][1])
            return self.pack.read(PAGE)
        return None

    def map_page(self, page: int) -> bool:
        data = self.page_bytes(page)
        if data is None:
            return False
        self.uc.mem_map(page, PAGE, UC_PROT_ALL)
        self.uc.mem_write(page, data)
        self.original[page] = data
        return True

    def executable(self, address: int) -> bool:
        page = address & ~(PAGE - 1)
        return page in self.exe.pages or page in self.dll.pages

    def on_unmapped(self, uc, access, address, size, value, _):
        page = address & ~(PAGE - 1)
        if access == UC_MEM_FETCH_UNMAPPED and not self.executable(address):
            return self.foreign_call(address)
        ok = self.map_page(page)
        if ok and (address + size - 1) & ~(PAGE - 1) != page:
            ok = self.map_page((address + size - 1) & ~(PAGE - 1))
        if not ok:
            self.error = f"access to unmapped memory {address:#010x} (size {size})"
        return ok

    def on_write(self, uc, access, address, size, value, _):
        for i in range(size):
            self.writes.add(address + i)

    def foreign_call(self, target: int) -> bool:
        """A call into code outside the game/DLL (a COM method in a system DLL): stub it like an import.
        Its stdcall argument size is inferred from the first `ret imm16` reachable in the snapshot's code."""
        arg_bytes = self.infer_ret_imm(target)
        if arg_bytes is None:
            self.error = f"call into foreign code at {target:#010x} (argument size unknown)"
            return False
        stub = self.imports.stub_for("<com>", f"{target:#010x}")
        self.imports.by_stub[stub] = (f"<com>@{target:#010x}", "stdcall", arg_bytes)
        self.uc.mem_map(target & ~(PAGE - 1), PAGE, UC_PROT_ALL)
        self.uc.mem_write(target, jump(target, stub))
        return True

    def infer_ret_imm(self, address: int, depth: int = 0) -> int | None:
        if depth > 4:
            return None
        page = address & ~(PAGE - 1)
        code = b"".join(self.page_bytes(p) or b"" for p in (page, page + PAGE))
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
        for insn in md.disasm(code[address - page:], address):
            if insn.mnemonic == "ret":
                return int(insn.op_str, 0) if insn.op_str else 0
            if insn.mnemonic == "jmp":
                try:
                    return self.infer_ret_imm(int(insn.op_str, 0), depth + 1)
                except ValueError:
                    return None
            if insn.address - address > 0x2000:
                return None
        return None

    def on_stub(self, uc, address, size, _):
        if address == SENTINEL:
            self.done = True
            uc.emu_stop()
        else:
            self.call_stub(address)

    def on_block(self, uc, address, size, _):
        self.count += 1

    def call_stub(self, address: int) -> None:
        name, conv, arg_bytes = self.imports.by_stub.get(address, (f"stub@{address:#x}", "unknown", -1))
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        ret = struct.unpack("<I", self.uc.mem_read(esp, 4))[0]
        n_args = arg_bytes // 4 if arg_bytes >= 0 else 0
        args = struct.unpack(f"<{n_args}I", self.uc.mem_read(esp + 4, 4 * n_args)) if n_args else ()
        self.calls.append((name, tuple(self.normalize(a) for a in args)))
        if conv == "unknown":
            self.error = f"call to {name} with unknown calling convention"
            self.uc.emu_stop()
            return
        result = self.fake_api(name, struct.unpack(f"<{n_args}I", self.uc.mem_read(esp + 4, 4 * n_args)) if n_args else ())
        self.uc.reg_write(UC_X86_REG_EAX, (result or 0) & 0xFFFFFFFF)
        self.uc.reg_write(UC_X86_REG_ESP, esp + 4 + (arg_bytes if conv == "stdcall" else 0))
        self.uc.reg_write(UC_X86_REG_EIP, ret)

    # ---- Win32 API emulation: enough for allocation, game-file reads and (optionally) GDI/timer paths.
    def fake_api(self, name: str, args: tuple) -> int | None:
        dll, _, function = name.partition("!")
        if function == "HeapAlloc":
            return self.heap_alloc(args[2])
        if function == "HeapReAlloc":
            return self.heap_realloc(args[2], args[3])
        if function == "VirtualAlloc":
            return self.heap_alloc(args[1])
        if function in ("HeapFree", "VirtualFree"):
            return 1
        if function == "_lopen":
            return self.host_open(args[0])
        if function == "CreateFileA":
            return self.host_open(args[0])
        if function == "_lread":
            return self.host_read(args[0], args[1], args[2])
        if function == "ReadFile":
            n = self.host_read(args[0], args[1], args[2])
            if args[3]:
                self.uc.mem_write(args[3], struct.pack("<I", max(n, 0)))
            return 1 if n >= 0 else 0
        if function in ("_llseek", "SetFilePointer"):
            return self.host_seek(args[0], args[1], args[3] if function == "SetFilePointer" else args[2])
        if function == "GetFileSize":
            return len(self.files[args[0]][0]) if args[0] in self.files else 0xFFFFFFFF
        if function in ("_lclose", "CloseHandle") and args and args[0] in self.files:
            del self.files[args[0]]
            return 1 if function == "CloseHandle" else 0
        if function in ("GetLocalTime", "GetSystemTime"):  # fixed instant: time() must not read garbage
            self.uc.mem_write(args[0], struct.pack("<8H", 1998, 3, 1, 2, 12, 0, 0, 0))
            return 0
        if function == "GetTimeZoneInformation":
            self.uc.mem_write(args[0], bytes(172))
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
        if CFG.gdi_handles and dll.upper() == "GDI32.DLL":
            return self.fake_gdi(function, args)
        return None

    def heap_alloc(self, size: int) -> int:
        """Deterministic bump allocator: both runs get identical blocks for identical request sequences."""
        if not self.heap_mapped:
            self.uc.mem_map(HEAP_BASE, HEAP_SIZE, UC_PROT_ALL)
            self.heap_mapped = True
        block = self.heap_next
        self.heap_next += (size + 15) & ~15
        if self.heap_next > HEAP_BASE + HEAP_SIZE:
            self.error = "emulated heap exhausted"
            self.uc.emu_stop()
            return 0
        return block

    def heap_realloc(self, block: int, size: int) -> int:
        new = self.heap_alloc(size)
        if block and HEAP_BASE <= block < self.heap_next:
            self.uc.mem_write(new, bytes(self.uc.mem_read(block, min(size, self.heap_next - block))))
        return new

    def host_open(self, path_pointer: int) -> int:
        """Open a game file by base name (read-only, from the original install)."""
        raw = bytearray()
        while (b := self.uc.mem_read(path_pointer + len(raw), 1)[0]) != 0:
            raw.append(b)
        base = raw.decode(errors="replace").replace("\\", "/").rsplit("/", 1)[-1].lower()
        for candidate in GAME_DIR.rglob("*"):
            if candidate.is_file() and candidate.name.lower() == base:
                handle = FILE_HANDLE_BASE + 4 * (len(self.files) + 1)
                self.files[handle] = [candidate.read_bytes(), 0]
                return handle
        return 0xFFFFFFFF

    def host_read(self, handle: int, buffer: int, count: int) -> int:
        if handle not in self.files:
            return -1
        data, position = self.files[handle]
        chunk = data[position:position + count]
        self.files[handle][1] = position + len(chunk)
        self.uc.mem_write(buffer, chunk)
        return len(chunk)

    def host_seek(self, handle: int, distance: int, method: int) -> int:
        if handle not in self.files:
            return 0xFFFFFFFF
        data, position = self.files[handle]
        distance = distance - (1 << 32) if distance & 0x80000000 else distance
        base = {0: 0, 1: position, 2: len(data)}.get(method, 0)
        self.files[handle][1] = max(0, base + distance)
        return self.files[handle][1]

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

    # ---- --poke: overwrite state after the snapshot, before both runs
    def eval_expression(self, text: str) -> int:
        text = text.strip()

        def term(i: int) -> tuple[int, int]:
            if text[i] == "[":
                value, j = total(i + 1)
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
        if end != len(text):
            raise ValueError(f"bad --poke expression: {text}")
        return value

    def ensure_mapped(self, address: int) -> None:
        page = address & ~(PAGE - 1)
        if page not in self.original:
            self.map_page(page)

    def apply_pokes(self) -> None:
        for item in filter(None, CFG.pokes.split(";")):
            where, data = item.rsplit("=", 1)
            address = self.eval_expression(where)
            value = self.eval_expression(data[1:]).to_bytes(4, "little") if data.startswith("@") else bytes.fromhex(data)
            for i, byte in enumerate(value):
                self.ensure_mapped(address + i)
                page = (address + i) & ~(PAGE - 1)
                self.uc.mem_write(address + i, bytes([byte]))
                self.original[page] = bytearray(self.original[page])
                self.original[page][address + i - page] = byte

    def normalize(self, value: int):
        return "<stack>" if self.stack_lo <= value < self.frame_hi + 0x10000 else value

    def setup(self) -> None:
        uc, cap = self.uc, self.cap
        uc.mem_map(STUB_BASE, 0x01000000, UC_PROT_ALL)
        uc.mem_write(STUB_BASE, b"\xC3" * 0x1000)
        for slot, stub in self.imports.exe_iat.items():
            self.exe.write(slot, struct.pack("<I", stub))
        self.setup_fs(cap.teb)
        names = {"eax": UC_X86_REG_EAX, "ebx": UC_X86_REG_EBX, "ecx": UC_X86_REG_ECX, "edx": UC_X86_REG_EDX,
                 "esi": UC_X86_REG_ESI, "edi": UC_X86_REG_EDI, "ebp": UC_X86_REG_EBP, "esp": UC_X86_REG_ESP,
                 "eflags": UC_X86_REG_EFLAGS}
        for k, r in names.items():
            uc.reg_write(r, cap.regs[k])
        uc.reg_write(UC_X86_REG_FPCW, cap.fpu_control)
        uc.hook_add(UC_HOOK_MEM_UNMAPPED, self.on_unmapped)
        uc.hook_add(UC_HOOK_MEM_WRITE, self.on_write)
        uc.hook_add(UC_HOOK_CODE, self.on_stub, begin=STUB_BASE, end=SENTINEL)
        uc.hook_add(UC_HOOK_BLOCK, self.on_block)
        esp = cap.regs["esp"]
        if not self.map_page(esp & ~(PAGE - 1)):
            raise RuntimeError("stack page missing from capture")
        uc.mem_write(esp, struct.pack("<I", SENTINEL))
        self.apply_pokes()
        self.writes.clear()

    def setup_fs(self, teb: int) -> None:
        # Flat 4 GiB code/data segments plus an FS segment based at the captured TEB.
        def desc(base, limit, access, flags):
            return struct.pack("<HHBBBB", limit & 0xFFFF, base & 0xFFFF, (base >> 16) & 0xFF, access,
                               ((limit >> 16) & 0xF) | (flags << 4), (base >> 24) & 0xFF)
        gdt = b"\0" * 8 + desc(0, 0xFFFFF, 0x9A, 0xC) + desc(0, 0xFFFFF, 0x92, 0xC) + desc(teb, 0xFFF, 0x92, 0x4)
        self.uc.mem_map(GDT_BASE, PAGE, UC_PROT_ALL)
        self.uc.mem_write(GDT_BASE, gdt)
        self.uc.reg_write(UC_X86_REG_GDTR, (0, GDT_BASE, len(gdt) - 1, 0x0))
        for seg in (UC_X86_REG_SS, UC_X86_REG_DS, UC_X86_REG_ES):
            self.uc.reg_write(seg, 2 << 3)
        self.uc.reg_write(UC_X86_REG_FS, 3 << 3)

    def run(self, entry: int) -> RunResult:
        self.setup()
        try:
            self.uc.emu_start(entry, SENTINEL + 1, count=self.max_insns)
        except UcError as e:
            if not self.error:
                eip = self.uc.reg_read(UC_X86_REG_EIP)
                self.error = f"emulation fault at {eip:#010x}: {e}"
        if not self.done and not self.error:
            self.error = f"stopped without returning (instruction limit {self.max_insns}?)"
        res = RunResult(ok=self.done and not self.error, error=self.error, calls=self.calls, instructions=self.count)
        regs = {"eax": UC_X86_REG_EAX, "edx": UC_X86_REG_EDX, "esp": UC_X86_REG_ESP, **CALLEE_SAVED}
        res.regs = {k: self.uc.reg_read(r) for k, r in regs.items()}
        fpsw = self.uc.reg_read(UC_X86_REG_FPSW)
        res.fpu_top = (fpsw >> 11) & 7
        st0 = self.uc.reg_read(UC_X86_REG_FP0)
        res.st0 = struct.pack("<QH", *st0) if isinstance(st0, tuple) else b""
        for a in sorted(self.writes):
            if self.stack_lo <= a < self.frame_hi:
                continue
            page = a & ~(PAGE - 1)
            if page not in self.original:  # emulator-only pages (stubs, GDT) or a write that faulted
                continue
            new = self.uc.mem_read(a, 1)[0]
            if new != self.original[page][a - page]:
                res.memory[a] = new
        if self.heap_mapped:
            used = bytes(self.uc.mem_read(HEAP_BASE, self.heap_next - HEAP_BASE))
            res.memory.update({HEAP_BASE + i: b for i, b in enumerate(used) if b})
        return res


def describe_memory(mem: dict[int, int]) -> list[str]:
    """Coalesce changed bytes into runs: 'addr: hex'."""
    out, run_start, run = [], None, bytearray()
    for a in sorted(mem):
        if run_start is not None and a == run_start + len(run):
            run.append(mem[a])
            continue
        if run_start is not None:
            out.append(f"{run_start:#010x}: {run.hex()}")
        run_start, run = a, bytearray([mem[a]])
    if run_start is not None:
        out.append(f"{run_start:#010x}: {run.hex()}")
    return out


def compare(orig: RunResult, new: RunResult, initial: dict[str, int], return_kind: int) -> list[str]:
    problems = []
    if not new.ok:
        problems.append(f"reimplementation did not complete: {new.error}")
        return problems
    if orig.regs["esp"] != new.regs["esp"]:
        problems.append(f"esp after return: original {orig.regs['esp']:#x}, reimplementation {new.regs['esp']:#x}")
    if return_kind in (1, 2, 4, RETURN_EDX_EAX):
        mask = (1 << (8 * min(return_kind, 4))) - 1
        if orig.regs["eax"] & mask != new.regs["eax"] & mask:
            problems.append(f"return value: original {orig.regs['eax'] & mask:#x}, reimplementation {new.regs['eax'] & mask:#x}")
        if return_kind == RETURN_EDX_EAX and orig.regs["edx"] != new.regs["edx"]:
            problems.append(f"return value high dword (edx): original {orig.regs['edx']:#x}, reimplementation {new.regs['edx']:#x}")
    for reg in CALLEE_SAVED:
        if new.regs[reg] != initial[reg]:
            problems.append(f"callee-saved {reg} clobbered: {initial[reg]:#x} -> {new.regs[reg]:#x}")
    if orig.fpu_top != new.fpu_top:
        problems.append(f"x87 stack depth differs (TOP {orig.fpu_top} vs {new.fpu_top})")
    elif return_kind == RETURN_X87 and orig.st0 != new.st0:
        problems.append(f"ST0 differs: {orig.st0.hex()} vs {new.st0.hex()}")
    if orig.memory != new.memory:
        only_o = {a: v for a, v in orig.memory.items() if new.memory.get(a) != v}
        only_n = {a: v for a, v in new.memory.items() if orig.memory.get(a) != v}
        problems.append("memory effects differ:\n      original:       " +
                        "\n                      ".join(describe_memory(only_o)[:20]) +
                        "\n      reimplemented:  " + "\n                      ".join(describe_memory(only_n)[:20]))
    if orig.calls != new.calls:
        problems.append(f"API calls differ:\n      original:      {orig.calls[:12]}\n      reimplemented: {new.calls[:12]}")
    return problems


def run_capture(path: Path, args, exe_pristine: Image, imports: Imports) -> bool:
    cap = Capture.load(path)
    if CFG.entry is not None:  # replay another function with the same signature on this capture's state
        cap.function = CFG.entry
    pack = next(d / "pages.pack" for d in path.resolve().parents if (d / "pages.pack").exists())

    def fresh(image: Image) -> Image:
        return Image({k: bytearray(v) for k, v in image.pages.items()}, image.pe)

    dll = Image.load(args.dll)
    bind_dll_imports(dll, imports)  # binds a copy per capture; cheap relative to emulation
    hooks = hook_table(dll)
    target = next((h for h in hooks if h.original == cap.function), None)
    arg_bytes = stack_arg_bytes(target.name) if target else 0
    label = f"{path.name} [{cap.function:#010x}]"
    try:
        orig = Emulator(cap, pack, fresh(exe_pristine), dll, imports, args.max_insns, arg_bytes).run(cap.function)
    except RuntimeError as e:  # malformed capture (e.g. the stack page was not snapshotted)
        print(f"SKIP {label}: unusable capture: {e}")
        return True
    if not orig.ok:
        print(f"SKIP {label}: original run failed: {orig.error}")
        return True
    if args.orig_only:
        print(f"{label}: eax={orig.regs['eax']:#x} esp+={orig.regs['esp'] - cap.regs['esp']} blocks={orig.instructions}")
        for line in describe_memory(orig.memory):
            print("   ", line)
        for c in orig.calls:
            print("    call", c)
        return True
    exe = fresh(exe_pristine)
    active = [h for h in hooks if args.integration or h.original == cap.function]
    if target is None:
        print(f"SKIP {label}: no reimplementation in {args.dll.name}")
        return True
    for h in active:
        exe.write(h.original, jump(h.original, h.replacement))
    new = Emulator(cap, pack, exe, dll, imports, args.max_insns, arg_bytes).run(cap.function)
    problems = compare(orig, new, cap.regs, target.return_kind)
    name = target.name
    if problems:
        print(f"FAIL {label} {name}")
        for p in problems:
            print("    -", p)
        return False
    print(f"PASS {label} {name} ({orig.instructions}/{new.instructions} blocks, "
          f"{len(orig.memory)} bytes written, {len(orig.calls)} API calls)")
    return True


def run_captures(captures: list[Path], dll: Path = DLL_PATH, integration: bool = False, orig_only: bool = False,
                 max_insns: int = 20_000_000) -> list[bool]:
    args = argparse.Namespace(dll=dll, integration=integration, orig_only=orig_only, max_insns=max_insns)
    exe = Image.load(EXE_PATH, sections=(".text", ".rdata"))
    imports = Imports.load()
    return [run_capture(path, args, exe, imports) for path in captures]


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("captures", nargs="+", type=Path)
    ap.add_argument("--dll", type=Path, default=DLL_PATH)
    ap.add_argument("--integration", action="store_true", help="replace every reimplemented function, not just the captured one")
    ap.add_argument("--orig-only", action="store_true", help="only emulate the original and print its effects")
    ap.add_argument("--max-insns", type=int, default=20_000_000)
    g = ap.add_argument_group("state/API overrides (for exercising other paths from the same capture)")
    g.add_argument("--poke", default="", help='overwrite memory before both runs: "ecx+0xc=0a000000;[ecx]+0xe=@esp+8" '
                   "(<addr expr>=<hex bytes> or =@<expr> for a dword; expr: registers, numbers, [deref], + -)")
    g.add_argument("--entry", type=lambda v: int(v, 0), help="run this function instead (same signature)")
    g.add_argument("--gdi-handles", action="store_true", help="GDI32 calls return non-zero handles")
    g.add_argument("--qpc", type=lambda v: int(v, 0), help="QueryPerformanceCounter value")
    g.add_argument("--qpf", type=lambda v: int(v, 0), help="QueryPerformanceFrequency value")
    g.add_argument("--time-get-time", type=lambda v: int(v, 0), help="timeGetTime value")
    g.add_argument("--lock-hex", default="", help="FindResource/LoadResource/LockResource yield these bytes")
    args = ap.parse_args()
    CFG.pokes, CFG.gdi_handles, CFG.qpc, CFG.qpf = args.poke, args.gdi_handles, args.qpc, args.qpf
    CFG.time_get_time, CFG.lock_hex, CFG.entry = args.time_get_time, args.lock_hex, args.entry
    results = run_captures(args.captures, args.dll, args.integration, args.orig_only, args.max_insns)
    print(f"{sum(results)}/{len(results)} passed or skipped")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
