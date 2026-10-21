"""Render tactical battle sprites inside Blender; tools/build_tactical_sprites.py runs it as
`blender --background --factory-startup --python tools/tactical_sprites_bpy.py -- <job.json>`.

Model convention (shared by every source: original meshes, procedural recipes and remastered models in
work/hd/tactical/models/<typeIndex>_<slug>.blend):

- 1 Blender unit = 1 metre. +X is the bow (forward), +Y is port, +Z is dorsal (up).
- The origin is the centre of the model's bounding box. It is the unit's position on the battle plane and the
  sprite pivot. The renderer recentres every model on its bounding-box centre, so a small offset is harmless.
- Scale is the original game's compressed one: TACTICAL.DLL LOD0 mesh units x 800 m. The Imperial Star Destroyer
  is 1,602 m and the Super Star Destroyer 4,795 m (3x), not 1,600 m and 19,000 m. Class length is the X extent:

    0 Mon Calamari Cruiser 1750    10 CC-7700 Frigate          996    20 Imperial Star Destroyer    1602
    1 Bulk Cruiser         1258    11 CC-9600 Frigate          862    21 Super Star Destroyer       4795
    2 Assault Frigate      1138    12 Bulwark Battlecruiser   1559    22 Assault Transport           535
    3 Nebulon-B Frigate     805    13 Liberator Cruiser        907    23 Galleon                     819
    4 Alliance Escort Car. 1414    14 Dauntless Cruiser       1600    24 Victory II Star Destroyer  1469
    5 Corellian Corvette    796    15 Strike Cruiser           889    25 Imperial II Star Destroyer 1602
    6 Medium Transport     1193    16 Lancer Frigate           807    26 Star Galleon                927
    7 Bulk Transport       1047    17 Interdictor Cruiser     1024    27 Imperial Escort Carrier    1171
    8 Corellian Gunship     746    18 Carrack Light Cruiser    903    28 Imperial Dreadnaught       1056
    9 Alliance Dreadnaught 1056    19 Victory Destroyer       1469

  Fighters (procedural proxies, types 29-36) are scaled so their largest dimension is 8x the canonical one (X-wing
  100 m long, TIE Fighter 60 m tall). The Death Star (type 37) is a 9,600 m sphere.
- A remastered model is a .blend file with one mesh object named `ship`, optionally with child objects; everything
  else in the file (cameras, lights, world) is ignored, and the renderer supplies the lighting. Build it at its
  class length above. The renderer scales it uniformly so its X extent equals the class length and records the
  factor in the manifest, so a mismatch shows there instead of skewing the relative scale. Materials are rendered
  as authored (EEVEE); emission shows as glow. Pack image textures into the file. Keep normals pointing outward.

Rendering (orthographic, transparent background, Standard view transform):

- Top view: one frame, camera straight down -Z. The frame shows heading 0: bow toward image right (+x), port toward
  image top. The game rotates it.
- Angled view: the camera looks toward +Y, pitched 50 degrees below the horizon. Frame k of n shows the ship at
  heading 2*pi*k/n, counter-clockwise seen from above, so frame 0 points right and frame n/4 points away from the
  camera. Capital ships have 64 headings, fighters 32 and the Death Star 16.
- Pixel scale: capital ships 8 m/px (large) and 32 m/px (small: the large frame box-filtered 4x), so an Imperial
  Star Destroyer is 200 px and 50 px long; fighters 2 and 8 m/px; the Death Star 16 and 64 m/px. The manifest
  records metres per pixel for every frame set, so the game scales sprites to world size.
- Light is fixed to the camera: a key sun from the upper left of the image, a dim fill from the lower right, and a
  soft ambient. In the top view it therefore rotates with the sprite.
"""
from __future__ import annotations

import importlib.util
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

# Light directions in camera space (x right, y up, z toward the viewer): where each light comes from.
KEY_LIGHT = (Vector((-1.0, 1.0, 0.6)).normalized(), 3.8)
FILL_LIGHT = (Vector((1.0, -0.6, 0.4)).normalized(), 0.45)
AMBIENT = ((0.55, 0.6, 0.7), 0.2)


