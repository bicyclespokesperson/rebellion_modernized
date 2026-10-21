"""Shared bpy building blocks for tactical ship recipes: meshes from code, materials, detail, final assembly.

Model frame: +X forward, +Z up, +Y to port, one unit per tactical model unit.
"""
from __future__ import annotations

import math
from collections.abc import Callable, Iterable, Sequence

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

Vec3 = tuple[float, float, float]


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def linear(hex_colour: str) -> tuple[float, float, float, float]:
    def channel(c: float) -> float:
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    rgb = [int(hex_colour[i:i + 2], 16) / 255 for i in (1, 3, 5)]
    return (*(channel(c) for c in rgb), 1.0)


def mix(a: Sequence[float], b: Sequence[float], t: float) -> tuple[float, float, float, float]:
    return (*(a[i] * (1 - t) + b[i] * t for i in range(3)), 1.0)


def scaled(c: Sequence[float], k: float) -> tuple[float, float, float, float]:
    return (*(min(1.0, c[i] * k) for i in range(3)), 1.0)


def saturated(c: Sequence[float], k: float) -> tuple[float, float, float, float]:
    """Push a colour's chroma away from its own grey by factor k, keeping its luminance."""
    grey = luminance(c)
    return (*(min(1.0, max(0.0, grey + (c[i] - grey) * k)) for i in range(3)), 1.0)


def luminance(c: Sequence[float]) -> float:
    return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]


def relit(c: Sequence[float], target: float) -> tuple[float, float, float, float]:
    """The same hue and saturation at luminance `target` (linear)."""
    k = target / max(luminance(c), 1e-6)
    return (*(min(1.0, c[i] * k) for i in range(3)), 1.0)


class Palette:
    """Encyclopedia-sampled hull colours (linear RGBA): light, mid and dark luminance bands, an accent, and
    clustered tints sorted dark to light, each scaled by the tool's per-channel calibration gain (sRGB)."""

    def __init__(self, sampled: dict[str, object]):
        gain = [g ** 2.2 for g in sampled["gain"]]

        def calibrated(hex_colour: str) -> tuple[float, float, float, float]:
            return (*(min(1.0, c * g) for c, g in zip(linear(hex_colour), gain)), 1.0)

        self.light = calibrated(sampled["light"])
        self.mid = calibrated(sampled["mid"])
        self.dark = calibrated(sampled["dark"])
        self.accent = calibrated(sampled["accent"])
        self.tints = [calibrated(t) for t in sampled["tints"]]


# Materials ----------------------------------------------------------------------------------------------------------

def _node_material(name: str) -> tuple[bpy.types.Material, bpy.types.NodeTree, bpy.types.Node]:
    material = bpy.data.materials.new(name)
    tree = material.node_tree
    bsdf = tree.nodes["Principled BSDF"]
    return material, tree, bsdf


def plain(name: str, colour: Sequence[float], roughness: float = 0.6, metallic: float = 0.0) -> bpy.types.Material:
    material, _, bsdf = _node_material(name)
    bsdf.inputs["Base Color"].default_value = colour
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    return material


def emissive(name: str, colour: Sequence[float], strength: float) -> bpy.types.Material:
    material, _, bsdf = _node_material(name)
    bsdf.inputs["Base Color"].default_value = colour
    bsdf.inputs["Emission Color"].default_value = colour
    bsdf.inputs["Emission Strength"].default_value = strength
    material["tactical_emissive"] = True
    return material


def _object_coords(tree: bpy.types.NodeTree, scale: float) -> bpy.types.NodeSocket:
    coords = tree.nodes.new("ShaderNodeTexCoord")
    mapping = tree.nodes.new("ShaderNodeMapping")
    mapping.inputs["Scale"].default_value = (scale, scale, scale)
    tree.links.new(coords.outputs["Object"], mapping.inputs["Vector"])
    return mapping.outputs["Vector"]


