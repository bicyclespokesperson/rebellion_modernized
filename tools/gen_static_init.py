#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["unicorn>=2.1", "pefile", "capstone>=5"]
# ///
"""Generate static_init_table.cpp: the original's C++ static initializers, in the original order, for the standalone build.

REBEXE.EXE's start-up (`__cinit`, 0x61d5d0) walks two tables with `_initterm`: the C initializers (`__xi_a..__xi_z`,
CRT-owned) and the C++ initializers (`__xc_a..__xc_z`, 1,781 entries from 0x6a6000: one thunk per translation unit's
static constructors). The standalone program has no `_cinit` of its own, so this tool writes the equivalent sequence:

  * an initializer that is reconstructed (it has a hook in the built DLL) becomes a call, by the reconstructed symbol;
  * any other initializer (almost all of them tactical ID initializers that do constant arithmetic on .rdata and store
    into the game's globals) is run in Unicorn against the pristine EXE image, and the bytes it changes are baked into
    the table as data (`kBakedStores`), applied to the data block in order. An initializer that calls out (heap, atexit,
    unreconstructed game code) cannot be baked; it is reported as unhandled and emitted as a call to the original
    address's declared name `FUN_xxxxxxxx` (a link error until it is reconstructed).

The generated sequence is then validated in the same emulator: the original sequence (all original code) against the
generated one (hooks for the reconstructed entries, baked stores for the rest) from a fresh EXE image; the resulting
.data/.bss, heap and any other written byte must be equal.

  gen_static_init.py [--dll build/hybrid/smackw32.dll] [--out build/hybrid/gen/static_init_table.cpp] [--no-validate]
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import re
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

import capstone
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP

sys.path.insert(0, str(Path(__file__).resolve().parent))
import difftest  # noqa: E402
import own_data  # noqa: E402
from difftest import (CALLEE_SAVED, EXE_PATH, HEAP_BASE, HEAP_SIZE, PAGE, SENTINEL, Capture, Emulator, Image,  # noqa: E402
                      Imports, bind_dll_imports, hook_table, jump)
from paths import ROOT  # noqa: E402

CINIT = 0x61D5D0
STACK_TOP = 0x00200000
TEB_BASE = 0x00300000
MAX_INSNS = 5_000_000
CRT_ALLOCATORS = {0x618B70: "operator new", 0x619170: "malloc"}
CRT_FREERS = {0x618B60: "operator delete", 0x618E60: "free"}
CRT_ATEXIT = 0x618C20


@dataclass(frozen=True)
class InitTables:
    """Where `__cinit` finds its initializers: an optional floating-point hook, then two _initterm ranges."""
    fp_hook_slot: int
    c_range: tuple[int, int]
    cpp_range: tuple[int, int]


def read_tables(exe: Image) -> InitTables:
    """Decode `__cinit` instead of trusting constants: [mov eax,[slot]; test; jz; call eax] then two
    `push end; push begin; call _initterm` pairs (C initializers first)."""
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    md.detail = False
    slot = 0
    pushes: list[int] = []
    ranges: list[tuple[int, int]] = []
    for ins in md.disasm(exe.read(CINIT, 0x40), CINIT):
        if ins.mnemonic == "mov" and ins.op_str.startswith("eax, dword ptr [0x") and not slot:
            slot = int(ins.op_str.split("[")[1].rstrip("]"), 16)
        elif ins.mnemonic == "push" and ins.op_str.startswith("0x"):
            pushes.append(int(ins.op_str, 16))
        elif ins.mnemonic == "call" and len(pushes) >= 2 and not ins.op_str.startswith("eax"):
            ranges.append((pushes[-1], pushes[-2]))
            pushes.clear()
        elif ins.mnemonic == "ret":
            break
    if len(ranges) != 2 or not slot:
        raise SystemExit(f"__cinit at {CINIT:#x} does not have the expected shape: slot={slot:#x} ranges={ranges}")
    return InitTables(slot, ranges[0], ranges[1])


def read_dwords(exe: Image, lo: int, hi: int) -> list[int]:
    return [struct.unpack("<I", exe.read(a, 4))[0] for a in range(lo, hi, 4)]


# ---- names of the reconstructed initializers -------------------------------------------------------------------

MAP_LINE = re.compile(r"^\s*[0-9a-f]{4}:[0-9a-f]{8}\s+(\S+)\s+([0-9a-f]{8,16})\s+\S+\.obj\s*$")


def mangled_names(map_path: Path) -> dict[int, str]:
    """Replacement address (VA) -> linker symbol, from the DLL's link map."""
    out: dict[int, str] = {}
    for line in map_path.read_text(errors="replace").splitlines():
        m = MAP_LINE.match(line)
        if m:
            out.setdefault(int(m.group(2), 16), m.group(1))
    return out


