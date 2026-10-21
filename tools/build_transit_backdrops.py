#!/usr/bin/env -S uv run
"""Draw shared SVG transit backdrops and install their indexed HD companions."""
# /// script
# requires-python = ">=3.11,<3.14"
# dependencies = ["pillow>=11", "pefile>=2023.2.7", "resvg-py==0.5.0"]
# ///
from __future__ import annotations

import argparse
import hashlib
import importlib.metadata
import io
import json
import struct
from pathlib import Path

from PIL import Image

from build_hd_pack import bitmap_file, entries_for, fnv1a64, merge_asset_records, palette_for, preview_rgb

SCALE = 4
TRANSIT_RESOURCES = {0x2ced: (61, 25), 0x2d00: (122, 50)}
FACILITY_TRANSIT_RESOURCES = {0x2cf1: (61, 25), 0x2cf2: (122, 50), 0x2cf5: (70, 40)}


def transit_svg() -> str:
    streaks = [(12, 9, 25, 0.8), (45, 7, 24, 1.0), (86, 10, 31, 0.7),
               (25, 18, 19, 0.8), (60, 17, 24, 0.6), (94, 22, 18, 1.0),
               (10, 28, 17, 0.6), (39, 27, 28, 0.9), (74, 31, 21, 0.7),
               (106, 33, 17, 0.6)]
    trails = "\n".join(
        f'<rect x="{x}" y="{y - width * 2:.2f}" width="{length}" height="{width * 4:.2f}" '
        f'rx="{width * 2:.2f}" fill="url(#trail)" opacity=".22"/>'
        f'<rect x="{x}" y="{y - width:.2f}" width="{length}" height="{width * 2:.2f}" '
        f'rx="{width:.2f}" fill="url(#trail)"/>'
        f'<path d="M{x + length * 0.68:.2f} {y}h{length * 0.14:.2f}" '
        f'stroke="#cadce9" stroke-opacity=".75" stroke-width="{width * 0.65:.2f}"/>'
        for x, y, length, width in streaks)
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="244" height="100" viewBox="0 0 244 100">
<defs>
  <linearGradient id="wall" x2="0" y2="1">
    <stop stop-color="#bbc0c4"/><stop offset=".45" stop-color="#91999e"/>
    <stop offset="1" stop-color="#a6adb2"/>
  </linearGradient>
  <linearGradient id="bevel" x2="0" y2="1">
    <stop stop-color="#e0e3e4"/><stop offset=".45" stop-color="#b6bec4"/>
    <stop offset=".6" stop-color="#77818b"/><stop offset="1" stop-color="#626b74"/>
  </linearGradient>
  <linearGradient id="spine">
    <stop stop-color="#737e88"/><stop offset=".12" stop-color="#e2e4e5"/>
    <stop offset=".3" stop-color="#b0b7bc"/><stop offset=".73" stop-color="#949ea6"/>
    <stop offset=".9" stop-color="#c9cfd3"/><stop offset="1" stop-color="#77828b"/>
  </linearGradient>
  <linearGradient id="glass" x2="0" y2="1">
    <stop stop-color="#101c2a"/><stop offset=".5" stop-color="#02070e"/>
    <stop offset="1" stop-color="#0b1420"/>
  </linearGradient>
  <linearGradient id="trail">
    <stop stop-color="#243955" stop-opacity="0"/><stop offset=".35" stop-color="#446181"/>
    <stop offset=".72" stop-color="#9dbcd3"/><stop offset="1" stop-color="#395573" stop-opacity=".1"/>
  </linearGradient>
  <clipPath id="glass-clip"><path d="M8 7H116L120 11V29L116 33H8L4 29V11Z"/></clipPath>
  <g id="window">
    <path d="M6 0H118L125 7V33L118 40H6L-1 33V7Z" fill="#5a636a"/>
    <path d="M6 1H118L124 7V33L118 39H6L0 33V7Z" fill="url(#bevel)"/>
    <path d="M8 4H116L122 10V30L116 36H8L2 30V10Z" fill="#2c3741"/>
    <path d="M8 6H116L120 10V30L116 34H8L4 30V10Z" fill="url(#glass)" stroke="#c0ccd3" stroke-width=".6"/>
    <g clip-path="url(#glass-clip)" stroke-linecap="round">{trails}</g>
    <path d="M7 2H117L122 7M2 9V30" fill="none" stroke="#edf0f1" stroke-width=".65"/>
    <path d="M5 37H117L122 32" fill="none" stroke="#404b55" stroke-width=".65"/>
  </g>
