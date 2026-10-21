"""Galleon's broad freight hull, stepped forward neck and low dorsal navigation mast."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 23
NAME = "Galleon"
ENCYCLOPEDIA = "EDATA.066"
LENGTH = 819.0
VIEW = {"yaw": (30, 85), "pitch": (0, 30)}


def section(x: float, width: float, inner: float, top: float, shoulder: float,
            bottom: float, keel: float) -> list[kit.Vec3]:
    return [(x,0,keel),(x,width,bottom),(x,width,shoulder),(x,inner,top),
            (x,-inner,top),(x,-width,shoulder),(x,-width,bottom)]


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    armour = kit.plated("galleon_armour", palette.light, 27, contrast=.08,seam=.7)
    panel = kit.plated("galleon_panels",kit.mix(palette.light,palette.mid,.25),19,contrast=.08)
    dark = kit.plain("galleon_vents",kit.mix(palette.dark,palette.mid,.12),metallic=.3)
    trim = kit.plain("galleon_trim",kit.mix(palette.mid,palette.light,.3),metallic=.4)
    bronze = kit.plain("galleon_bronze",kit.linear("#6c5a3f"),metallic=.5)
    glass = kit.plain("galleon_glass",kit.linear("#18252b"),metallic=.5,roughness=.2)
    glow = kit.emissive("galleon_exhaust",(.2,.45,1,1),3)
    stations = [(-409,164,55,22,-2,-91,-123),(-288,176,38,91,-2,-91,-126),
                (31,209,83,90,-1,-93,-135),(73,214,70,90,-1,-93,-135),
                (125,137,58,90,18,-102,-130),(161,83,58,70,28,-102,-119),
                (236,61,60,28,27,-96,-96),(379,63,62,12,10,-85,-85),
                (409,66,65,9,8,-30,-30)]
    parts = [kit.bevel(kit.loft("freight_hull",[section(*s) for s in stations],armour),7,3)]
    parts.append(kit.loft("navigation_mast", [
        [(-288,-38,78),(-168,-18,78),(-168,18,78),(-288,38,78)],
        [(-256,-11,135),(-183,-11,135),(-183,11,135),(-256,11,135)]],panel))
    parts.append(kit.box("mast_cap",(-219,0,136),(70,24,3),trim))
    parts.append(kit.loft("command_glazing", [
        [(x,-44,z),(x,44,z),(x,44,z+2),(x,-44,z+2)]
        for x,z in ((164,68.5),(208,43.8))],glass))
    parts.append(kit.sphere("navigation_sensor",(144,30,83),7,trim,scale=(1,1,.7)))
    parts.append(kit.box("mast_signal_band",(-219,0,123),(75,26,6),dark))
    for y in (-26,26):
        parts.append(kit.box("forward_glazing",(355,y,15),(65,20,3),glass))
    for side in (-1,1):
        for x,w in ((-340,171),(-230,182),(-120,194),(-10,205)):
            parts.append(kit.box("cargo_door",(x,side*(w+1),-49),(75,4,59),panel))
            parts.append(kit.box("cargo_door_seam",(x,side*(w+4),-49),(3,2,55),dark))
            parts.append(kit.box("door_top_rail",(x,side*(w+3),-18),(76,4,4),trim))
        parts.append(kit.box("forward_cargo_vent",(120,side*139,-45),(6,9,54),dark))
        for x in (-380,-363,-346):
            parts.append(kit.box("aft_vent",(x,side*169,-40),(6,4,48),dark))
        parts.append(kit.cylinder("forward_bumper",(405,side*66,-11),5,55,(1,0,0),bronze))
    for x in (-220,-100,20):
        parts.append(kit.box("dorsal_panel",(x,0,92),(86,50,3),panel))
    for y in (-95,0,95):
        parts.append(kit.cylinder("drive_socket",(-403,y,-45),26,15,(-1,0,0),dark))
        parts.append(kit.cylinder("drive_rim",(-409,y,-45),22,7,(-1,0,0),trim,cap_end=False))
        parts.append(kit.cylinder("drive_light",(-411,y,-45),17,2,(-1,0,0),glow))
    return kit.finalize(parts,length)
