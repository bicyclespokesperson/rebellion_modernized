#!/usr/bin/env python3
"""Regression store for difftest captures, shared by all checkouts (MAIN/work/regression).

  capture_store.py add <run dir or capture files...>   import captures (pages deduplicated globally)
  capture_store.py list                                 captures per function
  capture_store.py paths [0x5f35b0 ...]                 print stored capture paths (all or per function)

Layout: work/regression/pages.pack (unique 4 KiB pages), pages.idx (sha1 -> offset), and
caps/<function>/<origin>_<n>.cap whose page offsets point into the global pack. At most --keep captures are
kept per function (newest win), so the suite stays fast.
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from paths import MAIN  # noqa: E402

STORE = MAIN / "work" / "regression"
PACK = STORE / "pages.pack"
INDEX = STORE / "pages.idx"
HEADER_SIZE = 4 * 4 + 4 + 10 * 4 + 3 * 4 + 80 + 4
PAGE_COUNT_AT = HEADER_SIZE - 4
PAGEREF = struct.Struct("<IIQ")
INDEX_REC = struct.Struct("<20sQ")
PAGE = 4096


def load_index() -> dict[bytes, int]:
    if not INDEX.exists():
        return {}
    data = INDEX.read_bytes()
    return {h: off for h, off in INDEX_REC.iter_unpack(data)}


def import_capture(cap: Path, origin: str, index: dict[bytes, int], pack, idx, keep: int) -> Path:
    data = bytearray(cap.read_bytes())
    function = struct.unpack_from("<I", data, 8)[0]
    count = struct.unpack_from("<I", data, PAGE_COUNT_AT)[0]
    with open(cap.parent / "pages.pack", "rb") as src:
        for i in range(count):
            at = HEADER_SIZE + i * PAGEREF.size
            address, protect, offset = PAGEREF.unpack_from(data, at)
            src.seek(offset)
            page = src.read(PAGE)
            digest = hashlib.sha1(page).digest()
            if digest not in index:
                index[digest] = pack.seek(0, 2)
                pack.write(page)
                idx.write(INDEX_REC.pack(digest, index[digest]))
            PAGEREF.pack_into(data, at, address, protect, index[digest])
    dest_dir = STORE / "caps" / f"{function:08x}"
    dest_dir.mkdir(parents=True, exist_ok=True)
    dest = dest_dir / f"{origin}_{cap.stem}.cap"
    dest.write_bytes(data)
    for old in sorted(dest_dir.glob("*.cap"), key=lambda p: p.stat().st_mtime, reverse=True)[keep:]:
        old.unlink()
    return dest


def cmd_add(args) -> int:
    caps: list[Path] = []
    for p in args.paths:
        p = Path(p)
        caps += sorted(p.rglob("*.cap")) if p.is_dir() else [p]
    STORE.mkdir(parents=True, exist_ok=True)
    index = load_index()
    before = len(index)
    with open(PACK, "ab") as pack, open(INDEX, "ab") as idx:
        for cap in caps:
            origin = args.origin or cap.parent.parent.name
            import_capture(cap, origin, index, pack, idx, args.keep)
    print(f"imported {len(caps)} captures; {len(index) - before} new pages "
          f"(store: {PACK.stat().st_size >> 20} MiB)")
    return 0


def cmd_list(args) -> int:
    for d in sorted((STORE / "caps").glob("*")):
        print(f"0x{d.name} {len(list(d.glob('*.cap')))}")
    return 0


def cmd_paths(args) -> int:
    dirs = [STORE / "caps" / f"{int(a, 16):08x}" for a in args.functions] if args.functions else \
        sorted((STORE / "caps").glob("*"))
    for d in dirs:
        for c in sorted(d.glob("*.cap")):
            print(c)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("add")
    a.add_argument("paths", nargs="+")
    a.add_argument("--origin", help="label for the imported captures (default: run directory name)")
    a.add_argument("--keep", type=int, default=8, help="max captures kept per function")
    sub.add_parser("list")
    p = sub.add_parser("paths")
    p.add_argument("functions", nargs="*")
    args = ap.parse_args()
    return {"add": cmd_add, "list": cmd_list, "paths": cmd_paths}[args.cmd](args)


if __name__ == "__main__":
    sys.exit(main())
