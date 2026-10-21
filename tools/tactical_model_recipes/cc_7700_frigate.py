"""CC-7700's broad command wedge, drooping gravity vanes and paired forward guns."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 10
NAME = "CC-7700 Frigate"
ENCYCLOPEDIA = "EDATA.052"
LENGTH = 996.0
VIEW = {"yaw": (30, 80), "pitch": (0, 30)}


def hull_section(x: float, width: float, roof_width: float, roof: float,
                 edge: float, bottom: float) -> list[kit.Vec3]:
    return [(x,-width,bottom),(x,width,bottom),(x,width,edge),
            (x,roof_width,roof),(x,-roof_width,roof),(x,-width,edge)]


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("cc7700_hull", palette.light, 29, contrast=.1, seam=.65)
    panel = kit.plated("cc7700_armour", kit.mix(palette.light,palette.mid,.25), 18, contrast=.08)
    dark = kit.plain("cc7700_recess", kit.mix(palette.dark,palette.mid,.12), metallic=.3)
    rim = kit.plain("cc7700_trim", kit.mix(palette.mid,palette.light,.25), metallic=.4)
    glass = kit.emissive("cc7700_windows", (.08,.65,.85,1), 1.8)
    glow = kit.emissive("cc7700_drive", (.15,.4,1,1), 3)
    parts = [kit.loft("command_wedge", [
        hull_section(-498,252,68,95,17,10),
        hull_section(-258,252,77,95,17,-62),
        hull_section(-221,252,68,95,17,-62),
        hull_section(44,252,133,95,17,-62),
        hull_section(498,120,119,-19,-19,-70)], hull)]
    # Vanes follow the original downward-canted slabs, not horizontal wings.
    for side in (-1,1):
        rings = [[(x,side*250,17),(x,side*408,-103),(x,side*378,-149),(x,side*251,-62)]
                 for x in (-258,-4)]
        parts.append(kit.loft("gravity_vane", rings, hull))
        for x in (-210,-90):
            # Disc axis is the normal of the vane's sloping upper face.
            parts.append(kit.cylinder("gravity_well_rim", (x,side*332,-42), 36, 5,
                                      (0,side*.6,.8), rim, segments=40))
            parts.append(kit.cylinder("gravity_well_recess", (x,side*334,-39), 27, 3,
                                      (0,side*.6,.8), dark, segments=40))
        parts.append(kit.box("flank_window", (200,side*207,-10), (18,3,12), glass))
    parts.append(kit.loft("aft_command_house", [
        [(-457,-214,29),(-302,-214,29),(-302,214,29),(-457,214,29)],
        [(-457,-214,123),(-302,-214,123),(-302,214,123),(-457,214,123)],
        [(-457,-138,149),(-291,-138,149),(-291,138,149),(-457,138,149)]], panel))
    for y in (-125,125):
        parts.append(kit.loft("gun_bed", [
            [(x,y-16,z-15),(x,y+16,z-15),(x,y+16,z),(x,y-16,z)]
            for x,z in ((-80,84),(140,78))], panel))
        parts.append(kit.cylinder("gun_collar", (105,y,80), 13, 55, (1,0,-.22), rim))
        parts.append(kit.cylinder("forward_barrel", (250,y,48), 6, 270, (1,0,-.22), panel))
        parts.append(kit.cylinder("barrel_muzzle", (381,y,19), 8, 7, (1,0,-.22), dark))
    parts.append(kit.prism("navigation_house", [(-190,-48),(-30,-48),(-30,48),(-190,48)], 94,130,panel,top_inset=.3))
    for side in (-1,1):
        for x in (-426,-404,-382,-360):
            parts.append(kit.box("command_louvre", (x,side*215,90), (8,3,35), dark))
        for x in (180,360):
            width = 252-(x-44)*132/454
            parts.append(kit.box("navigation_lamp", (x,side*(width+1),-27), (12,3,8), glass))
    for y0,y1 in ((-65,-10),(10,65)):
        parts.append(kit.loft("dorsal_inset", [
            [(x,y0,z),(x,y1,z),(x,y1,z+2),(x,y0,z+2)]
            for x,z in ((-180,95),(44,95),(380,10.6))], panel))
    parts.append(kit.box("navigation_viewport", (-29,0,107), (3,58,8), dark))
    for x in (-390,-340):
        parts.append(kit.box("command_roof_panel", (x,0,151), (34,200,3), rim))
    for y in (-45,45):
        parts.append(kit.cylinder("drive_socket", (-489,y,48), 28, 12,(-1,0,0),dark))
        parts.append(kit.cylinder("drive_rim", (-497,y,48), 24, 8,(-1,0,0),rim,cap_end=False))
        parts.append(kit.cylinder("drive_glow", (-499,y,48), 19, 2,(-1,0,0),glow))
    return kit.finalize(parts,length)
