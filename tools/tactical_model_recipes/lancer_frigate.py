"""Lancer: narrow armoured trough, raised command prow and faceted drive pod."""
from __future__ import annotations

import math

import bpy
import _kit as kit

TYPE_INDEX = 16
NAME = "Lancer Frigate"
ENCYCLOPEDIA = "EDATA.058"
LENGTH = 807.0
VIEW = {"yaw": (0, 85), "pitch": (0, 30)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("lancer_hull", kit.mix(palette.light, palette.mid, .2), 24, contrast=.07, seam=.75)
    plate = kit.plated("lancer_panels", palette.light, 18, contrast=.06, seam=.7)
    dark = kit.plain("lancer_recesses", kit.mix(palette.dark, palette.mid, .2), metallic=.3)
    windows = kit.plain("lancer_windows", kit.linear("#242e37"))
    glow = kit.emissive("lancer_exhaust", (.23,.48,1,1), 2.8)
    sections = [
        [(403,-18,68),(403,18,68),(393,24,-2),(393,-24,-2)],
        [(288,-42,78),(288,42,78),(245,0,-74),(245,0,-74)],
        [(198,-42,40),(198,42,40),(198,42,-74),(198,-42,-74)],
        [(-140,-42,40),(-140,42,40),(-163,42,-74),(-163,-42,-74)],
    ]
    parts = [kit.loft("armoured_forebody", sections, hull)]
    rear = [(-405,38,44),(-386,53,63),(-350,70,80),(-320,74,84),(-265,68,78),(-215,55,65),(-185,47,54),(-140,42,40)]
    parts.append(kit.loft("faceted_drive_pod", [kit.superellipse_ring(x,w,h,h,2,16) for x,w,h in rear], hull))
    for side in (-1,1):
        parts.append(kit.box("longitudinal_shoulder", (22,side*42,24), (365,5,18), plate))
        parts.append(kit.box("flank_recess", (12,side*43,-20), (310,2,50), dark))
        for x in range(-125,176,30):
            parts.append(kit.box("flank_armour", (x,side*45,-20), (25,4,45), hull))
        for x in (-110,-40,30,100,165):
            parts.append(kit.box("roof_shoulder_panels", (x,side*31,41), (45,17,3), plate))
        window_vertices: list[kit.Vec3] = []
        for t,u in ((.12,.035),(.82,.035),(.82,.115),(.12,.115)):
            a,b,c = (403,18,68),(288,42,78),(245,0,-74)
            position = tuple((a[i]*(1-t)+b[i]*t)*(1-u)+c[i]*u for i in range(3))
            window_vertices.append((position[0],side*(position[1]+.3),position[2]))
        parts.append(kit.mesh_object("command_windows", window_vertices, [(0,1,2,3)], windows))
        for x in (-280,-330):
            parts.append(kit.box("drive_access_panel", (x,side*65,5), (25,8,30), plate))
    parts.append(kit.cylinder("command_access_hatch", (295,0,78), 15, 3, (.08,0,1), plate))
    parts.append(kit.box("dorsal_spine", (20,0,42), (365,18,8), plate))
    for x in range(-130,190,25):
        parts.append(kit.box("spine_vent", (x,0,47), (12,11,3), dark))
    for x,z in [(-315,78),(-220,63),(30,56)]:
        for sign in (-1,1):
            base_z = z if sign == 1 else (-78 if x == -315 else -60 if x == -220 else -74)
            if x == 30 and sign == 1:
                parts.append(kit.cylinder("gun_pedestal", (x,0,49), 10, 18, material=hull))
            parts.append(kit.cylinder("gun_foundation", (x,0,base_z), 13, 8, material=dark))
            parts.append(kit.sphere("quad_laser_turret", (x,0,base_z+sign*7), 13, hull, scale=(1,1,.75)))
            for y in (-4,4):
                for offset_z in (-2.2,2.2):
                    parts.append(kit.cylinder("laser_barrel", (x+14,y,base_z+sign*8+offset_z), 1.6, 22, (1,0,0), dark, segments=10))
    for y,z in [(0,22),(-19,-11),(19,-11)]:
        parts.append(kit.cylinder("engine_socket", (-402,y,z), 15, 9, (-1,0,0), dark, segments=24))
        parts.append(kit.cylinder("engine_rim", (-407,y,z), 13, 6, (-1,0,0), hull, segments=24, cap_end=False))
        parts.append(kit.cylinder("engine_light", (-408,y,z), 10, 2, (-1,0,0), glow, segments=24))
    return kit.finalize(parts, length)
