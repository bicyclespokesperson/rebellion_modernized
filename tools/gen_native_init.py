#!/usr/bin/env -S uv run
"""Native build: the original's C++ static initializers as native_init.cpp (docs/native_linux.md).

REBEXE.EXE's start-up walks a table of 1,781 initializer thunks. tools/gen_static_init.py classifies them (emulating the original
code): an initializer that has a reconstruction becomes a call; the others (tactical ID initializers: constant arithmetic on
.rdata stored into the game's globals) are baked as byte stores. This tool reuses that analysis and writes the equivalent
sequence for the native objects:

  * a reconstructed initializer is called by its Itanium symbol (found in the objects by the function's name in symbols.csv);
  * the baked stores of the original (address, bytes) are mapped through native_data.json to (global, offset): the global is
    wherever tools/gen_native_data.py put it. Stores into globals that no object references are dropped.

Runs in the rebellion-native container, after gen_native_data.py. Output: native_init.cpp defining `void RebRunStaticInitializers()`.
"""
# /// script
# dependencies = ["pefile", "capstone>=5", "unicorn>=2.1"]
# ///
from __future__ import annotations

import argparse
import bisect
import csv
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_static_init as gsi  # noqa: E402
import own_data  # noqa: E402
from native_symbols import linker_symbol, llvm_tool, source_symbol