</defs>
<path d="M0 0H244V100H0Z" fill="url(#wall)"/>
<path d="M0 47H244M0 49H244" stroke="#65717b" stroke-width=".6"/>
<path d="M0 48H244" stroke="#d3d9dd" stroke-width=".7"/>
<use href="#window" x="8" y="2"/>
<use href="#window" x="8" y="53"/>
<use href="#window" x="208" y="2"/>
<use href="#window" x="208" y="53"/>
<path d="M140 0H201V100H140Z" fill="url(#wall)"/>
<path d="M143 0H157V100H143ZM176 0H190V100H176Z" fill="url(#spine)"/>
<path d="M139 0V100M160 0V100M173 0V100M193 0V100" stroke="#65717a" stroke-width=".7"/>
<path d="M141 0V100M162 0V100M175 0V100M195 0V100" stroke="#d0d5d8" stroke-width=".7"/>
</svg>'''


def facility_transit_svg(width: int = 268, height: int = 140) -> str:
    trails = []
    for row, y in enumerate((7, 22, 38, 54, 70, 86, 103, 119, 133)):
        for x, length, thickness in ((-15 + row * 13 % 38, 43 + row * 11 % 35, 1.6),
                                     (172 + row * 17 % 42, 50 + row * 7 % 27, 2.0)):
            trails.append(f'<rect x="{x}" y="{y - 2}" width="{length}" height="4" '
                          f'rx="2" fill="url(#trail)" opacity=".28"/>')
            trails.append(f'<rect x="{x}" y="{y - thickness / 2}" width="{length}" '
                          f'height="{thickness}" rx="{thickness / 2}" fill="url(#trail)"/>')
            trails.append(f'<path d="M{x + length * .72:.2f} {y}h{length * .12:.2f}" '
                          'stroke="#d0e1ee" stroke-width=".65" stroke-linecap="round"/>')
    for x, y, length in ((95, 3, 40), (109, 25, 19), (141, 51, 14), (88, 84, 22), (119, 127, 32)):
        trails.append(f'<rect x="{x}" y="{y}" width="{length}" height=".8" '
                      'rx=".4" fill="url(#trail)" opacity=".5"/>')
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}"
viewBox="0 0 268 140" preserveAspectRatio="none">
<defs><linearGradient id="trail">
<stop stop-color="#203454" stop-opacity="0"/>
<stop offset=".3" stop-color="#3c6389" stop-opacity=".65"/>
<stop offset=".7" stop-color="#91b7d1"/>
<stop offset=".84" stop-color="#bbd1e0"/>
<stop offset="1" stop-color="#355878" stop-opacity="0"/>
</linearGradient></defs>''' + "\n".join(trails) + "</svg>"


def index_backdrop(rgba: Image.Image, source: Image.Image, key: int,
                   preserve_source_mask: bool = True) -> Image.Image:
    palette = palette_for(source)
    quantization_palette = palette.copy()
    replacement = 0 if key != 0 else 1
    quantization_palette[key * 3:key * 3 + 3] = palette[replacement * 3:replacement * 3 + 3]
    reference = Image.new("P", (1, 1))
    reference.putpalette(quantization_palette)
    rgb = Image.alpha_composite(Image.new("RGBA", rgba.size, (0, 0, 0, 255)), rgba.convert("RGBA")).convert("RGB")
    result = rgb.quantize(palette=reference, dither=Image.Dither.NONE)
    result = result.point(lambda value: replacement if value == key else value)
    result.putpalette(palette)
    if preserve_source_mask:
        mask = source.point(lambda value: 255 if value == key else 0, mode="L")
        mask = mask.resize(result.size, Image.Resampling.NEAREST)
    else:
        mask = rgba.getchannel("A").point(lambda alpha: 255 if alpha < 16 else 0)
    result.paste(key, mask=mask)
    return result


