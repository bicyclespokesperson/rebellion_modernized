#!/usr/bin/env -S uv run
"""Render the tactical battle sprite pack from TACTICAL.DLL ship models, procedural recipes and remodelled classes."""
# /// script
# requires-python = ">=3.11,<3.14"
# dependencies = ["pefile>=2023.2.7", "pillow>=11", "numpy>=2"]
# ///
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import shutil
import struct
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np
import pefile
from PIL import Image, ImageDraw

REVISION = 1
TOOLS = Path(__file__).resolve().parent
BPY_SCRIPT = TOOLS / "tactical_sprites_bpy.py"
RECIPE_DIR = TOOLS / "tactical_model_recipes"
MESH_TYPE = 301
IMAGE_TYPE = 303
PALETTE_BITMAP = 1000
METRES_PER_MESH_UNIT = 800.0
SMALL_FACTOR = 4
PITCH_DEGREES = 50.0
MAX_ATLAS = 2048


@dataclass(frozen=True)
class Kind:
    headings: int
    metres_per_pixel: float


CAPITAL = Kind(64, 8.0)
FIGHTER = Kind(32, 2.0)
STATION = Kind(16, 16.0)

# Tactical ship-type index order (src/game/app/battle_handoff.cpp). Capital ship i uses mesh 2010 + 10 i
# (Alliance, 0-14) or 2510 + 10 (i - 15) (Empire, 15-28); LOD1 and LOD2 are the next two ids.
CAPITAL_SHIPS = [
    ("mon_calamari_cruiser", "Mon Calamari Cruiser"), ("bulk_cruiser", "Bulk Cruiser"),
    ("assault_frigate", "Assault Frigate"), ("nebulon_b_frigate", "Nebulon-B Frigate"),
    ("alliance_escort_carrier", "Alliance Escort Carrier"), ("corellian_corvette", "Corellian Corvette"),
    ("medium_transport", "Medium Transport"), ("bulk_transport", "Bulk Transport"),
    ("corellian_gunship", "Corellian Gunship"), ("alliance_dreadnaught", "Alliance Dreadnaught"),
    ("cc7700_frigate", "CC-7700 Frigate"), ("cc9600_frigate", "CC-9600 Frigate"),
    ("bulwark_battlecruiser", "Bulwark Battlecruiser"), ("liberator_cruiser", "Liberator Cruiser"),
    ("dauntless_cruiser", "Dauntless Cruiser"), ("strike_cruiser", "Strike Cruiser"),
    ("lancer_frigate", "Lancer Frigate"), ("interdictor_cruiser", "Interdictor Cruiser"),
    ("carrack_light_cruiser", "Carrack Light Cruiser"), ("victory_destroyer", "Victory Destroyer"),
    ("imperial_star_destroyer", "Imperial Star Destroyer"), ("super_star_destroyer", "Super Star Destroyer"),
    ("assault_transport", "Assault Transport"), ("galleon", "Galleon"),
    ("victory_ii_star_destroyer", "Victory II Star Destroyer"),
    ("imperial_ii_star_destroyer", "Imperial II Star Destroyer"), ("star_galleon", "Star Galleon"),
    ("imperial_escort_carrier", "Imperial Escort Carrier"), ("imperial_dreadnaught", "Imperial Dreadnaught"),
]
# Fighters have no meshes: procedural proxies whose largest dimension is the canonical one, drawn 8x (see the bpy
# script header). The B-wing's is its upright wing, the TIE Fighter's its wing height.
FIGHTERS = [
    ("xwing", "X-wing", 12.5), ("ywing", "Y-wing", 16.0), ("awing", "A-wing", 9.6), ("bwing", "B-wing", 16.9),
    ("tie_fighter", "TIE Fighter", 7.5), ("tie_interceptor", "TIE Interceptor", 9.6),
    ("tie_bomber", "TIE Bomber", 7.8), ("tie_defender", "TIE Defender", 9.2),
]
FIGHTER_SCALE = 8.0
DEATH_STAR = (37, "death_star", "Death Star", 9600.0)


@dataclass
class ClassSpec:
    type_index: int
    slug: str
    name: str
    kind: Kind
    size_m: float
    fit: str
    source: dict
    hashes: dict[str, str] = field(default_factory=dict)
    derived: dict[str, str] = field(default_factory=dict)

    @property
    def key(self) -> str:
        return f"{self.type_index:02d}_{self.slug}"


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fingerprint(value: object) -> str:
    return sha256(json.dumps(value, sort_keys=True).encode())


def find_file(directory: Path, name: str) -> Path:
    matches = [path for path in directory.iterdir() if path.name.lower() == name.lower()]
    if not matches:
        raise SystemExit(f"{name} not found in {directory}")
    return matches[0]


