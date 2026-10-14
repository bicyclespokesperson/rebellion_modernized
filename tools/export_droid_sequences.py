#!/usr/bin/env -S uv run
"""Decode and compare complete Rebellion droid delta-frame sequences offline."""
# /// script
# requires-python = ">=3.11"
# dependencies = ["pefile>=2023.2.7", "pillow>=10"]
# ///
from __future__ import annotations

import argparse
from functools import lru_cache
import hashlib
import json
import struct
from pathlib import Path

import pefile
from PIL import Image, ImageDraw

try:
    from . import build_hd_pack
except ImportError:
    import build_hd_pack


RESOURCE_BITMAP = 2
RESOURCE_RCDATA = 10
RESOURCE_DELTA_FRAME = 302


def resource_entries(module: Path, resource_type: int) -> dict[int, bytes]:
    with pefile.PE(str(module), fast_load=True) as pe:
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
        root = getattr(pe, "DIRECTORY_ENTRY_RESOURCE", None)
        if root is None:
            return {}
        type_entry = next((entry for entry in root.entries if entry.id == resource_type), None)
        if type_entry is None:
            return {}
        result: dict[int, bytes] = {}
        for item in type_entry.directory.entries:
            for language in item.directory.entries:
                data = language.data.struct
                payload = pe.get_data(data.OffsetToData, data.Size)
                if len(payload) != data.Size:
                    raise ValueError(f"Truncated resource {resource_type}/{item.id}/{language.id}")
                result[item.id] = payload
        return result


def sequence_references(module: Path, base_id: int) -> tuple[int, list[int]]:
    references: list[tuple[int, int]] = []
    for script_id, payload in resource_entries(module, RESOURCE_RCDATA).items():
        if len(payload) < 10 or len(payload) % 2:
            continue
        words = struct.unpack(f"<{len(payload) // 2}H", payload)
        if words[4] == base_id:
            references.append((script_id, words[3]))
    if not references:
        raise ValueError(f"No type-10 script references bitmap {base_id} as a delta sequence")
    counts = {count for _, count in references}
    if len(counts) != 1:
        raise ValueError(f"Conflicting frame counts for bitmap {base_id}: {sorted(counts)}")
    return counts.pop(), sorted(script_id for script_id, _ in references)


def bitmap_image(raw: bytes) -> Image.Image:
    bmp = build_hd_pack.bitmap_file(raw)
    image = Image.open(__import__("io").BytesIO(bmp))
    image.load()
    result = image.copy()
    build_hd_pack.palette_for(result)
    return result


def decode_delta(raw: bytes, image: Image.Image) -> None:
    if len(raw) < 17:
        raise ValueError("Delta header is truncated")
    width, height, data_size = struct.unpack_from("<hhI", raw)
    if width != image.width or height != image.height or width <= 0 or height <= 0:
        raise ValueError(f"Delta dimensions {width}x{height} disagree with bitmap {image.size}")
    if data_size > len(raw) - 17 or height > (len(raw) - 17) // 4:
        raise ValueError("Delta row table or payload is truncated")
    row_table_end = 17 + height * 4
    payload = raw[row_table_end:row_table_end + data_size]
    if row_table_end + data_size != len(raw):
        raise ValueError("Delta resource contains unexpected trailing bytes")
    pixels = bytearray(image.tobytes())
    for y in range(height):
        row_offset = struct.unpack_from("<i", raw, 17 + y * 4)[0]
        if row_offset < 0 or row_offset >= len(payload):
            raise ValueError(f"Delta row {y} points outside its payload")
        cursor = row_offset
        x = 0
        skipping = True
        while x < width:
            if cursor >= len(payload):
                raise ValueError(f"Delta row {y} ended before its width")
            count = payload[cursor]
            cursor += 1
            if count > width - x:
                raise ValueError(f"Delta row {y} exceeds bitmap width")
            if not skipping:
                if count > len(payload) - cursor:
                    raise ValueError(f"Delta row {y} has truncated pixel data")
                for offset in range(count):
                    index = y * width + x + offset
                    pixels[index] = (pixels[index] + payload[cursor + offset]) & 0xFF
                cursor += count
            x += count
            skipping = not skipping
    updated = Image.frombytes("P", image.size, bytes(pixels))
    updated.putpalette(image.getpalette())
    image.paste(updated)


@lru_cache(maxsize=4)
def bilinear_palette_lut(palette: tuple[int, ...], key: int) -> tuple[int, ...]:
    table: list[int] = []
    candidate_indices = tuple(index for index in range(256) if index != key)
    for red5 in range(32):
        red = red5 * 8 + 4
        for green5 in range(32):
            green = green5 * 8 + 4
            for blue5 in range(32):
                blue = blue5 * 8 + 4
                table.append(min(
                    candidate_indices,
                    key=lambda index: (palette[index * 3] - red) ** 2
                    + (palette[index * 3 + 1] - green) ** 2
                    + (palette[index * 3 + 2] - blue) ** 2,
                ))
    return tuple(table)


