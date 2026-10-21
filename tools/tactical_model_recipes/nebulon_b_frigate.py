"""Nebulon-B: hanging medical prow, exposed spar and armoured seven-drive stern."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 3
NAME = "Nebulon-B Frigate"
ENCYCLOPEDIA = "EDATA.045"
LENGTH = 805.0
VIEW = {"yaw": (0, 85), "pitch": (0, 25)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("nebulon_hull", palette.light, 18, contrast=.08, seam=.65)
    machinery = kit.plated("nebulon_machinery", palette.mid, 12, contrast=.14)
    dark = kit.plain("nebulon_recess", kit.mix(palette.dark, palette.mid,.15), metallic=.3)
    windows = kit.emissive("nebulon_windows", kit.linear("#286c78"), .6)
    glow = kit.emissive("nebulon_exhaust", (.2,.45,1,1), 3)
    parts: list[bpy.types.Object] = []
    prow = [(402,1,108,108),(385,17,120,88),(350,32,128,64),(315,41,132,55),(280,39,140,72),(200,27,125,70),(94,23,107,90)]
    for name, sections in [("forward_shell",prow),("stern_armour",[(-402,52,140,31),(-390,74,140,31),(-285,74,140,31),(-270,52,140,31)])]:
        parts.append(kit.loft(name,[[(x,-w,hi),(x,w,hi),(x,w*.72,lo),(x,-w*.72,lo)] for x,w,hi,lo in sections],hull))
    parts.append(kit.loft("exposed_spinal_spar", [[(x,-12,lo),(x,12,lo),(x,12,hi),(x,-12,hi)] for x,lo,hi in [(110,87,107),(-280,90,112)]],machinery))
    parts.append(kit.loft("keel_column", [[(x-22,-10,z),(x+22,-10,z),(x+22,10,z),(x-22,10,z)] for x,z in [(245,90),(255,0),(283,-140)]],hull))
    parts.append(kit.box("lower_keel",(281,0,-127),(47,20,25),hull))
    for side in (-1,1):
        for x in range(-250,100,25):
            parts.append(kit.box("spar_ribs",(x,side*13,99+(110-x)*.009),(4,4,19),hull))
        parts.append(kit.box("spar_cable",(-78,side*15,98),(365,3,4),dark))
        for z, reach, height in [(60,70,10),(37,86,10),(14,73,12),(-13,90,16),(-38,70,9),(-59,80,9),(-83,66,12),(-106,86,14)]:
            centre_x = 271-z*.14
            parts.append(kit.bevel(kit.box("forward_terrace",(centre_x,side*12,z),(reach,12,height),hull),3,2))
            parts.append(kit.box("terrace_recess",(centre_x,side*19,z-4),(reach-12,2,4),dark))
        parts.append(kit.box("medical_viewport",(290,side*37,105),(38,2,15),windows))
        for x in (-380,-350,-320,-290):
            parts.append(kit.box("stern_band",(x,side*73,97),(5,4,43),machinery))
        parts.append(kit.cylinder("hanging_drive_strut",(-304,side*36,11),7,88,(.49,0,-.87),machinery))
        parts.append(kit.cylinder("stern_service_pod",(-325,side*51,45),15,67,(1,0,0),hull))
    parts.append(kit.box("stern_bridge",(-310,0,141),(67,50,14),hull))
    parts.append(kit.cylinder("stern_sensor",(-310,0,157),4,22,material=dark))
    parts.append(kit.box("lower_communications_crossbar",(282,0,-119),(12,94,5),dark))
    for side in (-1,1):
        for y in (22,30,38,46):
            parts.append(kit.box("crossbar_fins",(282,side*y,-115),(17,3,11),hull))
    roof = kit.profile([(x,hi) for x,_,hi,_ in prow])
    for x in (210,260,305):
        parts.append(kit.box("prow_roof_panel",(x,0,roof(x)+1),(22,20,5),machinery))
    for side in (-1,1):
        belt_vertices: list[kit.Vec3] = []
        for x,w,hi,lo in prow[:5]:
            for t in (.33,.63):
                belt_vertices.append((x,side*(w*(.72+.28*t)+.4),lo+(hi-lo)*t))
        parts.append(kit.mesh_object("dark_prow_belt",belt_vertices,[(i,i+1,i+3,i+2) for i in range(0,len(belt_vertices)-2,2)],dark))
    parts.append(kit.cylinder("communications_mast",(252,0,168),2,96,material=dark,segments=10))
    parts.append(kit.box("mast_foundation",(252,0,roof(252)+2),(18,16,12),hull))
    for y,z in [(0,113),(-24,105),(24,105),(-44,88),(44,88),(-22,65),(22,65)]:
        parts.append(kit.cylinder("engine_socket",(-400,y,z),14,8,(-1,0,0),dark))
        parts.append(kit.cylinder("engine_rim",(-404,y,z),12,4,(-1,0,0),hull,cap_end=False))
        parts.append(kit.cylinder("engine_glow",(-405,y,z),9,2,(-1,0,0),glow))
    return kit.finalize(parts,length)