class TacticalResources:
    def __init__(self, path: Path) -> None:
        self.dll_sha256 = sha256(path.read_bytes())
        pe = pefile.PE(str(path), fast_load=True)
        pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
        self.entries: dict[tuple[int, str], bytes] = {}
        for type_entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
            for name_entry in type_entry.directory.entries:
                name = str(name_entry.name).upper() if name_entry.name is not None else str(name_entry.id)
                data = name_entry.directory.entries[0].data.struct
                self.entries[(type_entry.id, name)] = pe.get_data(data.OffsetToData, data.Size)

    def get(self, type_id: int, name: str | int) -> bytes:
        return self.entries[(type_id, str(name).upper())]

    def palette(self) -> bytes:
        bitmap = self.get(pefile.RESOURCE_TYPE["RT_BITMAP"], PALETTE_BITMAP)
        header = struct.unpack_from("<I", bitmap, 0)[0]
        return bytes(channel for i in range(256) for channel in bitmap[header + 4 * i:header + 4 * i + 3][::-1])

    def image(self, name: str) -> Image.Image:
        # Type 303: u32 width, u32 height, then (count, index) pairs where a count of 0 means 256.
        data = self.get(IMAGE_TYPE, name)
        width, height = struct.unpack_from("<II", data, 0)
        pixels = bytearray()
        for offset in range(8, len(data) - 1, 2):
            pixels += bytes([data[offset + 1]]) * (data[offset] or 256)
            if len(pixels) >= width * height:
                break
        if len(pixels) < width * height:
            raise SystemExit(f"type 303 image {name} decodes short")
        image = Image.frombytes("P", (width, height), bytes(pixels[:width * height]))
        image.putpalette(self.palette())
        return image.convert("RGB")


# ---- DirectX binary .x meshes ---------------------------------------------------------------------------------------

def x_tokens(data: bytes) -> list[tuple[str, object]]:
    if not data.startswith(b"xof ") or data[8:12] != b"bin ":
        raise ValueError("not a binary .x file")
    tokens: list[tuple[str, object]] = []
    pos = 16
    while pos < len(data):
        token = struct.unpack_from("<H", data, pos)[0]
        pos += 2
        if token == 1:
            length = struct.unpack_from("<I", data, pos)[0]
            tokens.append(("name", data[pos + 4:pos + 4 + length].decode("latin1")))
            pos += 4 + length
        elif token == 2:
            length = struct.unpack_from("<I", data, pos)[0]
            tokens.append(("string", data[pos + 4:pos + 4 + length].decode("latin1")))
            pos += 4 + length + 2
        elif token == 3:
            tokens.append(("ints", [struct.unpack_from("<I", data, pos)[0]]))
            pos += 4
        elif token == 5:
            pos += 16
        elif token in (6, 7):
            count = struct.unpack_from("<I", data, pos)[0]
            values = list(struct.unpack_from(f"<{count}{'I' if token == 6 else 'f'}", data, pos + 4))
            tokens.append(("ints" if token == 6 else "floats", values))
            pos += 4 + 4 * count
        elif token in (10, 11):
            tokens.append(("{" if token == 10 else "}", None))
        else:
            raise ValueError(f"unexpected .x token {token}")
    return tokens


@dataclass
class XObject:
    template: str
    numbers: list[tuple[str, float]] = field(default_factory=list)
    strings: list[str] = field(default_factory=list)
    children: list[XObject] = field(default_factory=list)

    def child(self, template: str) -> XObject | None:
        return next((item for item in self.children if item.template == template), None)


def x_objects(tokens: list[tuple[str, object]]) -> list[XObject]:
    roots: list[XObject] = []
    stack: list[XObject] = []
    pending: list[str] = []
    for kind, value in tokens:
        if kind == "name":
            pending.append(value)
        elif kind == "{":
            item = XObject(pending[0] if pending else "")
            pending.clear()
            (stack[-1].children if stack else roots).append(item)
            stack.append(item)
        elif kind == "}":
            pending.clear()
            stack.pop()
        elif kind == "string":
            stack[-1].strings.append(value)
        else:
            stack[-1].numbers += [("i" if kind == "ints" else "f", number) for number in value]
    return roots


class Reader:
    def __init__(self, item: XObject) -> None:
        self.numbers = item.numbers
        self.pos = 0

    def take(self, count: int = 1) -> list[float]:
        values = [number for _, number in self.numbers[self.pos:self.pos + count]]
        self.pos += count
        return values

    def int(self) -> int:
        return int(self.take()[0])

    def faces(self) -> list[list[int]]:
        return [[int(i) for i in self.take(self.int())] for _ in range(self.int())]


