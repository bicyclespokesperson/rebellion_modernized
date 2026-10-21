"""Three radial pylons carrying six swept opaque solar blades around a ball cockpit."""
from __future__ import annotations

import math
import bpy
import _kit as kit
import _tie

TYPE_INDEX = 36
NAME = "TIE Defender"
ENCYCLOPEDIA = "EDATA.041"
FIT = "size"
LENGTH = 73.6
VIEW = {"yaw": (-65, -5), "pitch": (-20, 45), "roll_seeds": (-60, -30, 0, 30, 60, 90)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    m = _tie.materials(palette, "tie_defender")
    solar_shader=m.solar.node_tree.nodes.get("Principled BSDF")
    solar_shader.inputs["Metallic"].default_value=0
    solar_shader.inputs["Roughness"].default_value=.9
    solar_shader.inputs["Specular IOR Level"].default_value=.15
    parts = _tie.cockpit(m, radius=8.5)
    forward_tip=30.0
    aft_tip=-14.0
    cap_midpoint=(forward_tip+aft_tip)/2
    for degrees in (0,120,240):
        angle = math.radians(degrees)
        def point(x: float,t: float,r: float,offset: float=0) -> kit.Vec3:
            return _tie.transformed(x,t,r+offset,angle)
        central=[point(-7,-12,24),point(7,-12,24),point(7,12,24),point(-7,12,24)]
        parts.append(kit.loft("central_wing_web",[central,[point(x,t,24,.55) for x,t in ((-7,-12),(7,-12),(7,12),(-7,12))]],m.solar))
        for a,b in zip(central,central[1:]+central[:1]):
            parts.append(_tie.beam("web_frame",a,b,.55,m.frame))
        for t in (-4,4):
            parts.append(_tie.beam("web_crossbeam",point(-7,t,24,.7),point(7,t,24,.7),.5,m.frame))
        for direction in (-1,1):
            corners=[point(-7,direction*12,24),point(7,direction*12,24),
                     point(forward_tip,direction*23,30),point(aft_tip,direction*23,30)]
            parts.append(kit.loft("trapezoid_solar_cap",[corners,[(x,y+.35*math.cos(angle),z+.35*math.sin(angle)) for x,y,z in corners]],m.solar))
            for a,b in zip(corners,corners[1:]+corners[:1]):
                parts.append(_tie.beam("solar_cap_frame",a,b,.45,m.frame))
            parts.append(_tie.beam("cap_cell_divider",point(0,direction*12,24,.5),point(cap_midpoint,direction*23,30,.5),.25,m.frame))
            for x in (aft_tip,forward_tip):
                parts.append(_tie.beam("wingtip_cannon",point(x-2,direction*23,30),point(x+3,direction*23,30),.28,m.frame))
        parts.extend(_tie.pylon(m,24,angle))
    return kit.finalize(parts,length,FIT)
