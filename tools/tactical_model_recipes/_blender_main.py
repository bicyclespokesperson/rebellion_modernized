"""Blender side of tools/build_tactical_models.py: `blender --background --python _blender_main.py -- job.json`.

A job either builds a recipe (save the .blend, export a triangle soup for camera fitting) or renders a saved model
from the cameras the tool computed.
"""
from __future__ import annotations

import hashlib
import importlib.util
import json
import math
import sys
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix

sys.path.insert(0, str(Path(__file__).resolve().parent))
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import _kit as kit  # noqa: E402
import tactical_sprites_bpy as sprites  # noqa: E402  (the sprite renderer's light rig, so previews match the pack)


def load_recipe(path: str):
    spec = importlib.util.spec_from_file_location(Path(path).stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def triangles(obj: bpy.types.Object) -> np.ndarray:
    mesh = obj.data
    mesh.calc_loop_triangles()
    coords = np.empty(len(mesh.vertices) * 3, np.float64)
    mesh.vertices.foreach_get("co", coords)
    indices = np.empty(len(mesh.loop_triangles) * 3, np.int64)
    mesh.loop_triangles.foreach_get("vertices", indices)
    return coords.reshape(-1, 3)[indices.reshape(-1, 3)]


def decimated_triangles(ship: bpy.types.Object, max_tris: int) -> np.ndarray:
    proxy = ship.copy()
    proxy.data = ship.data.copy()
    bpy.context.scene.collection.objects.link(proxy)
    count = len(triangles(proxy))
    if count > max_tris:
        modifier = proxy.modifiers.new("decimate", "DECIMATE")
        modifier.ratio = max_tris / count
        kit.apply_modifiers(proxy)
    tris = triangles(proxy)
    bpy.data.objects.remove(proxy)
    return tris


def build(job: dict) -> None:
    kit.reset_scene()
    recipe = load_recipe(job["recipe"])
    ship = recipe.build(kit.Palette(job["palette"]), float(job["length"]))
    assert ship.name == "ship" and len(bpy.context.scene.collection.objects) == 1
    mesh = ship.data
    mesh.calc_loop_triangles()
    coords = np.empty(len(mesh.vertices) * 3)
    mesh.vertices.foreach_get("co", coords)
    extent = np.ptp(coords.reshape(-1, 3), axis=0)
    fit = job.get("fit", "length")
    actual_size = extent[0] if fit == "length" else extent.max()
    if not math.isclose(float(actual_size), float(job["length"]), rel_tol=1e-4):
        raise ValueError(f"recipe produced {actual_size:g} m for {fit} fit; expected {job['length']} m. "
                         "Pass the recipe's FIT to kit.finalize.")
    indices = np.empty(len(mesh.loop_triangles) * 3, np.int64)
    mesh.loop_triangles.foreach_get("vertices", indices)
    digest = hashlib.sha256(np.round(coords, 3).tobytes() + indices.tobytes()).hexdigest()[:16]
    stats = {"vertices": len(mesh.vertices), "triangles": len(mesh.loop_triangles),
             "materials": [m.name for m in mesh.materials],
             "length": round(float(extent[0]), 1), "width": round(float(extent[1]), 1),
             "height": round(float(extent[2]), 1), "geometry_sha256": digest}
    Path(job["blend"]).parent.mkdir(parents=True, exist_ok=True)
    bpy.context.preferences.filepaths.save_version = 0
    bpy.ops.wm.save_as_mainfile(filepath=job["blend"], compress=True)
    np.savez_compressed(job["tris"], tris=decimated_triangles(ship, int(job["max_tris"])).astype(np.float32),
                        stats=json.dumps(stats))
    print("stats", stats)


def configure_render(scene: bpy.types.Scene) -> None:
    """EEVEE with the sprite renderer's ambient, so previews and colour calibration match the sprite pack."""
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    world = bpy.data.worlds.new("ambient")
    background = next(node for node in world.node_tree.nodes if node.type == "BACKGROUND")
    background.inputs["Color"].default_value = (*sprites.AMBIENT[0], 1.0)
    background.inputs["Strength"].default_value = sprites.AMBIENT[1]
    scene.world = world


def camera_object(scene: bpy.types.Scene, spec: dict) -> bpy.types.Object:
    data = bpy.data.cameras.new("camera")
    camera = bpy.data.objects.new("camera", data)
    scene.collection.objects.link(camera)
    camera.matrix_world = Matrix(spec["matrix"])
    data.sensor_fit = "HORIZONTAL"
    data.clip_start, data.clip_end = spec["clip"]
    if spec["type"] == "ORTHO":
        data.type = "ORTHO"
        data.ortho_scale = spec["ortho_scale"]
    else:
        data.sensor_width = spec["sensor"]
        data.lens = spec["lens"]
    scene.camera = camera
    return camera


def camera_lights(camera: bpy.types.Object) -> list[bpy.types.Object]:
    sprites.sun("key", camera, sprites.KEY_LIGHT[0], sprites.KEY_LIGHT[1], True)
    sprites.sun("fill", camera, sprites.FILL_LIGHT[0], sprites.FILL_LIGHT[1], False)
    return list(camera.children)


def render(job: dict) -> None:
    bpy.ops.wm.open_mainfile(filepath=job["blend"])
    scene = bpy.context.scene
    configure_render(scene)
    ship = bpy.data.objects["ship"]
    for spec in job["renders"]:
        camera = camera_object(scene, spec["camera"])
        lights = camera_lights(camera)
        scene.render.resolution_x, scene.render.resolution_y = spec["width"], spec["height"]
        scene.eevee.taa_render_samples = spec["samples"]
        headings = spec.get("headings")
        for step in range(headings or 1):
            ship.rotation_euler = (0, 0, 2 * math.pi * step / headings if headings else 0)
            scene.render.filepath = spec["path"].format(heading=step)
            bpy.ops.render.render(write_still=True)
        ship.rotation_euler = (0, 0, 0)
        for obj in [camera, *lights]:
            bpy.data.objects.remove(obj)


def main() -> None:
    job = json.loads(Path(sys.argv[sys.argv.index("--") + 1]).read_text())
    if "recipe" in job:
        build(job)
    else:
        render(job)
    print("TACTICAL_MODEL_JOB_DONE")


main()
