#!/usr/bin/env -S uv run
"""Extract encyclopedia artwork and build local, palette-compatible 4x game icons."""
# /// script
# requires-python = ">=3.11,<3.14"
# dependencies = ["rembg[cpu]==2.0.76", "pillow>=11", "pefile>=2023.2.7", "opencv-python-headless>=4.10"]
# ///
from __future__ import annotations

import argparse
import hashlib
import io
import importlib.metadata
import json
import struct
import subprocess
import sys
from collections.abc import Callable
from pathlib import Path

import cv2
import numpy as np
import pefile
from PIL import Image, ImageChops, ImageDraw, ImageEnhance

from build_hd_pack import bitmap_file, entries_for, fnv1a64, merge_asset_records, palette_for, preview_rgb


MODEL = "isnet-general-use"
MODEL_SHA256 = "60920e99c45464f2ba57bee2ad08c919a52bbf852739e96947fbb4358c0d964a"
REVISION = 4
SCALE = 4
RECIPE_PATH = Path(__file__).with_name("encyclopedia_icon_recipes.json")
FACILITY_BRIGHTNESS_THRESHOLD = 80.0
FACILITY_BRIGHTNESS_GAIN = 1.10
FACILITY_ROWS = {0x2329: 1, 0x2346: 2, 0x232e: 3, 0x2336: 4,
                 0x233e: 5, 0x2332: 6, 0x233a: 7, 0x2342: 8}
FACILITY_TABS = {0x2850: 1, 0x2853: 2, 0x2856: 3, 0x2859: 4, 0x285c: 5}
DEFENSE_TABS = {0x2936: 10, 0x2939: 11, 0x293c: 36, 0x293f: 38,
                0x2943: 15, 0x2946: 20, 0x294a: 94, 0x294d: 116}
FLEET_TABS = {0x28a9: 47, 0x28aa: 36, 0x28ab: 15, 0x28ac: 94,
              0x28db: 62, 0x28dc: 38, 0x28dd: 20, 0x28de: 116}
EXTRA_GOKRES_PICTURES = {0x850: "EDATA.192", 0x4850: "EDATA.192"}


def fingerprint(value: object) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def apply_mask_recipe(mask: Image.Image, recipe: dict, source_sha256: str) -> Image.Image:
    if recipe.get("source_sha256") != source_sha256 or recipe.get("size") != list(mask.size):
        raise ValueError(f"Artwork differs from the reviewed recipe for {recipe.get('label')}; "
                         "review the source and update the recipe or supply a local mask override")
    result = mask.copy()
    for operation in recipe["operations"]:
        shape = Image.new("L", mask.size)
        ImageDraw.Draw(shape).polygon([tuple(point) for point in operation["points"]], fill=255)
        match operation["operation"]:
            case "add":
                result = ImageChops.lighter(result, shape)
            case "subtract":
                result = ImageChops.subtract(result, shape)
            case "intersect":
                result = ImageChops.darker(result, shape)
            case "replace":
                result = shape
            case unknown:
                raise ValueError(f"Unknown mask recipe operation: {unknown}")
    return result


def cached_mask(source: Image.Image, source_sha256: str, kind: str, recipe: dict | None,
                directory: Path, environment: str, force: bool,
                predict: Callable[[Image.Image], Image.Image]) -> Image.Image:
    raw_signature = fingerprint([MODEL, MODEL_SHA256, environment, source_sha256])
    signature = fingerprint([REVISION, raw_signature, recipe])
    mask_file, stamp_file = directory / "mask.png", directory / "mask.signature"
    if not force and mask_file.exists() and stamp_file.exists() and stamp_file.read_text() == signature:
        return Image.open(mask_file).convert("L")
    raw_file, raw_stamp = directory / "raw-mask.png", directory / "raw-mask.signature"
    if not force and raw_file.exists() and raw_stamp.exists() and raw_stamp.read_text() == raw_signature:
        raw_mask = Image.open(raw_file).convert("L")
    else:
        raw_mask = predict(source)
        raw_mask.save(raw_file)
        raw_stamp.write_text(raw_signature)
    try:
        mask = clean_mask(raw_mask, kind)
    except ValueError:
        mask = Image.new("L", source.size)
    if kind == "planets":
        mask = refine_planet_mask(mask)
    if recipe:
        mask = apply_mask_recipe(mask, recipe, source_sha256)
    mask.save(mask_file)
    stamp_file.write_text(signature)
    return mask


