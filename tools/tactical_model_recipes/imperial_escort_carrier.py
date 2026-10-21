"""Imperial carrier's faceted freight shell, forward hangar and recessed stern drives."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 27
NAME = "Imperial Escort Carrier"
ENCYCLOPEDIA = "EDATA.070"
LENGTH = 1171.0
VIEW = {"yaw": (30, 85), "pitch": (0, 30)}


def section(x: float, width: float, top: float, bottom: float) -> list[kit.Vec3]:
    return [(x,-width*.75,bottom-26),(x,width*.75,bottom-26),
            (x,width,bottom),(x,width*.667,top),(x,-width*.667,top),(x,-width,bottom)]


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("imperial_carrier_hull",palette.light,35,contrast=.08,seam=.7)
    panel = kit.plated("imperial_carrier_panels",kit.mix(palette.light,palette.mid,.25),24,contrast=.08)
    dark = kit.plain("imperial_carrier_recess",kit.linear("#11161d"),metallic=.35)
    trim = kit.plain("imperial_carrier_trim",kit.linear("#6a6252"),metallic=.5)
    machinery = kit.plated("imperial_carrier_hangar",kit.mix(palette.mid,palette.dark,.4),9,contrast=.18)
    lamps = kit.emissive("imperial_carrier_hangar_lamps",(.17,.32,.48,1),.8)
    exhaust = kit.emissive("imperial_carrier_exhaust",(.18,.42,1,1),3)
    parts = [kit.loft("carrier_shell",[
        [( -585,-106,47),(-585,106,47),(-585,106,47),(-585,106,47),(-585,-106,47),(-585,-106,47)],
        section(-348,196,106,-98),section(-215,212,133,-136),
        section(177,254,160,-131),
        [(539,-138,-104),(539,138,-104),(543,184,-88),(585,122,68),(585,-122,68),(543,-184,-88)]],hull)]
    # A shallow black bay follows the forward face's slope rather than floating ahead of it.
    outer = [(547,-170,-80),(547,170,-80),(581,116,50),(581,-116,50)]
    inner = [(549,-155,-70),(549,155,-70),(578,107,42),(578,-107,42)]
    parts.append(kit.loft("hangar_frame",[outer,[(x+3,y,z) for x,y,z in outer]],trim))
    parts.append(kit.loft("hangar_shadow",[inner,[(x+5,y,z) for x,y,z in inner]],dark))
    def face_x(z: float) -> float:
        return 549+(z+70)*29/112+6
    def hangar_plate(name: str, y0: float, y1: float, z0: float, z1: float,
                     material: bpy.types.Material, offset: float = 0) -> bpy.types.Object:
        outline = [(face_x(z)-3+offset,y,z) for y,z in ((y0,z0),(y1,z0),(y1,z1),(y0,z1))]
        return kit.loft(name,[outline,[(x+4,y,z) for x,y,z in outline]],material)
    parts.append(hangar_plate("hangar_control_frame",-44,44,-54,-12,trim))
    parts.append(hangar_plate("hangar_control_shadow",-39,39,-50,-16,dark,1))
    for y in (-30,-20,-10,0,10,20,30):
        parts.append(hangar_plate("hangar_control_grille",y-1.5,y+1.5,-47,-19,machinery,2))
    parts.append(hangar_plate("hangar_threshold",-140,140,-67,-63,trim))
    equipment = [(-125,-51,17,8),(-105,-23,11,7),(-89,5,14,10),(-70,-39,10,6),
                 (-55,20,19,7),(-28,16,12,5),(5,25,16,6),(35,12,9,6),
                 (58,-45,15,8),(77,-19,11,7),(95,7,19,8),(112,-38,13,7),(132,-55,11,6)]
    for y,z,width,height in equipment:
        parts.append(hangar_plate("hangar_service_block",y-width/2,y+width/2,
                                 z-height/2,z+height/2,machinery))
    for y,z in ((-141,-47),(-116,9),(-82,-12),(-62,-58),(-37,30),(18,5),
                (56,-8),(84,-53),(107,-11),(126,-29)):
        parts.append(hangar_plate("hangar_indicator",y-1.5,y+1.5,z-1,z+1,lamps,2))
    def roof(x: float) -> float:
        if x <= 177:
            return 133+(x+215)*27/392
        return 160-(x-177)*92/408
    for x in (-200,0,190):
        parts.append(kit.loft("roof_maintenance_plate",[
            [(end,-45,roof(end)-1),(end,45,roof(end)-1),
             (end,45,roof(end)+2),(end,-45,roof(end)+2)]
            for end in (x-55,x+55)],panel))
    for side in (-1,1):
        for x in (-230,-70,90,250):
            def wall_point(end: float, z: float) -> kit.Vec3:
                width = 212+(end+215)*42/392 if end <=177 else 254-(end-177)*70/366
                top = roof(end)
                bottom = -136+(end+215)*5/392 if end <=177 else -131+(end-177)*43/366
                y = width*(.667+.333*(top-z)/(top-bottom))
                return (end,side*(y-2),z)
            outline = [wall_point(x+dx,z) for dx,z in ((-25,-38),(25,-38),(25,17),(-25,17))]
            parts.append(kit.loft("flank_service_hatch",[outline,[(px,py+side*3,pz) for px,py,pz in outline]],panel))
    for y in (-80,0,80):
        parts.append(kit.cylinder("drive_housing",(-423,y,-67),28,70,(-1,0,0),panel))
        parts.append(kit.cylinder("drive_socket",(-445,y,-67),24,16,(-1,0,0),dark))
        parts.append(kit.cylinder("drive_rim",(-460,y,-67),24,9,(-1,0,0),trim,cap_end=False))
        parts.append(kit.cylinder("drive_glow",(-462,y,-67),19,2,(-1,0,0),exhaust))
    return kit.finalize(parts,length)
