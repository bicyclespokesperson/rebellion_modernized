#!/usr/bin/env -S uv run
"""Inventory and export local Rebellion bitmap resources for HD artwork review."""
# /// script
# requires-python = ">=3.11"
# dependencies = ["pefile>=2023.2.7", "pillow>=10"]
# ///
from __future__ import annotations

import argparse
import hashlib
import io
import json
import struct
from pathlib import Path

import pefile
from PIL import Image, ImageDraw


def bitmap_file(dib: bytes) -> bytes:
    if len(dib) < 40:
        raise ValueError("DIB header is truncated")
    size, width, height, planes, depth, compression = struct.unpack_from("<IiiHHI", dib)
    if size < 40 or size > len(dib) or width <= 0 or height == 0 or planes != 1:
        raise ValueError("Unsupported DIB header")
    colors = struct.unpack_from("<I", dib, 32)[0]
    palette_bytes = (colors or (1 << depth)) * 4 if depth <= 8 else 0
    masks = 12 if size == 40 and compression == 3 else 0
    offset = 14 + size + palette_bytes + masks
    if offset > 14 + len(dib):
        raise ValueError("DIB palette is truncated")
    return struct.pack("<2sIHHI", b"BM", 14 + len(dib), 0, 0, offset) + dib


def safe_name(value: object) -> str:
    return "".join(c if c.isalnum() or c in "-_" else "_" for c in str(value))


def resource_token(entry: "pefile.ResourceDirEntryData") -> str:
    if entry.name is None:
        return f"id-{entry.id}"
    name = str(entry.name)
    digest = hashlib.sha256(name.encode("utf-8", errors="surrogatepass")).hexdigest()[:12]
    return f"name-{safe_name(name)}-{digest}"


def export_module(module: Path, output: Path, ids: set[int], scale: int) -> list[dict]:
    records: list[dict] = []
    with module.open("rb") as source_file:
        module_sha256 = hashlib.file_digest(source_file, "sha256").hexdigest()
    with pefile.PE(str(module), fast_load=True) as pe:
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
        root = getattr(pe, "DIRECTORY_ENTRY_RESOURCE", None)
        if root is None:
            return records
        for kind in root.entries:
            if kind.id != 2:
                continue
            for item in kind.directory.entries:
                if ids and item.id not in ids:
                    continue
                identifier = item.id if item.name is None else str(item.name)
                identifier_token = resource_token(item)
                for language in item.directory.entries:
                    entry = language.data.struct
                    dib = pe.get_data(entry.OffsetToData, entry.Size)
                    language_token = resource_token(language)
                    name = f"{safe_name(module.stem)}-{module_sha256}-type-2-{identifier_token}-{language_token}"
                    record = {"module": module.name, "type": 2, "id": identifier, "language": language.id,
                              "module_sha256": module_sha256,
                              "source_sha256": hashlib.sha256(dib).hexdigest(),
                              "palette_dependent": "unknown", "transparency": "unknown",
                              "animation_membership": "unknown", "replacement_approved": False}
                    try:
                        bmp = bitmap_file(dib)
                        source = Image.open(io.BytesIO(bmp))
                        source.load()
                        directory = output / name
                        directory.mkdir(parents=True, exist_ok=True)
                        (directory / "original.bmp").write_bytes(bmp)
                        source.save(directory / "indexed.png")
                        rgb = source.convert("RGB")
                        target = (source.width * scale, source.height * scale)
                        rgb.resize(target, Image.Resampling.NEAREST).save(directory / "nearest.png")
                        rgb.resize(target, Image.Resampling.LANCZOS).save(directory / "lanczos.png")
                        record.update({"size": list(source.size), "mode": source.mode,
                                       "compression": struct.unpack_from("<I", dib, 16)[0], "scale": scale,
                                       "directory": name, "route": "deterministic comparison; requires draw-site audit"})
                    except (ValueError, OSError, struct.error) as error:
                        record["error"] = str(error)
                    records.append(record)
    return records


def contact_sheet(output: Path, records: list[dict]) -> None:
    valid = [r for r in records if "directory" in r]
    columns, cell_width, cell_height = 6, 180, 150
    for page in range(0, len(valid), 60):
        batch = valid[page:page + 60]
        sheet = Image.new("RGB", (columns * cell_width, ((len(batch) + columns - 1) // columns) * cell_height), "#25282c")
        draw = ImageDraw.Draw(sheet)
        for i, record in enumerate(batch):
            with Image.open(output / record["directory"] / "indexed.png") as source:
                thumbnail = source.convert("RGB")
                thumbnail.thumbnail((cell_width - 12, cell_height - 30))
            x, y = (i % columns) * cell_width, (i // columns) * cell_height
            sheet.paste(thumbnail, (x + (cell_width - thumbnail.width) // 2, y))
            draw.text((x + 5, y + cell_height - 27), f'{record["id"]}  {record["size"][0]}x{record["size"][1]}', fill="white")
        sheet.save(output / f"contact-{page // 60 + 1:02d}.png")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", type=Path, action="append", required=True, help="local resource DLL; repeatable")
    parser.add_argument("--id", type=int, action="append", default=[], help="optional bitmap ID filter; repeatable")
    parser.add_argument("--out", type=Path, required=True, help="ignored local output directory, e.g. work/hd/strategy")
    parser.add_argument("--scale", type=int, choices=(2, 4), default=2)
    args = parser.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)
    records = [record for module in args.module for record in export_module(module, args.out, set(args.id), args.scale)]
    manifest = {"schema": "rebellion-artwork-inventory-v1", "loader_pack": False, "assets": records}
    (args.out / "inventory.json").write_text(json.dumps(manifest, indent=2) + "\n")
    contact_sheet(args.out, records)
    print(f"Exported {sum('directory' in r for r in records)}/{len(records)} bitmaps to {args.out}")


if __name__ == "__main__":
    main()