def reset_scene(settings: dict) -> bpy.types.Scene:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.eevee.taa_render_samples = settings["samples"]
    scene.render.film_transparent = True
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.image_settings.color_depth = "8"
    scene.view_settings.view_transform = "Standard"
    scene.view_settings.look = "None"
    world = bpy.data.worlds.new("ambient")
    background = next(node for node in world.node_tree.nodes if node.type == "BACKGROUND")
    background.inputs["Color"].default_value = (*AMBIENT[0], 1.0)
    background.inputs["Strength"].default_value = AMBIENT[1]
    scene.world = world
    return scene


def textured_material(name: str, texture: Path) -> bpy.types.Material:
    material = bpy.data.materials.new(name)
    nodes = material.node_tree.nodes
    shader = nodes["Principled BSDF"]
    image = nodes.new("ShaderNodeTexImage")
    image.image = bpy.data.images.load(str(texture))
    image.interpolation = "Cubic"
    image.extension = "EXTEND"
    material.node_tree.links.new(image.outputs["Color"], shader.inputs["Base Color"])
    shader.inputs["Roughness"].default_value = 0.55
    shader.inputs["Specular IOR Level"].default_value = 0.3
    return material


def original_mesh(source: dict) -> bpy.types.Object:
    data = json.loads(Path(source["mesh"]).read_text())
    mesh = bpy.data.meshes.new("ship")
    mesh.from_pydata([tuple(v) for v in data["vertices"]], [], [tuple(f) for f in data["faces"]])
    uv_layer = mesh.uv_layers.new(name="UVMap")
    uv_layer.data.foreach_set("uv", [component for uv in data["uvs"] for component in uv])
    mesh.shade_smooth()
    mesh.normals_split_custom_set([tuple(n) for n in data["normals"]])
    mesh.materials.append(textured_material("hull", Path(source["texture"])))
    ship = bpy.data.objects.new("ship", mesh)
    bpy.context.scene.collection.objects.link(ship)
    return ship


def remastered_model(source: dict) -> bpy.types.Object:
    with bpy.data.libraries.load(source["path"], link=False) as (available, loaded):
        if "ship" not in available.objects:
            raise RuntimeError(f"{source['path']} has no object named 'ship'")
        loaded.objects = list(available.objects)
    ship = bpy.data.objects["ship"]
    world = ship.matrix_world.copy()
    ship.parent = None
    ship.matrix_world = world
    for part in [ship, *ship.children_recursive]:
        bpy.context.scene.collection.objects.link(part)
    return ship