def find_file(directory: Path, name: str) -> Path:
    return next(path for path in directory.iterdir() if path.name.lower() == name.lower())


def strings(module: Path) -> dict[int, str]:
    result: dict[int, str] = {}
    with pefile.PE(str(module), fast_load=True) as pe:
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
        for kind in pe.DIRECTORY_ENTRY_RESOURCE.entries:
            if kind.id != 6:
                continue
            for block in kind.directory.entries:
                data = block.directory.entries[0].data.struct
                raw = pe.get_data(data.OffsetToData, data.Size)
                offset = 0
                for index in range(16):
                    length = struct.unpack_from("<H", raw, offset)[0]
                    offset += 2
                    value = raw[offset:offset + length * 2].decode("utf-16-le")
                    offset += length * 2
                    if value:
                        result[(block.id - 1) * 16 + index] = value
    return result


def category(number: int) -> str:
    for last, name in [(14, "facilities"), (33, "troops"), (41, "fighters"),
                       (71, "ships"), (131, "characters"), (164, "missions"), (191, "planets")]:
        if number <= last:
            return name
    return "characters"


def clean_mask(mask: Image.Image, kind: str) -> Image.Image:
    pixels = np.asarray(mask).copy()
    if kind == "characters":
        pixels[:, :int(mask.width * 0.30)] = 0
    binary = (pixels >= 96).astype(np.uint8)
    count, labels, stats, _ = cv2.connectedComponentsWithStats(binary, 8)
    if count <= 1:
        raise ValueError("background removal found no foreground")
    largest = 1 + int(np.argmax(stats[1:, cv2.CC_STAT_AREA]))
    keep = [largest]
    if kind in {"facilities", "troops", "missions", "planets"}:
        minimum = max(12, stats[largest, cv2.CC_STAT_AREA] * 0.03)
        keep = [index for index in range(1, count) if stats[index, cv2.CC_STAT_AREA] >= minimum]
    return Image.fromarray(np.where(np.isin(labels, keep), 255, 0).astype(np.uint8))


def refine_planet_mask(mask: Image.Image) -> Image.Image:
    pixels = np.asarray(mask)
    contours, _ = cv2.findContours(pixels, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_NONE)
    if not contours:
        return mask
    contour = max(contours, key=cv2.contourArea)
    if len(contour) < 5:
        return mask
    (center_x, center_y), (width, height), angle = cv2.fitEllipse(contour)
    radius_x, radius_y = width / 2, height / 2
    if min(radius_x, radius_y) <= 0 or min(width, height) / max(width, height) < 0.85:
        return mask
    y, x = np.indices(pixels.shape)
    radians = np.deg2rad(angle)
    u = (x - center_x) * np.cos(radians) + (y - center_y) * np.sin(radians)
    v = -(x - center_x) * np.sin(radians) + (y - center_y) * np.cos(radians)
    ellipse = (u / radius_x) ** 2 + (v / radius_y) ** 2 <= 1
    overlap = np.count_nonzero(ellipse & (pixels > 0)) / np.count_nonzero(ellipse | (pixels > 0))
    if overlap < 0.94:
        return mask
    inset = min(radius_x, radius_y) * 0.075
    radius_x -= inset
    radius_y -= inset
    distance = np.sqrt((u / radius_x) ** 2 + (v / radius_y) ** 2)
    coverage = np.clip(0.5 + (1 - distance) * min(radius_x, radius_y), 0, 1)
    return Image.fromarray(np.round(coverage * 255).astype(np.uint8))


