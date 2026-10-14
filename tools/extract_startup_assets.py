#!/usr/bin/env -S uv run
"""Extract the versioned startup text asset pack from a local game installation."""
# /// script
# requires-python = ">=3.11"
# dependencies = ["pefile"]
# ///
from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

import pefile


def extract(executable: Path, recipe: Path, destination: Path) -> None:
    manifest = json.loads(recipe.read_text())
    if manifest["schema"] != 1:
        raise ValueError("Unsupported extraction recipe")
    image = pefile.PE(str(executable))
    entries: list[tuple[bytes, bytes]] = []
    identifiers: set[str] = set()
    for asset in manifest["assets"]:
        identifier = asset["id"]
        if identifier in identifiers:
            raise ValueError(f"Duplicate asset identifier: {identifier}")
        identifiers.add(identifier)
        if "source_pointer_rva" in asset:
            pointer = image.get_data(asset["source_pointer_rva"], 4)
            if len(pointer) != 4:
                raise ValueError(f"Missing pointer for {identifier}")
            address = int.from_bytes(pointer, "little")
            rva = address - image.OPTIONAL_HEADER.ImageBase if address else None
        else:
            rva = asset["source_rva"]
        payload = b""
        if rva is not None:
            if rva < 0 or rva >= image.OPTIONAL_HEADER.SizeOfImage:
                raise ValueError(f"Invalid source location for {identifier}")
            raw = image.get_data(rva, asset["max_bytes"])
            terminator = raw.find(b"\0")
            if terminator < 0:
                raise ValueError(f"Unterminated text for {identifier}")
            payload = raw[:terminator]
        entries.append((identifier.encode("ascii"), payload))
    pack = bytearray(b"RBASSET1" + struct.pack("<I", len(entries)))
    for identifier, payload in entries:
        pack.extend(struct.pack("<HI", len(identifier), len(payload)))
        pack.extend(identifier)
        pack.extend(payload)
    destination.parent.mkdir(parents=True, exist_ok=True)
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    temporary.write_bytes(pack)
    temporary.replace(destination)
    print(f"Extracted {len(entries)} startup assets to {destination}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--recipe", type=Path, default=Path(__file__).with_name("startup_assets_v1.json"))
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    extract(args.exe, args.recipe, args.out)


if __name__ == "__main__":
    main()
