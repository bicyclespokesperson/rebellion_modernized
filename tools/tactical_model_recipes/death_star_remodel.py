"""Panelled station sphere with a cut equatorial trench and an inset superlaser bowl."""
from __future__ import annotations

import math
import bpy
from mathutils import Vector
import _kit as kit

TYPE_INDEX = 37
NAME = "Death Star"
ENCYCLOPEDIA = "EDATA.065"
FIT = "size"
LENGTH = 9600.0
VIEW = {"yaw": (-24,-22), "pitch": (5,7)}


def cut(target: bpy.types.Object, cutter: bpy.types.Object) -> None:
    modifier=target.modifiers.new("inset_cut", "BOOLEAN")
    modifier.operation="DIFFERENCE"
    modifier.solver="EXACT"
    modifier.material_mode="TRANSFER"
    modifier.object=cutter
    kit.apply_modifiers(target)
    bpy.data.objects.remove(cutter,do_unlink=True)


def panel_material(palette: kit.Palette) -> bpy.types.Material:
    base=kit.mix(palette.light,palette.mid,.35)
    material=kit.plain("station_panels",base,metallic=.25,roughness=.7)
    tree=material.node_tree
    nodes,links=tree.nodes,tree.links
    shader=nodes.get("Principled BSDF")
    coordinates=nodes.new("ShaderNodeTexCoord")
    normal=nodes.new("ShaderNodeVectorMath");normal.operation="NORMALIZE"
    links.new(coordinates.outputs["Object"],normal.inputs[0])
    separate=nodes.new("ShaderNodeSeparateXYZ")
    links.new(normal.outputs["Vector"],separate.inputs["Vector"])
    longitude=nodes.new("ShaderNodeMath");longitude.operation="ARCTAN2"
    links.new(separate.outputs["Y"],longitude.inputs[0]);links.new(separate.outputs["X"],longitude.inputs[1])
    latitude=nodes.new("ShaderNodeMath");latitude.operation="ARCSINE"
    links.new(separate.outputs["Z"],latitude.inputs[0])
    combine=nodes.new("ShaderNodeCombineXYZ")
    for source,target,scale in ((longitude,"X",18.0),(latitude,"Y",8.0)):
        multiply=nodes.new("ShaderNodeMath");multiply.operation="MULTIPLY";multiply.inputs[1].default_value=scale
        links.new(source.outputs[0],multiply.inputs[0]);links.new(multiply.outputs[0],combine.inputs[target])
    brick=nodes.new("ShaderNodeTexBrick")
    brick.inputs["Scale"].default_value=1
    brick.inputs["Brick Width"].default_value=1.8
    brick.inputs["Row Height"].default_value=.55
    brick.inputs["Mortar Size"].default_value=.02
    brick.inputs["Color1"].default_value=kit.mix(base,palette.mid,.35)
    brick.inputs["Color2"].default_value=kit.mix(base,palette.light,.25)
    brick.inputs["Mortar"].default_value=kit.mix(base,palette.dark,.28)
    links.new(combine.outputs["Vector"],brick.inputs["Vector"])
    bands=nodes.new("ShaderNodeMath");bands.operation="MULTIPLY";bands.inputs[1].default_value=18
    links.new(latitude.outputs[0],bands.inputs[0])
    wave=nodes.new("ShaderNodeMath");wave.operation="SINE"
    links.new(bands.outputs[0],wave.inputs[0])
    band_range=nodes.new("ShaderNodeMapRange")
    band_range.inputs["From Min"].default_value=-1;band_range.inputs["From Max"].default_value=1
    band_range.inputs["To Min"].default_value=.87;band_range.inputs["To Max"].default_value=1.07
    links.new(wave.outputs[0],band_range.inputs["Value"])
    modulation=nodes.new("ShaderNodeMixRGB");modulation.blend_type="MULTIPLY";modulation.inputs[0].default_value=1
    links.new(brick.outputs["Color"],modulation.inputs[1]);links.new(band_range.outputs[0],modulation.inputs[2])
    fine_coordinates=nodes.new("ShaderNodeVectorMath");fine_coordinates.operation="MULTIPLY"
    fine_coordinates.inputs[1].default_value=(2.7,2.2,1)
    links.new(combine.outputs["Vector"],fine_coordinates.inputs[0])
    fine=nodes.new("ShaderNodeTexBrick")
    fine.inputs["Scale"].default_value=1
    fine.inputs["Brick Width"].default_value=1.0
    fine.inputs["Row Height"].default_value=.45
    fine.inputs["Mortar Size"].default_value=.018
    fine.inputs["Color1"].default_value=(.82,.82,.82,1)
    fine.inputs["Color2"].default_value=(1.08,1.08,1.08,1)
    fine.inputs["Mortar"].default_value=(.74,.74,.74,1)
    links.new(fine_coordinates.outputs["Vector"],fine.inputs["Vector"])
    micro_panels=nodes.new("ShaderNodeMixRGB");micro_panels.blend_type="MULTIPLY";micro_panels.inputs[0].default_value=.65
    links.new(modulation.outputs["Color"],micro_panels.inputs[1]);links.new(fine.outputs["Color"],micro_panels.inputs[2])
    links.new(micro_panels.outputs["Color"],shader.inputs["Base Color"])
    bump=nodes.new("ShaderNodeBump");bump.inputs["Strength"].default_value=.12;bump.inputs["Distance"].default_value=2
    bump.invert=True
    links.new(brick.outputs["Fac"],bump.inputs["Height"]);links.new(bump.outputs["Normal"],shader.inputs["Normal"])
    return material