def portrait_crop(cutout: Image.Image) -> tuple[int, int, int, int]:
    alpha = np.asarray(cutout.getchannel("A"))
    box = cutout.getbbox()
    assert box is not None
    left, top, right, bottom = box
    upper = alpha[top:top + max(1, int((bottom - top) * 0.60))]
    ys, xs = np.nonzero(upper)
    center = (int(xs.min()) + int(xs.max())) / 2
    size = max(int(xs.max()) - int(xs.min()) + 1, int((bottom - top) * 0.82))
    size = int(size * 1.05)
    return (round(center - size / 2), max(0, top - 2), round(center + size / 2), min(cutout.height, top + size - 2))


def brighten_dark_facility(cutout: Image.Image, crop: list[int] | None) -> tuple[Image.Image, dict]:
    visible = cutout.crop(tuple(crop) if crop else cutout.getbbox())
    pixels = np.asarray(visible, dtype=np.float64)
    weights = pixels[:, :, 3] / 255
    coverage = float(weights.sum())
    mean = None
    if coverage:
        luma = (pixels[:, :, :3] @ np.array([2126, 7152, 722])) / 10000
        mean = float((luma * weights).sum() / coverage)
    gain = FACILITY_BRIGHTNESS_GAIN if mean is not None and mean < FACILITY_BRIGHTNESS_THRESHOLD else 1.0
    result = cutout
    if gain != 1.0:
        result = ImageEnhance.Brightness(cutout.convert("RGB")).enhance(gain).convert("RGBA")
        result.putalpha(cutout.getchannel("A"))
    return result, {"mean_visible_luma": mean, "threshold": FACILITY_BRIGHTNESS_THRESHOLD, "gain": gain}


