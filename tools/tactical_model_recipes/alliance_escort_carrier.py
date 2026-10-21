"""Alliance carrier: broad aft flight deck, tapered hangar walls and a low command bow."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 4
NAME = "Alliance Escort Carrier"
ENCYCLOPEDIA = "EDATA.046"
LENGTH = 1414.0
VIEW = {"yaw": (0, 85), "pitch": (0, 30)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("carrier_armour",palette.light,25,contrast=.075,seam=.75)
    deck = kit.plated("carrier_deck",kit.mix(palette.mid,palette.light,.4),22,contrast=.1)
    dark = kit.plain("carrier_hangar_recess",kit.mix(palette.dark,palette.mid,.1),metallic=.3)
    hangar = kit.emissive("carrier_hangar_doors",kit.linear("#7890b0"),.55)
    glow = kit.emissive("carrier_exhaust",(.17,.4,1,1),3)
    windows = kit.emissive("carrier_command_glass",kit.linear("#487c89"),.4)
    sections = [(-707,438,514,482,22,-82),(-620,438,514,429,82,-82),(-180,438,514,429,82,-82),(528,43,149,149,55,-48),(622,25,108,108,67,-48),(707,1,70,70,55,25)]
    aft_sections = [(-707,438,514,482,22,-82),(-180,438,514,455,55,-82)]
    rings = [[(x,-top,hi),(x,top,hi),(x,mid,-12),(x,bottom,lo),(x,-bottom,lo),(x,-mid,-12)] for x,bottom,mid,top,hi,lo in aft_sections]
    parts: list[bpy.types.Object] = [kit.loft("aft_hangar_hull",rings,hull)]
    upper = [(-707,482,482,22),(-620,429,472,82),(-180,429,455,82),(528,127,149,82),(675,64,84,82),(707,70,70,55)]
    parts.append(kit.loft("tapered_flight_deck",[[(x,-w,z),(x,w,z),(x,b,min(z,55)),(x,-b,min(z,55))] for x,w,b,z in upper],hull))
    parts.append(kit.loft("forward_command_hull",[[(528,-149,55),(528,149,55),(528,43,-48),(528,-43,-48)],[(622,-107,55),(622,107,55),(622,25,-48),(622,-25,-48)],[(707,-70,55),(707,70,55),(707,0,25),(707,0,25)]],hull))
    lower_width = kit.profile([(x,bottom) for x,bottom,_,_,_,_ in sections])
    shoulder_width = kit.profile([(x,mid) for x,_,mid,_,_,_ in sections])
    roof_width = kit.profile([(x,top) for x,_,_,top,_,_ in sections])
    roof_height = kit.profile([(x,hi) for x,_,_,_,hi,_ in sections])
    keel_height = kit.profile([(x,lo) for x,_,_,_,_,lo in sections])

    def side_width(x: float, z: float) -> float:
        if x <= -180:
            hi = 22+(x+707)*33/527
            top = 482-(x+707)*27/527
            if z >= -12:
                t = (z+12)/(hi+12)
                return 514*(1-t)+top*t
            return 438+(514-438)*(z+82)/70
        if z >= -12:
            t = (z+12)/(roof_height(x)+12)
            return shoulder_width(x)*(1-t)+roof_width(x)*t
        t = (z-keel_height(x))/(-12-keel_height(x))
        return lower_width(x)*(1-t)+shoulder_width(x)*t

    def side_patch(name: str, side: int, x0: float, x1: float, z0: float, z1: float,
                   material: bpy.types.Material, offset: float = 1.0) -> bpy.types.Object:
        levels = [z0]+([-12.0] if z0 < -12 < z1 else [])+[z1]
        vertices = [(x,side*(side_width(x,z)+offset),z) for z in levels for x in (x0,x1)]
        faces = [(i,i+1,i+3,i+2) for i in range(0,len(vertices)-2,2)]
        return kit.mesh_object(name,vertices,faces,material)

    for side in (-1,1):
        for x in range(-600,-185,36):
            parts.append(side_patch("hangar_recess",side,x-15,x+15,-63,22,dark))
            parts.append(side_patch("hangar_door",side,x-11,x+11,-57,18,hangar,1.5))
            parts.append(side_patch("hangar_frame",side,x+14,x+18,-65,24,hull,2))
        parts.append(side_patch("longitudinal_bumper",side,-675,-185,-17,-9,dark,3))
        for x in (-545,-430,-315):
            parts.append(kit.box("flight_deck_hatch",(x,side*220,83),(90,75,3),deck))
    for side in (-1,1):
        for x in range(-150,505,28):
            def width_at(position: float, z: float) -> float:
                top = 429-(position+180)*(365/855)
                bottom = 455-(position+180)*(306/708)
                t = (z-55)/27
                return bottom*(1-t)+top*t
            points = [(position,side*(width_at(position,z)+1),z) for position,z in [(x-9,57),(x+9,57),(x+9,79),(x-9,79)]]
            parts.append(kit.mesh_object("flight_deck_side_bay",points,[(0,1,2,3)],hangar))
        glass_points: list[kit.Vec3] = []
        for x,z in ((540,20),(605,20),(605,31),(540,31)):
            top_width = 149-(x-528)*42/94
            bottom_width = 43-(x-528)*18/94
            width = bottom_width+(top_width-bottom_width)*(z+48)/103
            glass_points.append((x,side*(width+.7),z))
        parts.append(kit.mesh_object("forward_glazing",glass_points,[(0,1,2,3)],windows))
    for y in range(-408,409,36):
        for name,x,half_width,z0,z1,material in [("front_hangar_recess",-179,16,-67,40,dark),("front_hangar_door",-178.5,12,-60,34,hangar)]:
            points = [(x,y-half_width,z0),(x,y+half_width,z0),(x,y+half_width,z1),(x,y-half_width,z1)]
            parts.append(kit.mesh_object(name,points,[(0,1,2,3)],material))
        for z in (-28,8):
            parts.append(kit.box("hangar_cross_frame",(-177.5,y,z),(2,30,3),hull))
    parts.append(kit.box("dorsal_island",(-20,0,91),(135,60,22),hull))
    parts.append(kit.box("aft_offset_island",(-440,285,89),(130,85,18),hull))
    for x in (-540,-420,-300):
        parts.append(kit.box("dorsal_machinery",(x,0,86),(55,35,8),deck))
    for y in (-310,-155,0,155,310):
        parts.append(kit.cylinder("engine_socket",(-706,y,-20),31,10,(-1,0,0),dark))
        parts.append(kit.cylinder("engine_rim",(-713,y,-20),26,8,(-1,0,0),hull,cap_end=False))
        parts.append(kit.cylinder("engine_light",(-714,y,-20),21,2,(-1,0,0),glow))
    for part in parts:
        for vertex in part.data.vertices:
            vertex.co.y *= 1.04
            vertex.co.z *= 1.12
    return kit.finalize(parts,length)
