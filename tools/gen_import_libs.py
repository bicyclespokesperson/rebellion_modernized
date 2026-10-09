#!/usr/bin/env python3
"""Import libraries for the DLLs REBEXE.EXE imports that the build container's SDK has no .lib for.

Reads symbols/imports.csv (the original's import table) and writes one import library per DLL with llvm-lib, from
hand-made COFF short-import members, so that each import binds the way the original does:

  * D3DRM (Microsoft DLL): by name; stdcall exports are undecorated (`Direct3DRMCreate`), the link symbol is
    `_Direct3DRMCreate@4`.
  * DPLAYX and SMACKW32: by ordinal (REBEXE.EXE's import table has no names for them). SMACKW32's ordinals come from
    src/hybrid/smackw32.def, DPLAYX's are fixed below.

Runs inside the rebellion-dev container (needs llvm-lib).
"""
from __future__ import annotations

import argparse
import csv
import re
import struct
import subprocess
import tempfile
from pathlib import Path

MACHINE_I386 = 0x14C
IMPORT_ORDINAL, IMPORT_NAME_UNDECORATE = 0, 3
NAME_TYPE_SHIFT = 2
DLLS = ("DPLAYX.DLL", "D3DRM.DLL", "SMACKW32.DLL")
DPLAYX_ORDINALS = {"DirectPlayCreate": 1, "DirectPlayEnumerateA": 2, "DirectPlayLobbyCreateA": 4}
SMACK_DEF_LINE = re.compile(r"=\s*smackorig\.(\S+?)@(\d+)\s+@(\d+)")


def short_import(symbol: str, dll: str, name_type: int, ordinal: int) -> bytes:
    strings = symbol.encode() + b"\0" + dll.encode() + b"\0"
    header = struct.pack("<HHHHIIHH", 0, 0xFFFF, 0, MACHINE_I386, 0, len(strings), ordinal, name_type << NAME_TYPE_SHIFT)
    return header + strings


def smack_ordinals(def_file: Path) -> dict[str, int]:
    """Plain name -> ordinal, from the hybrid proxy's export list (`_SmackOpen@12 = smackorig._SmackOpen@12 @14`)."""
    out = {}
    for line in def_file.read_text().splitlines():
        m = SMACK_DEF_LINE.search(line)
        if m:
            out[m.group(1).lstrip("_")] = int(m.group(3))
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--imports", type=Path, required=True)
    ap.add_argument("--smack-def", type=Path, required=True)
    ap.add_argument("--outdir", type=Path, required=True)
    args = ap.parse_args()
    args.outdir.mkdir(parents=True, exist_ok=True)
    with open(args.imports, newline="") as fh:
        rows = [r for r in csv.DictReader(fh) if r["dll"] in DLLS]
    ordinals = {"DPLAYX.DLL": DPLAYX_ORDINALS, "SMACKW32.DLL": smack_ordinals(args.smack_def), "D3DRM.DLL": {}}
    for dll in DLLS:
        with tempfile.TemporaryDirectory() as tmp:
            members = []
            for i, row in enumerate(r for r in rows if r["dll"] == dll):
                ordinal = ordinals[dll].get(row["name"], 0)
                name_type = IMPORT_ORDINAL if ordinal else IMPORT_NAME_UNDECORATE
                member = Path(tmp) / f"{i:03}.obj"
                member.write_bytes(short_import(f"_{row['name']}@{row['arg_bytes']}", dll.lower(), name_type, ordinal))
                members.append(str(member))
            out = args.outdir / (dll.lower().removesuffix(".dll") + ".lib")
            subprocess.run(["llvm-lib", "/machine:x86", f"/out:{out}", *members], check=True)
            print(f"gen_import_libs: {out.name}: {len(members)} imports")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
