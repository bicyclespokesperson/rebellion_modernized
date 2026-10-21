"""Four forward swept solar blades on a compact spoke-framed cockpit."""
from __future__ import annotations

import bpy
import _kit as kit
import _tie

TYPE_INDEX = 34
NAME = "TIE Interceptor"
ENCYCLOPEDIA = "EDATA.039"
FIT = "size"
LENGTH = 76.8
VIEW = {"yaw": (-35, -5), "pitch": (0, 35)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    materials = _tie.materials(palette, "tie_interceptor")
    parts = _tie.cockpit(materials, radius=12)
    parts[0].scale.x=1.4
    parts[0].location.x=-4.8
    blades = [
        [(-10,0,24),(-10.8,23.3,18.2),(55.5,8.5,36.2),(-3.8,5.8,27.7),(-3.8,0,24)],
        [(-10,0,24),(4,0,24),(4,-6.9,30.3),(59.2,-9.7,34.4),(-4.4,-28.5,21.5)],
    ]
    for side in (-1,1):
        def point(vertex: tuple[float,float,float],offset: float=0) -> kit.Vec3:
            x,z,radial=vertex
            return x,side*(radial+offset),z
        for vertices in blades:
            parts.append(kit.loft("canted_solar_blade",[[point(v,d) for v in vertices] for d in (-.35,.35)],materials.solar))
            for a,b in zip(vertices,vertices[1:]+vertices[:1]):
                parts.append(_tie.beam("blade_edge",point(a),point(b),.48,materials.frame))
            for vertex in vertices:
                parts.append(_tie.beam("blade_cell_rib",point((0,0,24),.52),point(vertex,.52),.18,materials.frame))
        web=[blades[0][1],(-35,0,24),blades[1][-1],blades[1][2],(-2,0,27),blades[0][3]]
        parts.append(kit.loft("folded_central_wing_web",[[point(v,d) for v in web] for d in (-.5,.5)],materials.solar))
        for a,b in zip(web,web[1:]+web[:1]):
            parts.append(_tie.beam("wing_web_frame",point(a,.6),point(b,.6),.55,materials.frame))
        parts.append(_tie.beam("wing_pylon",(0,side*5,0),(0,side*24,0),3.2,materials.armour))
        parts.append(kit.cylinder("wing_collar",(0,side*23,0),4.7,4,(0,side,0),materials.frame,segments=16))
        parts.append(kit.box("armoured_wing_root",(-2,side*24,0),(17,7,10),materials.armour))
        for vertex in (blades[0][2],blades[1][3]):
            start=point(vertex)
            parts.append(_tie.beam("wingtip_cannon",(start[0]-3,start[1],start[2]),
                                   (start[0]+4,start[1],start[2]),.32,materials.frame))
    return kit.finalize(parts, length, FIT)