def fit_icon(cutout: Image.Image, source: Image.Image, key: int, kind: str,
             crop: list[int] | None, bounds: tuple[int, int, int, int] | None = None) -> Image.Image:
    if crop:
        cutout = cutout.crop(tuple(crop))
    elif kind == "characters":
        cutout = cutout.crop(portrait_crop(cutout))
    else:
        cutout = cutout.crop(cutout.getbbox())
    if bounds is None:
        original_mask = source.point(lambda index: 0 if index == key else 255, mode="L")
        bounds = original_mask.getbbox()
    if bounds is None:
        raise ValueError("original icon is entirely transparent")
    left, top, right, bottom = (value * SCALE for value in bounds)
    available = (right - left, bottom - top)
    ratio = min(available[0] / cutout.width, available[1] / cutout.height)
    size = (max(1, round(cutout.width * ratio)), max(1, round(cutout.height * ratio)))
    scaled = cutout.convert("RGBa").resize(size, Image.Resampling.LANCZOS).convert("RGBA")
    canvas = Image.new("RGBA", (source.width * SCALE, source.height * SCALE))
    canvas.paste(scaled, (left + (available[0] - size[0]) // 2, top + (available[1] - size[1]) // 2))
    return canvas


def quantize_icon(rgba: Image.Image, palette: list[int], key: int) -> Image.Image:
    colors = np.asarray(palette, dtype=np.int32).reshape(256, 3)
    allowed = np.array([index for index in range(256) if index != key])
    rgb = np.asarray(rgba.convert("RGB"), dtype=np.int32).reshape(-1, 3)
    unique, inverse = np.unique(rgb, axis=0, return_inverse=True)
    nearest = np.empty(len(unique), dtype=np.uint8)
    for start in range(0, len(unique), 2048):
        distance = ((unique[start:start + 2048, None] - colors[allowed]) ** 2).sum(axis=2)
        nearest[start:start + 2048] = allowed[distance.argmin(axis=1)]
    result = nearest[inverse].reshape(rgba.height, rgba.width)
    result[np.asarray(rgba.getchannel("A")) < 128] = key
    indexed = Image.fromarray(result).convert("P")
    indexed.putpalette(palette)
    return indexed


def facility_targets() -> dict[int, tuple[str, int, int]]:
    return {base + state: (layout, state, number)
            for layout, bases, states in (("row", FACILITY_ROWS, 3), ("tab", FACILITY_TABS, 3))
            for base, number in bases.items() for state in range(states)}


def forces_tab_targets() -> dict[int, tuple[str, int, int]]:
    targets = {base + state: ("defense", state, number)
               for base, number in DEFENSE_TABS.items() for state in range(3)}
    for base, number in FLEET_TABS.items():
        for offset, state in ((0, 0), (7, 1)):
            targets[base + offset] = ("fleet", state, number)
        if base not in (0x28a9, 0x28db):
            targets[base + 3] = ("fleet", 2, number)
    return targets


def forces_tab_icon(cutout: Image.Image, source: Image.Image, key: int, kind: str,
                    crop: list[int] | None, layout: str, state: int) -> Image.Image:
    background = source.copy()
    if layout == "defense":
        if source.size != (36, 33):
            raise ValueError("Unexpected planetary defense tab dimensions")
        bottom = 31 if state == 1 else 29
        bounds = (5, 5, 31, bottom)
        color = 183 if kind == "facilities" else 131 if kind == "characters" else 0
        draw = ImageDraw.Draw(background)
        draw.rectangle((5, 5, 30, bottom - 1), fill=color)
        if state == 2:
            for y in range(5, 29):
                for x in range(5, 31):
                    if (x + y) % 2 == 0:
                        background.putpixel((x, y), 60)
    else:
        if source.size not in ((30, 29), (31, 29)):
            raise ValueError("Unexpected fleet tab dimensions")
        bounds = (2, 2, 29, 27)
        frame = source.getpixel((3, 0))
        for y in range(27):
            for x in range(source.width):
                if source.getpixel((x, y)) != frame:
                    background.putpixel((x, y), 0)
        ImageDraw.Draw(background).line(((3, 0), (27, 0), (28, 1), (29, 2), (29, 26)), fill=frame)
    if state == 2:
        alpha = cutout.getchannel("A")
        cutout = ImageEnhance.Brightness(cutout.convert("L")).enhance(0.55).convert("RGBA")
        cutout.putalpha(alpha)
    rgba = fit_icon(cutout, source, key, kind, crop, bounds)
    indexed = quantize_icon(rgba, palette_for(source), key)
    result = background.resize(indexed.size, Image.Resampling.NEAREST)
    result.paste(indexed, mask=rgba.getchannel("A").point(lambda alpha: 255 if alpha >= 128 else 0))
    return result


def facility_icon(cutout: Image.Image, source: Image.Image, key: int, crop: list[int] | None,
                  layout: str, state: int) -> Image.Image:
    palette = palette_for(source)
    if layout == "tab":
        if source.size != (36, 33):
            raise ValueError("Unexpected facility tab dimensions")
        background = source.copy()
        ImageDraw.Draw(background).rectangle((5, 5, 30, 30 if state == 0 else 28),
                                             fill=64 if state == 2 else 183)
        bounds = (5, 5, 31, 29)
        if state == 2:
            alpha = cutout.getchannel("A")
            cutout = ImageEnhance.Brightness(cutout.convert("L")).enhance(0.55).convert("RGBA")
            cutout.putalpha(alpha)
    else:
        if source.size != (67, 35):
            raise ValueError("Unexpected facility row dimensions")
        background = Image.new("P", source.size, key)
        background.putpalette(palette)
        bounds = (2, 2, 65, 33)
        if state in (1, 2):
            draw = ImageDraw.Draw(background)
            draw.rectangle((1, 1, 65, 33), fill=0)
            color = 2 if state == 1 else 249
            for x in range(1, 66, 8):
                draw.line((x, 1, x, 33), fill=color)
            for y in range(1, 34, 8):
                draw.line((1, y, 65, y), fill=color)
    rgba = fit_icon(cutout, source, key, "facilities", crop, bounds)
    indexed = quantize_icon(rgba, palette, key)
    result = background.resize(indexed.size, Image.Resampling.NEAREST)
    result.paste(indexed, mask=rgba.getchannel("A").point(lambda alpha: 255 if alpha >= 128 else 0))
    return result


def save_contacts(items: list[tuple[Image.Image, str]], directory: Path) -> None:
    for page, start in enumerate(range(0, len(items), 24), 1):
        group = items[start:start + 24]
        sheet = Image.new("RGB", (1200, ((len(group) + 3) // 4) * 160), (38, 47, 55))
        draw = ImageDraw.Draw(sheet)
        for index, (im, label) in enumerate(group):
            im = im.copy()
            im.thumbnail((290, 132), Image.Resampling.LANCZOS)
            x, y = index % 4 * 300, index // 4 * 160
            if im.mode == "RGBA":
                sheet.paste(im, (x + (300 - im.width) // 2, y), im)
            else:
                sheet.paste(im, (x + (300 - im.width) // 2, y))
            draw.text((x + 5, y + 137), label[:46], fill="white")
        sheet.save(directory / f"contact-{page:02}.png")


def build(args: argparse.Namespace) -> None:
    from rembg import new_session, remove
    from rembg.sessions.dis_general_use import DisSession

    args.out.mkdir(parents=True, exist_ok=True)
    args.cache_dir.mkdir(parents=True, exist_ok=True)
    if not args.no_overrides and args.overrides is None and (args.cache_dir / "overrides.json").exists():
        args.overrides = args.cache_dir / "overrides.json"
    overrides = json.loads(args.overrides.read_text()) if args.overrides else {}
    recipe_document = json.loads(RECIPE_PATH.read_text())
    if recipe_document["schema"] != 1:
        raise ValueError("Unsupported encyclopedia recipe schema")
    recipes = recipe_document["assets"]
    versions = {name: importlib.metadata.version(name) for name in
                ("rembg", "pillow", "pefile", "numpy", "opencv-python-headless", "onnxruntime")}
    lock_digest = hashlib.sha256(Path(__file__ + ".lock").read_bytes()).hexdigest()
    environment = fingerprint([versions, lock_digest])
    file_strings = strings(find_file(args.install_dir, "ENCYBMAP.DLL"))
    names = strings(find_file(args.install_dir, "TEXTSTRA.DLL"))
    edata = find_file(args.install_dir, "EData")
    aliases: dict[str, list[int]] = {}
    for resource, filename in file_strings.items():
        aliases.setdefault(Path(filename.replace("\\", "/")).name.upper(), []).append(resource)
    session = None

    def predict(source: Image.Image) -> Image.Image:
        nonlocal session
        if session is None:
            model_path = Path(DisSession.download_models())
            if hashlib.sha256(model_path.read_bytes()).hexdigest() != MODEL_SHA256:
                raise ValueError(f"Background-removal model checksum mismatch: {model_path}")
            session = new_session(MODEL, providers=["CPUExecutionProvider"])
        return remove(source, session=session, only_mask=True)

    inventory: list[dict] = []
    cutouts: dict[str, Image.Image] = {}
    contacts: list[tuple[Image.Image, str]] = []
    for path in sorted(edata.iterdir()):
        if not path.name.upper().startswith("EDATA."):
            continue
        number = int(path.suffix[1:])
        kind = category(number)
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        asset_dir = args.cache_dir / path.name.upper()
        asset_dir.mkdir(exist_ok=True)
        source = Image.open(path).convert("RGB")
        source.save(asset_dir / "source.png")
        override = overrides.get(path.name.upper(), {})
        recipe = recipes.get(path.name.upper()) if not override.get("mask") else None
        mask_file = asset_dir / "mask.png"
        if override.get("mask"):
            mask = Image.open(args.overrides.parent / override["mask"]).convert("L")
            if mask.size != source.size:
                raise ValueError(f"Override mask size mismatch for {path.name}")
        else:
            mask = cached_mask(source, digest, kind, recipe, asset_dir, environment, args.force, predict)
        cutout = source.convert("RGBA")
        cutout.putalpha(mask)
        brightness = None
        if kind == "facilities":
            crop = override.get("crop", (recipe or {}).get("crop"))
            cutout, brightness = brighten_dark_facility(cutout, crop)
        cutout.save(asset_dir / "cutout.png")
        cutouts[path.name.upper()] = cutout
        ids = aliases.get(path.name.upper(), [])
        label = ("Luke Skywalker (Jedi)" if path.name.upper() == "EDATA.192" else
                 next((names.get((value & 4095) + 8192) for value in ids if names.get((value & 4095) + 8192)), path.name))
        inventory.append({"file": path.name.upper(), "category": kind, "label": label,
                          "encyclopedia_ids": ids, "sha256": digest, "size": list(source.size),
                          "override": override, "mask": str((args.overrides.parent / override["mask"]).resolve())
                          if override.get("mask") else str(mask_file.resolve()),
                          "mask_sha256": hashlib.sha256(mask.tobytes()).hexdigest(),
                          "recipe": recipe,
                          "brightness": brightness,
                          "status": "ready" if mask.getbbox() else "no foreground; original icon retained"})
        contacts.append((cutout, f"{path.name} {label}"))
        print(f"Extracted {path.name}: {label}", flush=True)
    save_contacts(contacts, args.cache_dir)
    (args.cache_dir / "inventory.json").write_text(json.dumps(inventory, indent=2) + "\n")
    by_file = {item["file"]: item for item in inventory}
    targets: dict[str, dict[int, str]] = {"GOKRES.DLL": {}, "STRATEGY.DLL": {}}
    for filename, ids in aliases.items():
        if filename not in by_file or by_file[filename]["category"] == "planets":
            continue
        for resource in ids:
            for target in (resource - 4096, resource + 12288):
                targets["GOKRES.DLL"][target] = filename
    targets["GOKRES.DLL"].update(EXTRA_GOKRES_PICTURES)
    for kind in range(1, 27):
        resource = 0x27e3 + kind if kind <= 23 else {24: 0x27ff, 25: 0x27fd, 26: 0x27fe}[kind]
        number = 165 + kind if kind <= 23 else {24: 191, 25: 189, 26: 190}[kind]
        targets["STRATEGY.DLL"][resource] = f"EDATA.{number:03}"
    facilities = facility_targets()
    forces_tabs = forces_tab_targets()
    targets["STRATEGY.DLL"].update({resource: f"EDATA.{number:03}"
                                  for resource, (_, _, number) in facilities.items()})
    targets["STRATEGY.DLL"].update({resource: f"EDATA.{number:03}"
                                  for resource, (_, _, number) in forces_tabs.items()})
    records: list[dict] = []
    skipped: set[str] = set()
    previews: list[tuple[Image.Image, str]] = []
    review = ["# module_filename\tresource_id\tcategory\tlabel"]
    for module_name, mapping in targets.items():
        module = find_file(args.install_dir, module_name)
        for resource, language, dib in entries_for(module, set(mapping)):
            filename = mapping[resource]
            item = by_file[filename]
            source = Image.open(io.BytesIO(bitmap_file(dib)))
            source.load()
            palette = palette_for(source)
            signed_height = struct.unpack_from("<i", dib, 8)[0]
            key = source.getpixel((0, source.height - 1 if signed_height > 0 else 0))
            override = overrides.get(filename, {})
            if override.get("skip") or not cutouts[filename].getbbox():
                skipped.add(f"{fnv1a64(dib):016x}.rhd")
                continue
            crop = override.get("crop", (item.get("recipe") or {}).get("crop"))
            facility = facilities.get(resource) if module_name == "STRATEGY.DLL" else None
            forces_tab = forces_tabs.get(resource) if module_name == "STRATEGY.DLL" else None
            if forces_tab:
                layout, state, _ = forces_tab
                indexed = forces_tab_icon(cutouts[filename], source, key, item["category"], crop, layout, state)
            elif facility:
                layout, state, _ = facility
                indexed = facility_icon(cutouts[filename], source, key, crop, layout, state)
            else:
                rgba = fit_icon(cutouts[filename], source, key, item["category"], crop)
                indexed = quantize_icon(rgba, palette, key)
            source_fnv = fnv1a64(dib)
            pack_file = f"{source_fnv:016x}.rhd"
            payload = indexed.tobytes()
            (args.out / pack_file).write_bytes(b"REBHD02\0" + struct.pack("<IIIIQ", source.width, source.height,
                                                                       SCALE, len(payload), source_fnv) + payload)
            asset_dir = args.out / f"{module.stem.lower()}-{resource}-{language}"
            asset_dir.mkdir(exist_ok=True)
            indexed.save(asset_dir / "accepted-indexed.png")
            source.save(asset_dir / "source-indexed.png")
            preview = preview_rgb(indexed, key)
            preview.save(asset_dir / "preview.png")
            records.append({"module": module.name, "resource_id": resource, "language_id": language,
                            "pack_file": pack_file, "source_sha256": hashlib.sha256(dib).hexdigest(),
                            "source_fnv1a64": f"{source_fnv:016x}", "logical_size": list(source.size),
                            "scale": SCALE, "key_index": key, "format": "REBHD02",
                            "method": "encyclopedia-cutout", "encyclopedia": item,
                            "facility_layout": facility,
                            "forces_tab_layout": forces_tab,
                            "indexed_png": str((asset_dir / "accepted-indexed.png").resolve())})
            label = f"{resource:04x} {item['label']}"
            previews.append((preview, label))
            review.append(f"{module.name}\t{resource}\t{item['category']}\t{item['label']}")
    manifest_path = args.out / "manifest.json"
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {"schema": "rebellion-hd-indexed-pack-v1"}
    previous = manifest.get("assets", [])
    owned = {item["pack_file"] for item in previous if item.get("method") == "encyclopedia-cutout"}
    removed = skipped & owned
    retired_transit = {base + 3 for base in FACILITY_ROWS}
    removed.update(item["pack_file"] for item in previous
                   if item.get("method") == "encyclopedia-cutout"
                   and item.get("module", "").upper() == "STRATEGY.DLL"
                   and item.get("resource_id") in retired_transit)
    for filename in removed:
        (args.out / filename).unlink(missing_ok=True)
    manifest["assets"] = merge_asset_records([item for item in previous if item["pack_file"] not in removed], records)
    manifest["encyclopedia_extraction"] = {"revision": REVISION, "model": MODEL, "source_count": len(inventory),
                                           "model_sha256": MODEL_SHA256, "dependencies": versions,
                                           "dependency_lock_sha256": lock_digest,
                                           "recipes_sha256": fingerprint(recipe_document),
                                           "icon_count": len(records), "cache": str(args.cache_dir.resolve())}
    (args.out / "targets.tsv").write_text("\n".join(review) + "\n")
    preview_dir = args.cache_dir / "icons"
    preview_dir.mkdir(exist_ok=True)
    save_contacts(previews, preview_dir)
    used = {record["encyclopedia"]["file"] for record in records}
    manifest["encyclopedia_extraction"]["unmapped_sources"] = [item["file"] for item in inventory if item["file"] not in used]
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Extracted {len(inventory)} pictures; wrote {len(records)} icons to {args.out}", flush=True)
    for script in ("build_transit_backdrops.py", "build_headquarters_icons.py"):
        subprocess.run(["uv", "run", "--locked", "--python", sys.executable,
                        str(Path(__file__).with_name(script)),
                        "--install-dir", str(args.install_dir), "--out", str(args.out)], check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=Path("work/hd/gameplay-pack"))
    parser.add_argument("--cache-dir", type=Path, default=Path("work/hd/encyclopedia"))
    override_options = parser.add_mutually_exclusive_group()
    override_options.add_argument("--overrides", type=Path, help="local JSON keyed by EDATA filename: mask path, crop [l,t,r,b], skip")
    override_options.add_argument("--no-overrides", action="store_true", help="use only repository recipes, ignoring local overrides")
    parser.add_argument("--force", action="store_true", help="regenerate cached masks")
    build(parser.parse_args())


if __name__ == "__main__":
    main()
