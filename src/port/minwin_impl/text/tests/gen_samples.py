#!/usr/bin/env -S uv run --script
# /// script
# dependencies = ["pefile"]
# ///
"""Collect the text the game draws (string tables of the resource DLLs, encyclopedia paragraphs) into a samples file for the
text comparison against Wine. One sample per line; a newline inside a sample is written \\n, a tab \\t. The file is derived from
the user's installation and stays under build/ (never committed)."""
import argparse
from pathlib import Path

import pefile

STRING_DLLS = ["TEXTCOMM.DLL", "TEXTTACT.DLL", "ENCYBMAP.DLL", "REBDLOG.DLL", "TEXTSTRA.DLL", "CDDERR.DLL"]


def string_tables(pe: "pefile.PE") -> list[str]:
    out: list[str] = []
    if not hasattr(pe, "DIRECTORY_ENTRY_RESOURCE"):
        return out
    for type_entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if type_entry.id != 6:
            continue
        for name_entry in type_entry.directory.entries:
            leaf = name_entry.directory.entries[0].data.struct
            data = pe.get_data(leaf.OffsetToData, leaf.Size)
            pos = 0
            for _ in range(16):
                if pos + 2 > len(data):
                    break
                length = int.from_bytes(data[pos : pos + 2], "little")
                text = data[pos + 2 : pos + 2 + 2 * length].decode("utf-16-le", "replace")
                pos += 2 + 2 * length
                if text:
                    out.append(text)
    return out


def rcdata_texts(pe: "pefile.PE", limit: int) -> list[str]:
    out: list[str] = []
    for type_entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if type_entry.id != 10:
            continue
        for name_entry in type_entry.directory.entries[:limit]:
            leaf = name_entry.directory.entries[0].data.struct
            data = pe.get_data(leaf.OffsetToData, leaf.Size)
            text = data.split(b"\0")[0].decode("cp1252", "replace")
            if text.strip():
                out.append(text)
    return out


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("install", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    samples: list[str] = []
    for name in STRING_DLLS:
        path = next(args.install.glob(name), None)
        if path:
            samples += string_tables(pefile.PE(str(path)))[:60]
    encyclopedia = next(args.install.glob("ENCYTEXT.DLL"), None)
    if encyclopedia:
        samples += rcdata_texts(pefile.PE(str(encyclopedia)), 40)
    seen: set[str] = set()
    lines: list[str] = []
    for s in samples:
        s = s.replace("\r\n", "\n")
        if s in seen or len(s) > 2000:
            continue
        seen.add(s)
        lines.append(s.replace("\\", "\\\\").replace("\n", "\\n").replace("\t", "\\t"))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n", encoding="cp1252", errors="replace")
    print(f"{len(lines)} samples")


if __name__ == "__main__":
    main()
