"""Simple Blender proxies for the eight fighters, which have no meshes in TACTICAL.DLL.

`build(slug)` returns one mesh object in canonical metres (+X forward, +Z up, +Y port); tools/tactical_sprites_bpy.py
scales it so its largest dimension is the class size. Proxies only need a readable silhouette at 15-60 px.
"""
from __future__ import annotations

import math
from collections.abc import Callable

import bmesh
import bpy
from mathutils import Matrix, Vector

ALLIANCE_HULL = (0.78, 0.76, 0.72)
ALLIANCE_STRIPE = (0.62, 0.12, 0.09)
ALLIANCE_YELLOW = (0.8, 0.62, 0.18)
IMPERIAL_HULL = (0.46, 0.49, 0.54)
IMPERIAL_PANEL = (0.11, 0.12, 0.14)
GLASS = (0.04, 0.05, 0.07)
ENGINE_METAL = (0.32, 0.32, 0.34)


class Model:
    def __init__(self, slug: str) -> None:
        self.mesh = bmesh.new()
        self.materials: list[tuple[str, tuple[float, float, float], float]] = []
        self.slug = slug

    def material(self, name: str, colour: tuple[float, float, float], glow: float = 0.0) -> int:
        names = [entry[0] for entry in self.materials]
        if name not in names:
            self.materials.append((name, colour, glow))
            names.append(name)
        return names.index(name)

    def add(self, faces: list[bmesh.types.BMFace], material: int) -> None:
        for face in faces:
            face.material_index = material

    def box(self, centre: tuple, size: tuple, material: int, matrix: Matrix | None = None) -> None:
        result = bmesh.ops.create_cube(self.mesh, size=1.0)
        transform = (matrix or Matrix()) @ Matrix.Translation(centre) @ Matrix.Diagonal((*size, 1.0))
        bmesh.ops.transform(self.mesh, matrix=transform, verts=result["verts"])
        self.add(faces_of(result["verts"]), material)

    def frustum(self, back: float, front: float, back_size: tuple, front_size: tuple, material: int,
                z: float = 0.0) -> None:
        """A box tapered along X: (width, height) at the back and front."""
        corners = []
        for x, (width, height) in ((back, back_size), (front, front_size)):
            corners += [(x, sy * width / 2, z + sz * height / 2) for sy, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        self.hull(corners, material)

    def hull(self, points: list[tuple], material: int) -> None:
        verts = [self.mesh.verts.new(point) for point in points]
        result = bmesh.ops.convex_hull(self.mesh, input=verts)
        inside = {item for item in result["geom_interior"] + result["geom_unused"] if isinstance(item, bmesh.types.BMVert)}
        bmesh.ops.delete(self.mesh, geom=list(inside), context="VERTS")
        self.add([item for item in result["geom"] if isinstance(item, bmesh.types.BMFace) and item.is_valid], material)

    def cylinder(self, start: tuple, end: tuple, radius: float, material: int, segments: int = 12,
                 end_radius: float | None = None) -> None:
        axis = Vector(end) - Vector(start)
        result = bmesh.ops.create_cone(self.mesh, cap_ends=True, segments=segments, radius1=radius,
                                       radius2=radius if end_radius is None else end_radius, depth=axis.length)
        rotation = axis.to_track_quat("Z", "Y").to_matrix().to_4x4()
        transform = Matrix.Translation((Vector(start) + Vector(end)) / 2) @ rotation
        bmesh.ops.transform(self.mesh, matrix=transform, verts=result["verts"])
        self.add(faces_of(result["verts"]), material)

    def ball(self, centre: tuple, radius: float, material: int, scale: tuple = (1.0, 1.0, 1.0)) -> None:
        result = bmesh.ops.create_uvsphere(self.mesh, u_segments=20, v_segments=12, radius=radius)
        transform = Matrix.Translation(centre) @ Matrix.Diagonal((*scale, 1.0))
        bmesh.ops.transform(self.mesh, matrix=transform, verts=result["verts"])
        self.add(faces_of(result["verts"]), material)

    def panel(self, outline: list[tuple[float, float]], y: float, thickness: float, material: int,
              rim: int | None = None, matrix: Matrix | None = None) -> None:
        """A flat plate in the XZ plane at `y`; `outline` is (x, z) in order. An optional rim frames its edge."""
        transform = matrix or Matrix()
        points = [transform @ Vector((x, y + side * thickness / 2, z)) for side in (-1, 1) for x, z in outline]
        self.hull([tuple(point) for point in points], material)
        if rim is not None:
            for (x0, z0), (x1, z1) in zip(outline, outline[1:] + outline[:1]):
                start = transform @ Vector((x0, y, z0))
                end = transform @ Vector((x1, y, z1))
                self.cylinder(tuple(start), tuple(end), thickness * 0.9, rim, segments=6)

    def finish(self) -> bpy.types.Object:
        mesh = bpy.data.meshes.new(self.slug)
        bmesh.ops.recalc_face_normals(self.mesh, faces=self.mesh.faces)
        self.mesh.to_mesh(mesh)
        self.mesh.free()
        for name, colour, glow in self.materials:
            mesh.materials.append(make_material(f"{self.slug}_{name}", colour, glow))
        ship = bpy.data.objects.new("ship", mesh)
        bpy.context.scene.collection.objects.link(ship)
        return ship


def faces_of(verts: list[bmesh.types.BMVert]) -> list[bmesh.types.BMFace]:
    return list({face for vert in verts for face in vert.link_faces})


def make_material(name: str, colour: tuple[float, float, float], glow: float) -> bpy.types.Material:
    material = bpy.data.materials.new(name)
    shader = material.node_tree.nodes["Principled BSDF"]
    shader.inputs["Base Color"].default_value = (*colour, 1.0)
    shader.inputs["Roughness"].default_value = 0.5
    if glow:
        shader.inputs["Emission Color"].default_value = (*colour, 1.0)
        shader.inputs["Emission Strength"].default_value = glow
    return material


def hexagon(radius: float, pointy: bool = True) -> list[tuple[float, float]]:
    start = math.pi / 2 if pointy else 0.0
    return [(radius * math.cos(start + i * math.pi / 3), radius * math.sin(start + i * math.pi / 3)) for i in range(6)]


def xwing(model: Model) -> None:
    hull = model.material("hull", ALLIANCE_HULL)
    stripe = model.material("stripe", ALLIANCE_STRIPE)
    metal = model.material("metal", ENGINE_METAL)
    glass = model.material("glass", GLASS)
    exhaust = model.material("exhaust", (1.0, 0.45, 0.3), 6.0)
    model.frustum(-5.0, 6.25, (1.5, 1.3), (0.35, 0.35), hull)
    model.frustum(0.6, 2.4, (0.9, 0.5), (0.6, 0.3), glass, z=0.7)
    for side in (-1, 1):
        for level in (-1, 1):
            tilt = Matrix.Rotation(math.radians(12) * side * level, 4, "X")
            y0, y1 = side * 0.6, side * 5.5
            wing = [(-4.7, y0, 0), (-1.9, y0, 0), (-3.0, y1, 0), (-4.5, y1, 0)]
            model.hull([tuple(tilt @ Vector((x, y, z + dz))) for x, y, z in wing for dz in (-0.08, 0.08)], hull)
            stripe_points = [(-4.2, side * 2.0, 0), (-2.6, side * 2.0, 0), (-2.8, side * 2.8, 0), (-4.25, side * 2.8, 0)]
            model.hull([tuple(tilt @ Vector((x, y, z + dz))) for x, y, z in stripe_points for dz in (-0.1, 0.1)],
                       stripe)
            tip = tilt @ Vector((-3.7, side * 5.5, 0.0))
            model.cylinder((tip.x - 1.0, tip.y, tip.z), (tip.x + 4.2, tip.y, tip.z), 0.12, metal, segments=6)
            engine = tilt @ Vector((0.0, side * 0.95, 0.0)) + Vector((0, 0, level * 0.55))
            model.cylinder((-5.3, engine.y, engine.z), (-1.6, engine.y, engine.z), 0.42, metal)
            model.cylinder((-5.42, engine.y, engine.z), (-5.3, engine.y, engine.z), 0.32, exhaust)


def ywing(model: Model) -> None:
    hull = model.material("hull", ALLIANCE_HULL)
    accent = model.material("accent", ALLIANCE_YELLOW)
    metal = model.material("metal", ENGINE_METAL)
    glass = model.material("glass", GLASS)
    exhaust = model.material("exhaust", (1.0, 0.45, 0.3), 6.0)
    model.cylinder((2.5, 0, 0), (6.8, 0, 0), 1.05, hull, end_radius=0.75)
    model.cylinder((6.8, 0, 0), (8.0, 0, 0), 0.75, accent, end_radius=0.25)
    model.frustum(3.8, 5.6, (0.8, 0.5), (0.6, 0.3), glass, z=0.95)
    model.box((-2.0, 0, 0), (9.0, 0.7, 0.55), metal)
    model.box((-3.0, 0, 0), (1.4, 7.4, 0.45), hull)
    for side in (-1, 1):
        model.cylinder((-8.0, side * 3.7, 0), (0.6, side * 3.7, 0), 0.85, hull)
        model.cylinder((0.6, side * 3.7, 0), (1.6, side * 3.7, 0), 0.85, accent, end_radius=0.5)
        model.cylinder((-8.12, side * 3.7, 0), (-8.0, side * 3.7, 0), 0.6, exhaust)


def awing(model: Model) -> None:
    hull = model.material("hull", ALLIANCE_HULL)
    stripe = model.material("stripe", ALLIANCE_STRIPE)
    glass = model.material("glass", GLASS)
    metal = model.material("metal", ENGINE_METAL)
    exhaust = model.material("exhaust", (1.0, 0.45, 0.3), 6.0)
    model.hull([(4.8, 0, 0.15), (4.8, 0, -0.1), (-4.0, 3.2, 0.1), (-4.0, 3.2, -0.3), (-4.0, -3.2, 0.1),
                (-4.0, -3.2, -0.3), (-2.0, 0, 0.9), (-4.4, 0, 0.6)], hull)
    for side in (-1, 1):
        model.hull([(2.6, side * 0.5, 0.25), (-3.0, side * 2.0, 0.55), (-3.0, side * 2.6, 0.35),
                    (2.6, side * 0.8, 0.12)], stripe)
        model.cylinder((-4.8, side * 2.6, 0), (-1.0, side * 2.6, 0), 0.55, metal)
        model.cylinder((-4.92, side * 2.6, 0), (-4.8, side * 2.6, 0), 0.4, exhaust)
    model.ball((-0.6, 0, 0.75), 0.75, glass, scale=(1.4, 1.0, 0.6))


def bwing(model: Model) -> None:
    hull = model.material("hull", (0.62, 0.6, 0.56))
    dark = model.material("dark", (0.25, 0.25, 0.27))
    glass = model.material("glass", GLASS)
    exhaust = model.material("exhaust", (1.0, 0.45, 0.3), 6.0)
    model.panel([(-1.6, -8.0), (1.0, -8.0), (2.2, -1.0), (2.2, 1.5), (0.8, 7.2), (-1.4, 7.2)], 0.0, 0.5, hull)
    model.cylinder((-1.2, 0, 7.8), (2.6, 0, 7.8), 0.95, dark, end_radius=0.7)
    model.frustum(0.8, 2.4, (0.8, 0.7), (0.5, 0.4), glass, z=7.9)
    model.cylinder((-2.6, 0, 0), (2.4, 0, 0), 1.25, dark)
    model.cylinder((-2.72, 0, 0), (-2.6, 0, 0), 0.9, exhaust)
    for side in (-1, 1):
        model.box((-0.3, side * 2.6, -0.4), (1.8, 4.2, 0.3), hull)
        model.cylinder((-0.5, side * 4.6, -0.4), (2.8, side * 4.6, -0.4), 0.16, dark, segments=6)
    model.cylinder((0.0, 0, -7.6), (3.6, 0, -7.6), 0.25, dark, segments=8)


def tie_cockpit(model: Model, hull: int, glass: int, radius: float = 1.55) -> None:
    model.ball((0, 0, 0), radius, hull)
    model.cylinder((radius * 0.75, 0, 0), (radius * 1.02, 0, 0), radius * 0.55, glass, segments=8)


def tie_fighter(model: Model) -> None:
    hull = model.material("hull", IMPERIAL_HULL)
    panel = model.material("panel", IMPERIAL_PANEL)
    glass = model.material("glass", GLASS)
    tie_cockpit(model, hull, glass)
    for side in (-1, 1):
        model.cylinder((0, side * 1.2, 0), (0, side * 3.1, 0), 0.42, hull, segments=8)
        model.panel(hexagon(3.75), side * 3.2, 0.18, panel, rim=hull)


def tie_interceptor(model: Model) -> None:
    hull = model.material("hull", IMPERIAL_HULL)
    panel = model.material("panel", IMPERIAL_PANEL)
    glass = model.material("glass", GLASS)
    tie_cockpit(model, hull, glass)
    dagger = [(3.6, 3.9), (0.3, 1.2), (0.3, -1.2), (3.6, -3.9), (-1.6, -2.3), (-1.6, 2.3)]
    for side in (-1, 1):
        model.cylinder((0, side * 1.2, 0), (0, side * 3.1, 0), 0.42, hull, segments=8)
        model.panel(dagger, side * 3.2, 0.18, panel, rim=hull)
        for z in (-3.9, 3.9):
            model.cylinder((2.0, side * 3.2, z), (4.4, side * 3.2, z), 0.12, hull, segments=6)


def tie_bomber(model: Model) -> None:
    hull = model.material("hull", IMPERIAL_HULL)
    panel = model.material("panel", IMPERIAL_PANEL)
    glass = model.material("glass", GLASS)
    model.cylinder((-2.6, 1.0, 0), (2.6, 1.0, 0), 1.2, hull)
    model.ball((2.6, 1.0, 0), 1.2, hull, scale=(0.7, 1, 1))
    model.cylinder((3.2, 1.0, 0), (3.55, 1.0, 0), 0.6, glass, segments=8)
    model.cylinder((-3.9, -1.0, 0), (3.6, -1.0, 0), 1.0, hull)
    model.box((0, 0, 0), (2.2, 1.6, 0.8), hull)
    bend = math.radians(18)
    for side in (-1, 1):
        model.cylinder((0, side * 1.8, 0), (0, side * 2.9, 0), 0.35, hull, segments=8)
        outline = [(x, z) for x, z in hexagon(3.4) if z >= 0]
        for sign in (1, -1):
            half = [(x, sign * z) for x, z in outline]
            pivot = Matrix.Translation((0, side * 3.0, 0))
            tilt = Matrix.Rotation(-side * sign * bend, 4, "X")
            model.panel(half, 0.0, 0.18, panel, rim=hull, matrix=pivot @ tilt)


def tie_defender(model: Model) -> None:
    hull = model.material("hull", IMPERIAL_HULL)
    panel = model.material("panel", IMPERIAL_PANEL)
    glass = model.material("glass", GLASS)
    tie_cockpit(model, hull, glass, radius=1.4)
    model.cylinder((-4.0, 0, 0), (-0.6, 0, 0), 0.95, hull, end_radius=1.2)
    for wing in range(3):
        roll = Matrix.Rotation(math.radians(wing * 120), 4, "X")
        model.panel([(1.6, 1.2), (0.6, 4.6), (-1.8, 4.6), (-2.9, 1.2)], 0.0, 0.2, panel, rim=hull, matrix=roll)
        tip = roll @ Vector((0.0, 0.0, 4.6))
        model.cylinder((-1.5, tip.y, tip.z), (3.2, tip.y, tip.z), 0.18, hull, segments=6)


BUILDERS: dict[str, Callable[[Model], None]] = {
    "xwing": xwing, "ywing": ywing, "awing": awing, "bwing": bwing, "tie_fighter": tie_fighter,
    "tie_interceptor": tie_interceptor, "tie_bomber": tie_bomber, "tie_defender": tie_defender,
}


def build(slug: str) -> bpy.types.Object:
    model = Model(slug)
    BUILDERS[slug](model)
    return model.finish()