def to_blender(vector: list[float]) -> np.ndarray:
    # D3D is left-handed with +Y up; the meshes' bows point along -Z. Blender: +X bow, +Y port, +Z up.
    x, y, z = vector
    return np.array([-z, x, y])


def load_mesh(data: bytes, texture_name: str) -> dict:
    mesh = next(item for item in x_objects(x_tokens(data)) if item.template == "Mesh")
    reader = Reader(mesh)
    count = reader.int()
    vertices = np.array([to_blender(reader.take(3)) for _ in range(count)]) * METRES_PER_MESH_UNIT
    faces = reader.faces()
    normals_object = mesh.child("MeshNormals")
    normals_reader = Reader(normals_object)
    normals = np.array([to_blender(normals_reader.take(3)) for _ in range(normals_reader.int())])
    face_normals = normals_reader.faces()
    uv_reader = Reader(mesh.child("MeshTextureCoords"))
    uvs = [uv_reader.take(2) for _ in range(uv_reader.int())]
    centre = (vertices.min(axis=0) + vertices.max(axis=0)) / 2
    vertices -= centre
    out_faces, corner_normals, corner_uvs, flipped = [], [], [], 0
    for face, normal_face in zip(faces, face_normals):
        if len(set(face)) < 3:
            continue
        corners = list(zip(face, normal_face))
        a, b, c = (vertices[i] for i in face[:3])
        geometric = np.cross(b - a, c - a)
        if np.dot(geometric, sum(normals[n] for _, n in corners)) < 0:
            corners.reverse()
            flipped += 1
        out_faces.append([v for v, _ in corners])
        corner_normals += [normals[n].tolist() for _, n in corners]
        corner_uvs += [[uvs[v][0], 1.0 - uvs[v][1]] for v, _ in corners]
    extent = vertices.max(axis=0) - vertices.min(axis=0)
    return {"vertices": vertices.round(4).tolist(), "faces": out_faces, "normals": corner_normals,
            "uvs": corner_uvs, "texture": texture_name, "dimensions_m": extent.round(2).tolist(),
            "flipped_faces": flipped}


# ---- QOI ----------------------------------------------------------------------------------------------------------

def qoi_encode(rgba: np.ndarray) -> bytes:
    """Encode an (h, w, 4) uint8 array; vectorised, emitting the decoder-visible ops of the QOI specification."""
    height, width = rgba.shape[:2]
    pixels = rgba.reshape(-1, 4).astype(np.int32)
    count = len(pixels)
    previous = np.vstack([[0, 0, 0, 255], pixels[:-1]])
    in_run = np.all(pixels == previous, axis=1)
    hashes = (pixels[:, 0] * 3 + pixels[:, 1] * 5 + pixels[:, 2] * 7 + pixels[:, 3] * 11) % 64
    # A run before the first explicit op repeats the implicit start pixel, which decoders differ on indexing
    # (qoi.h does, Pillow does not), so those pixels never serve as an INDEX source.
    leading = np.cumprod(in_run).astype(bool)
    order = np.lexsort((np.arange(count), hashes))
    seen = np.zeros((count, 4), np.int32)
    same_bucket = np.r_[False, hashes[order][1:] == hashes[order][:-1]]
    same_bucket[1:] &= ~leading[order[:-1]]
    seen[order[same_bucket]] = pixels[order[np.flatnonzero(same_bucket) - 1]]
    indexed = ~in_run & np.all(seen == pixels, axis=1)
    delta = (pixels - previous + 128) % 256 - 128
    same_alpha = delta[:, 3] == 0
    small = same_alpha & np.all((delta[:, :3] >= -2) & (delta[:, :3] <= 1), axis=1)
    dr_dg, db_dg = delta[:, 0] - delta[:, 1], delta[:, 2] - delta[:, 1]
    luma = same_alpha & ~small & (delta[:, 1] >= -32) & (delta[:, 1] <= 31) & (dr_dg >= -8) & (dr_dg <= 7) & \
        (db_dg >= -8) & (db_dg <= 7)
    rest = ~in_run & ~indexed
    op_diff, op_luma = rest & small, rest & luma
    op_rgb, op_rgba = rest & same_alpha & ~small & ~luma, rest & ~same_alpha
    run_start = in_run & ~np.r_[False, in_run[:-1]]
    run_id = np.cumsum(run_start) - 1
    run_position = np.arange(count) - np.flatnonzero(run_start)[run_id] if in_run.any() else np.zeros(count, int)
    run_end = in_run & ~np.r_[in_run[1:], False]
    chunk_end = in_run & ((run_position % 62 == 61) | run_end)
    lengths = np.zeros(count, np.int64)
    lengths[chunk_end | indexed | op_diff] = 1
    lengths[op_luma], lengths[op_rgb], lengths[op_rgba] = 2, 4, 5
    offsets = np.cumsum(lengths) - lengths
    body = np.zeros(int(lengths.sum()), np.uint8)
    body[offsets[chunk_end]] = 0xC0 | (run_position[chunk_end] % 62)
    body[offsets[indexed]] = hashes[indexed]
    d = delta[op_diff]
    body[offsets[op_diff]] = 0x40 | ((d[:, 0] + 2) << 4) | ((d[:, 1] + 2) << 2) | (d[:, 2] + 2)
    body[offsets[op_luma]] = 0x80 | (delta[op_luma, 1] + 32)
    body[offsets[op_luma] + 1] = ((dr_dg[op_luma] + 8) << 4) | (db_dg[op_luma] + 8)
    for op, tag, channels in ((op_rgb, 0xFE, 3), (op_rgba, 0xFF, 4)):
        body[offsets[op]] = tag
        for channel in range(channels):
            body[offsets[op] + 1 + channel] = pixels[op, channel]
    header = b"qoif" + struct.pack(">IIBB", width, height, 4, 0)
    return header + body.tobytes() + b"\0" * 7 + b"\1"


