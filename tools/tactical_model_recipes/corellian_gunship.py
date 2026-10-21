"""Corellian Gunship: round command pods, exposed necks and an armoured aft machinery block."""
from __future__ import annotations

import math

import bpy
import _kit as kit

TYPE_INDEX = 8
NAME = "Corellian Gunship"
ENCYCLOPEDIA = "EDATA.050"
LENGTH = 746.0
VIEW = {"yaw": (0, 85), "pitch": (0, 35)}


def barrel(name: str, stations: list[tuple[float, float]], material: bpy.types.Material) -> bpy.types.Object:
    sections = [[(x, r * math.cos(a * math.tau / 32), r * math.sin(a * math.tau / 32))
                 for a in range(32)] for x, r in stations]
    return kit.loft(name, sections, material, smooth=True)


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("gunship_ivory", kit.mix(palette.light, palette.mid, .18), 27, contrast=.07, seam=.65)
    plate = kit.plated("gunship_panels", palette.light, 18, contrast=.08, seam=.6)
    dark = kit.plain("gunship_mechanical", kit.mix(palette.dark, palette.mid, .35), metallic=.45)
    copper = kit.plated("gunship_copper", kit.linear("#886340"), 20, contrast=.1, seam=.6)
    red = kit.plain("gunship_rust_markings", kit.linear("#8e3928"))
    windows = kit.plain("gunship_windows", kit.linear("#202629"), metallic=.2)
    glow = kit.emissive("gunship_drive", (.3, .58, 1, 1), 2.4)
    parts = [barrel("forward_command", [(373, 42), (365, 46), (270, 64), (256, 65)], hull),
             barrel("forward_neck", [(256, 44), (151, 44)], dark),
             barrel("central_hull", [(134, 68), (0, 68), (-24, 46)], hull),
             barrel("central_collar", [(154, 46), (134, 68)], copper),
             barrel("aft_neck", [(-24, 43), (-120, 43)], dark)]
    aft = [(-373, 96, 62), (-357, 99, 60), (-140, 99, 60), (-118, 80, 52)]
    sections = [[(x, -w*.82, -h), (x, w*.82, -h), (x, w, -h*.65), (x, w, h*.65),
                 (x, w*.82, h), (x, -w*.82, h), (x, -w, h*.65), (x, -w, -h*.65)] for x,w,h in aft]
    parts.append(kit.bevel(kit.loft("aft_armour", sections, hull), 2, 2))
    for x, r in [(365, 46), (275, 63), (254, 65), (134, 68), (111, 68), (19, 68), (-3, 68)]:
        parts.append(barrel("pod_seam", [(x-1, r+.7), (x+1, r+.7)], dark))
    parts.append(barrel("bow_face", [(373.2, 34), (374, 34)], dark))
    parts.append(barrel("bow_face_ring", [(374.2, 28), (375, 28)], copper))
    parts.append(barrel("bow_viewport", [(375.2, 19), (375.7, 19)], windows))
    for x0,x1,r in [(158,248,44),(-118,-24,43)]:
        for a in range(8):
            angle = a*math.tau/8
            parts.append(kit.cylinder("neck_pipe", ((x0+x1)/2, r*math.cos(angle), r*math.sin(angle)),
                                      2.4, x1-x0, (1,0,0), plate, segments=12))
    for side in (-1,1):
        for x in (-335,-285,-234,-182):
            parts.append(kit.bevel(kit.box("aft_panel", (x,side*99,0), (42,4,88), plate), 2, 1))
            parts.append(kit.box("side_recess", (x,side*102,6), (18,2,42), dark))
            for z in (-14,0,14):
                parts.append(kit.box("recess_rib", (x,side*104,z), (15,3,3), hull))
        for x,r in [(80,68),(319,55)]:
            parts.append(kit.cylinder("pod_access", (x,side*(r-.5),0), 13, 3, (0,side,0), dark))
            parts.append(kit.cylinder("access_hatch", (x,side*(r+1),0), 10, 2, (0,side,0), plate))
        parts.append(kit.box("bow_stripe", (327,side*53,8), (23,2,7), red))
        parts.append(kit.box("aft_stripe", (-310,side*102,-35), (70,2,5), red))
    parts.append(kit.cylinder("central_turret_base", (70,0,69), 17, 5, material=copper))
    parts.append(kit.sphere("central_turret", (70,0,75), 13, hull, scale=(1,1,.75)))
    parts.append(kit.cylinder("central_gun", (91,0,78), 2.5, 31, (1,0,0), dark, segments=12))
    for side in (-1,1):
        parts.append(kit.bevel(kit.box("forward_machinery", (-141,side*87,0), (60,25,57), copper), 3, 1))
        for x in (-157,-140,-123):
            parts.append(kit.box("machinery_slots", (x,side*100.4,4), (8,2,22), dark))
    parts.append(kit.bevel(kit.box("dorsal_deck", (-244,0,67), (190,100,17), plate), 4, 2))
    parts.append(kit.box("bridge_foundation", (-250,0,76), (80,60,4), hull))
    parts.append(kit.bevel(kit.box("command_bridge", (-250,0,87), (88,69,21), hull), 5, 2))
    for side in (-1,1):
        parts.append(kit.box("bridge_windows", (-235,side*35,88), (42,2,6), windows))
    for x in (-290,-205):
        parts.append(kit.cylinder("dorsal_turret_pedestal", (x,0,85), 14, 24, material=hull))
        parts.append(kit.cylinder("ventral_turret_pedestal", (x,0,-61), 14, 6, material=hull))
        for zsign in (-1,1):
            turret_z = 99 if zsign == 1 else -65
            parts.append(kit.cylinder("turret_base", (x,0,turret_z), 14, 7, material=dark))
            parts.append(kit.sphere("turret", (x,0,turret_z+zsign*4), 12, hull, scale=(1,1,.65)))
            for y in (-4,4):
                parts.append(kit.cylinder("gun", (x+16,y,turret_z+zsign*5), 2, 26, (1,0,0), dark, segments=10))
    for y in (-59,0,59):
        for z in (-30,30):
            parts.append(kit.cylinder("drive_bell", (-372,y,z), 23, 7, (-1,0,0), dark, cap_end=False))
            parts.append(kit.cylinder("drive_glow", (-373,y,z), 18, 2, (-1,0,0), glow))
    for x in range(-330,-150,25):
        roof_height = 97.5 if -289 <= x-6 and x+6 <= -211 else 75.5
        parts.append(kit.box("dorsal_vent", (x,0,roof_height+1), (12,28,3), dark))
    return kit.finalize(parts, length)
