"""Corellian Corvette with a transverse command pod and eleven ribbed drive cans."""
from __future__ import annotations

import math
import bpy
import _kit as kit

TYPE_INDEX = 5
NAME = "Corellian Corvette"
ENCYCLOPEDIA = "EDATA.047"
LENGTH = 796.0
VIEW = {"yaw": (0, 90), "pitch": (0, 35)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("ivory plating", palette.light, panel_size=18)
    dark = kit.plated("recessed machinery", palette.dark, panel_size=9)
    red = kit.plated("ochre red markings", kit.mix(palette.mid, kit.linear("#984d32"), .7), panel_size=15)
    glow = kit.emissive("ion drive glow", kit.linear("#ffac64"), 2.2)
    parts: list[bpy.types.Object] = []
    sections = [(-265, 97, 47), (-195, 113, 53), (-140, 98, 33), (28, 47, 33),
                (208, 23, 33), (318, 23, 33)]
    parts.append(kit.loft("tapered engineering hull", [kit.superellipse_ring(x,w,h,h,3.5,24) for x,w,h in sections], hull))
    parts.append(kit.cylinder("neck", (242,0,0), 30, 155, (1,0,0), hull, 32))
    # The bow is a barrel across the ship, with the elliptical ends visible in the source view.
    rings = []
    for y, factor in [(-69, .82), (-61,1), (61,1), (69,.82)]:
        rings.append([(355+43*math.cos(a)*factor,y,36*math.sin(a)*factor) for a in [2*math.pi*i/32 for i in range(32)]])
    parts.append(kit.loft("hammerhead bridge", rings,hull,True))
    glass = kit.plain("turquoise bridge glass", kit.linear("#285e6b"), roughness=.25)
    for side in (-1,1):
        for name, start, stop, near, far, material in [
            ("bow red armor", -38, -14, 12, 53, red),
            ("cockpit window recess", 17, 31, 17, 45, dark),
            ("cockpit glazing", 20, 28, 19, 43, glass),
        ]:
            angles = [math.radians(start+(stop-start)*i/8) for i in range(9)]
            offset = .2 if material is glass else 0.0
            rings = [[(355+(43.35+offset)*math.cos(a),side*y,(36.35+offset)*math.sin(a)) for a in angles] for y in (near,far)]
            vertices = rings[0]+rings[1]
            faces = [(i,i+1,i+10,i+9) for i in range(8)]
            parts.append(kit.mesh_object(name,vertices,faces,material))
    for y in (-67,67):
        parts.append(kit.cylinder("bridge end marking",(355,y,0),35,3,(0,1,0),red,32))
        parts.append(kit.cylinder("bridge end plate",(355,y*1.025,0),29,3,(0,1,0),hull,32))
    for x in (207,266,308):
        parts.append(kit.cylinder("neck collar",(x,0,0),32,6,(1,0,0),red if x==266 else hull,32))
    for z in (-1,1):
        parts.append(kit.cylinder("turret pedestal",(205,0,z*40),19,22,(0,0,1),dark))
        parts.append(kit.cylinder("turret cupola",(205,0,z*54),16,18,(0,0,1),hull))
        for y in (-5,5):
            parts.append(kit.cylinder("paired cannon",(224,y,z*57),2,29,(1,0,0),dark,12))
    for side in (-1,1):
        outline = [(40,side*45),(50,side*69),(175,side*69),(205,side*23)]
        parts.append(kit.prism("midship shoulder",outline,-13,24,hull, .08))
        parts.append(kit.cylinder("midship gun dome",(110,side*35,30),15,13,(0,0,1),hull))
        parts.append(kit.sphere("midship rounded cap",(110,side*35,36),15,hull,scale=(1,1,.5)))

        for xx in (68,95,122,149):
            parts.append(kit.box("flank service hatch",(xx,side*70,7),(16,3,10),dark))
        parts.append(kit.cylinder("docking collar",(-125,side*103,0),35,24,(0,1,0),red,32))
        parts.append(kit.cylinder("docking face",(-125,side*117,0),27,7,(0,1,0),hull,32))
        parts.append(kit.box("midship flank rail",(92,side*50,-3),(135,12,17),red))
    engines = [(y,z) for z in (-35,35) for y in (-90,-30,30,90)] + [(y,0) for y in (-60,0,60)]
    for i,(y,z) in enumerate(engines):
        x = -307 if abs(y)<70 else -298
        parts.append(kit.cylinder(f"drive can {i}",(x,y,z),25,174 if abs(y)<70 else 192,(1,0,0),hull,32))
        for xx in (-385,-357,-275,-235):
            parts.append(kit.cylinder("drive reinforcement ring",(xx,y,z),26,5,(1,0,0),dark if xx==-357 else hull,32))
        parts.append(kit.cylinder("drive exhaust rim",(-396,y,z),24,4,(1,0,0),dark,32))
        parts.append(kit.cylinder("drive exhaust inset",(-398,y,z),18,2,(1,0,0),glow,32))
    for x in (-165,-125,-85,-45,0,45,90,135):
        width = 20 if x>20 else 32
        roof = next(h0+(h1-h0)*(x-x0)/(x1-x0) for (x0,_,h0),(x1,_,h1) in zip(sections,sections[1:]) if x0 <= x <= x1)
        parts.append(kit.box("dorsal access plate",(x,0,roof+2),(25,width,4),red if x in (-125,45) else hull))
    parts.append(kit.box("sensor mast",(-175,0,66),(14,9,42),red))
    parts.append(kit.box("sensor vane",(-175,0,87),(36,12,24),hull))
    return kit.finalize(parts,length)