def check_qoi(path: Path, rgba: np.ndarray) -> None:
    with Image.open(path) as image:
        if not np.array_equal(np.asarray(image.convert("RGBA")), rgba):
            raise SystemExit(f"QOI round trip failed for {path}")


# ---- Frames and atlases --------------------------------------------------------------------------------------------

def load_rgba(path: Path) -> np.ndarray:
    with Image.open(path) as image:
        return np.asarray(image.convert("RGBA"))


def downsample(rgba: np.ndarray, factor: int) -> np.ndarray:
    pixels = rgba.astype(np.float64) / 255.0
    pixels[..., :3] *= pixels[..., 3:4]
    height, width = pixels.shape[0] // factor, pixels.shape[1] // factor
    pixels = pixels[:height * factor, :width * factor].reshape(height, factor, width, factor, 4).mean(axis=(1, 3))
    alpha = pixels[..., 3:4]
    pixels[..., :3] = np.where(alpha > 0, pixels[..., :3] / np.maximum(alpha, 1e-9), 0)
    return np.clip(np.round(pixels * 255.0), 0, 255).astype(np.uint8)


def bleed_edges(rgba: np.ndarray, rings: int = 2) -> np.ndarray:
    """Give transparent pixels next to the silhouette their neighbours' colour, so filtered scaling has no dark fringe."""
    result = rgba.copy()
    filled = result[..., 3] > 0
    for _ in range(rings):
        colour = np.zeros(result.shape[:2] + (3,))
        weight = np.zeros(result.shape[:2])
        padded_rgb = np.pad(result[..., :3].astype(np.float64), ((1, 1), (1, 1), (0, 0)))
        padded_filled = np.pad(filled, 1)
        for dy in range(3):
            for dx in range(3):
                window = padded_filled[dy:dy + filled.shape[0], dx:dx + filled.shape[1]]
                colour += padded_rgb[dy:dy + filled.shape[0], dx:dx + filled.shape[1]] * window[..., None]
                weight += window
        grow = ~filled & (weight > 0)
        result[grow, :3] = np.round(colour[grow] / weight[grow, None]).astype(np.uint8)
        filled = filled | grow
    return result


@dataclass
class Frame:
    view: str
    heading: int
    pixels: np.ndarray
    pivot: tuple[float, float]
    atlas: int = -1
    x: int = 0
    y: int = 0


def trim(rgba: np.ndarray, centre: float, view: str, heading: int) -> Frame:
    rows = np.flatnonzero(rgba[..., 3].any(axis=1))
    columns = np.flatnonzero(rgba[..., 3].any(axis=0))
    if rows.size == 0:
        raise SystemExit(f"empty {view} frame {heading}")
    top, bottom = max(rows[0] - 1, 0), min(rows[-1] + 2, rgba.shape[0])
    left, right = max(columns[0] - 1, 0), min(columns[-1] + 2, rgba.shape[1])
    return Frame(view, heading, rgba[top:bottom, left:right], (centre - left, centre - top))