def recipe_model(source: dict, recipe_dir: Path) -> bpy.types.Object:
    path = recipe_dir / source["module"]
    spec = importlib.util.spec_from_file_location(path.stem, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    ship = module.build(source["model"])
    if ship.name != "ship":
        ship.name = "ship"
    return ship


def world_points(ship: bpy.types.Object) -> list[Vector]:
    """Every vertex of the ship and its children, after modifiers, in world space."""
    depsgraph = bpy.context.evaluated_depsgraph_get()
    points = []
    for part in [ship, *ship.children_recursive]:
        if part.type != "MESH" or part.hide_render:
            continue
        evaluated = part.evaluated_get(depsgraph)
        mesh = evaluated.to_mesh()
        points += [evaluated.matrix_world @ vertex.co for vertex in mesh.vertices]
        evaluated.to_mesh_clear()
    return points


def normalise(ship: bpy.types.Object, size: float, fit: str) -> tuple[float, list[float], float]:
    """Scale the model so its X extent (fit "length") or largest extent (fit "size") is `size`, and centre its
    bounding box on the origin."""
    bpy.context.view_layer.update()
    points = world_points(ship)
    low = Vector(tuple(min(p[axis] for p in points) for axis in range(3)))
    high = Vector(tuple(max(p[axis] for p in points) for axis in range(3)))
    scale = size / ((high - low).x if fit == "length" else max(high - low))
    ship.matrix_world = Matrix.Scale(scale, 4) @ Matrix.Translation(-(low + high) / 2) @ ship.matrix_world
    bpy.context.view_layer.update()
    radius = max(p.length for p in world_points(ship))
    return scale, [round(value * scale, 2) for value in high - low], radius


def sun(name: str, camera: bpy.types.Object, direction: Vector, strength: float, shadow: bool) -> None:
    light = bpy.data.lights.new(name, "SUN")
    light.energy = strength
    light.angle = math.radians(4.0)
    light.use_shadow = shadow
    lamp = bpy.data.objects.new(name, light)
    bpy.context.scene.collection.objects.link(lamp)
    lamp.parent = camera
    lamp.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()


def camera_rig(scene: bpy.types.Scene, canvas: int, metres_per_pixel: float, radius: float) -> bpy.types.Object:
    camera_data = bpy.data.cameras.new("camera")
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = canvas * metres_per_pixel
    distance = radius * 3 + 10
    camera_data.clip_start = 1.0
    camera_data.clip_end = distance * 2
    camera = bpy.data.objects.new("camera", camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    scene.render.resolution_x = scene.render.resolution_y = canvas
    sun("key", camera, KEY_LIGHT[0], KEY_LIGHT[1], True)
    sun("fill", camera, FILL_LIGHT[0], FILL_LIGHT[1], False)
    camera["distance"] = distance
    return camera


def aim(camera: bpy.types.Object, elevation_degrees: float) -> None:
    """Point the camera at the origin from the -Y side, `elevation_degrees` above the battle plane."""
    tilt = math.radians(90.0 - elevation_degrees)
    distance = camera["distance"]
    camera.rotation_euler = (tilt, 0.0, 0.0)
    camera.location = (0.0, -math.sin(tilt) * distance, math.cos(tilt) * distance)


def render_to(scene: bpy.types.Scene, path: Path) -> None:
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)


def render_class(spec: dict, settings: dict, recipe_dir: Path) -> None:
    scene = reset_scene(settings)
    source = spec["source"]
    if source["kind"] == "mesh":
        ship = original_mesh(source)
    elif source["kind"] == "blend":
        ship = remastered_model(source)
    else:
        ship = recipe_model(source, recipe_dir)
    scale, dimensions, radius = normalise(ship, spec["size_m"], spec["fit"])
    rig = bpy.data.objects.new("heading", None)
    scene.collection.objects.link(rig)
    ship.parent = rig
    metres_per_pixel = spec["metres_per_pixel"]
    align = settings["align_px"]
    canvas = math.ceil((2 * radius / metres_per_pixel + 2 * settings["margin_px"]) / align) * align
    camera = camera_rig(scene, canvas, metres_per_pixel, radius)
    out = Path(spec["out"])
    aim(camera, 90.0)
    render_to(scene, out / "top.png")
    aim(camera, settings["pitch_degrees"])
    for heading in range(spec["headings"]):
        rig.rotation_euler = (0.0, 0.0, 2 * math.pi * heading / spec["headings"])
        render_to(scene, out / f"angled_{heading:03d}.png")
    meta = {"fingerprint": spec["fingerprint"], "canvas_px": canvas, "metres_per_pixel": metres_per_pixel,
            "headings": spec["headings"], "length_m": dimensions[0], "dimensions_m": dimensions,
            "scale_applied": scale, "bounding_radius_m": radius, "source": source["kind"]}
    (out / "meta.json").write_text(json.dumps(meta, indent=1))
    print(f"rendered {spec['key']}: {canvas} px canvas, scale {scale:.4f}", flush=True)


def main() -> None:
    job = json.loads(Path(sys.argv[sys.argv.index("--") + 1]).read_text())
    for spec in job["classes"]:
        render_class(spec, job["settings"], Path(job["recipe_dir"]))


if __name__ == "__main__":
    main()
