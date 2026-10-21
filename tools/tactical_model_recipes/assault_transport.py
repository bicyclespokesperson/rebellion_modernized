"""Assault transport: low command prow, stepped dorsal cabin and a ventral landing bay."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 22
NAME = "Assault Transport"
ENCYCLOPEDIA = "EDATA.064"
LENGTH = 535.0
VIEW = {"yaw": (0,85), "pitch": (0,40)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull=kit.plated("assault_transport_hull",palette.light,18,contrast=.08,seam=.65)
    metal=kit.plated("assault_transport_panels",palette.mid,12,contrast=.1)
    dark=kit.plain("assault_transport_recess",kit.mix(palette.dark,palette.mid,.1),metallic=.4)
    glass=kit.plain("assault_transport_glass",kit.linear("#182127"))
    glow=kit.emissive("assault_transport_exhaust",(.22,.45,1,1),3)
    parts: list[bpy.types.Object]=[kit.loft("low_fuselage",[[(x,-w,hi),(x,w,hi),(x,w,lo),(x,-w,lo)] for x,w,hi,lo in [(267,100,0,-23),(60,127,15,-34),(59,127,30,-34),(-59,141,30,-34),(-267,168,30,-34)]],hull)]
    parts.append(kit.loft("dorsal_cabin",[[(x,-wt,hi),(x,wt,hi),(x,wm,lo),(x,-wm,lo)] for x,wt,hi,wm,lo in [(-71,80,34,141,30),(-250,55,69,147,65),(-267,0,44,168,30)]],hull))
    parts.append(kit.loft("ventral_landing_bay",[[(x,-w,-31),(x,w,-31),(x,w,-79),(x,-w,-79)] for x,w in [(-18,137),(-202,158)]],metal))
    for side in (-1,1):
        points=[(x,side*(137+(-x-18)*21/184+.7),z) for x,z in ((-162,-70),(-48,-70),(-48,-40),(-162,-40))]
        parts.append(kit.mesh_object("landing_bay_grille",points,[(0,1,2,3)],dark))
        for x in range(-155,-44,14):
            parts.append(kit.box("bay_grille_rib",(x,side*(137+(-x-18)*21/184+1),-55),(3,3,30),hull))
        for x,y,z in [(180,127+(100-127)*(180-59)/208,-7),(25,127+(141-127)*(59-25)/118,-3),(-210,141+(168-141)*(210-59)/208,15)]:
            parts.append(kit.box("side_access_port",(x,side*y,z),(18,2,17),dark))
            parts.append(kit.cylinder("side_port_bolt",(x,side*(y+2),z),3,3,(0,side,0),hull,segments=12))
    parts.append(kit.loft("bow_command_spine",[
        [(x,-w,lo),(x,w,lo),(x,w*.72,hi),(x,-w*.72,hi)]
        for x,w,hi,lo in [(250,30,5,0),(185,43,15,5),(75,48,32,14),(58,48,30,15)]],hull))
    parts.append(kit.loft("raised_aft_command_spine",[
        [(x,-w,lo),(x,w,lo),(x,w*.7,hi),(x,-w*.7,hi)]
        for x,w,hi,lo in [(-60,35,51,29),(-210,32,85,56),(-250,28,79,65),(-263,20,47,34)]],hull))
    parts.append(kit.mesh_object("command_windshield",[(-60.1,-24,37),(-60.1,24,37),(-60.1,24,47),(-60.1,-24,47)],[(0,1,2,3)],glass))
    for side in (-1,1):
        parts.append(kit.loft("sloped_cabin_shoulder",[
            [(x,side*inner,lo),(x,side*outer,lo),(x,side*outer,hi-8),(x,side*inner,hi)]
            for x,inner,outer,hi,lo in [(-86,73,136,46,29),(-210,65,145,71,48),(-249,64,146,67,58)]],hull))
        parts.append(kit.mesh_object("neck_step_face",[(59.1,side*102,-32),(59.1,side*126,-32),(59.1,side*126,29),(59.1,side*102,29)],[(0,1,2,3)],dark))
    for y in (-112,0,112):
        parts.append(kit.cylinder("engine_socket",(-266,y,4),21,7,(-1,0,0),dark))
        parts.append(kit.cylinder("engine_rim",(-272,y,4),18,6,(-1,0,0),hull,cap_end=False))
        parts.append(kit.cylinder("engine_light",(-274,y,4),14,2,(-1,0,0),glow))
    return kit.finalize(parts,length)