def pack_atlases(frames: list[Frame]) -> list[tuple[int, int]]:
    """Shelf-pack frames, tallest first, into pages of at most MAX_ATLAS square; returns each page's size."""
    area = sum((frame.pixels.shape[0] + 1) * (frame.pixels.shape[1] + 1) for frame in frames)
    widest = max(frame.pixels.shape[1] for frame in frames)
    page_width = min(MAX_ATLAS, max(widest, int(math.ceil(math.sqrt(area) * 1.1))))
    pages: list[list[int]] = []
    x = y = shelf = 0
    for frame in sorted(frames, key=lambda item: (-item.pixels.shape[0], item.view, item.heading)):
        height, width = frame.pixels.shape[:2]
        if pages and x + width > page_width:
            x, y, shelf = 0, y + shelf + 1, 0
        if not pages or y + height > MAX_ATLAS:
            pages.append([page_width, 0])
            x = y = shelf = 0
        frame.atlas, frame.x, frame.y = len(pages) - 1, x, y
        x += width + 1
        shelf = max(shelf, height)
        pages[-1][1] = max(pages[-1][1], y + height)
    return [(width, height) for width, height in pages]


# ---- Build --------------------------------------------------------------------------------------------------------

def remastered_model_path(models_dir: Path | None, type_index: int) -> Path | None:
    matches = sorted(models_dir.glob(f"{type_index}_*.blend")) if models_dir else []
    if len(matches) > 1:
        raise SystemExit(f"several remastered models for type {type_index}: {matches}")
    return matches[0] if matches else None


def procedural_class(type_index: int, slug: str, name: str, kind: Kind, size: float,
                     module: str, recipes_sha: dict[str, str], models_dir: Path | None) -> ClassSpec:
    model = remastered_model_path(models_dir, type_index)
    if model:
        return ClassSpec(type_index, slug, name, kind, size, "size",
                         {"kind": "blend", "path": str(model.resolve())}, {"blend": sha256(model.read_bytes())})
    return ClassSpec(type_index, slug, name, kind, size, "size",
                     {"kind": "recipe", "module": module, "model": slug}, {"recipe": recipes_sha[module]})


def class_specs(resources: TacticalResources, extract_dir: Path, models_dir: Path | None) -> list[ClassSpec]:
    specs: list[ClassSpec] = []
    recipes_sha = {path.name: sha256(path.read_bytes()) for path in sorted(RECIPE_DIR.glob("*.py"))}
    for type_index, (slug, name) in enumerate(CAPITAL_SHIPS):
        mesh_id = 2010 + 10 * type_index if type_index < 15 else 2510 + 10 * (type_index - 15)
        mesh_bytes = resources.get(MESH_TYPE, mesh_id)
        texture_name = next(item for item in x_objects(x_tokens(mesh_bytes)) if item.template == "Mesh") \
            .child("MeshMaterialList").child("Material").child("TextureFilename").strings[0].upper()
        texture_bytes = resources.get(IMAGE_TYPE, texture_name)
        directory = extract_dir / f"{type_index:02d}_{slug}"
        directory.mkdir(parents=True, exist_ok=True)
        mesh = load_mesh(mesh_bytes, "texture.png")
        (directory / "mesh.json").write_text(json.dumps(mesh))
        texture = resources.image(texture_name)
        texture.save(directory / "texture.png")
        derived = {"mesh": sha256((directory / "mesh.json").read_bytes()), "texture": sha256(texture.tobytes())}
        hashes = {f"mesh_{mesh_id}": sha256(mesh_bytes), f"texture_{texture_name}": sha256(texture_bytes)}
        source = {"kind": "mesh", "mesh": str(directory / "mesh.json"), "texture": str(directory / "texture.png"),
                  "mesh_id": mesh_id, "texture_name": texture_name}
        remastered = remastered_model_path(models_dir, type_index)
        if remastered:
            hashes = {"blend": sha256(remastered.read_bytes()), **hashes}
            source = {"kind": "blend", "path": str(remastered.resolve()), "replaces_mesh": mesh_id}
        specs.append(ClassSpec(type_index, slug, name, CAPITAL, mesh["dimensions_m"][0], "length", source, hashes,
                               {} if remastered else derived))
    for offset, (slug, name, canonical) in enumerate(FIGHTERS):
        specs.append(procedural_class(29 + offset, slug, name, FIGHTER, canonical * FIGHTER_SCALE,
                                      "fighter_proxies.py", recipes_sha, models_dir))
    type_index, slug, name, diameter = DEATH_STAR
    specs.append(procedural_class(type_index, slug, name, STATION, diameter,
                                  "death_star.py", recipes_sha, models_dir))
    return specs


def render_settings() -> dict:
    return {"pitch_degrees": PITCH_DEGREES, "samples": 32, "margin_px": 6, "align_px": 2 * SMALL_FACTOR}


def blender_version(blender: str) -> str:
    output = subprocess.run([blender, "--version"], capture_output=True, text=True, check=True).stdout
    return output.splitlines()[0].strip()


