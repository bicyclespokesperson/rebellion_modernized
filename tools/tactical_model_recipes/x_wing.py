"""X-wing: tapered command fuselage, four opened foils and four exposed drives."""
from __future__ import annotations

import math

import bpy
import _kit as kit

TYPE_INDEX = 29
NAME = "X-wing"
ENCYCLOPEDIA = "EDATA.036"
LENGTH = 100.0
FIT = "size"
VIEW = {"yaw": (0,85), "pitch": (0,65)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull=kit.plated("xwing_ivory",palette.light,.45,contrast=.1,seam=.012)
    machinery=kit.plated("xwing_drive_machinery",palette.mid,.22,contrast=.2)
    dark=kit.plain("xwing_recess",palette.dark,metallic=.3)
    glass=kit.plain("xwing_canopy",kit.linear("#10191c"),roughness=.22)
    red=kit.plain("xwing_red_markings",kit.linear("#88352d"))
    glow=kit.emissive("xwing_exhaust",(.9,.13,.24,1),3)
    stations=[(7.8,.18,.05,-.17),(6.6,.4,.17,-.4),(2.5,.68,.42,-.67),(.2,.8,.58,-.72),(-2.4,.85,.58,-.65),(-3.8,.58,.36,-.4)]
    rings=[]
    for x,w,hi,lo in stations:
        chamfer=min(.15,(hi-lo)*.22)
        rings.append([(x,-w*.7,hi),(x,w*.7,hi),(x,w,hi-chamfer),(x,w,lo+chamfer),(x,w*.7,lo),(x,-w*.7,lo),(x,-w,lo+chamfer),(x,-w,hi-chamfer)])
    parts: list[bpy.types.Object]=[kit.loft("rounded_command_fuselage",rings,hull)]
    parts.append(kit.sphere("rounded_nose_cap",(7.68,0,-.05),.22,hull,scale=(.8,.8,.6)))
    def section_value(x: float,field: int) -> float:
        for a,b in zip(stations,stations[1:]):
            if b[0] <= x <= a[0]:
                t=(x-b[0])/(a[0]-b[0])
                return a[field]*t+b[field]*(1-t)
        return stations[-1][field]

    parts.append(kit.loft("glazed_cockpit",[[(x,-w,lo),(x,w,lo),(x,w*.65,hi),(x,-w*.65,hi)] for x,w,hi,lo in [(3.1,.43,.6,.3),(1.2,.58,1.0,.45),(.35,.58,.96,.5)]],glass))
    parts.append(kit.cylinder("cockpit_transverse_frame",(1.2,0,1.012),.025,.78,(0,1,0),machinery,segments=10))
    for side in (-1,1):
        start,end=(3.1,side*.28,.61),(1.2,side*.377,1.012)
        axis=tuple(end[i]-start[i] for i in range(3))
        centre=tuple((start[i]+end[i])/2 for i in range(3))
        parts.append(kit.cylinder("cockpit_longitudinal_frame",centre,.025,sum(v*v for v in axis)**.5,axis,machinery,segments=10))
    for side in (-1,1):
        parts.append(kit.mesh_object("canopy_side_frame",[(3.12,side*.44,.32),(1.2,side*.59,.47),(.32,side*.59,.52),(.32,side*.4,.98),(1.2,side*.39,1.02)],[(0,1,2,3,4)],machinery))
        for a,b in [(.5,2.2),(2.4,5.8)]:
            points=[]
            for x,fraction in ((a,.22),(b,.22),(b,.72),(a,.72)):
                hi,lo=section_value(x,2),section_value(x,3)
                points.append((x,side*(section_value(x,1)+.006),lo+(hi-lo)*fraction))
            parts.append(kit.mesh_object("dark_sidewall_recess",points,[(0,1,2,3)],dark))
        for up in (-1,1):
            zroot=up*.38
            ztip=up*1.6
            outline=[(-2.8,side*.74,zroot),(-.8,side*.74,zroot),(-1.75,side*6.3,ztip),(-3.1,side*6.3,ztip)]
            ring=[(x,y,z-.07) for x,y,z in outline]+[(x,y,z+.07) for x,y,z in outline]
            parts.append(kit.mesh_object("open_sfoil",ring,[(0,3,2,1),(4,5,6,7),(0,1,5,4),(1,2,6,5),(2,3,7,6),(3,0,4,7)],hull))
            def wing_z(y: float) -> float:
                return zroot+(ztip-zroot)*(abs(y)-.74)/(6.3-.74)
            parts.append(kit.mesh_object("red_foil_identification",[(x,side*y,wing_z(y)+up*.075) for x,y in [(-2.55,4.5),(-1.42,4.5),(-1.68,5.85),(-2.88,5.85)]],[(0,1,2,3)],red))
            parts.append(kit.cylinder("foil_root_hinge",(-1.8,side*.78,zroot),.23,1.9,(1,0,0),machinery))
            cy,cz=side*1.43,(1.05 if up>0 else -.85)
            parts.append(kit.cylinder("drive_casing",(-2.05,cy,cz),.57,2.9,(1,0,0),hull))
            for x in (-3.15,-2.7,-1.25):
                parts.append(kit.cylinder("drive_band",(x,cy,cz),.59,.12,(1,0,0),machinery))
            parts.append(kit.cylinder("drive_intake",(-.57,cy,cz),.46,.035,(1,0,0),dark))
            parts.append(kit.cylinder("intake_centre",(-.54,cy,cz),.12,.04,(1,0,0),machinery))
            for angle in (0,math.tau/3,2*math.tau/3):
                axis=(0,math.cos(angle),math.sin(angle))
                parts.append(kit.cylinder("intake_radial_support",(-.54,cy+.27*axis[1],cz+.27*axis[2]),.045,.38,axis,hull,segments=10))
            parts.append(kit.cylinder("exhaust_nozzle",(-3.62,cy,cz),.47,.28,(-1,0,0),machinery,cap_end=False))
            parts.append(kit.cylinder("exhaust_light",(-3.78,cy,cz),.36,.035,(-1,0,0),glow))
            parts.append(kit.cylinder("wingtip_cannon_mount",(-2.2,side*6.25,ztip),.16,.9,(1,0,0),machinery))
            parts.append(kit.cylinder("laser_barrel",(.1,side*6.25,ztip),.055,4.6,(1,0,0),dark,segments=10))
            parts.append(kit.cylinder("laser_muzzle",(2.43,side*6.25,ztip),.08,.15,(1,0,0),hull,segments=10))
    parts.append(kit.cylinder("astromech_socket",(-.55,0,.6),.28,.16,material=machinery))
    parts.append(kit.sphere("astromech_dome",(-.55,0,.72),.26,hull,scale=(1,1,.7)))
    for x in (-1.2,-1.65,-2.1,-2.55):
        parts.append(kit.box("exposed_rear_deck",(x,0,.61),(.15,.95,.12),machinery))
    return kit.finalize(parts,length,FIT)
