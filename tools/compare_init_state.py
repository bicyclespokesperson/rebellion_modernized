#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# ///
"""Compare the game-owned data block after static initialisation: standalone exe vs the original code.

Reference: a hybrid run with every hook disabled and REB_CAPTURE=0x4165b0:1 (a capture of all memory at WinMain's first
instruction, i.e. after the original CRT ran its full initializer table). Standalone: REB_DUMP_INIT_STATE=<file> makes
the exe write the block (preceded by its run-time address) once its generated initializer table is done. The script
undoes the relocation of pointers into the block, then reports every differing dword. Pointers to code or .rdata differ
by construction (the standalone has its own copies); they are listed separately and are expected to be few.

  tools/compare_init_state.py <capture dir> <standalone dump> [--list]

tools/check_init_state.sh runs both sides and this comparison.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import own_data  # noqa: E402

PAGE = 4096
HEADER = struct.Struct("<4I I 10I 3I 80s I")
PAGEREF = struct.Struct("<IIQ")
CODE_LIMIT = 0x6A6000


def load_reference(capture_dir: Path) -> bytes:
    caps = sorted(capture_dir.glob("004165b0_*.cap"))
    if not caps:
        raise SystemExit(f"{capture_dir}: no 004165b0_*.cap (capture WinMain with REB_CAPTURE=0x4165b0:1)")
    data = caps[0].read_bytes()
    count = HEADER.unpack_from(data, 0)[19]
    pages = {a: off for a, _, off in (PAGEREF.unpack_from(data, HEADER.size + i * PAGEREF.size) for i in range(count))}
    pack = (capture_dir / "pages.pack").read_bytes()
    out = bytearray()
    for page in range(own_data.BLOCK_BASE & ~(PAGE - 1), own_data.BLOCK_END, PAGE):
        out += pack[pages[page]:pages[page] + PAGE] if page in pages else bytes(PAGE)
    start = own_data.BLOCK_BASE & (PAGE - 1)
    return bytes(out[start:start + own_data.BLOCK_END - own_data.BLOCK_BASE])


def load_standalone(dump: Path) -> tuple[bytes, int]:
    raw = dump.read_bytes()
    base = struct.unpack_from("<I", raw, 0)[0]
    return raw[4:], base


def dwords(data: bytes) -> list[int]:
    return list(struct.unpack(f"<{len(data) // 4}I", data[: len(data) // 4 * 4]))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture_dir", type=Path)
    ap.add_argument("dump", type=Path)
    ap.add_argument("--list", action="store_true", help="list every differing dword")
    args = ap.parse_args()

    reference = load_reference(args.capture_dir)
    standalone, base = load_standalone(args.dump)
    if len(reference) != len(standalone):
        print(f"FAIL: block sizes differ: reference {len(reference):#x}, standalone {len(standalone):#x}")
        return 1
    size = len(reference)
    relocated = 0
    code_pointers: list[tuple[int, int, int]] = []
    other: list[tuple[int, int, int]] = []
    for index, (ref, got) in enumerate(zip(dwords(reference), dwords(standalone))):
        address = own_data.BLOCK_BASE + 4 * index
        if not own_data.in_segment(address):
            continue
        if base <= got < base + size:
            got = got - base + own_data.BLOCK_BASE
            relocated += 1
        if ref == got:
            continue
        (code_pointers if 0x401000 <= ref < CODE_LIMIT else other).append((address, ref, got))
    owned = sum(hi - lo for lo, hi in own_data.OWN_SEGMENTS)
    print(f"game-owned data {own_data.OWN_SEGMENTS[0][0]:#x}-{own_data.OWN_SEGMENTS[0][1]:#x} + "
          f"{own_data.OWN_SEGMENTS[1][0]:#x}-{own_data.OWN_SEGMENTS[1][1]:#x} ({owned:#x} bytes): {relocated} pointers into the block "
          f"relocated; {len(code_pointers)} differing code/.rdata pointers (expected), {len(other)} other differing dwords")
    for kind, rows in (("code/.rdata pointer", code_pointers), ("OTHER", other)):
        for address, ref, got in rows[: None if args.list or kind == "OTHER" else 0]:
            print(f"  {kind} {address:#x}: reference {ref:#010x}, standalone {got:#010x}")
    if other:
        print("FAIL: the standalone's post-initialisation data differs from the original's")
        return 1
    print("OK: post-initialisation .data/.bss equal (modulo relocated pointers)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