def render_fingerprint(spec: ClassSpec, version: str) -> str:
    return fingerprint({"revision": REVISION, "bpy": sha256(BPY_SCRIPT.read_bytes()), "hashes": spec.hashes,
                        "derived": spec.derived,
                        "settings": render_settings(), "size": spec.size_m, "fit": spec.fit, "kind": vars(spec.kind),
                        "source": {key: value for key, value in spec.source.items() if key in ("kind", "model")},
                        "blender": version})


def cached(render_dir: Path, expected: str) -> bool:
    meta = render_dir / "meta.json"
    return meta.exists() and json.loads(meta.read_text()).get("fingerprint") == expected


def render(specs: list[ClassSpec], render_root: Path, blender: str, version: str, log_path: Path) -> None:
    job = {"settings": render_settings(), "recipe_dir": str(RECIPE_DIR), "classes": []}
    for spec in specs:
        out = render_root / spec.key
        if out.exists():
            shutil.rmtree(out)
        out.mkdir(parents=True)
        job["classes"].append({"key": spec.key, "type": spec.type_index, "name": spec.name, "size_m": spec.size_m,
                               "fit": spec.fit,
                               "headings": spec.kind.headings, "metres_per_pixel": spec.kind.metres_per_pixel,
                               "source": spec.source, "out": str(out),
                               "fingerprint": render_fingerprint(spec, version)})
    job_path = render_root / "job.json"
    job_path.write_text(json.dumps(job, indent=1))
    print(f"Rendering {len(specs)} classes in Blender (log: {log_path})", flush=True)
    with log_path.open("w") as log:
        result = subprocess.run([blender, "--background", "--factory-startup", "--python-exit-code", "1",
                                 "--python", str(BPY_SCRIPT), "--", str(job_path)], stdout=log, stderr=subprocess.STDOUT)
    if result.returncode != 0:
        raise SystemExit(f"Blender failed ({result.returncode}); see {log_path}")


def class_frames(render_dir: Path, meta: dict, size: str) -> list[Frame]:
    canvas = meta["canvas_px"]
    factor = 1 if size == "large" else SMALL_FACTOR
    centre = canvas / 2 / factor
    names = [("top", 0, "top.png")] + [("angled", k, f"angled_{k:03d}.png") for k in range(meta["headings"])]
    frames = []
    for view, heading, filename in names:
        rgba = load_rgba(render_dir / filename)
        if factor > 1:
            rgba = downsample(rgba, factor)
        frames.append(trim(bleed_edges(rgba), centre, view, heading))
    return frames


def pack_fingerprint(specs: list[ClassSpec], version: str) -> str:
    return fingerprint({"tool": sha256(Path(__file__).read_bytes()),
                        "classes": [render_fingerprint(spec, version) for spec in specs]})


def pack_is_current(out: Path, expected: str) -> bool:
    manifest = out / "manifest.txt"
    if not manifest.exists():
        return False
    lines = manifest.read_text().splitlines()
    files = [line.split()[2] for line in lines if line.startswith("atlas ")]
    return f"setting pack_fingerprint {expected}" in lines and all((out / name).exists() for name in files)