def demangled_names(path_json: Path) -> dict[int, str]:
    report = json.loads(path_json.read_text())
    return {int(k, 16): v for k, v in report["reimplemented"].items()}


# ---- emulation of the initializer sequence ---------------------------------------------------------------------

def function_sizes() -> dict[int, int]:
    import csv
    with open(ROOT / "symbols" / "functions.csv", newline="") as fh:
        return {int(r["address"], 16): int(r["size"]) for r in csv.DictReader(fh)}


@dataclass
class EntryResult:
    address: int
    ok: bool = True
    error: str = ""
    calls: list[int] = field(default_factory=list)       # call targets outside the entry's own function
    api_calls: list[str] = field(default_factory=list)   # import stubs reached
    atexit: list[int] = field(default_factory=list)
    allocations: int = 0
    stores: list[tuple[int, bytes]] = field(default_factory=list)  # net byte changes outside stack/heap, as runs
    instructions: int = 0


class InitEmulator(Emulator):
    """Runs initializers one after another in one persistent emulated process (fresh EXE image, empty stack/TEB)."""

    def __init__(self, exe: Image, dll: Image, imports: Imports, sizes: dict[int, int]):
        cap = Capture(Path("/dev/null"), 0, TEB_BASE, {k: 0 for k in
                      ("eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "eip", "eflags")} | {"esp": STACK_TOP - 0x10},
                      0x27F, {})
        super().__init__(cap, Path(os.devnull), exe, dll, imports, MAX_INSNS)
        self.sizes = sizes
        self.current = EntryResult(0)
        self.entry_end = 0
        self.first_old: dict[int, int] = {}
        self.stubs: dict[int, str] = {**CRT_ALLOCATORS, **CRT_FREERS, CRT_ATEXIT: "atexit"}
        self.ready = False

    def page_bytes(self, page: int):
        data = super().page_bytes(page)
        if data is not None:
            return data
        if STACK_TOP - 0x100000 <= page < STACK_TOP + PAGE or TEB_BASE <= page < TEB_BASE + PAGE:
            return bytes(PAGE)
        return None

    # -- hooks
    def prepare(self) -> None:
        self.setup()
        self.uc.hook_add(UC_HOOK_CODE, self.on_instruction)
        self.uc.hook_add(UC_HOOK_MEM_WRITE, self.on_first_write)
        self.ready = True

    def on_first_write(self, uc, access, address, size, value, _):
        for i in range(size):
            a = address + i
            if a not in self.first_old:
                try:
                    self.first_old[a] = uc.mem_read(a, 1)[0]
                except Exception:
                    self.first_old[a] = 0

    def on_instruction(self, uc, address, size, _):
        if address in self.stubs:
            self.run_crt_stub(address)
            return
        if address >= difftest.STUB_BASE:
            return
        self.ensure_mapped(address + 5)
        op = bytes(uc.mem_read(address, 6))
        target = None
        if op[0] == 0xE8:
            target = (address + 5 + struct.unpack("<i", op[1:5])[0]) & 0xFFFFFFFF
        elif op[0] == 0xFF and op[1] == 0x15:
            slot = struct.unpack("<I", op[2:6])[0]
            self.ensure_mapped(slot)
            self.current.api_calls.append(self.imports.by_stub.get(struct.unpack("<I", uc.mem_read(slot, 4))[0], ("?",))[0])
            return
        if target is not None and not (self.current.address <= target < self.entry_end) and target not in self.stubs:
            self.current.calls.append(target)

    def run_crt_stub(self, address: int) -> None:
        esp = self.uc.reg_read(UC_X86_REG_ESP)
        ret, arg = struct.unpack("<II", self.uc.mem_read(esp, 8))
        result = 0
        if address in CRT_ALLOCATORS:
            result = self.heap_alloc(arg)
            self.current.allocations += 1
        elif address == CRT_ATEXIT:
            self.current.atexit.append(arg)
        self.uc.reg_write(UC_X86_REG_EAX, result)
        self.uc.reg_write(UC_X86_REG_ESP, esp + 4)
        self.uc.reg_write(UC_X86_REG_EIP, ret)

    # -- one initializer
    def run_entry(self, address: int) -> EntryResult:
        self.current = EntryResult(address)
        self.entry_end = address + self.sizes.get(address, 0x10000)
        self.first_old.clear()
        self.done, self.error, self.count = False, "", 0
        esp = STACK_TOP - 0x10
        self.uc.reg_write(UC_X86_REG_ESP, esp)
        self.uc.mem_write(esp, struct.pack("<I", SENTINEL))
        try:
            self.uc.emu_start(address, SENTINEL + 1, count=self.max_insns)
        except Exception as e:  # unicorn raises UcError
            self.error = self.error or f"emulation fault: {e}"
        if not self.done and not self.error:
            self.error = "did not return"
        res = self.current
        res.ok, res.error, res.instructions = self.done and not self.error, self.error, self.count
        res.stores = self.net_stores()
        return res

    def net_stores(self) -> list[tuple[int, bytes]]:
        changed = sorted(a for a, old in self.first_old.items()
                         if self.in_image(a) and self.uc.mem_read(a, 1)[0] != old)
        runs: list[tuple[int, bytearray]] = []
        for a in changed:
            byte = self.uc.mem_read(a, 1)[0]
            if runs and a == runs[-1][0] + len(runs[-1][1]):
                runs[-1][1].append(byte)
            else:
                runs.append((a, bytearray([byte])))
        return [(a, bytes(b)) for a, b in runs]

    @staticmethod
    def in_image(address: int) -> bool:
        return 0x400000 <= address < 0x6C0000

    # -- results
    def snapshot(self) -> dict[int, bytes]:
        """Every mapped page of the image's data region and the emulated heap, by page address."""
        pages = {}
        for page in range(0x6A6000, 0x6C0000, PAGE):
            pages[page] = bytes(self.uc.mem_read(page, PAGE)) if page in self.original else bytes(self.exe.pages[page])
        if self.heap_mapped:
            used = (self.heap_next - HEAP_BASE + PAGE - 1) & ~(PAGE - 1)
            pages[HEAP_BASE] = bytes(self.uc.mem_read(HEAP_BASE, used))
        return pages


def pristine_images(dll_path: Path, hooks_enabled: bool, wanted: set[int] | None = None):
    exe = Image.load(EXE_PATH)
    dll = Image.load(dll_path)
    imports = Imports.load()
    bind_dll_imports(dll, imports)
    hooks = hook_table(dll)
    if hooks_enabled:
        for h in hooks:
            if wanted is None or h.original in wanted:
                exe.write(h.original, jump(h.original, h.replacement))
    return exe, dll, imports, hooks


def run_original_sequence(dll_path: Path, entries: list[int], sizes: dict[int, int]) -> tuple[list[EntryResult], dict[int, bytes]]:
    exe, dll, imports, _ = pristine_images(dll_path, hooks_enabled=False)
    emu = InitEmulator(exe, dll, imports, sizes)
    emu.prepare()
    results = [emu.run_entry(e) for e in entries]
    return results, emu.snapshot()


def run_generated_sequence(dll_path: Path, plan: list["Step"], sizes: dict[int, int]) -> tuple[list[EntryResult], dict[int, bytes]]:
    exe, dll, imports, _ = pristine_images(dll_path, hooks_enabled=True)
    emu = InitEmulator(exe, dll, imports, sizes)
    emu.prepare()
    results = []
    for step in plan:
        if step.kind == "baked":
            for address, data in step.stores:
                emu.write_guest(address, data)
            results.append(EntryResult(step.address))
        else:
            results.append(emu.run_entry(step.address))
    return results, emu.snapshot()


# ---- classification and emission ---------------------------------------------------------------------------------

@dataclass
class Step:
    index: int
    address: int
    kind: str                       # "call" (reconstructed), "baked", "unhandled"
    name: str = ""
    stores: list[tuple[int, bytes]] = field(default_factory=list)
    why: str = ""


def classify(entries: list[int], reconstructed: dict[int, str], results: list[EntryResult]) -> list[Step]:
    plan = []
    for i, (address, res) in enumerate(zip(entries, results)):
        if address in reconstructed:
            plan.append(Step(i, address, "call", reconstructed[address]))
        elif not res.ok:
            plan.append(Step(i, address, "unhandled", why=res.error))
        elif res.api_calls or res.allocations:
            parts = [f"imports {', '.join(sorted(set(res.api_calls)))}" if res.api_calls else "",
                     "allocates" if res.allocations else ""]
            plan.append(Step(i, address, "unhandled", why="; ".join(p for p in parts if p)))
        else:
            outside = [a for a, _ in res.stores if not own_data.in_segment(a)]
            if outside:
                plan.append(Step(i, address, "unhandled", why=f"stores outside the data block ({outside[0]:#x})"))
            else:
                note = f"atexit({', '.join(f'{c:#x}' for c in res.atexit)}) not registered" if res.atexit else ""
                plan.append(Step(i, address, "baked", stores=res.stores, why=note))
    return plan


def library_names() -> dict[int, str]:
    import csv
    with open(ROOT / "symbols" / "library_functions.csv", newline="") as fh:
        return {int(r["address"], 16): r["name"] for r in csv.DictReader(fh)}


def emit(plan: list[Step], tables: InitTables, symbols: dict[int, str], demangled: dict[int, str], out: Path,
         crt_initializers: list[int]) -> dict:
    """Write static_init_table.cpp. Baked stores are grouped into maximal runs of consecutive baked entries; within a
    run later stores simply follow earlier ones (applying them in order gives the same bytes)."""
    pool = bytearray()
    store_rows: list[tuple[int, int, int]] = []   # (block offset, pool offset, length)
    lines: list[str] = []
    declared: dict[int, str] = {}
    body: list[str] = []
    run_first_row = None
    run_entries = 0
    run_lo = run_hi = 0

    def flush_run() -> None:
        nonlocal run_first_row, run_entries
        if run_first_row is None:
            return
        body.append(f"    ApplyBakedStores({run_first_row}, {len(store_rows)});"
                    f"  // {run_entries} initializers, {run_lo:#x}..{run_hi:#x}")
        run_first_row, run_entries = None, 0

    for step in plan:
        if step.kind == "baked":
            if run_first_row is None:
                run_first_row, run_lo = len(store_rows), step.address
            run_entries += 1
            run_hi = step.address
            if step.why:
                body.append(f"    // {step.address:#x}: {step.why}: its destructor only runs at process exit")
            for address, data in step.stores:
                store_rows.append((address - own_data.BLOCK_BASE, len(pool), len(data)))
                pool += data
        else:
            flush_run()
            if step.kind == "call":
                label = symbols[step.address]
                declared[step.address] = label
                body.append(f"    init_{step.address:08x}();  // {demangled.get(step.address, step.name)}")
            else:
                body.append(f"    FUN_{step.address:08x}();  // UNHANDLED: {step.why}")
    flush_run()

    lines += [
        "// Generated by tools/gen_static_init.py from REBEXE.EXE's initializer tables. Do not edit.",
        f"// C++ initializers {tables.cpp_range[0]:#x}..{tables.cpp_range[1]:#x}, "
        f"{len(plan)} entries in table order; see the generator for how each is handled.",
        "// Not generated (CRT start-up; the standalone program uses the host CRT's): "
        + ", ".join(f"{library_names().get(a, '?')} {a:#x}" for a in crt_initializers) + ".",
        '#include <cstddef>', '#include <cstdint>', '',
        'extern "C" uint8_t reb_data_block[];', '',
        'namespace {', '', 'struct BakedStore {', '    uint32_t blockOffset;', '    uint32_t poolOffset;',
        '    uint32_t length;', '};', '',
    ]
    lines.append("const uint8_t kBakedBytes[] = {")
    for i in range(0, len(pool), 24):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in pool[i:i + 24]) + ",")
    lines.append("};\n")
    lines.append("const BakedStore kBakedStores[] = {")
    for off, pool_off, length in store_rows:
        lines.append(f"    {{{off:#x}, {pool_off:#x}, {length}}},")
    lines.append("};\n")
    lines += [
        "void ApplyBakedStores(size_t first, size_t end) {",
        "    for (size_t i = first; i < end; ++i) {",
        "        const BakedStore& s = kBakedStores[i];",
        "        for (uint32_t b = 0; b < s.length; ++b) reb_data_block[s.blockOffset + b] = kBakedBytes[s.poolOffset + b];",
        "    }", "}", "", "}  // namespace", "",
    ]
    for address, label in sorted(declared.items()):
        lines.append(f'void __cdecl init_{address:08x}() __asm__("\\x01{label}");')
    unhandled = [s for s in plan if s.kind == "unhandled"]
    for s in unhandled:
        lines.append(f'void __cdecl FUN_{s.address:08x}();')
    lines += ["", "void RunStaticInitializers() {"]
    lines += body
    lines += ["}", ""]
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(lines))
    return {"baked_stores": len(store_rows), "baked_bytes": len(pool)}