def defined_functions(objects: list[Path]) -> dict[str, str]:
    """demangled name (with parameter list) -> mangled symbol, of the functions the objects define."""
    names: list[str] = []
    for i in range(0, len(objects), 400):
        out = subprocess.run([llvm_tool("llvm-nm"), "-P", "--defined-only", *map(str, objects[i:i + 400])], check=True, capture_output=True,
                             text=True).stdout
        for line in out.splitlines():
            parts = line.split()
            if len(parts) >= 2 and not line.endswith(":") and parts[1] in ("T", "t") and source_symbol(parts[0]).startswith("_Z"):
                names.append(source_symbol(parts[0]))
    names = sorted(set(names))
    demangled = subprocess.run([llvm_tool("llvm-cxxfilt"), "--no-strip-underscore"], input="\n".join(names) + "\n", capture_output=True, text=True, check=True).stdout
    return dict(zip(demangled.splitlines(), names))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", type=Path, required=True)
    ap.add_argument("--exe", type=Path, required=True)
    ap.add_argument("--symbols", type=Path, required=True)
    ap.add_argument("--out", type=Path, required=True)
    args = ap.parse_args()

    gsi.EXE_PATH = args.exe
    exe = gsi.Image.load(args.exe)
    tables = gsi.read_tables(exe)
    cpp_entries = [e for e in gsi.read_dwords(exe, *tables.cpp_range) if e]

    objects = sorted(o for o in args.build.glob("CMakeFiles/*.dir/**/*.o") if "reb_native_gen" not in o.parts[-6:-1][0:0] and "reb_native_gen.dir" not in str(o))
    functions = defined_functions(objects)
    names = {int(r["address"], 16): r["name"] for r in csv.DictReader(open(args.symbols / "symbols.csv")) if r["kind"] == "func"}
    reconstructed: dict[int, str] = {}
    for address in cpp_entries:
        name = names.get(address)
        if name and f"{name}()" in functions:
            reconstructed[address] = functions[f"{name}()"]
    print(f"gen_native_init: {len(reconstructed)} of {len(cpp_entries)} initializers are reconstructed")

    sizes = gsi.function_sizes()
    results, _ = gsi.run_original_sequence(None, cpp_entries, sizes)
    plan = gsi.classify(cpp_entries, reconstructed, results)
    unhandled = [s for s in plan if s.kind == "unhandled"]

    layout = json.loads((args.out / "native_data.json").read_text())["globals"]
    by_address = sorted((g["address"], g["extent32"], name) for name, g in layout.items())
    starts = [a for a, _, _ in by_address]

    def locate(address: int) -> tuple[str, int] | None:
        i = bisect.bisect_right(starts, address) - 1
        if i >= 0:
            start, extent, name = by_address[i]
            if address < start + extent:
                return name, address - start
        return None

    pool = bytearray()
    rows: list[tuple[str, int, int, int]] = []
    dropped = 0
    body: list[str] = []
    run_first = None
    run_count = 0

    def flush() -> None:
        nonlocal run_first, run_count
        if run_first is not None:
            body.append(f"    ApplyBakedStores({run_first}, {len(rows)});  // {run_count} initializers")
            run_first, run_count = None, 0

    kinds = {name: g["kind"] for name, g in layout.items()}
    problems: list[str] = []
    for step in plan:
        if step.kind == "baked":
            if run_first is None:
                run_first = len(rows)
            run_count += 1
            for address, data in step.stores:
                cursor = 0
                while cursor < len(data):
                    where = locate(address + cursor)
                    if where is None:
                        dropped += 1
                        cursor += 1
                        continue
                    name, offset = where
                    g = layout[name]
                    room = min(len(data) - cursor, g["extent32"] - offset)
                    if g.get("pointer_like"):
                        # the 32-bit offsets inside a pointer or an object that holds pointers mean nothing in the LP64 layout
                        dropped += room
                        cursor += room
                        continue
                    if offset >= g["size"]:
                        dropped += room
                        cursor += room
                        continue
                    room = min(room, g["size"] - offset)
                    if kinds[name] == "string pointers":
                        problems.append(f"store at {address + cursor:#x} into {name}+{offset}: {kinds[name]}, size {g['size']}")
                        cursor += room
                        continue
                    rows.append((name, offset, len(pool), room))
                    pool += data[cursor:cursor + room]
                    cursor += room
        elif step.kind == "call":
            flush()
            body.append(f"    init_{step.address:08x}();  // {names.get(step.address, '')}")
        else:
            flush()
            problems.append(f"initializer {step.address:#x} cannot be run natively: {step.why}")
    flush()

    used = sorted({name for name, _, _, _ in rows})
    lines = [
        "// Generated by tools/gen_native_init.py from REBEXE.EXE's initializer tables. Do not edit.",
        f"// {len(cpp_entries)} C++ initializers in table order; {sum(1 for s in plan if s.kind == 'call')} reconstructed calls, "
        f"{sum(1 for s in plan if s.kind == 'baked')} baked store sets ({len(rows)} stores, {dropped} bytes dropped: unreferenced globals).",
        "#include <cstddef>", "#include <cstdint>", "",
    ]
    for name in used:
        lines.append(f'extern unsigned char {name}_s[] __asm__("{linker_symbol(name)}");')
    for address, symbol in sorted(reconstructed.items()):
        if any(s.kind == "call" and s.address == address for s in plan):
            lines.append(f'void init_{address:08x}() __asm__("{linker_symbol(symbol)}");')
    lines += ["", "namespace {", "", "struct BakedStore {", "    unsigned char* destination;", "    uint32_t poolOffset;", "    uint32_t length;", "};", "",
              "const uint8_t kBakedBytes[] = {"]
    for i in range(0, len(pool), 24):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in pool[i:i + 24]) + ",")
    if not pool:
        lines.append("    0,")
    lines += ["};", "", "BakedStore kBakedStores[] = {"]
    for name, offset, pool_offset, length in rows:
        lines.append(f"    {{{name}_s + {offset}, {pool_offset}, {length}}},")
    lines += ["};", "", "void ApplyBakedStores(size_t first, size_t end) {", "    for (size_t i = first; i < end; ++i) {",
              "        const BakedStore& s = kBakedStores[i];",
              "        for (uint32_t b = 0; b < s.length; ++b) s.destination[b] = kBakedBytes[s.poolOffset + b];",
              "    }", "}", "", "}  // namespace", "", "void RebRunStaticInitializers() {"]
    lines += body
    lines += ["}", ""]
    (args.out / "native_init.cpp").write_text("\n".join(lines))
    print(f"gen_native_init: wrote native_init.cpp: {len(rows)} stores ({len(pool)} bytes), {dropped} dropped bytes, {len(problems)} problems")
    for p in problems[:40]:
        print("  PROBLEM:", p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