def write_pack(specs: list[ClassSpec], render_root: Path, out: Path, version: str, dll_sha256: str,
               pack_digest: str) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    for stale in list(out.glob("*.qoi")) + [out / "manifest.txt"]:
        stale.unlink(missing_ok=True)
    lines = [f"rebellion-tactical-sprites 1",
             "# class <type> <slug> <origin> <length_m> <name>",
             "# view <type> <top|angled> <large|small> <headings> <metres_per_pixel>",
             "# atlas <id> <file> <width> <height>",
             "# frame <type> <view> <size> <heading> <atlas> <x> <y> <w> <h> <pivot_x> <pivot_y>",
             f"setting revision {REVISION}", f"setting blender {version.replace(' ', '_')}",
             f"setting pitch_degrees {PITCH_DEGREES:g}", f"setting small_factor {SMALL_FACTOR}",
             f"setting metres_per_mesh_unit {METRES_PER_MESH_UNIT:g}", f"setting pack_fingerprint {pack_digest}",
             f"source TACTICAL.DLL {dll_sha256}",
             f"source tools/{BPY_SCRIPT.name} {sha256(BPY_SCRIPT.read_bytes())}",
             f"source tools/{Path(__file__).name} {sha256(Path(__file__).read_bytes())}"]
    lines += [f"source tools/{RECIPE_DIR.name}/{path.name} {sha256(path.read_bytes())}"
              for path in sorted(RECIPE_DIR.glob("*.py"))]
    stats = {"bytes": 0, "atlases": 0, "frames": 0}
    atlas_id = 0
    for spec in specs:
        meta = json.loads((render_root / spec.key / "meta.json").read_text())
        origin = {"mesh": "original", "blend": "remastered", "recipe": "procedural"}[spec.source["kind"]]
        lines.append(f"class {spec.type_index} {spec.slug} {origin} {meta['length_m']:.1f} {spec.name}")
        for label, digest in spec.hashes.items():
            lines.append(f"source {spec.type_index}:{label} {digest}")
        if origin == "remastered":
            lines.append(f"setting {spec.type_index}:remastered_scale {meta['scale_applied']:.4f}")
        for size in ("large", "small"):
            frames = class_frames(render_root / spec.key, meta, size)
            mpp = meta["metres_per_pixel"] * (1 if size == "large" else SMALL_FACTOR)
            pages = pack_atlases(frames)
            for page, (width, height) in enumerate(pages):
                atlas = np.zeros((height, width, 4), np.uint8)
                for frame in (item for item in frames if item.atlas == page):
                    h, w = frame.pixels.shape[:2]
                    atlas[frame.y:frame.y + h, frame.x:frame.x + w] = frame.pixels
                filename = f"{spec.key}_{size}_{page}.qoi"
                (out / filename).write_bytes(qoi_encode(atlas))
                check_qoi(out / filename, atlas)
                stats["bytes"] += (out / filename).stat().st_size
                lines.append(f"atlas {atlas_id + page} {filename} {width} {height}")
            for view, headings in (("top", 1), ("angled", meta["headings"])):
                lines.append(f"view {spec.type_index} {view} {size} {headings} {mpp:g}")
                for frame in sorted((item for item in frames if item.view == view), key=lambda item: item.heading):
                    h, w = frame.pixels.shape[:2]
                    lines.append(f"frame {spec.type_index} {view} {size} {frame.heading} {atlas_id + frame.atlas} "
                                 f"{frame.x} {frame.y} {w} {h} {frame.pivot[0]:g} {frame.pivot[1]:g}")
            atlas_id += len(pages)
            stats["atlases"] += len(pages)
            stats["frames"] += len(frames)
    (out / "manifest.txt").write_text("\n".join(lines) + "\n")
    stats["bytes"] += (out / "manifest.txt").stat().st_size
    return stats


# ---- Review --------------------------------------------------------------------------------------------------------

def read_pack(directory: Path) -> tuple[dict, dict, dict]:
    classes, atlases, frames = {}, {}, {}
    for line in (directory / "manifest.txt").read_text().splitlines():
        fields = line.split()
        if not fields or fields[0] == "#":
            continue
        if fields[0] == "class":
            classes[int(fields[1])] = (fields[2], fields[3], float(fields[4]), " ".join(fields[5:]))
        elif fields[0] == "atlas":
            atlases[int(fields[1])] = fields[2]
        elif fields[0] == "frame":
            type_index, view, size, heading, atlas, x, y, w, h = fields[1:10]
            frames[(int(type_index), view, size, int(heading))] = (int(atlas), int(x), int(y), int(w), int(h),
                                                                   float(fields[10]), float(fields[11]))
    return classes, atlases, frames


