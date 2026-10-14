#!/usr/bin/env python3
"""Push the memory layouts of reconstructed classes/structs (declared under src/game) into Ghidra as
structures, so the decompiler shows field names in every function that uses them.

Layouts come from clang's -fdump-record-layouts run over each src/game translation unit in the dev
container; the Ghidra side is tools/py/import_types.py (run through tools/gq).
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from paths import MAIN, ROOT  # noqa: E402

DECL = re.compile(r"^\s*(?:class|struct|union)\s+(\w+)\s*(?:final\s*)?[:{]", re.M)
FIELD = re.compile(r"^\s*(\d+)(?::(\d+)-(\d+))?\s*\|( +)(.*)$")
SIZE = re.compile(r"\[sizeof=(\d+), (?:dsize=\d+, )?align=(\d+)")
CLANG_FLAGS = ("--target=i686-pc-windows-msvc /imsvc/opt/xwin/crt/include /imsvc/opt/xwin/sdk/include/ucrt "
               "/imsvc/opt/xwin/sdk/include/um /imsvc/opt/xwin/sdk/include/shared -Isrc/game -Isrc /GR- /Zs "
               "-DREB_HYBRID=1 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_NO_CRT_STDIO_INLINE /std:c++23 "
               "-Xclang -fdump-record-layouts -Xclang -fdump-record-layouts-canonical")


def our_record_names() -> set[str]:
    names: set[str] = set()
    for header in (ROOT / "src" / "game").rglob("*.h"):
        names.update(DECL.findall(header.read_text(errors="replace")))
    return names


def dump_layouts(sources: list[Path]) -> str:
    rel = " ".join(str(s.relative_to(ROOT)) for s in sources)
    cmd = f"for f in {rel}; do clang-cl {CLANG_FLAGS} $f; done"
    out = subprocess.run(["docker", "run", "--rm", "-u", "1000:1000", "-v", f"{ROOT}:{ROOT}", "-w", str(ROOT),
                          "rebellion-dev", "bash", "-c", cmd], capture_output=True, text=True)
    return out.stdout + out.stderr


def parse_field(text: str) -> dict:
    """'char * m_text' -> type/name; '(String vftable pointer)' -> vptr; 'class Base (base)' -> base."""
    if text.endswith("vftable pointer)") or text.endswith("vbtable pointer)"):
        return {"kind": "vptr", "name": "vftable", "type": "void *"}
    m = re.match(r"^(?:class|struct|union) (\w+) \((?:primary )?(?:virtual )?base\)$", text)
    if m:
        return {"kind": "base", "name": f"base_{m.group(1)}", "type": m.group(1)}
    m = re.match(r"^(.*?)\s*(\w+)$", text)
    return {"kind": "field", "name": m.group(2), "type": m.group(1).strip()} if m else {"kind": "unknown", "text": text}


def parse_layouts(dump: str, wanted: set[str]) -> dict[str, dict]:
    records: dict[str, dict] = {}
    for block in dump.split("*** Dumping AST Record Layout")[1:]:
        lines = [l for l in block.splitlines() if "|" in l]
        if not lines:
            continue
        head = FIELD.match(lines[0])
        if not head:
            continue
        m = re.match(r"(?:class|struct|union) (\w+)$", head.group(5).strip())
        if not m or m.group(1) not in wanted or m.group(1) in records:
            continue
        name = m.group(1)
        fields = []
        top_indent = None
        size = None
        for line in lines[1:]:
            sm = SIZE.search(line)
            if sm:
                size = int(sm.group(1))
                break
            fm = FIELD.match(line)
            if not fm:
                continue
            indent = len(fm.group(4))
            top_indent = indent if top_indent is None else top_indent
            if indent != top_indent:
                continue  # members of a nested record, described by that record's own layout
            if fm.group(2) is not None:
                continue  # bitfield: left undefined in Ghidra
            f = parse_field(fm.group(5).strip())
            f["offset"] = int(fm.group(1))
            fields.append(f)
        if size is not None:
            records[name] = {"size": size, "fields": fields}
    return records


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--dry-run", action="store_true", help="print the parsed layouts instead of importing")
    args = ap.parse_args()
    sources = sorted((ROOT / "src" / "game").rglob("*.cpp"))
    records = parse_layouts(dump_layouts(sources), our_record_names())
    if args.dry_run:
        print(json.dumps(records, indent=1))
        return 0
    payload = MAIN / "work" / "types.json"
    payload.write_text(json.dumps(records))
    out = subprocess.run([str(ROOT / "tools" / "gq"), "-f", str(ROOT / "tools" / "py" / "import_types.py")],
                         capture_output=True, text=True)
    print(out.stdout.strip())
    save = subprocess.run([str(ROOT / "tools" / "gq"), "--save"], capture_output=True, text=True)
    return 0 if "saved" in save.stdout else 1


if __name__ == "__main__":
    sys.exit(main())
