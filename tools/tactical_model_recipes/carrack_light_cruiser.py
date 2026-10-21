"""Carrack's armoured coach hull, dark forward glazing and four stern drives."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 18
NAME = "Carrack Light Cruiser"
ENCYCLOPEDIA = "EDATA.060"
LENGTH = 903.0
VIEW = {"yaw": (35, 85), "pitch": (0, 25)}


def section(x: float, width: float, top: float, bottom: float) -> list[kit.Vec3]:
    chamfer = min(2 if x < 220 else 14, (top-bottom)*.28)
    return [(x, -width*.98, bottom), (x, width*.98, bottom),
            (x, width, bottom+chamfer), (x, width, top-chamfer), (x, width*.98, top),
            (x, -width*.98, top), (x, -width, top-chamfer), (x, -width, bottom+chamfer)]


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    armour = kit.plated("carrack_armour", palette.light, 32, contrast=.09, seam=.65)
    panels = kit.plated("carrack_panel", kit.mix(palette.light, palette.mid, .3), 19, contrast=.08)
    dark = kit.plain("carrack_recess", kit.mix(palette.dark, palette.mid, .2), metallic=.35)
    bronze = kit.plain("carrack_bands", kit.linear("#665a45"), metallic=.35)
    glass = kit.plain("carrack_glazing", kit.linear("#101a25"), metallic=.5, roughness=.18)
    glow = kit.emissive("carrack_exhaust", (.22,.5,1,1), 3)
    stations = [(-451,105,66,-66),(-383,105,79,-89),(-327,105,89,-81),
                (-170,105,61,-67),(212,100,58,-68),(303,125,65,-74),
                (370,65,43,-79),(428,48,7,-51),(451,42,-8,-25)]
    def roof(x: float) -> float:
        for first, second in zip(stations, stations[1:]):
            if first[0] <= x <= second[0]:
                t = (x-first[0])/(second[0]-first[0])
                return first[2]*(1-t)+second[2]*t
        return stations[-1][2]

    profile = [(-451,105,66),(-327,105,89),(-170,105,61),(212,100,58),
               (303,81,65),(370,63,43),(451,42,-8),(428,48,-51),
               (364,65,-79),(231,100,-68),(-246,105,-67),(-384,105,-89),(-422,105,-66)]
    vertices = [(x,side*w,z) for side in (-1,1) for x,w,z in profile]
    count = len(profile)
    vertices += [(322,-130,-7),(322,130,-7)]
    faces = [(i,(i+1)%count,(i+1)%count+count,i+count) for i in range(count)]
    for side in range(2):
        offset = side*count
        faces.append(tuple(offset+i for i in (0,1,2,3,9,10,11,12)))
        faces += [(2*count+side,offset+i,offset+i+1) for i in range(3,9)]
        faces.append((2*count+side,offset+9,offset+3))
    hull = kit.mesh_object("armoured_hull", vertices, faces, armour)
    bow_armour = kit.plated("carrack_bow_armour", kit.mix(palette.mid, palette.light, .3), 31, contrast=.07)
    hull.data.materials.append(bow_armour)
    for polygon in hull.data.polygons:
        if sum(hull.data.vertices[index].co.x for index in polygon.vertices)/len(polygon.vertices) > 250:
            polygon.material_index = 1
    parts = [kit.bevel(hull, 1.5, 2)]
    for x in (-305,-204,-104,-4,96,196):
        top = roof(x)
        parts.append(kit.loft("armour_band", [section(x-4,106,top+3,-70),section(x+4,106,top+3,-70)], bronze))
    for side in (-1,1):
        for x in (-260,-150,-40,70,180):
            parts.append(kit.bevel(kit.box("flank_armour", (x,side*103,-5), (85,9,79), panels), 3, 2))
            parts.append(kit.box("lower_flank_rail", (x,side*109,-38), (80,4,5), bronze))
        parts.append(kit.sphere("aft_shoulder", (-360,side*106,27), 35, armour, scale=(1.45,.55,.65)))
    # Forward glazing follows the sloped roof rather than standing above it.
    parts.append(kit.loft("forward_viewports", [[(318,-49,59),(318,49,59),(360,40,45),(360,-40,45)],
                                               [(318,-49,63),(318,49,63),(360,40,49),(360,-40,49)]], glass))
    for x in (-245,-130,-15,100):
        z = max(roof(x-16), roof(x+16))+3
        sections = [[(end,-57,roof(end)-1), (end,57,roof(end)-1),
                     (end,57,roof(end)+3), (end,-57,roof(end)+3)] for end in (x-37,x+37)]
        parts.append(kit.loft("roof_panel", sections, panels))
        parts.append(kit.box("vent_foundation", (x,0,z-4), (32,40,12), panels))
        parts.append(kit.box("roof_vent", (x,0,z+3), (30,38,3), dark))
        for dx in (-10,0,10):
            parts.append(kit.box("vent_louvre", (x+dx,0,z+5), (3,38,2), bronze))
    parts.append(kit.cylinder("sensor_pedestal", (75,0,66), 6, 20, material=bronze))
    parts.append(kit.sphere("sensor_dish", (75,0,91), 17, panels, scale=(.23,1,1)))
    parts.append(kit.cylinder("ventral_sensor_mount", (-90,0,-70), 5, 12, material=bronze))
    parts.append(kit.sphere("ventral_sensor", (-90,0,-77), 11, bronze, scale=(1,.7,.4)))
    for y in (-54,54):
        for z in (-34,30):
            parts.append(kit.cylinder("drive_socket", (-444,y,z), 30, 18, (-1,0,0), dark))
            parts.append(kit.cylinder("drive_rim", (-451,y,z), 26, 9, (-1,0,0), bronze, cap_end=False))
            parts.append(kit.cylinder("drive_light", (-454,y,z), 21, 2, (-1,0,0), glow))
    return kit.finalize(parts, length)