def build(install_dir: Path, output: Path) -> None:
    import resvg_py

    module = next(path for path in install_dir.iterdir() if path.name.lower() == "strategy.dll")
    output.mkdir(parents=True, exist_ok=True)
    records: list[dict] = []
    targets: list[str] = []
    resources = TRANSIT_RESOURCES | FACILITY_TRANSIT_RESOURCES
    for resource, language, dib in entries_for(module, set(resources)):
        is_facility = resource in FACILITY_TRANSIT_RESOURCES
        svg = facility_transit_svg(*resources[resource]) if is_facility else transit_svg()
        category = "facility-transit" if is_facility else "character-transit"
        source = Image.open(io.BytesIO(bitmap_file(dib)))
        source.load()
        if source.size != resources[resource]:
            raise ValueError(f"Unexpected transit backdrop dimensions: {source.size}")
        key = source.getpixel((0, source.height - 1 if struct.unpack_from("<i", dib, 8)[0] > 0 else 0))
        rendered = resvg_py.svg_to_bytes(svg_string=svg, width=source.width * SCALE,
                                         height=source.height * SCALE, skip_system_fonts=True)
        rgba = Image.open(io.BytesIO(rendered)).convert("RGBA")
        if rgba.size != (source.width * SCALE, source.height * SCALE):
            raise ValueError(f"SVG renderer returned incorrect dimensions: {rgba.size}")
        indexed = index_backdrop(rgba, source, key,
                                 preserve_source_mask=not is_facility)
        directory = output / f"{module.stem.lower()}-{resource}-{language}"
        directory.mkdir(exist_ok=True)
        (directory / "transit.svg").write_text(svg)
        source.save(directory / "source-indexed.png")
        indexed.save(directory / "accepted-indexed.png")
        preview_rgb(indexed, key).save(directory / "preview.png")
        source_fnv = fnv1a64(dib)
        payload = indexed.tobytes()
        pack_file = f"{source_fnv:016x}.rhd"
        (output / pack_file).write_bytes(b"REBHD02\0" + struct.pack("<IIIIQ", source.width, source.height,
                                                                 SCALE, len(payload), source_fnv) + payload)
        records.append({"module": module.name, "resource_type": 2, "resource_id": resource,
                        "language_id": language, "source_sha256": hashlib.sha256(dib).hexdigest(),
                        "source_fnv1a64": f"{source_fnv:016x}", "logical_size": list(source.size),
                        "scale": SCALE, "key_index": key, "format": "REBHD02", "pack_file": pack_file,
                        "method": "procedural-svg", "svg_sha256": hashlib.sha256(svg.encode()).hexdigest(),
                        "renderer": f"resvg-py {importlib.metadata.version('resvg-py')}",
                        "dependency_lock_sha256": hashlib.sha256(Path(__file__ + ".lock").read_bytes()).hexdigest(),
                        "svg": str((directory / "transit.svg").resolve()),
                        "indexed_png": str((directory / "accepted-indexed.png").resolve())})
        targets.append(f"{module.name}\t{resource}\t{category}\t{category.replace('-', ' ').title()} backdrop")
    if {record["resource_id"] for record in records} != resources.keys():
        raise ValueError("Required transit backdrop resources were not found")
    manifest_path = output / "manifest.json"
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {"schema": "rebellion-hd-indexed-pack-v1"}
    manifest["assets"] = merge_asset_records(manifest.get("assets", []), records)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    review_path = output / "targets.tsv"
    previous = review_path.read_text().splitlines() if review_path.exists() else []
    previous = [line for line in previous
                if "\tcharacter-transit\t" not in line and "\tfacility-transit\t" not in line]
    review_path.write_text("\n".join(previous + targets) + "\n")
    print(f"Drew {len(records)} SVG transit backdrops in {output}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=Path("work/hd/gameplay-pack"))
    args = parser.parse_args()
    build(args.install_dir, args.out)


if __name__ == "__main__":
    main()