def ring(name: str, centre: Vector, axis: Vector, radius: float, thickness: float,
         material: bpy.types.Material) -> bpy.types.Object:
    side=axis.cross(Vector((0,0,1))).normalized()
    if side.length<.1:
        side=Vector((1,0,0))
    up=axis.cross(side).normalized()
    sections: list[list[kit.Vec3]]=[]
    for i in range(193):
        angle=2*math.pi*i/192
        radial=math.cos(angle)*side+math.sin(angle)*up
        sections.append([tuple(centre+(radius+thickness*math.cos(2*math.pi*j/8))*radial+
                               thickness*math.sin(2*math.pi*j/8)*axis) for j in range(8)])
    return kit.loft(name,sections,material,smooth=True,cap=False)


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    radius=length/2
    armour=panel_material(palette)
    trench=kit.plain("trench_shadow",kit.mix(palette.dark,palette.mid,.2),roughness=.8)
    bowl=kit.plated("dish_panels",kit.mix(palette.mid,palette.light,.25),70,contrast=.07,seam=.8,grime=.03)
    trim=kit.plain("station_groove_rims",kit.mix(palette.mid,palette.light,.4),metallic=.3)
    station=kit.sphere("station_shell",(0,0,0),radius,armour,segments=192,rings=96)
    cutter=kit.cylinder("equatorial_cut",(0,0,0),radius*1.03,48,material=trench,segments=256)
    inner=kit.cylinder("trench_depth",(0,0,0),radius-30,80,segments=256)
    cut(cutter,inner)
    cut(station,cutter)
    axis=Vector((math.cos(math.radians(28)),0,math.sin(math.radians(28))))
    dish_centre=axis*(radius*1.3)
    dish_radius=radius*.42
    dish=kit.sphere("superlaser_bowl_cut",tuple(dish_centre),dish_radius,bowl,segments=128,rings=64)
    cut(station,dish)
    parts=[station]
    rim_axial=(radius*radius+(radius*1.3)**2-dish_radius*dish_radius)/(2*radius*1.3)
    rim_radius=math.sqrt(radius*radius-rim_axial*rim_axial)
    parts.append(ring("dish_outer_rim",axis*rim_axial,axis,rim_radius,12,trim))
    for fraction in (.08,.16,.23):
        radial=radius*fraction
        axial=radius*1.3-math.sqrt(dish_radius*dish_radius-radial*radial)
        parts.append(ring("dish_concentric_groove",axis*axial,axis,radial,7,trench))
    side=Vector((0,1,0))
    up=axis.cross(side).normalized()
    for index in range(8):
        direction=side*math.cos(index*math.tau/8)+up*math.sin(index*math.tau/8)
        for step in range(8):
            points=[]
            for radial in (radius*(.04+.024*step),radius*(.04+.024*(step+1))):
                axial=radius*1.3-math.sqrt(dish_radius*dish_radius-radial*radial)
                points.append(axis*(axial+2)+direction*radial)
            segment=points[1]-points[0]
            parts.append(kit.cylinder("dish_radial_track",tuple((points[0]+points[1])/2),4,segment.length,tuple(segment),trench,segments=8))
    focus=axis*(radius*.881)
    parts.append(kit.cylinder("superlaser_focus",tuple(focus),radius*.022,12,tuple(axis),trench,segments=32))
    for z in (-31,31):
        r=math.sqrt(radius*radius-z*z)
        parts.append(ring("trench_edge",Vector((0,0,z)),Vector((0,0,1)),r,2,trim))
    return kit.finalize(parts,length,FIT)
