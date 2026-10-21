"""Bulk Cruiser: solid faceted prow and a heavy stepped machinery stern."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 1
NAME = "Bulk Cruiser"
ENCYCLOPEDIA = "EDATA.043"
LENGTH = 1258.0
VIEW = {"yaw": (90,180), "pitch": (0,25)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull=kit.plated("bulk_cruiser_armour",palette.light,25,contrast=.08,seam=.65)
    metal=kit.plated("bulk_cruiser_machinery",palette.mid,14,contrast=.13)
    dark=kit.plain("bulk_cruiser_recess",kit.mix(palette.dark,palette.mid,.2),metallic=.4)
    bronze=kit.plain("bulk_cruiser_sensors",kit.mix(palette.mid,palette.accent,.4),metallic=.35)
    glow=kit.emissive("bulk_cruiser_exhaust",(.2,.43,1,1),3)
    stern=[(-629,158,119,-71),(-453,158,157,-93),(-289,149,122,-57),(-187,104,97,7)]
    parts: list[bpy.types.Object]=[kit.loft("stepped_drive_hull",[[(x,-w,hi),(x,w,hi),(-587 if x==-629 else x,w,lo),(-587 if x==-629 else x,-w,lo)] for x,w,hi,lo in stern],hull)]
    neck=[(-187,104,97,7),(-136,104,85,3),(37,92,80,-8),(185,75,77,16),(262,95,75,53)]
    parts.append(kit.loft("machinery_neck",[[(x,-w,hi),(x,w,hi),(x,w,lo),(x,-w,lo)] for x,w,hi,lo in neck],metal))
    parts.append(kit.loft("prow_roof",[[(x,-w,z),(x,w,z),(x,w,z-8),(x,-w,z-8)] for x,w,z in [(262,95,75),(451,114,110),(629,46,54)]],hull))
    parts.append(kit.mesh_object("solid_prow_front",[(629,-46,54),(629,46,54),(428,0,-157)],[(0,1,2)],hull))
    parts.append(kit.mesh_object("prow_lower_closure",[(262,-133,53),(262,133,53),(428,0,-157)],[(0,1,2)],metal))
    profile_sections=stern+neck[1:]

    def section_value(x: float,field: int) -> float:
        for left,right in zip(profile_sections,profile_sections[1:]):
            if x <= right[0]:
                t=max(0.,(x-left[0])/(right[0]-left[0]))
                return left[field]*(1-t)+right[field]*t
        return profile_sections[-1][field]

    def roof(x: float) -> float:
        return section_value(x,2)
    for side in (-1,1):
        points=[(262,side*133,53),(477,side*188,14),(428,side*3,-157),(629,side*46,54)]
        parts.append(kit.mesh_object("prow_cheek",points,[(0,1,2),(1,3,2)],hull))
        a,b=points[1],points[3]
        axis=tuple(b[i]-a[i] for i in range(3))
        centre=tuple((a[i]+b[i])/2 for i in range(3))
        parts.append(kit.cylinder("open_prow_bridge_edge",centre,3,sum(v*v for v in axis)**.5,axis,hull,segments=10))
        shoulder=[(262,side*95,75),(451,side*114,110),(477,side*188,14),(262,side*133,53)]
        parts.append(kit.mesh_object("prow_shoulder",shoulder,[(0,1,2,3)],hull))
        for x,z in [(-540,137),(-450,155),(-360,138)]:
            parts.append(kit.loft("stern_roof_panel", [[(px,side*87-50,roof(px)-1),(px,side*87+50,roof(px)-1),(px,side*87+50,roof(px)+4),(px,side*87-50,roof(px)+4)] for px in (x-27,x+27)],metal))
        parts.append(kit.sphere("stern_shoulder_pod",(-500,side*158,112),45,hull,scale=(1,.7,.35),up=(0,side,0)))
        for x in (-260,-290,-320):
            parts.append(kit.box("drive_transition_collar",(x,side*(section_value(x,1)+2),26),(12,10,60),metal))
        parts.append(kit.sphere("neck_blister",(-30,side*65,roof(-30)-4),30,hull,scale=(1.1,.7,.4)))
        parts.append(kit.cylinder("neck_feed_pipe",(25,side*82,22),6,225,(1,0,0),dark))
        parts.append(kit.loft("horizontal_shoulder_armour",[
            [(x,side*(section_value(x,1)-4),roof(x)-18),
             (x,side*(section_value(x,1)+15),roof(x)-35),
             (x,side*(section_value(x,1)+15),section_value(x,3)-10),
             (x,side*(section_value(x,1)-4),section_value(x,3)+6)]
            for x in (-305,-260,-205,-145)],hull))
    parts.append(kit.cylinder("sensor_stalk",(245,0,(roof(245)+143)/2),3,143-roof(245),material=bronze))
    parts.append(kit.sphere("sensor_dish",(245,0,143),43,bronze,scale=(1,.65,.15)))
    parts.append(kit.cylinder("ventral_sensor_stalk",(-180,0,-50),2,118,material=bronze))
    parts.append(kit.cylinder("ventral_sensor_dish",(-180,0,-110),14,4,(0,1,0),bronze))
    for y in (-96,0,96):
        for z in (-20,66):
            end_x=-629+(119-z)*42/190
            parts.append(kit.cylinder("engine_socket",(end_x-3,y,z),32,9,(-1,0,0),dark))
            parts.append(kit.cylinder("engine_rim",(end_x-9,y,z),27,7,(-1,0,0),hull,cap_end=False))
            parts.append(kit.cylinder("engine_light",(end_x-11,y,z),22,2,(-1,0,0),glow))
    return kit.finalize(parts,length)
