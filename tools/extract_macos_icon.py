#!/usr/bin/env -S uv run
"""Convert the locally installed game's application icon to a macOS icon bundle."""
# /// script
# requires-python = ">=3.11"
# dependencies = ["pefile", "pillow"]
# ///
from __future__ import annotations

import argparse
import io
import struct
import subprocess
import tempfile
from pathlib import Path

import pefile
from PIL import Image


def read_icon(executable: Path) -> bytes:
    with pefile.PE(str(executable)) as image:
        resources = {entry.id: entry for entry in image.DIRECTORY_ENTRY_RESOURCE.entries}
        group = resources[14].directory.entries[0].directory.entries[0].data.struct
        data = image.get_data(group.OffsetToData, group.Size)
        reserved, kind, count = struct.unpack_from("<HHH", data)
        if reserved != 0 or kind != 1 or len(data) != 6 + 14 * count or count == 0:
            raise ValueError("Invalid application icon group")
        icons = {entry.id: entry for entry in resources[3].directory.entries}
        header = bytearray(struct.pack("<HHH", 0, 1, count))
        pixels = bytearray()
        offset = 6 + 16 * count
        for index in range(count):
            entry = data[6 + 14 * index : 20 + 14 * index]
            identifier = struct.unpack_from("<H", entry, 12)[0]
            resource = icons[identifier].directory.entries[0].data.struct
            payload = image.get_data(resource.OffsetToData, resource.Size)
            if len(payload) != struct.unpack_from("<I", entry, 8)[0]:
                raise ValueError("Invalid application icon image size")
            header.extend(entry[:12] + struct.pack("<I", offset))
            pixels.extend(payload)
            offset += len(payload)
        return bytes(header + pixels)


def extract(executable: Path, destination: Path) -> None:
    with Image.open(io.BytesIO(read_icon(executable))) as source:
        icon = source.convert("RGBA")
    with tempfile.TemporaryDirectory(prefix="rebellion-icon-") as temporary:
        directory = Path(temporary)
        iconset = directory / "Rebellion.iconset"
        iconset.mkdir()
        for size in (16, 32, 128, 256, 512):
            for scale in (1, 2):
                suffix = "@2x" if scale == 2 else ""
                resized = icon.resize((size * scale, size * scale), Image.Resampling.NEAREST)
                resized.save(iconset / f"icon_{size}x{size}{suffix}.png")
        output = directory / "Rebellion.icns"
        subprocess.run(["iconutil", "-c", "icns", "-o", str(output), str(iconset)], check=True)
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(output.read_bytes())
    print(f"Extracted application icon to {destination}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    arguments = parser.parse_args()
    extract(arguments.exe, arguments.out)


if __name__ == "__main__":
    main()
