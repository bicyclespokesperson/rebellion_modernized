"""The Death Star for tactical sprites: a sphere with an equatorial trench and the superlaser dish facing +X.

`build("death_star")` returns one mesh object of unit radius; tools/tactical_sprites_bpy.py scales it to the
class diameter. The trench and dish are boolean cuts, so they catch the key light as real grooves.
"""
from __future__ import annotations

import math

import bmesh
import bpy
from mathutils import Matrix, Vector

HULL = (0.42, 0.44, 0.47)
TRENCH = (0.16, 0.17, 0.19)
DISH = (0.3, 0.32, 0.35)
LENS = (0.35, 1.0, 0.45)
DISH_LATITUDE = math.radians(28)


def material(name: str, colour: tuple[float, float, float], glow: float = 0.0) -> bpy.types.Material:
    result = bpy.data.materials.new(name)
    shader = result.node_tree.nodes["Principled BSDF"]
    shader.inputs["Base Color"].default_value = (*colour, 1.0)
    shader.inputs["Roughness"].default_value = 0.65
    if glow:
        shader.inputs["Emission Color"].default_value = (*colour, 1.0)
        shader.inputs["Emission Strength"].default_value = glow
    return result


def hull_material() -> bpy.types.Material:
    """Grey plating whose brightness varies in panels and latitude bands, so the sphere does not read as a ball."""
    result = material("death_star_hull", HULL)
    nodes, links = result.node_tree.nodes, result.node_tree.links
    shader = nodes["Principled BSDF"]
    coordinates = nodes.new("ShaderNodeTexCoord")
    cells = nodes.new("ShaderNodeTexVoronoi")
    cells.inputs["Scale"].default_value = 40.0
    cells.feature = "F1"
    ramp = nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].color = (0.27, 0.3, 0.34, 1.0)
    ramp.color_ramp.elements[1].color = (0.44, 0.47, 0.52, 1.0)
    links.new(coordinates.outputs["Object"], cells.inputs["Vector"])
    links.new(cells.outputs["Color"], ramp.inputs["Fac"])
    links.new(ramp.outputs["Color"], shader.inputs["Base Color"])
    return result


def mesh_object(name: str, mesh: bmesh.types.BMesh, materials: list[bpy.types.Material]) -> bpy.types.Object:
    data = bpy.data.meshes.new(name)
    bmesh.ops.recalc_face_normals(mesh, faces=mesh.faces)
    mesh.to_mesh(data)
    mesh.free()
    for item in materials:
        data.materials.append(item)
    for polygon in data.polygons:
        polygon.use_smooth = True
    item = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(item)
    return item


def sphere(name: str, radius: float, centre: Vector, segments: int, rings: int,
           materials: list[bpy.types.Material]) -> bpy.types.Object:
    mesh = bmesh.new()
    bmesh.ops.create_uvsphere(mesh, u_segments=segments, v_segments=rings, radius=radius,
                              matrix=Matrix.Translation(centre))
    return mesh_object(name, mesh, materials)


def cut(target: bpy.types.Object, cutter: bpy.types.Object) -> None:
    """Subtract `cutter` from `target` for good, then delete the cutter."""
    modifier = target.modifiers.new(cutter.name, "BOOLEAN")
    modifier.operation = "DIFFERENCE"
    modifier.solver = "EXACT"
    modifier.material_mode = "TRANSFER"
    modifier.object = cutter
    apply_modifiers(target)
    bpy.data.objects.remove(cutter)


def apply_modifiers(item: bpy.types.Object) -> None:
    bpy.context.view_layer.update()
    evaluated = item.evaluated_get(bpy.context.evaluated_depsgraph_get())
    mesh = bpy.data.meshes.new_from_object(evaluated)
    item.modifiers.clear()
    item.data = mesh


def build(slug: str) -> bpy.types.Object:
    station = sphere("ship", 1.0, Vector(), 128, 64, [hull_material()])
    trench = bmesh.new()
    bmesh.ops.create_cone(trench, cap_ends=True, segments=128, radius1=1.05, radius2=1.05, depth=0.022)
    inner = bmesh.new()
    bmesh.ops.create_cone(inner, cap_ends=True, segments=128, radius1=0.985, radius2=0.985, depth=0.04)
    ring = mesh_object("trench", trench, [material("trench", TRENCH)])
    cut(ring, mesh_object("trench_floor", inner, []))
    cut(station, ring)
    axis = Vector((math.cos(DISH_LATITUDE), 0.0, math.sin(DISH_LATITUDE)))
    cut(station, sphere("dish", 0.42, axis * 1.3, 128, 64, [material("dish", DISH)]))
    sphere("lens", 0.04, axis * 0.885, 16, 8, [material("lens", LENS, 1.5)]).parent = station
    station.data.set_sharp_from_angle(angle=math.radians(30))
    return station
