"""Bulwark's high folded cowl, armoured side pods and ribbed forward intake."""
from __future__ import annotations

import math
import bpy
import _kit as kit

TYPE_INDEX = 12
NAME = "Bulwark Battlecruiser"
ENCYCLOPEDIA = "EDATA.053"
LENGTH = 1559.0
VIEW = {"yaw": (10, 85), "pitch": (-45, 35)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull=kit.plated("bulwark_hull",palette.light,38,contrast=.1,seam=.7)
    panel=kit.plated("bulwark_panels",kit.mix(palette.light,palette.mid,.25),22,contrast=.09)
    dark=kit.plain("bulwark_recess",kit.mix(palette.dark,palette.mid,.1),metallic=.3)
    trim=kit.plain("bulwark_trim",kit.mix(palette.mid,palette.light,.3),metallic=.4)
    exhaust=kit.emissive("bulwark_exhaust",(.15,.4,1,1),3)
    width=kit.profile([(-780,1),(-710,80),(-409,120),(-205,227),(179,227),(357,220),(688,273),(780,136)])
    roof=kit.profile([(-780,327),(-410,350),(-205,370),(179,387),(357,327),(688,-72),(780,-183)])
    edge=kit.profile([(-780,326),(-710,260),(-409,193),(-205,-135),(179,-135),(357,-180),(688,-257),(780,-257)])
    stations=sorted(set([-780,-710,-409,-205,179,357,688,780]+list(range(-750,780,40))))
    sections: list[list[kit.Vec3]]=[]
    for x in stations:
        w,top,base=width(x),roof(x),edge(x)
        ring=[(x,-w*.82,base-12),(x,w*.82,base-12),(x,w,base)]
        ring += [(x,w*math.cos(math.pi*i/24),base+(top-base)*math.sin(math.pi*i/24)**.8)
                 for i in range(1,25)]
        sections.append(ring)
    cowl=kit.loft("rounded_armour_cowl",sections,hull,smooth=True)
    cowl.data.materials.append(dark)
    cowl.data.polygons[-1].material_index=1
    cowl.data.polygons[-1].use_smooth=False
    parts=[cowl,kit.loft("lower_drive_trunk",[
        [(-409,-105,192),(-409,105,192),(-409,105,194),(-409,-105,194)],
        [(-137,-185,-238),(-137,185,-238),(-205,180,-135),(-205,-180,-135)],
        [(152,-185,-238),(152,185,-238),(152,180,-135),(152,-180,-135)],
        [(780,-136,-279),(780,136,-279),(780,111,-269),(780,-111,-269)]],panel)]
    for side in (-1,1):
        vertices=[(-405,side*444,-183),(280,side*444,-183),(127,side*367,-387),
                  (-137,side*273,-258),(152,side*273,-258),(-205,side*227,-135),(152,side*227,-135)]
        faces=[(0,1,2),(0,2,3),(2,1,4),(0,3,5),(1,6,4),(0,5,6,1),(3,2,4),(3,4,6,5)]
        parts.append(kit.bevel(kit.mesh_object("armoured_side_pod",vertices,faces,panel),32,6))
        # Upper aft shoulders are independent armour lobes supported by the cowl.
        shoulder=[(-710,side*197,297),(-270,side*236,288),(-384,side*237,102),
                  (-409,side*105,193),(-299,side*56,327),(-574,side*68,294)]
        faces=[(0,1,2),(2,3,4),(1,4,2),(2,5,3),(0,4,1),(5,0,2),(5,4,0),(3,5,4)]
        parts.append(kit.bevel(kit.mesh_object("aft_shoulder",shoulder,faces,hull),16,4))
        for x in (-100,25,150):
            parts.append(kit.cylinder("pod_support",(x,side*247,-180),20,105,(0,side,0),trim))
    # The end cap is the intake; its ribs stand forward of the cap plane.
    for z in (-194,-203,-212,-221,-230,-239,-248):
        width_at_z=136*math.sqrt(max(0,1-((z+257)/74)**2))
        parts.append(kit.box("intake_louvre",(783,0,z),(4,width_at_z*1.9,2.5),trim))
    glazing=kit.emissive("bulwark_glazing",(.05,.38,.6,1),.65)
    def surface(x: float,y: float,offset: float=0) -> kit.Vec3:
        ratio=min(.999,abs(y)/width(x))
        return x,y,edge(x)+(roof(x)-edge(x))*(1-ratio*ratio)**.4+offset
    for side in (-1,1):
        for x in (-380,-230,-80):
            corners=[surface(xx,side*width(xx)*fraction,3)
                     for xx,fraction in ((x-18,.69),(x+18,.69),(x+18,.78),(x-18,.78))]
            parts.append(kit.loft("dorsal_louvre",[[(a,b,c-6) for a,b,c in corners],corners],dark))
            for dx in (-9,0,9):
                point=surface(x+dx,side*width(x+dx)*.735,5)
                parts.append(kit.cylinder("louvre_rib",point,1.6,width(x)*.06,(0,side,-.8),trim))
        corners=[surface(x,side*width(x)*fraction,3)
                 for x,fraction in ((-465,.5),(-435,.5),(-435,.55),(-465,.55))]
        parts.append(kit.loft("bow_glazing",[[(a,b,c-6) for a,b,c in corners],corners],glazing))
    for y in (-28,28):
        parts.append(kit.cylinder("drive_pod",(-700,y,275),23,80,(-1,0,0),panel))
        parts.append(kit.cylinder("drive_socket",(-730,y,275),19,14,(-1,0,0),dark))
        parts.append(kit.cylinder("drive_rim",(-745,y,275),19,8,(-1,0,0),trim,cap_end=False))
        parts.append(kit.cylinder("drive_glow",(-747,y,275),14,2,(-1,0,0),exhaust))
    return kit.finalize(parts,length)
