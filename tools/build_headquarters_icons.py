#!/usr/bin/env -S uv run
"""Reconstruct the Alliance headquarters icon as reproducible indexed SVG artwork."""
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

from build_hd_pack import bitmap_file, entries_for, fnv1a64, merge_asset_records, preview_rgb
from build_transit_backdrops import index_backdrop

RESOURCES = {"GOKRES.DLL": {0x340: (122, 50), 0x4340: (61, 25)},
             "STRATEGY.DLL": {0x388: (37, 37)}}
SCALE = 4


def headquarters_svg(compact: bool = False, sector: bool = False) -> str:
    width, height = (37, 37) if sector else (122, 50)
    transform = ("translate(-23.55 4.5) scale(.7 .65)" if sector else
                 "translate(-23.4 1) scale(1.4)" if compact else "translate(0 0)")
    roof_seams = "\n".join(
        f'<path d="M{60 + offset * .35:.2f} 6.5 Q{60 + offset * .7:.2f} 10 {60 + offset:.2f} 15.2"/>'
        for offset in (-19, -13, -7, 1, 8, 15, 21))
    underside_ribs = "\n".join(
        f'<path d="M{x} 20.2l{(61 - x) * .17:.2f} 3.4"/>'
        for x in (44, 48, 52, 56, 60, 64, 68, 72, 76))
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">
<defs>
 <linearGradient id="mast"><stop stop-color="#211e14"/><stop offset=".25" stop-color="#75613a"/>
  <stop offset=".5" stop-color="#a28b50"/><stop offset=".72" stop-color="#554525"/><stop offset="1" stop-color="#252116"/></linearGradient>
 <linearGradient id="underside" x2="0" y2="1"><stop stop-color="#8d7950"/><stop offset=".45" stop-color="#4f4932"/>
  <stop offset=".8" stop-color="#353528"/><stop offset="1" stop-color="#686047"/></linearGradient>
 <radialGradient id="roof" cx=".3" cy=".22" r=".8"><stop stop-color="#e1c985"/>
  <stop offset=".42" stop-color="#b6a16a"/><stop offset=".75" stop-color="#8a7545"/><stop offset="1" stop-color="#4c4428"/></radialGradient>
 <linearGradient id="rim" x2="0" y2="1"><stop stop-color="#ddc890"/><stop offset=".2" stop-color="#c0ac70"/>
  <stop offset=".44" stop-color="#a89760"/><stop offset=".58" stop-color="#6d653d"/>
  <stop offset="1" stop-color="#403d29"/></linearGradient>
 <linearGradient id="light"><stop stop-color="#e3d294" stop-opacity="0"/>
  <stop offset=".25" stop-color="#fff1b0"/><stop offset=".55" stop-color="#f5e2a1"/>
  <stop offset="1" stop-color="#bba36b" stop-opacity="0"/></linearGradient>
 <linearGradient id="cap"><stop stop-color="#b3b5a1"/><stop offset=".4" stop-color="#e1e0bf"/>
  <stop offset="1" stop-color="#686d5d"/></linearGradient>
