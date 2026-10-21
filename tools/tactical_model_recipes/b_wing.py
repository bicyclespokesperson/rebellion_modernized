"""B-wing: asymmetric blade, rotating command pod and clustered drive nacelles."""
from __future__ import annotations

import math

import bpy
import _kit as kit

TYPE_INDEX = 32
NAME = "B-wing"
ENCYCLOPEDIA = "EDATA.035"
LENGTH = 135.2
FIT = "size"
VIEW = {"yaw": (-85,85), "pitch": (0,35), "roll_seeds": (0,90,180,270)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull=kit.plated("bwing_ivory",kit.linear("#b0b0ad"),.4,contrast=.12,seam=.012)
    metal=kit.plated("bwing_exposed_drive",kit.linear("#6d6d69"),.2,contrast=.2)
    dark=kit.plain("bwing_recess",kit.linear("#20252b"),metallic=.35)
    glass=kit.plain("bwing_glazing",kit.linear("#13232b"),roughness=.2)
    teal=kit.plain("bwing_blue_identification",kit.linear("#326575"))
    red=kit.plain("bwing_red_roundel",kit.linear("#8d3529"))
    glow=kit.emissive("bwing_exhaust",(1,.2,.19,1),3)
    parts: list[bpy.types.Object]=[kit.loft("central_drive_housing",[[(x,-w,hi),(x,w,hi),(x,w,lo),(x,-w,lo)] for x,w,hi,lo in [(1.4,.9,.45,-.4),(.2,1.25,.75,-.7),(-1.5,1.0,.7,-.65),(-2.0,.7,.4,-.4)]],hull)]
    blade=[(-1.4,.7),(1.6,.7),(1.0,7.8),(-.1,9.1),(-1.1,8.75)]
    parts.append(kit.prism("long_asymmetric_blade",blade,-.16,.16,hull))
    parts.append(kit.prism("blade_tip_marking",[(-1.08,8.3),(.55,8.3),(-.1,9.11),(-1.1,8.75)],.155,.18,teal))
    parts.append(kit.box("blade_recess",(.8,4.9,.17),(.7,1.0,.04),dark))
    parts.append(kit.prism("flared_tip_armour",[(-1.1,8.3),(1.25,8.3),(1.25,8.65),(.4,9.2),(-1.5,9.2)],-.18,.21,teal))
    parts.append(kit.cylinder("blade_roundel",(-.1,6.9,.17),.18,.03,material=red))
    parts.append(kit.cylinder("tip_cannon",(-.52,8.9,-.02),.14,2.1,(1,0,0),metal))
    parts.append(kit.cylinder("tip_laser",(1.15,8.9,-.02),.055,1.3,(1,0,0),dark,segments=10))
    for side in (-1,1):
        outline=[(.9,side*.7),(-.7,side*.7),(-1.8,side*2.2),(-.6,side*2.3)]
        vertices=[(x,y,z) for y in (.29,.41) for x,z in outline]
        parts.append(kit.mesh_object("opened_crossfoil",vertices,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],hull))
        parts.append(kit.cylinder("crossfoil_gun",(-.1,.35,side*2.2),.085,4.5,(1,0,0),dark,segments=12))
    parts.append(kit.cylinder("command_pod_neck",(.55,-1.55,.05),.35,1.2,(0,1,0),metal))
    parts.append(kit.cylinder("command_pod",(1.0,-2.4,.05),.65,2.5,(1,0,0),hull))
    parts.append(kit.sphere("command_pod_nose",(2.28,-2.4,.05),.55,dark,scale=(1,1,1.05),up=(1,0,0)))
    for angle in (0,math.pi/3,2*math.pi/3,math.pi,4*math.pi/3,5*math.pi/3):
        stations=[(x,.55*math.sqrt(max(0,1-((x-2.28)/.5775)**2))+.012) for x in (2.84,2.75,2.55,2.28)]
        for a,b in zip(stations,stations[1:]):
            start=(a[0],-2.4+a[1]*math.cos(angle),.05+a[1]*math.sin(angle))
            end=(b[0],-2.4+b[1]*math.cos(angle),.05+b[1]*math.sin(angle))
            axis=tuple(end[i]-start[i] for i in range(3))
            centre=tuple((end[i]+start[i])/2 for i in range(3))
            parts.append(kit.cylinder("cockpit_radial_frame",centre,.022,sum(v*v for v in axis)**.5,axis,metal,segments=10))
    parts.append(kit.loft("cockpit_canopy",[[(x,-2.4-w,z-.05),(x,-2.4+w,z-.05),(x,-2.4+w*.7,z+.25),(x,-2.4-w*.7,z+.25)] for x,w,z in [(1.9,.25,.35),(.9,.44,.5),(.2,.35,.42)]],glass))
    def housing_width(x: float) -> float:
        return 1.25+(x-.2)*(.9-1.25)/1.2 if x>.2 else 1.25+(.2-x)*(1.0-1.25)/1.7
    gold=kit.plain("bwing_housing_inlay",kit.linear("#837454"))
    for side in (-1,1):
        for a,b in ((-.6,.2),(.2,.7)):
            points=[(x,side*(housing_width(x)+.006),z) for x,z in ((a,-.35),(b,-.35),(b,.5),(a,.5))]
            parts.append(kit.mesh_object("blue_housing_side",points,[(0,1,2,3)],teal))
        for x in (-.52,.58):
            parts.append(kit.cylinder("housing_inlay_strip",(x,side*(housing_width(x)+.012),.05),.022,.95,(0,0,1),gold,segments=10))
        for a,b in ((-.4,-.05),(.25,.55)):
            points=[(x,side*(housing_width(x)+.009),z) for x,z in ((a,-.08),(b,-.08),(b,.16),(a,.16))]
            parts.append(kit.mesh_object("housing_service_window",points,[(0,1,2,3)],dark))
        for x in (-.7,.8):
            parts.append(kit.cylinder("housing_face_border",(x,side*housing_width(x),.05),.085,1.05,(0,0,1),hull))
    parts.append(kit.mesh_object("dark_housing_forward_face",[(1.406,-.7,-.3),(1.406,.7,-.3),(1.406,.7,.32),(1.406,-.7,.32)],[(0,1,2,3)],dark))
    for y in (-1.8,1.8):
        parts.append(kit.sphere("rounded_shoulder_armour",(-.65,y,.05),.85,hull,scale=(.8,.8,.8)))
    for x in (1.7,1.2,.7):
        parts.append(kit.cylinder("cockpit_frame_ring",(x,-2.4,.05),.66,.07,(1,0,0),hull))
    for y in (-.55,.55):
        for z in (-.2,.55):
            parts.append(kit.cylinder("clustered_drive",(-1.5,y,z),.42,2.5,(1,0,0),metal))
            parts.append(kit.cylinder("drive_front_cowl",(-.22,y,z),.44,.15,(1,0,0),hull))
            parts.append(kit.cylinder("drive_rim",(-2.82,y,z),.36,.17,(-1,0,0),hull,cap_end=False))
            parts.append(kit.cylinder("drive_light",(-2.92,y,z),.28,.025,(-1,0,0),glow))
    for x in (-1.7,-1.25,-.8):
        parts.append(kit.box("drive_roof_rib",(x,0,1.0),(.13,1.5,.12),metal))
    return kit.finalize(parts,length,FIT)
