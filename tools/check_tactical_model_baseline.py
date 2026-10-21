#!/usr/bin/env -S uv run
"""Compare an original mesh with its remodel using the same Blender silhouette renderer."""
# /// script
# requires-python = ">=3.11,<3.14"
# dependencies = ["numpy>=2", "opencv-python-headless>=4.10", "pillow>=11", "scipy>=1.14"]
# ///
from __future__ import annotations

import argparse
import dataclasses
import hashlib
import json
from pathlib import Path

import build_tactical_models as models

REPO = Path(__file__).resolve().parents[1]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("type_index", type=int)
    parser.add_argument("--samples", type=int, default=32)
    parser.add_argument("--blender")
    args = parser.parse_args()
    recipe = next((r for r in models.load_recipes() if r.type_index == args.type_index), None)
    if recipe is None:
        parser.error(f"No remodel recipe for type {args.type_index}")
    original_dir = REPO / "work/hd/tactical/extract"
    mesh_path = next(original_dir.glob(f"{recipe.type_index:02d}_*/mesh.json"), None)
    if mesh_path is None:
        parser.error("No extracted original mesh; fighters and Death Star have no mesh baseline")
    mask_path = REPO / "work/hd/encyclopedia" / recipe.encyclopedia / "mask.png"
    target = models.load_mask(mask_path)
    triangles = models.original_mesh_triangles(original_dir, recipe)
    fit = models.fit_camera(triangles, target, recipe.length, recipe.view, recipe.roll_seeds)
    height, width = target.shape
    cpu = models.coverage(triangles, fit.camera, width, height) >= 0.5
    out = REPO / "work/hd/tactical/gpu-baseline" / recipe.stem
    out.mkdir(parents=True, exist_ok=True)
    generated = out / "original_recipe.py"
    generated.write_text(f'''from __future__ import annotations
import json
from pathlib import Path
import bpy
import _kit as kit

def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    data = json.loads(Path({str(mesh_path)!r}).read_text())
    material = kit.plain("original_opaque", kit.linear("#aaaaaa"))
    ship = kit.mesh_object("original_mesh", data["vertices"], data["faces"], material)
    return kit.finalize([ship], length, {recipe.fit!r})
''')
    original_recipe = dataclasses.replace(recipe, path=generated)
    blend = out / "original.blend"
    blender = models.find_blender(args.blender)
    palette = {
        "light": "#aaaaaa", "mid": "#777777", "dark": "#333333",
        "accent": "#888888", "tints": ["#777777"], "gain": [1, 1, 1],
    }
    models.build_model(blender, original_recipe, palette, blend, out)
    renders = models.render_jobs(recipe, fit.camera, out, args.samples, False)[:1]
    models.run_blender(blender, {"blend": str(blend), "renders": renders}, out / "render_job.json")
    gpu = models.silhouette(out / "fit_view.png", width, height)
    report_path = REPO / "work/hd/tactical/models/review" / recipe.stem / "report.json"
    remodel = json.loads(report_path.read_text())
    report = {
        "type_index": args.type_index, "name": recipe.name,
        "method": "Original mesh independently fitted with fit_camera; same EEVEE renderer, "
                  "supersampled alpha coverage threshold 0.5.",
        "mesh_sha256": hashlib.sha256(mesh_path.read_bytes()).hexdigest(),
        "mask_sha256": hashlib.sha256(mask_path.read_bytes()).hexdigest(),
        "original_cpu_iou": models.binary_iou(cpu, target),
        "original_gpu_iou": models.binary_iou(gpu, target),
        "remodel_gpu_iou": remodel["iou"],
        "remodel_geometry_hash": remodel["mesh"]["geometry_sha256"],
        "camera": dataclasses.asdict(fit.camera), "samples": args.samples,
        "render_size": [renders[0]["width"], renders[0]["height"]],
    }
    models.overlay(cpu, target).save(out / "cpu_overlay.png")
    models.overlay(gpu, target).save(out / "gpu_overlay.png")
    (out / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