</defs>
<g transform="{transform}">
<path d="M60.2 23H62.1L61.8 50H60.7Z" fill="url(#mast)"/>
<path d="M59.4 24H62.8L62.1 27H60.1Z" fill="#5a5032"/>
<path d="M41.5 18.8Q60 15.8 80 18.8L76.5 22.6Q61 28 45.5 22.5Z" fill="url(#underside)"/>
<g fill="none" stroke="#272b23" stroke-width=".35" opacity=".5">{underside_ribs}</g>
<path d="M46 22.3Q61 26.6 76 22.4M51.5 24Q61 25.7 71 24" fill="none" stroke="#8e8260" stroke-width=".35" opacity=".65"/>
<path d="M36.5 16.8Q38.1 13.2 43 11.8Q48.4 5.3 60.7 5.4Q73 5.4 79 11.7Q83.2 13.4 85 17.1L83 19.2Q60.7 24.4 38 19.4Z" fill="url(#roof)"/>
<g fill="none" stroke="#6c5f3d" stroke-width=".2" opacity=".3">{roof_seams}</g>
<path d="M43 11.8Q61 15.3 79 11.7M46 9.1Q60 11.9 76.2 9.3" fill="none" stroke="#665a3c" stroke-width=".45" opacity=".7"/>
<path d="M43.2 11.4Q60.8 14.8 78.3 11.5" fill="none" stroke="#e0cb89" stroke-width=".35"/>
<path d="M36.5 16.8Q60.5 20.1 85 17.1L83 19.2Q60.7 24.4 38 19.4Z" fill="url(#rim)"/>
<path d="M37.2 16.5Q61 19.7 84.5 16.9" fill="none" stroke="url(#light)" stroke-width="1.05"/>
<path d="M39.6 19.9Q61 23.3 81 20" fill="none" stroke="#aa9c69" stroke-width=".35"/>
<path d="M40.6 13.8Q48 13.1 55.6 14.1L54.4 15.3Q46 15 39.2 15.6Z" fill="url(#light)"/>
<path d="M48.7 8.5Q59.5 5.9 72.4 8.4" fill="none" stroke="#d8c28a" stroke-width=".5" opacity=".8"/>
<path d="M56 5.8L57 4.4H65L66 6.1Q61 7.4 56 5.8Z" fill="#857a51"/>
<path d="M59.1 5.5V3.2L60 2.8H62.2L63 3.4V5.5Z" fill="url(#cap)"/>
<path d="M60.7 3V.7H61.5V3" fill="#b9bca4"/>
<path d="M57.3 4.8V3.1H58V4.8M64.3 5V3.7H65V5" stroke="#a9aa8e" stroke-width=".45"/>
</g>
</svg>'''


def build(install_dir: Path, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    records: list[dict] = []
    targets: list[str] = []
    for module_name, resources in RESOURCES.items():
        module = next(path for path in install_dir.iterdir() if path.name.upper() == module_name)
        build_module(module, resources, output, records, targets)
    manifest_path = output / "manifest.json"
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {"schema": "rebellion-hd-indexed-pack-v1"}
    manifest["assets"] = merge_asset_records(manifest.get("assets", []), records)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    review_path = output / "targets.tsv"
    previous = review_path.read_text().splitlines() if review_path.exists() else []
    review_path.write_text("\n".join([line for line in previous if "\theadquarters\t" not in line] + targets) + "\n")
    print(f"Drew {len(records)} SVG headquarters icons in {output}")


def build_module(module: Path, resources: dict[int, tuple[int, int]], output: Path,
                 records: list[dict], targets: list[str]) -> None:
    import resvg_py

    found: set[int] = set()
    for resource, language, dib in entries_for(module, set(resources)):
        found.add(resource)
        svg = headquarters_svg(compact=resource == 0x4340, sector=module.name.upper() == "STRATEGY.DLL")
        source = Image.open(io.BytesIO(bitmap_file(dib)))
        source.load()
        if source.size != resources[resource]:
            raise ValueError(f"Unexpected headquarters icon dimensions: {source.size}")
        key = source.getpixel((0, source.height - 1 if struct.unpack_from("<i", dib, 8)[0] > 0 else 0))
        png = resvg_py.svg_to_bytes(svg_string=svg, width=source.width * SCALE,
                                    height=source.height * SCALE, skip_system_fonts=True)
        rgba = Image.open(io.BytesIO(png)).convert("RGBA")
        if rgba.size != (source.width * SCALE, source.height * SCALE):
            raise ValueError(f"Incorrect SVG render dimensions: {rgba.size}")
        indexed = index_backdrop(rgba, source, key, preserve_source_mask=False)
        directory = output / f"{module.stem.lower()}-{resource}-{language}"
        directory.mkdir(exist_ok=True)
        (directory / "headquarters.svg").write_text(svg)
        source.save(directory / "source-indexed.png")
        indexed.save(directory / "accepted-indexed.png")
        preview_rgb(indexed, key).save(directory / "preview.png")
        fingerprint = fnv1a64(dib)
        payload = indexed.tobytes()
        filename = f"{fingerprint:016x}.rhd"
        (output / filename).write_bytes(b"REBHD02\0" + struct.pack("<IIIIQ", source.width, source.height,
                                                                  SCALE, len(payload), fingerprint) + payload)
        records.append({"module": module.name, "resource_type": 2, "resource_id": resource,
                        "language_id": language, "source_sha256": hashlib.sha256(dib).hexdigest(),
                        "source_fnv1a64": f"{fingerprint:016x}", "logical_size": list(source.size),
                        "scale": SCALE, "key_index": key, "format": "REBHD02", "pack_file": filename,
                        "method": "procedural-svg", "svg_sha256": hashlib.sha256(svg.encode()).hexdigest(),
                        "renderer": f"resvg-py {importlib.metadata.version('resvg-py')}",
                        "dependency_lock_sha256": hashlib.sha256(Path(__file__ + ".lock").read_bytes()).hexdigest(),
                        "svg": str((directory / "headquarters.svg").resolve()),
                        "indexed_png": str((directory / "accepted-indexed.png").resolve())})
        targets.append(f"{module.name}\t{resource}\theadquarters\tAlliance Headquarters")
    if found != resources.keys():
        raise ValueError(f"Required headquarters resources were not found in {module.name}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--install-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=Path("work/hd/gameplay-pack"))
    args = parser.parse_args()
    build(args.install_dir, args.out)


if __name__ == "__main__":
    main()