def validate(dll_path: Path, entries: list[int], plan: list[Step], sizes: dict[int, int],
             reference: dict[int, bytes], original_results: list[EntryResult]) -> list[str]:
    results, generated = run_generated_sequence(dll_path, plan, sizes)
    hooked = sum(r.instructions for r, s in zip(results, plan) if s.kind == "call")
    original = sum(r.instructions for r in original_results if r.address in {s.address for s in plan if s.kind == "call"})
    print(f"validation: reconstructed initializers ran {hooked} blocks (original code: {original})")
    problems = [f"{r.address:#x}: {r.error}" for r, s in zip(results, plan) if s.kind == "call" and not r.ok]
    for page in sorted(set(reference) | set(generated)):
        a, b = reference.get(page), generated.get(page)
        if a == b:
            continue
        if a is None or b is None:
            problems.append(f"page {page:#x} touched only by the {'original' if b is None else 'generated'} sequence")
            continue
        diffs = [i for i in range(PAGE) if a[i] != b[i]]
        problems.append(f"page {page:#x}: {len(diffs)} bytes differ, first at {page + diffs[0]:#x} "
                        f"(original {a[diffs[0]]:#04x}, generated {b[diffs[0]]:#04x})")
    return problems


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--dll", type=Path, default=ROOT / "build" / "hybrid" / "smackw32.dll")
    ap.add_argument("--map", type=Path, default=ROOT / "build" / "hybrid" / "smackw32.map")
    ap.add_argument("--report", type=Path, default=ROOT / "build" / "hybrid" / "gen" / "report.json")
    ap.add_argument("--out", type=Path, default=ROOT / "build" / "hybrid" / "gen" / "static_init_table.cpp")
    ap.add_argument("--no-validate", action="store_true")
    ap.add_argument("--list-unhandled", action="store_true")
    ap.add_argument("--negative-control", action="store_true",
                    help="flip one baked byte and require the validation to notice")
    args = ap.parse_args()

    exe = Image.load(EXE_PATH)
    tables = read_tables(exe)
    cpp_entries = [e for e in read_dwords(exe, *tables.cpp_range) if e]
    c_entries = [e for e in read_dwords(exe, *tables.c_range) if e]
    fp_hook = struct.unpack("<I", exe.read(tables.fp_hook_slot, 4))[0]
    print(f"C++ initializers {tables.cpp_range[0]:#x}..{tables.cpp_range[1]:#x}: {len(cpp_entries)} entries "
          f"({(tables.cpp_range[1] - tables.cpp_range[0]) // 4} slots)")
    print(f"C initializers {tables.c_range[0]:#x}..{tables.c_range[1]:#x}: {[hex(e) for e in c_entries]} "
          f"(CRT-owned); FP hook slot {tables.fp_hook_slot:#x} -> {fp_hook:#x}")

    demangled = demangled_names(args.report)
    dll = Image.load(args.dll)
    names = mangled_names(args.map)
    hooks = {h.original: names[h.replacement] for h in hook_table(dll) if h.replacement in names}
    reconstructed = {e: hooks[e] for e in cpp_entries if e in hooks}
    print(f"{len(reconstructed)} of {len(cpp_entries)} C++ initializers are reconstructed")

    sizes = function_sizes()
    results, reference = run_original_sequence(args.dll, cpp_entries, sizes)
    plan = classify(cpp_entries, reconstructed, results)
    counts = collections.Counter(s.kind for s in plan)
    stats = emit(plan, tables, hooks, demangled, args.out, [fp_hook, *c_entries])
    print(f"wrote {args.out}: {counts['call']} calls, {counts['baked']} baked values "
          f"({stats['baked_stores']} stores, {stats['baked_bytes']} bytes; "
          f"{sum(1 for s in plan if s.kind == 'baked' and s.why)} of them skip an atexit registration), "
          f"{counts['unhandled']} unhandled")
    reasons = collections.Counter(s.why for s in plan if s.kind == "unhandled")
    for why, n in reasons.most_common(12):
        print(f"  unhandled x{n}: {why}")
    if args.list_unhandled:
        for s in plan:
            if s.kind == "unhandled":
                print(f"  {s.address:#x}: {s.why}")
    failed_original = [r for r, s in zip(results, plan) if s.kind == "call" and not r.ok]
    for r in failed_original:
        print(f"note: reconstructed initializer {r.address:#x} did not run to completion in the emulated original "
              f"sequence: {r.error}")
    if args.no_validate:
        return 0
    if args.negative_control:
        last = [s for s in plan if s.kind == "baked" and s.stores][-1]
        address, data = last.stores[0]
        print(f"negative control: flipping a bit of {address:#x}")
        last.stores[0] = (address, bytes([data[0] ^ 1]) + data[1:])
    problems = validate(args.dll, cpp_entries, plan, sizes, reference, results)
    if args.negative_control:
        print(f"negative control: {'detected' if problems else 'NOT DETECTED'} ({problems[:1]})")
        return 0 if problems else 1
    if problems:
        print(f"VALIDATION FAILED ({len(problems)} problems)")
        for p in problems[:30]:
            print("   ", p)
        return 1
    print("validation: the generated sequence leaves the same .data/.bss and heap as the original sequence")
    return 0


if __name__ == "__main__":
    sys.exit(main())