def runtime_bilinear(source: Image.Image, scale: int, key: int) -> Image.Image:
    palette = build_hd_pack.palette_for(source)
    lut = bilinear_palette_lut(tuple(palette), key)
    pixels = source.tobytes()
    output_width, output_height = source.width * scale, source.height * scale
    output = bytearray([key]) * (output_width * output_height)
    subpixel_weights = ((5, -1), (7, -1), (7, 1), (5, 1))
    for y in range(source.height):
        for x in range(source.width):
            center_index = pixels[y * source.width + x]
            origin_x, origin_y = x * scale, y * scale
            if center_index == key:
                continue
            for sub_y, (center_y_weight, neighbor_y_direction) in enumerate(subpixel_weights):
                neighbor_y = max(0, min(source.height - 1, y + neighbor_y_direction))
                for sub_x, (center_x_weight, neighbor_x_direction) in enumerate(subpixel_weights):
                    neighbor_x = max(0, min(source.width - 1, x + neighbor_x_direction))
                    red_sum = green_sum = blue_sum = total = 0
                    for sample_y, y_weight in ((y, center_y_weight), (neighbor_y, 8 - center_y_weight)):
                        for sample_x, x_weight in ((x, center_x_weight), (neighbor_x, 8 - center_x_weight)):
                            sample_index = pixels[sample_y * source.width + sample_x]
                            if sample_index == key:
                                continue
                            weight = x_weight * y_weight
                            red_sum += palette[sample_index * 3] * weight
                            green_sum += palette[sample_index * 3 + 1] * weight
                            blue_sum += palette[sample_index * 3 + 2] * weight
                            total += weight
                    if total == 0:
                        result_index = center_index
                    else:
                        red = (red_sum + total // 2) // total
                        green = (green_sum + total // 2) // total
                        blue = (blue_sum + total // 2) // total
                        result_index = lut[(red >> 3) * 1024 + (green >> 3) * 32 + (blue >> 3)]
                        result_rgb = palette[result_index * 3:result_index * 3 + 3]
                        center_rgb = palette[center_index * 3:center_index * 3 + 3]
                        result_distance = sum((actual - chosen) ** 2 for actual, chosen in zip(
                            (red, green, blue), result_rgb))
                        center_distance = sum((actual - center) ** 2 for actual, center in zip(
                            (red, green, blue), center_rgb))
                        if center_distance <= result_distance:
                            result_index = center_index
                    output[(origin_y + sub_y) * output_width + origin_x + sub_x] = result_index
    result = Image.frombytes("P", (output_width, output_height), bytes(output))
    result.putpalette(source.getpalette())
    return result


def frame_key(frame: Image.Image) -> int:
    first_dib_pixel = frame.getpixel((0, frame.height - 1))
    return int(first_dib_pixel)


def preview(frame: Image.Image, key: int) -> Image.Image:
    return build_hd_pack.preview_rgb(frame, key)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--module", type=Path, required=True, help="ALSPRITE.DLL or EMSPRITE.DLL")
    parser.add_argument("--base-id", type=lambda text: int(text, 0), required=True, help="RT_BITMAP sequence base id")
    parser.add_argument("--out", type=Path, required=True, help="Ignored local output directory")
    parser.add_argument("--scale", type=int, choices=(4,), default=4)
    args = parser.parse_args()

    bitmaps = resource_entries(args.module, RESOURCE_BITMAP)
    deltas = resource_entries(args.module, RESOURCE_DELTA_FRAME)
    if args.base_id not in bitmaps:
        raise SystemExit(f"Bitmap {args.base_id} not found in {args.module}")
    frame_count, script_ids = sequence_references(args.module, args.base_id)
    source_bytes = bitmaps[args.base_id]
    image = bitmap_image(source_bytes)
    key = frame_key(image)
    frames = [image.copy()]
    for frame_id in range(1, frame_count):
        delta_id = args.base_id + frame_id
        raw = deltas.get(delta_id)
        if raw is None:
            raise ValueError(f"Sequence {args.base_id} is missing delta resource {delta_id}")
        decode_delta(raw, image)
        frames.append(image.copy())

    output = args.out / f"{args.module.stem}-{args.base_id}"
    output.mkdir(parents=True, exist_ok=True)
    bilinear_dir = output / "bilinear"
    bilinear_dir.mkdir(exist_ok=True)
    selected_indices = sorted({0, 1, frame_count // 4, frame_count // 2, (3 * frame_count) // 4, frame_count - 1})
    selected_indices = [index for index in selected_indices if 0 <= index < frame_count]
    panel_w, panel_h, label_h = image.width * args.scale, image.height * args.scale, 24
    montage = Image.new("RGB", (panel_w * 2, (panel_h + label_h) * len(selected_indices)), (25, 25, 25))
    draw = ImageDraw.Draw(montage)
    source_apng: list[Image.Image] = []
    bilinear_apng: list[Image.Image] = []
    key_mask_checks = 0
    for frame_index, frame in enumerate(frames):
        frame_path = output / "source" / f"frame-{frame_index:03}.png"
        frame_path.parent.mkdir(exist_ok=True)
        frame.save(frame_path)
        nearest = frame.resize((panel_w, panel_h), Image.Resampling.NEAREST)
        nearest.putpalette(frame.getpalette())
        enlarged = runtime_bilinear(frame, args.scale, key)
        if enlarged.getpalette() != frame.getpalette():
            raise ValueError(f"Filtered palette changed in frame {frame_index}")
        enlarged.save(bilinear_dir / f"frame-{frame_index:03}.png")
        source_pixels, upscaled_pixels = frame.tobytes(), enlarged.tobytes()
        for y in range(frame.height):
            for x in range(frame.width):
                original_index = source_pixels[y * frame.width + x]
                start_x, start_y = x * args.scale, y * args.scale
                block = [upscaled_pixels[(start_y + dy) * panel_w + start_x + dx]
                         for dy in range(args.scale) for dx in range(args.scale)]
                if original_index == key and any(value != key for value in block):
                    raise ValueError(f"Bilinear filter changed source key mask at frame {frame_index}, ({x},{y})")
                if original_index != key and any(value == key for value in block):
                    raise ValueError(f"Bilinear filter introduced key index at frame {frame_index}, ({x},{y})")
                key_mask_checks += 1
        source_rgba = frame.convert("RGBA")
        source_rgba.putalpha(frame.point(lambda index: 0 if index == key else 255, mode="L"))
        bilinear_rgba = enlarged.convert("RGBA")
        bilinear_rgba.putalpha(enlarged.point(lambda index: 0 if index == key else 255, mode="L"))
        source_apng.append(source_rgba)
        bilinear_apng.append(bilinear_rgba)
        if frame_index not in selected_indices:
            continue
        row = selected_indices.index(frame_index)
        for column, (pane, label) in enumerate(((nearest, f"nearest 4x — frame {frame_index:02d}"),
                                                (enlarged, f"runtime bilinear 4x — frame {frame_index:02d}"))):
            x, y = column * panel_w, row * (panel_h + label_h)
            montage.paste(preview(pane, key), (x, y + label_h))
            draw.text((x + 5, y + 4), label, fill=(255, 255, 255))
    montage_path = output / "method-comparison.png"
    montage.save(montage_path)
    source_apng_path = output / "source-animation.png"
    bilinear_apng_path = output / "bilinear-animation.png"
    source_apng[0].save(source_apng_path, save_all=True, append_images=source_apng[1:], duration=67,
                        loop=0, disposal=2, optimize=False)
    bilinear_apng[0].save(bilinear_apng_path, save_all=True, append_images=bilinear_apng[1:], duration=67,
                          loop=0, disposal=2, optimize=False)
    metadata = {
        "module": args.module.name,
        "module_sha256": hashlib.sha256(args.module.read_bytes()).hexdigest(),
        "resource_type": RESOURCE_BITMAP,
        "resource_id": args.base_id,
        "source_sha256": hashlib.sha256(source_bytes).hexdigest(),
        "logical_size": list(frames[0].size),
        "frame_count": frame_count,
        "sequence_script_ids": script_ids,
        "delta_resource_ids": [args.base_id + frame for frame in range(1, frame_count)],
        "delta_routing": "script RCDATA word[3] frame count and word[4] base bitmap; type-302 deltas are applied in order to one cumulative indexed image",
        "key_index": key,
        "key_rule": "cached from source DIB bits[0] (bottom-left pixel for positive-height RT_BITMAP); held constant across cumulative delta frames",
        "scale": args.scale,
        "method": "runtime-compatible fixed-palette bilinear: 4x subpixel weights (5,3)/(7,1)/(7,1)/(5,3), keyed neighbors discarded and weights renormalized, then 5-bit RGB palette LUT",
        "palette_rule": "original 256-entry source palette retained; preserve source center index when it is at least as close to interpolated RGB as the LUT-selected index",
        "key_mask_rule": "each source key cell stays fully key; no non-key source cell contains key index in its 4x output block",
        "key_mask_checks": key_mask_checks,
        "review_status": "pending_visual_review",
        "comparison_frames": selected_indices,
        "comparison_png": str(montage_path),
        "source_apng": str(source_apng_path),
        "bilinear_apng": str(bilinear_apng_path),
        "source_frames": str(output / "source"),
        "bilinear_frame_directory": str(bilinear_dir),
    }
    (output / "sequence-manifest.json").write_text(json.dumps(metadata, indent=2) + "\n")
    print(f"Decoded {frame_count} full frames for {args.module.name}:{args.base_id}; comparison: {montage_path}")


if __name__ == "__main__":
    main()
