"""A-wing: red command wedge, white shoulder armour and paired finned drives."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 31
NAME = "A-wing"
ENCYCLOPEDIA = "EDATA.034"
LENGTH = 76.8
FIT = "size"
VIEW = {"yaw": (0,85), "pitch": (0,65)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    white=kit.plated("awing_ivory",kit.linear("#b0ada3"),.45,contrast=.08,seam=.012)
    metal=kit.plated("awing_drive_machinery",kit.linear("#686762"),.18,contrast=.2)
    red=kit.plated("awing_red",kit.linear("#8b2928"),.5,contrast=.1,seam=.01)
    dark=kit.plain("awing_recess",palette.dark,metallic=.35)
    glass=kit.plain("awing_canopy",kit.linear("#142026"),roughness=.2)
    glow=kit.emissive("awing_exhaust",(1,.22,.18,1),3)
    stations=[(6.6,.56,.03,-.12),(4.5,1.35,.3,-.3),(1.4,2.45,.65,-.6),(-1.8,3.05,.6,-.55),(-3.8,2.75,.35,-.35)]
    parts: list[bpy.types.Object]=[kit.loft("command_wedge",[[(x,-w,hi),(x,w,hi),(x,w,lo),(x,-w,lo)] for x,w,hi,lo in stations],white)]
    parts.append(kit.loft("red_central_armour",[[(x,-w,hi-.035),(x,w,hi-.035),(x,w,hi+.035),(x,-w,hi+.035)] for x,w,hi in [(6.6,.56,.035),(4.5,1.05,.305),(1.4,1.45,.655),(-1.8,1.45,.605),(-3.8,1.1,.355)]],red))
    parts.append(kit.mesh_object("split_nose_seam",[(6.61,-.035,.08),(6.61,.035,.08),(3.2,.04,.46),(3.2,-.04,.46)],[(0,1,2,3)],dark))
    parts.append(kit.loft("cockpit",[[(x,-w,lo),(x,w,lo),(x,w*.75,hi),(x,-w*.75,hi)] for x,w,hi,lo in [(2.6,.33,.85,.5),(.5,.64,1.25,.6),(-.65,.65,1.1,.6)]],glass))
    parts.append(kit.box("canopy_rear_frame",(-.65,0,1.09),(.1,1.0,.08),metal))
    parts.append(kit.mesh_object("aft_command_recess",[(-.85,-1.2,.625),(-.85,1.2,.625),(-3.65,1.05,.38),(-3.65,-1.05,.38)],[(0,1,2,3)],dark))
    for side in (-1,1):
        start,end=(2.6,side*.25,.86),(.5,side*.48,1.26)
        axis=tuple(end[i]-start[i] for i in range(3))
        centre=tuple((start[i]+end[i])/2 for i in range(3))
        parts.append(kit.cylinder("canopy_longitudinal_frame",centre,.035,sum(v*v for v in axis)**.5,axis,metal,segments=10))
    parts.append(kit.cylinder("canopy_transverse_frame",(.5,0,1.26),.035,.96,(0,1,0),metal,segments=10))
    for side in (-1,1):
        y=side*2.38
        parts.append(kit.cylinder("drive_body",(-2.25,y,.03),.7,3.5,(1,0,0),dark))
        parts.append(kit.cylinder("forward_drive_cowl",(-.56,y,.03),.72,.5,(1,0,0),white))
        parts.append(kit.cylinder("rear_drive_cowl",(-3.94,y,.03),.75,.25,(-1,0,0),white))
        parts.append(kit.cylinder("exhaust_rim",(-4.12,y,.03),.6,.16,(-1,0,0),metal,cap_end=False))
        parts.append(kit.cylinder("exhaust_light",(-4.22,y,.03),.48,.03,(-1,0,0),glow))
        for x in (-1.5,-2.0,-2.5,-3):
            parts.append(kit.cylinder("drive_cooling_band",(x,y,.03),.715,.09,(1,0,0),metal))
        outline=[(-.9,.5),(-1.8,1.9),(-3.9,2.0),(-4.1,.45)]
        vertices=[(x,side*2.2+dy,z) for dy in (-.07,.07) for x,z in outline]
        parts.append(kit.mesh_object("vertical_tail_fin",vertices,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],white))
        parts.append(kit.cylinder("side_cannon_mount",(.15,side*2.9,-.05),.19,1.15,(1,0,0),metal))
        parts.append(kit.cylinder("side_laser",(1.35,side*2.9,-.05),.065,2.0,(1,0,0),dark,segments=10))
        parts.append(kit.box("shoulder_vent",(-.5,side*1.88,.64),(1.1,.22,.06),dark))
    return kit.finalize(parts,length,FIT)
