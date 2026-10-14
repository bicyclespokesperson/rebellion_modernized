#!/usr/bin/env -S uv run
"""Build a local indexed HD bitmap pack from selected RT_BITMAP resources."""
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


MAGIC = b"REBHD01\0"
FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3


def parse_number(value: str) -> int:
    return int(value, 0)


def bitmap_file(dib: bytes) -> bytes:
    if len(dib) < 40:
        raise ValueError("DIB header is truncated")
    header_size, width, signed_height, planes, depth, compression = struct.unpack_from("<IiiHHI", dib)
    if header_size < 40 or header_size > len(dib) or width <= 0 or signed_height in (0, -0x80000000) or planes != 1:
        raise ValueError("Unsupported DIB header")
    if depth != 8 or compression != 0:
        raise ValueError(f"HD pack requires uncompressed 8-bit pixels (depth={depth}, compression={compression})")
    colors = struct.unpack_from("<I", dib, 32)[0] or 256
    if colors > 256:
        raise ValueError(f"8-bit DIB has invalid palette size {colors}")
    offset = 14 + header_size + colors * 4
    height = abs(signed_height)
    stride = ((width * depth + 31) // 32) * 4
    pixel_bytes = stride * height
    if offset > 14 + len(dib) or pixel_bytes > len(dib) - (offset - 14):
        raise ValueError("DIB palette or pixel data is truncated")
    size_image = struct.unpack_from("<I", dib, 20)[0]
    if size_image not in (0, pixel_bytes):
        raise ValueError("DIB pixel byte count disagrees with its dimensions")
    return struct.pack("<2sIHHI", b"BM", 14 + len(dib), 0, 0, offset) + dib


def fnv1a64(data: bytes) -> int:
    value = FNV_OFFSET
    for byte in data:
        value = ((value ^ byte) * FNV_PRIME) & 0xFFFFFFFFFFFFFFFF
    return value


def palette_for(image: Image.Image) -> list[int]:
    palette = image.getpalette()
    if image.mode != "P" or palette is None or len(palette) < 768:
        raise ValueError("Resource bitmap must have a complete indexed palette")
    return palette[:768]


def scale2x(source: Image.Image) -> Image.Image:
    width, height = source.size
    pixels = source.tobytes()
    out = bytearray(width * height * 4)
    out_width = width * 2
    for y in range(height):
        row = y * width
        above = max(0, y - 1) * width
        below = min(height - 1, y + 1) * width
        for x in range(width):
            center = pixels[row + x]
            left = pixels[row + max(0, x - 1)]
            right = pixels[row + min(width - 1, x + 1)]
            top = pixels[above + x]
            bottom = pixels[below + x]
            a = top
            b = right
            c = left
            d = bottom
            e = center
            if b != c and a != d:
                e0 = c if c == a else e
                e1 = b if a == b else e
                e2 = c if c == d else e
                e3 = b if d == b else e
            else:
                e0 = e1 = e2 = e3 = e
            destination = (y * 2) * out_width + x * 2
            out[destination] = e0
            out[destination + 1] = e1
            out[destination + out_width] = e2
            out[destination + out_width + 1] = e3
    result = Image.frombytes("P", (out_width, height * 2), bytes(out))
    result.putpalette(source.getpalette())
    return result


def remap_key_cells(source: Image.Image, enlarged: Image.Image, key: int | None, scale: int) -> Image.Image:
    width, height = source.size
    expected = (width * scale, height * scale)
    if enlarged.size != expected:
        raise ValueError(f"Unexpected scaled size {enlarged.size}; expected {expected}")
    source_pixels = source.tobytes()
    pixels = bytearray(enlarged.tobytes())
    if key is None:
        result = Image.frombytes("P", expected, bytes(pixels))
        result.putpalette(source.getpalette())
        return result
    palette = source.getpalette()
    out_width = width * scale
    for y in range(height):
        for x in range(width):
            source_index = source_pixels[y * width + x]
            start_y, start_x = y * scale, x * scale
            if source_index == key:
                for row in range(start_y, start_y + scale):
                    start = row * out_width + start_x
                    pixels[start:start + scale] = bytes([key]) * scale
                continue
            for row in range(start_y, start_y + scale):
                start = row * out_width + start_x
                for offset in range(scale):
                    result_index = pixels[start + offset]
                    if result_index == key:
                        pixels[start + offset] = source_index
                    elif result_index != source_index and palette[result_index * 3:result_index * 3 + 3] == palette[source_index * 3:source_index * 3 + 3]:
                        pixels[start + offset] = source_index
    result = Image.frombytes("P", expected, bytes(pixels))
    result.putpalette(palette)
    return result


def indexed_lanczos(source: Image.Image, scale: int, key: int | None) -> Image.Image:
    palette = source.getpalette()
    source_rgb = source.convert("RGB")
    if key is None:
        enlarged_rgb = source_rgb.resize((source.width * scale, source.height * scale), Image.Resampling.LANCZOS)
        quantize_palette = palette
    else:
        alpha = source.point(lambda index: 0 if index == key else 255, mode="L")
        rgba = source_rgb.convert("RGBA")
        rgba.putalpha(alpha)
        enlarged_rgba = rgba.convert("RGBa").resize((source.width * scale, source.height * scale), Image.Resampling.LANCZOS).convert("RGBA")
        enlarged_rgb = enlarged_rgba.convert("RGB")
        quantize_palette = palette.copy()
        non_key = [index for index in range(256) if index != key]
        key_rgb = tuple(palette[key * 3:key * 3 + 3])
        replacement = min(non_key, key=lambda candidate: sum((a - b) ** 2 for a, b in zip(
            key_rgb, tuple(palette[candidate * 3:candidate * 3 + 3]))))
        quantize_palette[key * 3:key * 3 + 3] = palette[replacement * 3:replacement * 3 + 3]
    palette_image = Image.new("P", (1, 1))
    palette_image.putpalette(quantize_palette)
    indexed = enlarged_rgb.quantize(palette=palette_image, dither=Image.Dither.NONE)
    indexed.putpalette(palette)
    return indexed


def preview_rgb(image: Image.Image, key: int | None) -> Image.Image:
    rgb = image.convert("RGB")
    if key is None:
        return rgb
    background = Image.new("RGB", image.size, (72, 76, 82))
    draw = ImageDraw.Draw(background)
    tile = 16
    for y in range(0, image.height, tile):
        for x in range(0, image.width, tile):
            if (x // tile + y // tile) % 2:
                draw.rectangle((x, y, min(x + tile - 1, image.width - 1), min(y + tile - 1, image.height - 1)),
                               fill=(108, 112, 118))
    mask = image.point(lambda index: 0 if index == key else 255, mode="L")
    return Image.composite(rgb, background, mask)


def entries_for(module: Path, wanted: set[int]) -> list[tuple[int, int, bytes]]:
    found: list[tuple[int, int, bytes]] = []
    with pefile.PE(str(module), fast_load=True) as pe:
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
        root = getattr(pe, "DIRECTORY_ENTRY_RESOURCE", None)
        if root is None:
            return found
        for kind in root.entries:
            if kind.id != 2:
                continue
            for item in kind.directory.entries:
                if item.id not in wanted:
                    continue
                for language in item.directory.entries:
                    data = language.data.struct
                    dib = pe.get_data(data.OffsetToData, data.Size)
                    if len(dib) != data.Size:
                        raise ValueError(f"Truncated RT_BITMAP {item.id}/{language.id} in {module}")
                    found.append((item.id, language.id, dib))
    return found


def parse_options(values: list[str], option_name: str) -> dict[int, str]:
    parsed: dict[int, str] = {}
    for value in values:
        try:
            raw_id, setting = value.split("=", 1)
            resource_id = int(raw_id, 0)
        except ValueError as error:
            raise SystemExit(f"Invalid --{option_name} value {value!r}; expected ID=VALUE") from error
        if resource_id in parsed:
            raise SystemExit(f"Duplicate --{option_name} for resource {resource_id}")
        parsed[resource_id] = setting
    return parsed


def merge_asset_records(existing: list[dict], incoming: list[dict]) -> list[dict]:
    merged = {record["pack_file"]: record for record in existing}
    merged.update({record["pack_file"]: record for record in incoming})
    return sorted(merged.values(), key=lambda record: (record["module"].lower(), record["resource_id"], record["language_id"]))


def write_asset(module: Path, output: Path, resource_id: int, language: int, raw_dib: bytes,
                scale: int, method: str, key_spec: str) -> dict:
    if len(raw_dib) < 40:
        raise ValueError("DIB header is truncated")
    declared_width, declared_height = struct.unpack_from("<ii", raw_dib, 4)
    if declared_width <= 0 or declared_height in (0, -0x80000000):
        raise ValueError("Invalid DIB dimensions")
    if declared_width * abs(declared_height) * scale * scale > 64 * 1024 * 1024:
        raise ValueError("Scaled indexed payload would exceed the 64 MiB runtime limit")
    source_hash = hashlib.sha256(raw_dib).hexdigest()
    source_fnv = fnv1a64(raw_dib)
    source_bytes = bitmap_file(raw_dib)
    with Image.open(io.BytesIO(source_bytes)) as source_image:
        source_image.load()
        source = source_image.copy()
    palette = palette_for(source)
    width, signed_height = struct.unpack_from("<ii", raw_dib, 4)
    logical_height = abs(signed_height)
    color_count = struct.unpack_from("<I", raw_dib, 32)[0] or 256
    if max(source.getextrema()) >= color_count:
        raise ValueError("Bitmap pixels reference an index outside the declared palette")
    if source.size != (width, logical_height):
        raise ValueError("Decoded dimensions disagree with the DIB header")
    first_pixel = source.getpixel((0, logical_height - 1 if signed_height > 0 else 0))
    key = None if key_spec.lower() == "none" else (first_pixel if key_spec == "first" else int(key_spec, 0))
    if key is not None and not 0 <= key <= 255:
        raise ValueError(f"Key index must be in [0,255], got {key}")
    if method == "scale2x":
        enlarged = scale2x(scale2x(source))
    elif method == "lanczos":
        enlarged = indexed_lanczos(source, scale, key)
    else:
        raise ValueError(f"Unknown scaling method {method!r}")
    enlarged = remap_key_cells(source, enlarged, key, scale)
    payload = enlarged.tobytes()
    expected_size = width * logical_height * scale * scale
    if len(payload) != expected_size:
        raise ValueError(f"Unexpected payload size {len(payload)} != {expected_size}")

    name = f"{source_fnv:016x}.rhd"
    pack_bytes = MAGIC + struct.pack("<IIIIQ", width, logical_height, scale, len(payload), source_fnv) + payload
    if len(MAGIC) != 8 or len(pack_bytes) != 32 + expected_size:
        raise ValueError("Internal pack header size mismatch")
    asset_dir = output / f"{module.stem.lower()}-{resource_id}-{language}"
    asset_dir.mkdir(parents=True, exist_ok=True)
    pack_path = output / name
    pack_path.write_bytes(pack_bytes)
    indexed_path = asset_dir / "accepted-indexed.png"
    enlarged.save(indexed_path, format="PNG", optimize=False)
    source.save(asset_dir / "source-indexed.png", format="PNG", optimize=False)
    nearest = source.resize(enlarged.size, Image.Resampling.NEAREST)
    nearest.putpalette(palette)
    nearest_path = asset_dir / "nearest.png"
    nearest.save(nearest_path)
    baseline = indexed_lanczos(source, scale, key)
    baseline = remap_key_cells(source, baseline, key, scale)
    baseline_path = asset_dir / "lanczos-fixed-palette.png"
    baseline.save(baseline_path)
    label_height = 28
    comparison = Image.new("RGB", (enlarged.width * 2, (enlarged.height + label_height) * 2), (24, 24, 24))
    draw = ImageDraw.Draw(comparison)
    scale2x_result = scale2x(scale2x(source))
    scale2x_result = remap_key_cells(source, scale2x_result, key, scale)
    panes = [(nearest, "Nearest 4x"), (scale2x_result, "Scale2x twice"),
             (baseline, "Keyed Lanczos" if key is not None else "Lanczos fixed palette"),
             (enlarged, f"Accepted: {method}")]
    for index, (pane, label) in enumerate(panes):
        x = (index % 2) * enlarged.width
        y = (index // 2) * (enlarged.height + label_height)
        comparison.paste(preview_rgb(pane, key), (x, y + label_height))
        draw.text((x + 6, y + 6), label, fill=(255, 255, 255))
    comparison_path = asset_dir / "comparison.png"
    comparison.save(comparison_path)
    return {
        "module": module.name,
        "module_path": str(module),
        "resource_type": 2,
        "resource_id": resource_id,
        "language_id": language,
        "source_sha256": source_hash,
        "source_fnv1a64": f"{source_fnv:016x}",
        "logical_size": [width, logical_height],
        "source_mode": source.mode,
        "depth": 8,
        "compression": 0,
        "scale": scale,
        "key_index": key,
        "key_rule": ("opaque source; no key mask is applied" if key is None else
                     "explicit index; every source key cell remains entirely key and no non-key cell contains key index"),
        "palette_rule": "original 256-entry palette retained; pack stores indices only",
        "route": method,
        "audit_status": "pending_visual_review",
        "pack_file": name,
        "pack_sha256": hashlib.sha256(pack_bytes).hexdigest(),
        "payload_bytes": len(payload),
        "accepted_png": str(indexed_path),
        "nearest_comparison": str(nearest_path),
        "lanczos_comparison": str(baseline_path),
        "comparison_png": str(comparison_path),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--recipe", type=Path, help="JSON recipe with module, out, scale, and per-asset settings")
    parser.add_argument("--module", type=Path, help="resource DLL containing RT_BITMAPs; overrides recipe path")
    parser.add_argument("--id", type=parse_number, action="append", default=[],
                        help="RT_BITMAP id in decimal or 0x-prefixed hex; repeat for each selected source")
    parser.add_argument("--out", type=Path, help="local pack directory; overrides recipe path")
    parser.add_argument("--scale", type=int, choices=(4,), default=4)
    parser.add_argument("--method", action="append", default=[], metavar="ID=METHOD",
                        help="scaling method for each id: scale2x or lanczos; repeat")
    parser.add_argument("--key", action="append", default=[], metavar="ID=KEY",
                        help="key index for each id: first or numeric palette index; repeat")
    args = parser.parse_args()
    if args.recipe:
        recipe = json.loads(args.recipe.read_text())
        if args.id or args.method or args.key:
            raise SystemExit("Use either --recipe or explicit --id/--method/--key options")
        args.module = args.module or Path(recipe["module"])
        args.out = args.out or Path(recipe["out"])
        args.scale = recipe.get("scale", 4)
        args.id = [int(asset["id"]) for asset in recipe["assets"]]
        args.method = [f'{asset["id"]}={asset["method"]}' for asset in recipe["assets"]]
        args.key = [f'{asset["id"]}={asset["key"]}' for asset in recipe["assets"]]
    if not args.module or not args.out or not args.id or not args.method or not args.key:
        raise SystemExit("Provide --recipe or --module, --out, --id, --method, and --key")
    if args.scale != 4:
        raise SystemExit("Only 4x pack output is currently supported")
    if len(args.id) != len(set(args.id)):
        raise SystemExit("Duplicate --id values are not allowed")
    output = args.out
    output.mkdir(parents=True, exist_ok=True)
    rows = entries_for(args.module, set(args.id))
    by_id = {resource_id for resource_id, _, _ in rows}
    missing = set(args.id) - by_id
    if missing:
        raise SystemExit(f"Bitmap resource(s) not found: {', '.join(map(str, sorted(missing)))}")
    methods = parse_options(args.method, "method")
    keys = parse_options(args.key, "key")
    if set(methods) != set(args.id) or set(keys) != set(args.id):
        raise SystemExit("Provide exactly one --method and --key for every selected --id")
    new_records = [write_asset(args.module, output, resource_id, language, dib, args.scale,
                               methods[resource_id], keys[resource_id])
                   for resource_id, language, dib in rows]
    manifest_path = output / "manifest.json"
    existing_records: list[dict] = []
    if manifest_path.exists():
        try:
            existing_manifest = json.loads(manifest_path.read_text())
            if existing_manifest.get("schema") != "rebellion-hd-indexed-pack-v1":
                raise ValueError("unexpected schema")
            existing_records = existing_manifest["assets"]
        except (OSError, ValueError, KeyError, TypeError) as error:
            raise SystemExit(f"Cannot merge existing manifest {manifest_path}: {error}") from error
    merged_records = merge_asset_records(existing_records, new_records)
    module_hash = hashlib.sha256(args.module.read_bytes()).hexdigest()
    module_hashes = {}
    if manifest_path.exists():
        module_hashes.update(existing_manifest.get("module_sha256_by_name", {}))
    module_hashes[args.module.name] = module_hash
    manifest = {
        "schema": "rebellion-hd-indexed-pack-v1",
        "pack_contract": "32-byte header: REBHD01\\0 + uint32 width,height,scale,payloadSize + uint64 sourceFNV1a64; then top-down row-major indexed payload",
        "runtime_ready": True,
        "module_sha256": module_hash,
        "module_sha256_by_name": module_hashes,
        "assets": merged_records,
    }
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Built {len(new_records)} indexed HD assets in {output}; manifest has {len(merged_records)} assets")


if __name__ == "__main__":
    main()