def contact_sheets(pack_dir: Path, review_dir: Path) -> list[Path]:
    """Contact sheets built from the written pack itself, so they also check the manifest and atlases."""
    classes, atlases, frames = read_pack(pack_dir)
    images = {atlas_id: Image.open(pack_dir / name).convert("RGBA") for atlas_id, name in atlases.items()}
    review_dir.mkdir(parents=True, exist_ok=True)
    written = []
    for size, cell in (("large", 220), ("small", 64)):
        columns = [("top", 0)] + [("angled", step) for step in range(8)]
        label_width = 150
        sheet_height = len(classes) * (cell + 4) + 20
        for background_name, background, text in (("dark", (14, 17, 22), (220, 220, 220)),
                                                  ("light", (214, 218, 222), (20, 20, 20))):
            sheet = Image.new("RGBA", (label_width + len(columns) * (cell + 4), sheet_height), background + (255,))
            draw = ImageDraw.Draw(sheet)
            for column, (view, step) in enumerate(columns):
                draw.text((label_width + column * (cell + 4) + 4, 4),
                          "top" if view == "top" else f"angled {step}/8", fill=text)
            for row, type_index in enumerate(sorted(classes)):
                top = 20 + row * (cell + 4)
                slug, origin, length, name = classes[type_index]
                draw.text((4, top + cell // 2 - 12), f"{type_index} {name}", fill=text)
                draw.text((4, top + cell // 2 + 2), f"{origin} {length:.0f} m", fill=text)
                headings = max(key[3] for key in frames if key[:3] == (type_index, "angled", size)) + 1
                for column, (view, step) in enumerate(columns):
                    heading = 0 if view == "top" else step * headings // 8
                    atlas, x, y, w, h, px, py = frames[(type_index, view, size, heading)]
                    sprite = images[atlas].crop((x, y, x + w, y + h))
                    if max(w, h) > cell:
                        ratio = cell / max(w, h)
                        sprite = sprite.resize((max(1, round(w * ratio)), max(1, round(h * ratio))), Image.LANCZOS)
                        px, py = px * ratio, py * ratio
                    cx, cy = label_width + column * (cell + 4) + cell // 2, top + cell // 2
                    sheet.alpha_composite(sprite, (int(round(cx - px)), int(round(cy - py))))
            path = review_dir / f"sheet_{size}_{background_name}.png"
            sheet.convert("RGB").save(path)
            written.append(path)
    written.append(scale_lineup(classes, images, frames, review_dir))
    return written


def scale_lineup(classes: dict, images: dict, frames: dict, review_dir: Path) -> Path:
    """Every capital ship's large top frame at one shared scale (8 m/px), to check the relative sizes."""
    placed, x, y, row_height, width = [], 10, 10, 0, 1400
    for type_index in sorted(index for index in classes if index < 29):
        atlas, fx, fy, w, h, _, _ = frames[(type_index, "top", "large", 0)]
        if x + w > width:
            x, y, row_height = 10, y + row_height + 24, 0
        placed.append((type_index, atlas, (fx, fy, fx + w, fy + h), x, y))
        x += w + 10
        row_height = max(row_height, h)
    sheet = Image.new("RGBA", (width, y + row_height + 30), (14, 17, 22, 255))
    draw = ImageDraw.Draw(sheet)
    for type_index, atlas, box, px, py in placed:
        sheet.alpha_composite(images[atlas].crop(box), (px, py + 12))
        draw.text((px, py), f"{type_index} {classes[type_index][3]}", fill=(220, 220, 220))
    path = review_dir / "scale_lineup.png"
    sheet.convert("RGB").save(path)
    return path


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=Path("work/hd/tactical/pack"))
    parser.add_argument("--cache-dir", type=Path, default=Path("work/hd/tactical"),
                        help="extracted meshes and rendered frames (extract/, renders/) and review/")
    parser.add_argument("--models-dir", type=Path, default=Path("work/hd/tactical/models"),
                        help="remastered <typeIndex>_<slug>.blend models; each replaces its class's mesh or procedural fallback")
    parser.add_argument("--no-remastered", action="store_true", help="ignore remastered models")
    parser.add_argument("--blender", default=os.environ.get("BLENDER") or shutil.which("blender") or "blender")
    parser.add_argument("--only", type=int, nargs="+",
                        help="render only these type indices; the pack then holds just the classes already rendered")
    parser.add_argument("--force", action="store_true", help="re-render classes whose cached frames are current")
    args = parser.parse_args()

    started = time.monotonic()
    resources = TacticalResources(find_file(args.install_dir, "TACTICAL.DLL"))
    render_root = args.cache_dir / "renders"
    render_root.mkdir(parents=True, exist_ok=True)
    specs = class_specs(resources, args.cache_dir / "extract", None if args.no_remastered else args.models_dir)
    version = blender_version(args.blender)
    wanted = [spec for spec in specs if args.only is None or spec.type_index in args.only]
    stale = [spec for spec in wanted
             if args.force or not cached(render_root / spec.key, render_fingerprint(spec, version))]
    if stale:
        render(stale, render_root, args.blender, version, args.cache_dir / "blender.log")
    rendered_at = time.monotonic()
    current = [spec for spec in specs if cached(render_root / spec.key, render_fingerprint(spec, version))]
    if len(current) < len(specs):
        missing = sorted({spec.key for spec in specs} - {spec.key for spec in current})
        print(f"Warning: the pack omits classes without current frames: {', '.join(missing)}")
    digest = pack_fingerprint(current, version)
    if not args.force and pack_is_current(args.out, digest):
        print(f"Pack is current: {args.out}")
        return
    stats = write_pack(current, render_root, args.out, version, resources.dll_sha256, digest)
    sheets = contact_sheets(args.out, args.cache_dir / "review")
    finished = time.monotonic()
    print(f"Rendered {len(stale)} classes in {rendered_at - started:.0f} s; packed {stats['frames']} frames into "
          f"{stats['atlases']} atlases ({stats['bytes'] / 1e6:.1f} MB) in {finished - rendered_at:.0f} s: {args.out}")
    print("Review: " + ", ".join(str(path) for path in sheets))


if __name__ == "__main__":
    main()
