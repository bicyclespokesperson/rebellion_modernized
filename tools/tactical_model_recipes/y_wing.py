"""Y-wing: armoured command pod and exposed machinery between twin rail-cage drives."""
from __future__ import annotations

import math

import bpy
import _kit as kit

TYPE_INDEX = 30
NAME = "Y-wing"
ENCYCLOPEDIA = "EDATA.037"
LENGTH = 128.0
FIT = "size"
VIEW = {"yaw": (0,85), "pitch": (0,65)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull=kit.plated("ywing_weathered_ivory",palette.light,.4,contrast=.15,seam=.015)
    metal=kit.plated("ywing_exposed_drive",palette.mid,.22,contrast=.3)
    dark=kit.plain("ywing_recess",palette.dark,metallic=.45)
    glass=kit.plain("ywing_glazing",kit.linear("#101a20"),roughness=.2)
    gold=kit.plain("ywing_gold_identification",kit.linear("#8c7845"))
    glow=kit.emissive("ywing_exhaust",(1,.12,.16,1),3)
    parts: list[bpy.types.Object]=[kit.loft("command_pod",[[(x,-w,hi),(x,w,hi),(x,w,lo),(x,-w,lo)] for x,w,hi,lo in [(6.8,.28,.03,-.15),(6.2,.67,.22,-.3),(4.0,.82,.5,-.38),(2.5,.9,.52,-.4)]],hull)]
    parts.append(kit.loft("cockpit_canopy",[[(x,-w,lo),(x,w,lo),(x,w*.8,hi),(x,-w*.8,hi)] for x,w,hi,lo in [(5.3,.36,.48,.3),(3.4,.65,.85,.48),(2.7,.65,.85,.5)]],glass))
    frame_start,frame_end=(5.25,0,.5),(3.4,0,.875)
    frame_axis=tuple(frame_end[i]-frame_start[i] for i in range(3))
    frame_centre=tuple((frame_end[i]+frame_start[i])/2 for i in range(3))
    parts.append(kit.cylinder("canopy_central_frame",frame_centre,.035,sum(v*v for v in frame_axis)**.5,frame_axis,hull,segments=10))
    for side in (-1,1):
        start,end=(5.3,side*.288,.49),(3.4,side*.52,.865)
        axis=tuple(end[i]-start[i] for i in range(3))
        centre=tuple((start[i]+end[i])/2 for i in range(3))
        parts.append(kit.cylinder("canopy_side_frame",centre,.035,sum(v*v for v in axis)**.5,axis,hull,segments=10))
    parts.append(kit.cylinder("canopy_transverse_frame",(4.4,0,.665),.035,.8,(0,1,0),hull,segments=10))
    parts.append(kit.box("cockpit_rear_frame",(2.74,0,.87),(.08,1.06,.06),hull))
    parts.append(kit.mesh_object("gold_nose_panel",[(6.3,-.45,.208),(6.3,.45,.208),(5.15,.47,.356),(5.15,-.47,.356)],[(0,1,2,3)],gold))
    parts.append(kit.box("neck_machinery",(1.2,0,-.03),(2.6,1.05,.55),metal))
    parts.append(kit.box("central_drive_bed",(-1.5,0,-.18),(3.3,3.4,.65),metal))
    parts.append(kit.box("cross_drive_spar",(-.8,0,-.2),(1.5,6.3,.38),metal))
    for y in (-.55,0,.55):
        parts.append(kit.cylinder("neck_feed",(1.1,y,.19),.075,2.8,(1,0,0),gold,segments=10))
    for i,x in enumerate((-2.35,-1.95,-1.45,-.9,-.35)):
        parts.append(kit.box("drive_service_unit",(x,(-1)**i*.25,.3 if i%2 else .22),(.28,.6,.35 if i%2 else .2),metal))
    parts.append(kit.box("central_reactor_service_cover",(-1.5,0,.28),(.9,1.2,.3),dark))
    for side in (-1,1):
        for y in (.5,.75,1.0):
            parts.append(kit.cylinder("exposed_deck_feed",(-1.0,side*y,.18),.045,3.4,(1,0,0),gold,segments=10))
        for x,y in ((-2.65,.6),(-1.9,1.05),(-.5,.8)):
            parts.append(kit.box("offset_service_cabinet",(x,side*y,.26),(.33,.25,.26),metal))
    parts.append(kit.cylinder("turret_base",(1.0,0,.39),.22,.3,material=metal))
    for y in (-.12,.12):
        parts.append(kit.cylinder("ion_cannon",(1.65,y,.56),.045,1.2,(1,0,0),dark,segments=10))
    for side in (-1,1):
        cy=side*3.05
        parts.append(kit.loft("tapered_drive_pylon",[
            [(x,side*1.4,-.38),(x,side*2.65,-.2),(x,side*2.65,.2),(x,side*1.4,.38)]
            for x in (-1.6,.35)],metal))
        parts.append(kit.cylinder("drive_head",(-.25,cy,0),.72,3.2,(1,0,0),hull))
        parts.append(kit.sphere("rounded_drive_nose",(1.35,cy,0),.72,hull,scale=(1,1,.45),up=(1,0,0)))
        for x in (.8,-1.55):
            parts.append(kit.cylinder("drive_head_band",(x,cy,0),.74,.13,(1,0,0),gold))
        parts.append(kit.cylinder("aft_drive_recess",(-1.89,cy,0),.61,.07,(-1,0,0),dark))
        parts.append(kit.cylinder("drive_emitter",(-1.95,cy,0),.24,.1,(-1,0,0),glow))
        parts.append(kit.cylinder("exposed_exhaust_core",(-2.28,cy,0),.1,.65,(-1,0,0),glow,segments=12))
        for angle in (math.pi/4,3*math.pi/4,5*math.pi/4):
            parts.append(kit.cylinder("head_service_pipe",(-.2,cy+.72*math.cos(angle),.72*math.sin(angle)),.035,2.3,(1,0,0),metal,segments=10))
        for angle in (0,math.pi/2,math.pi,3*math.pi/2):
            y=cy+.68*math.cos(angle)
            z=.68*math.sin(angle)
            parts.append(kit.cylinder("longitudinal_cage_rail",(-3.5,y,z),.055,3.3,(1,0,0),hull,segments=10))
        for x in (-5.08,-5.22):
            vertices=[(px,cy+.73*math.cos(i*math.tau/32),.73*math.sin(i*math.tau/32)) for px in (x-.055,x+.055) for i in range(32)]
            parts.append(kit.mesh_object("open_aft_ring",vertices,[(i,(i+1)%32,(i+1)%32+32,i+32) for i in range(32)],metal))
        for angle in (math.pi/4,3*math.pi/4,5*math.pi/4,7*math.pi/4):
            y=cy+.36*math.cos(angle)
            z=.36*math.sin(angle)
            parts.append(kit.cylinder("ring_radial_spoke",(-5.16,y,z),.045,.72,(0,math.cos(angle),math.sin(angle)),hull,segments=10))
        for y in (cy-.22,cy+.22):
            parts.append(kit.box("drive_access_panel",(-.1,y,.716),(.72,.24,.06),metal))
        parts.append(kit.cylinder("nose_laser",(7.0,side*.19,-.04),.045,.9,(1,0,0),dark,segments=10))
    return kit.finalize(parts,length,FIT)