def _colour_ramp(tree: bpy.types.NodeTree, stops: Sequence[tuple[float, Sequence[float]]]) -> bpy.types.Node:
    ramp = tree.nodes.new("ShaderNodeValToRGB")
    elements = ramp.color_ramp.elements
    elements[0].position, elements[0].color = stops[0][0], stops[0][1]
    elements[1].position, elements[1].color = stops[-1][0], stops[-1][1]
    for position, colour in stops[1:-1]:
        element = elements.new(position)
        element.color = colour
    return ramp


def plated(name: str, base: Sequence[float], panel_size: float, *, contrast: float = 0.12,
           seam: float = 0.55, roughness: float = 0.55, metallic: float = 0.15, grime: float = 0.1,
           bump: float = 0.25) -> bpy.types.Material:
    """Hard-surface plating: per-plate tone variation and dark seams from two offset brick layers in object
    space (plates read on the dorsal and side faces alike), plus low-frequency grime."""
    material, tree, bsdf = _node_material(name)
    links = tree.links
    vec = _object_coords(tree, 1.0 / panel_size)

    def bricks(on_walls: bool) -> bpy.types.Node:
        mapping = tree.nodes.new("ShaderNodeMapping")
        mapping.inputs["Rotation"].default_value = (math.pi / 2 if on_walls else 0.0, 0.0, 0.0)
        links.new(vec, mapping.inputs["Vector"])
        brick = tree.nodes.new("ShaderNodeTexBrick")
        brick.inputs["Scale"].default_value = 1.0
        brick.inputs["Mortar Size"].default_value = 0.012
        brick.inputs["Brick Width"].default_value = 1.6
        brick.inputs["Row Height"].default_value = 0.55
        brick.inputs["Bias"].default_value = 0.0
        brick.inputs["Color1"].default_value = (1 - contrast, 1 - contrast, 1 - contrast, 1)
        brick.inputs["Color2"].default_value = (1 + contrast, 1 + contrast, 1 + contrast, 1)
        brick.inputs["Mortar"].default_value = (seam, seam, seam, 1)
        brick.offset_frequency = 2
        links.new(mapping.outputs["Vector"], brick.inputs["Vector"])
        return brick

    top = bricks(on_walls=False)
    side = bricks(on_walls=True)
    normal_z = tree.nodes.new("ShaderNodeSeparateXYZ")
    geometry = tree.nodes.new("ShaderNodeNewGeometry")
    links.new(geometry.outputs["Normal"], normal_z.inputs["Vector"])
    abs_z = tree.nodes.new("ShaderNodeMath")
    abs_z.operation = "ABSOLUTE"
    links.new(normal_z.outputs["Z"], abs_z.inputs[0])
    facing = tree.nodes.new("ShaderNodeMath")
    facing.operation = "GREATER_THAN"
    facing.inputs[1].default_value = 0.6
    links.new(abs_z.outputs[0], facing.inputs[0])
    pattern = tree.nodes.new("ShaderNodeMix")
    pattern.data_type = "RGBA"
    links.new(facing.outputs[0], pattern.inputs["Factor"])
    links.new(side.outputs["Color"], pattern.inputs[6])
    links.new(top.outputs["Color"], pattern.inputs[7])

    noise = tree.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = 0.35
    noise.inputs["Detail"].default_value = 6.0
    links.new(vec, noise.inputs["Vector"])
    grime_mix = tree.nodes.new("ShaderNodeMix")
    grime_mix.data_type = "RGBA"
    grime_mix.blend_type = "MULTIPLY"
    grime_mix.inputs["Factor"].default_value = grime
    links.new(pattern.outputs[2], grime_mix.inputs[6])
    links.new(noise.outputs["Color"], grime_mix.inputs[7])

    tint = tree.nodes.new("ShaderNodeMix")
    tint.data_type = "RGBA"
    tint.blend_type = "MULTIPLY"
    tint.inputs["Factor"].default_value = 1.0
    tint.inputs[6].default_value = base
    links.new(grime_mix.outputs[2], tint.inputs[7])
    links.new(tint.outputs[2], bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic

    seam_mask = tree.nodes.new("ShaderNodeMix")
    seam_mask.data_type = "FLOAT"
    links.new(facing.outputs[0], seam_mask.inputs["Factor"])
    links.new(side.outputs["Fac"], seam_mask.inputs[2])
    links.new(top.outputs["Fac"], seam_mask.inputs[3])
    bump_node = tree.nodes.new("ShaderNodeBump")
    bump_node.inputs["Strength"].default_value = bump
    bump_node.inputs["Distance"].default_value = panel_size * 0.01
    bump_node.invert = True
    links.new(seam_mask.outputs[0], bump_node.inputs["Height"])
    links.new(bump_node.outputs["Normal"], bsdf.inputs["Normal"])
    return material


def organic(name: str, stops: Sequence[tuple[float, Sequence[float]]], cell_size: float, *,
            mottle_size: float = 40.0, roughness: float = 0.5, metallic: float = 0.1, seam: float = 0.86,
            bump: float = 0.15, port_size: float = 0.0, port_density: float = 0.28,
            port_colour: Sequence[float] = (0.03, 0.025, 0.025, 1.0),
            belly: tuple[float, float, float] | None = None) -> bpy.types.Material:
    """Mottled organic skin: noise drives a colour ramp of the sampled hull tints, Voronoi cells of about
    `cell_size` add thin recessed panel seams, an optional fine Voronoi lays scattered dark viewports, and
    `belly` = (z_low, z_high, factor) darkens the underside below z_high, fully at z_low."""
    material, tree, bsdf = _node_material(name)
    links = tree.links
    vec = _object_coords(tree, 1.0 / cell_size)
    noise = tree.nodes.new("ShaderNodeTexNoise")
    noise.inputs["Scale"].default_value = cell_size / mottle_size
    noise.inputs["Detail"].default_value = 5.0
    noise.inputs["Roughness"].default_value = 0.6
    links.new(vec, noise.inputs["Vector"])
    ramp = _colour_ramp(tree, stops)
    links.new(noise.outputs["Fac"], ramp.inputs["Fac"])

    cells = tree.nodes.new("ShaderNodeTexVoronoi")
    cells.feature = "DISTANCE_TO_EDGE"
    cells.inputs["Scale"].default_value = 1.0
    links.new(vec, cells.inputs["Vector"])
    groove = tree.nodes.new("ShaderNodeMapRange")
    groove.inputs["From Min"].default_value = 0.0
    groove.inputs["From Max"].default_value = 0.018
    groove.inputs["To Min"].default_value = 1.0
    groove.inputs["To Max"].default_value = 0.0
    links.new(cells.outputs["Distance"], groove.inputs["Value"])
    seam_tone = tree.nodes.new("ShaderNodeMapRange")
    seam_tone.inputs["To Min"].default_value = 1.0
    seam_tone.inputs["To Max"].default_value = seam
    links.new(groove.outputs["Result"], seam_tone.inputs["Value"])
    shade = tree.nodes.new("ShaderNodeMix")
    shade.data_type = "RGBA"
    shade.blend_type = "MULTIPLY"
    shade.inputs["Factor"].default_value = 1.0
    links.new(ramp.outputs["Color"], shade.inputs[6])
    links.new(seam_tone.outputs["Result"], shade.inputs[7])
    colour_out = shade.outputs[2]

    if belly:
        low, high, factor = belly
        coords = tree.nodes.new("ShaderNodeTexCoord")
        split = tree.nodes.new("ShaderNodeSeparateXYZ")
        links.new(coords.outputs["Object"], split.inputs["Vector"])
        fade = tree.nodes.new("ShaderNodeMapRange")
        fade.inputs["From Min"].default_value = low
        fade.inputs["From Max"].default_value = high
        fade.inputs["To Min"].default_value = factor
        fade.inputs["To Max"].default_value = 1.0
        links.new(split.outputs["Z"], fade.inputs["Value"])
        darken = tree.nodes.new("ShaderNodeMix")
        darken.data_type = "RGBA"
        darken.blend_type = "MULTIPLY"
        darken.inputs["Factor"].default_value = 1.0
        links.new(colour_out, darken.inputs[6])
        links.new(fade.outputs["Result"], darken.inputs[7])
        colour_out = darken.outputs[2]

    if port_size > 0:
        ports = tree.nodes.new("ShaderNodeTexVoronoi")
        ports.inputs["Scale"].default_value = cell_size / port_size
        ports.inputs["Randomness"].default_value = 1.0
        links.new(vec, ports.inputs["Vector"])
        dots = tree.nodes.new("ShaderNodeMath")
        dots.operation = "LESS_THAN"
        dots.inputs[1].default_value = 0.16
        links.new(ports.outputs["Distance"], dots.inputs[0])
        keep = tree.nodes.new("ShaderNodeMath")
        keep.operation = "GREATER_THAN"
        keep.inputs[1].default_value = 1.0 - port_density
        links.new(ports.outputs["Color"], keep.inputs[0])
        gate = tree.nodes.new("ShaderNodeMath")
        gate.operation = "MULTIPLY"
        links.new(dots.outputs[0], gate.inputs[0])
        links.new(keep.outputs[0], gate.inputs[1])
        port_mix = tree.nodes.new("ShaderNodeMix")
        port_mix.data_type = "RGBA"
        links.new(gate.outputs[0], port_mix.inputs["Factor"])
        links.new(colour_out, port_mix.inputs[6])
        port_mix.inputs[7].default_value = port_colour
        colour_out = port_mix.outputs[2]

    links.new(colour_out, bsdf.inputs["Base Color"])
    bsdf.inputs["Roughness"].default_value = roughness
    bsdf.inputs["Metallic"].default_value = metallic
    bump_node = tree.nodes.new("ShaderNodeBump")
    bump_node.inputs["Strength"].default_value = bump
    bump_node.inputs["Distance"].default_value = cell_size * 0.01
    bump_node.invert = True
    links.new(groove.outputs["Result"], bump_node.inputs["Height"])
    links.new(bump_node.outputs["Normal"], bsdf.inputs["Normal"])
    return material


# Geometry ------------------------------------------------------------------------------------------------------------

def mesh_object(name: str, verts: Sequence[Vec3], faces: Sequence[Sequence[int]],
                material: bpy.types.Material | None = None, smooth: bool = False) -> bpy.types.Object:
    mesh = bpy.data.meshes.new(name)
    mesh.from_pydata([tuple(v) for v in verts], [], [tuple(f) for f in faces])
    mesh.validate()
    bm = bmesh.new()
    bm.from_mesh(mesh)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    if material:
        mesh.materials.append(material)
    for polygon in mesh.polygons:
        polygon.use_smooth = smooth
    return obj


def loft(name: str, sections: Sequence[Sequence[Vec3]], material: bpy.types.Material | None = None,
         smooth: bool = False, cap: bool = True) -> bpy.types.Object:
    """Skin closed cross-section rings of equal size; caps both ends with n-gons."""
    n = len(sections[0])
    verts = [v for ring in sections for v in ring]
    faces = []
    for s in range(len(sections) - 1):
        a, b = s * n, (s + 1) * n
        faces += [(a + i, a + (i + 1) % n, b + (i + 1) % n, b + i) for i in range(n)]
    if cap:
        faces += [tuple(reversed(range(n))), tuple(range((len(sections) - 1) * n, len(sections) * n))]
    return mesh_object(name, verts, faces, material, smooth)


def prism(name: str, outline: Sequence[tuple[float, float]], z0: float, z1: float,
          material: bpy.types.Material | None = None, top_inset: float = 0.0) -> bpy.types.Object:
    """Extrude a convex XY outline from z0 to z1, optionally shrinking the top towards its centroid."""
    cx = sum(p[0] for p in outline) / len(outline)
    cy = sum(p[1] for p in outline) / len(outline)
    top = [(cx + (x - cx) * (1 - top_inset), cy + (y - cy) * (1 - top_inset)) for x, y in outline]
    return loft(name, [[(x, y, z0) for x, y in outline], [(x, y, z1) for x, y in top]], material)


def box(name: str, centre: Vec3, size: Vec3, material: bpy.types.Material | None = None) -> bpy.types.Object:
    hx, hy, hz = (s / 2 for s in size)
    outline = [(centre[0] + x, centre[1] + y) for x, y in ((-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy))]
    return prism(name, outline, centre[2] - hz, centre[2] + hz, material)


def cylinder(name: str, centre: Vec3, radius: float, depth: float, axis: Vec3 = (0, 0, 1),
             material: bpy.types.Material | None = None, segments: int = 24,
             cap_end: bool = True) -> bpy.types.Object:
    """Cylinder along `axis`; `cap_end=False` leaves the +axis end open, as for an engine bell."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segments, radius1=radius, radius2=radius, depth=depth)
    if not cap_end:
        top = [f for f in bm.faces if len(f.verts) == segments and f.calc_center_median().z > 0]
        bmesh.ops.delete(bm, geom=top, context="FACES_ONLY")
    rotation = Vector((0, 0, 1)).rotation_difference(Vector(axis)).to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation(centre) @ rotation, verts=bm.verts)
    return _from_bmesh(name, bm, material, smooth=True, smooth_quads_only=True)


def sphere(name: str, centre: Vec3, radius: float, material: bpy.types.Material | None = None,
           scale: Vec3 = (1, 1, 1), segments: int = 32, rings: int = 16, up: Vec3 = (0, 0, 1)) -> bpy.types.Object:
    """Ellipsoid; `up` turns its local Z (the third scale axis) to a surface normal, keeping local X forward.
    Built from an explicit latitude-longitude grid: bmesh's create_uvsphere welds its poles in an order that
    varies between runs, which made rebuilds irreproducible."""
    rotation = Vector((0, 0, 1)).rotation_difference(Vector(up).normalized()).to_matrix()
    place = Matrix.Translation(centre) @ rotation.to_4x4() @ Matrix.Diagonal((*scale, 1))
    verts = [(0.0, 0.0, -radius)]
    for ring in range(1, rings):
        polar = math.pi * ring / rings
        z, r = -radius * math.cos(polar), radius * math.sin(polar)
        verts += [(r * math.cos(2 * math.pi * k / segments), r * math.sin(2 * math.pi * k / segments), z)
                  for k in range(segments)]
    verts.append((0.0, 0.0, radius))
    top = len(verts) - 1

    def at(ring: int, k: int) -> int:
        return 1 + (ring - 1) * segments + k % segments

    faces = [(0, at(1, k + 1), at(1, k)) for k in range(segments)]
    faces += [(at(ring, k), at(ring, k + 1), at(ring + 1, k + 1), at(ring + 1, k))
              for ring in range(1, rings - 1) for k in range(segments)]
    faces += [(at(rings - 1, k), at(rings - 1, k + 1), top) for k in range(segments)]
    obj = mesh_object(name, [tuple(place @ Vector(v)) for v in verts], faces, material, smooth=True)
    return obj


def _from_bmesh(name: str, bm: bmesh.types.BMesh, material: bpy.types.Material | None,
                smooth: bool, smooth_quads_only: bool = False) -> bpy.types.Object:
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    if material:
        mesh.materials.append(material)
    for polygon in mesh.polygons:
        polygon.use_smooth = smooth and (len(polygon.vertices) == 4 or not smooth_quads_only)
    return obj


def bevel(obj: bpy.types.Object, width: float, segments: int = 1, angle_deg: float = 30.0) -> bpy.types.Object:
    modifier = obj.modifiers.new("bevel", "BEVEL")
    modifier.width = width
    modifier.segments = segments
    modifier.limit_method = "ANGLE"
    modifier.angle_limit = math.radians(angle_deg)
    modifier.harden_normals = False
    return obj


def apply_modifiers(obj: bpy.types.Object) -> bpy.types.Object:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    mesh = bpy.data.meshes.new_from_object(evaluated, preserve_all_data_layers=True, depsgraph=depsgraph)
    old = obj.data
    obj.modifiers.clear()
    obj.data = mesh
    bpy.data.meshes.remove(old)
    return obj


def remesh_smooth(obj: bpy.types.Object, voxel: float, smooth_iterations: int = 0,
                  smooth_factor: float = 0.5) -> bpy.types.Object:
    """Union overlapping closed shells into one surface (OpenVDB voxel remesh), then relax the creases where they
    meet into fillets, approximating an SDF smooth union."""
    remesh = obj.modifiers.new("remesh", "REMESH")
    remesh.mode = "VOXEL"
    remesh.voxel_size = voxel
    remesh.use_smooth_shade = True
    if smooth_iterations:
        relax = obj.modifiers.new("relax", "CORRECTIVE_SMOOTH")
        relax.smooth_type = "LENGTH_WEIGHTED"
        relax.use_only_smooth = True
        relax.iterations = smooth_iterations
        relax.factor = smooth_factor
        relax.scale = 0.0
    apply_modifiers(obj)
    return obj


def _array(collection, attribute: str, dtype, width: int = 1) -> np.ndarray:
    values = np.empty(len(collection) * width, dtype)
    collection.foreach_get(attribute, values)
    return values.reshape(-1, width) if width > 1 else values


def join(parts: Iterable[bpy.types.Object], name: str) -> bpy.types.Object:
    """Concatenate the parts' evaluated meshes, in list order, into one new object. Unlike bpy.ops.object.join,
    whose order varies between runs, this makes rebuilds reproducible."""
    parts = list(parts)
    materials: list[bpy.types.Material] = []
    coords, loop_verts, loop_starts, material_indices, smooth = [], [], [], [], []
    vert_offset = loop_offset = 0
    for part in parts:
        if part.modifiers:
            apply_modifiers(part)
        mesh = part.data
        matrix = np.array(part.matrix_world)
        coords.append(_array(mesh.vertices, "co", np.float64, 3) @ matrix[:3, :3].T + matrix[:3, 3])
        loop_verts.append(_array(mesh.loops, "vertex_index", np.int32) + vert_offset)
        loop_starts.append(_array(mesh.polygons, "loop_start", np.int32) + loop_offset)
        slots = []
        for material in mesh.materials:
            if material not in materials:
                materials.append(material)
            slots.append(materials.index(material))
        local = _array(mesh.polygons, "material_index", np.int32)
        material_indices.append(np.array(slots or [0], np.int32)[np.minimum(local, max(len(slots) - 1, 0))])
        smooth.append(_array(mesh.polygons, "use_smooth", bool))
        vert_offset += len(mesh.vertices)
        loop_offset += len(mesh.loops)
    mesh = bpy.data.meshes.new(name)
    mesh.vertices.add(vert_offset)
    mesh.loops.add(loop_offset)
    mesh.polygons.add(sum(len(m) for m in material_indices))
    mesh.vertices.foreach_set("co", np.concatenate(coords).astype(np.float32).ravel())
    mesh.loops.foreach_set("vertex_index", np.concatenate(loop_verts))
    mesh.polygons.foreach_set("loop_start", np.concatenate(loop_starts))
    mesh.polygons.foreach_set("material_index", np.concatenate(material_indices))
    mesh.polygons.foreach_set("use_smooth", np.concatenate(smooth))
    mesh.update()
    mesh.validate()
    for material in materials:
        mesh.materials.append(material)
    for part in parts:
        old = part.data
        bpy.data.objects.remove(part)
        if old.users == 0:
            bpy.data.meshes.remove(old)
    joined = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(joined)
    return joined


def finalize(parts: Iterable[bpy.types.Object], length: float, fit: str = "length") -> bpy.types.Object:
    """Join everything into the single `ship` object, origin at the bounding-box centre, scaled so its extent
    along +X ("length" fit) or largest axis ("size" fit) is `length`. Bounds come from vertices."""
    if fit not in ("length", "size"):
        raise ValueError(f"unknown model fit: {fit}")
    ship = join(parts, "ship")
    coords = [v.co for v in ship.data.vertices]
    low = Vector((min(c.x for c in coords), min(c.y for c in coords), min(c.z for c in coords)))
    high = Vector((max(c.x for c in coords), max(c.y for c in coords), max(c.z for c in coords)))
    extent = high.x - low.x if fit == "length" else max(high - low)
    ship.data.transform(Matrix.Scale(length / extent, 4) @ Matrix.Translation(-(low + high) / 2))
    ship.location = (0, 0, 0)
    ship.data.update()
    for collection_object in list(bpy.context.scene.collection.objects):
        if collection_object is not ship:
            bpy.data.objects.remove(collection_object)
    return ship


def profile(points: Sequence[tuple[float, float]]) -> Callable[[float], float]:
    """Smooth curve through (x, value) control points: monotone cubic Hermite (Fritsch-Carlson), so hull
    profiles never overshoot their control values; constant beyond the ends."""
    pts = sorted(points)
    xs, ys = [p[0] for p in pts], [p[1] for p in pts]
    slopes = [(ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]) for i in range(len(xs) - 1)]
    tangents = [slopes[0]] + [0.0 if a * b <= 0 else 2 / (1 / a + 1 / b) for a, b in zip(slopes, slopes[1:])]
    tangents.append(slopes[-1])

    def evaluate(x: float) -> float:
        if x <= xs[0]:
            return ys[0]
        if x >= xs[-1]:
            return ys[-1]
        i = max(j for j in range(len(xs) - 1) if xs[j] <= x)
        h = xs[i + 1] - xs[i]
        t = (x - xs[i]) / h
        h00, h10 = 2 * t ** 3 - 3 * t ** 2 + 1, t ** 3 - 2 * t ** 2 + t
        h01, h11 = -2 * t ** 3 + 3 * t ** 2, t ** 3 - t ** 2
        return h00 * ys[i] + h10 * h * tangents[i] + h01 * ys[i + 1] + h11 * h * tangents[i + 1]
    return evaluate


def superellipse_ring(x: float, half_width: float, top: float, bottom: float, exponent: float,
                      count: int) -> list[Vec3]:
    """Cross-section at x: |y/w|^n + |z/h|^n = 1, with separate heights above (top) and below (bottom, as a
    positive depth) the axis."""
    ring = []
    for i in range(count):
        a = 2 * math.pi * i / count
        c, s = math.cos(a), math.sin(a)
        y = half_width * math.copysign(abs(c) ** (2 / exponent), c)
        z = (top if s >= 0 else bottom) * math.copysign(abs(s) ** (2 / exponent), s)
        ring.append((x, y, z))
    return ring


def section_normal(half_width: float, height: float, exponent: float, y: float, z: float) -> Vec3:
    """Outward normal of a superellipse cross-section at (y, z), in the YZ plane."""
    ny = math.copysign(abs(y / half_width) ** (exponent - 1) / half_width, y)
    nz = math.copysign(abs(z / height) ** (exponent - 1) / height, z)
    length = math.hypot(ny, nz) or 1.0
    return (0.0, ny / length, nz / length)


def surface_height(half_width: float, height: float, exponent: float, y: float) -> float:
    """Height of a superellipse cross-section at lateral offset y."""
    u = min(1.0, abs(y) / half_width) if half_width > 0 else 1.0
    return height * (1 - u ** exponent) ** (1 / exponent)


# Detail --------------------------------------------------------------------------------------------------------------

Placement = tuple[Vec3, Vec3, float]


def boxes(name: str, placements: Iterable[Placement], material: bpy.types.Material) -> bpy.types.Object:
    """Greebles: one mesh of boxes, each given as (centre, size, rotation about Z)."""
    verts: list[Vec3] = []
    faces: list[tuple[int, ...]] = []
    for (cx, cy, cz), (sx, sy, sz), rot in placements:
        c, s = math.cos(rot), math.sin(rot)
        base = len(verts)
        for dz in (-sz / 2, sz / 2):
            for dx, dy in ((-sx / 2, -sy / 2), (sx / 2, -sy / 2), (sx / 2, sy / 2), (-sx / 2, sy / 2)):
                verts.append((cx + dx * c - dy * s, cy + dx * s + dy * c, cz + dz))
        faces += [(base + 3, base + 2, base + 1, base), (base + 4, base + 5, base + 6, base + 7)]
        faces += [(base + i, base + (i + 1) % 4, base + 4 + (i + 1) % 4, base + 4 + i) for i in range(4)]
    return mesh_object(name, verts, faces, material)
