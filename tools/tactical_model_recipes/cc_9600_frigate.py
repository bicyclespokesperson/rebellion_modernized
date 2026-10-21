"""CC-9600's broad aft armour, tapered prow and long swept antenna vanes."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 11
NAME = "CC-9600 Frigate"
ENCYCLOPEDIA = "EDATA.055"
LENGTH = 862.0
VIEW = {"yaw": (30, 85), "pitch": (0, 30)}


def section(x: float, width: float, inner: float, top: float, edge: float,
            bottom: float) -> list[kit.Vec3]:
    return [(x,-inner,-top),(x,inner,-top),(x,width,bottom),
            (x,width,edge),(x,inner,top),(x,-inner,top),(x,-width,edge),(x,-width,bottom)]


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("cc9600_armour",palette.light,27,contrast=.09,seam=.67)
    panel = kit.plated("cc9600_panels",kit.mix(palette.light,palette.mid,.22),17,contrast=.1)
    dark = kit.plain("cc9600_recess",kit.mix(palette.dark,palette.mid,.1),metallic=.3)
    trim = kit.plain("cc9600_trim",kit.mix(palette.mid,palette.light,.4),metallic=.4)
    glass = kit.emissive("cc9600_windows",(.13,.45,.6,1),1.2)
    exhaust = kit.emissive("cc9600_exhaust",(.15,.4,1,1),3)
    parts = [kit.bevel(kit.loft("aft_armour",[
        section(-431,222,75,46,29,-29),section(-207,258,75,79,45,-45),
        section(-60,281,90,76,56,-56)],hull),12,4)]
    parts.append(kit.loft("forward_wedge",[
        section(-60,260,90,76,25,-25),section(-17,253,90,76,1,-1),
        section(223,188,105,33,1,-1),section(289,102,101,21,-16,-18),
        section(404,90,89,1,1,-1)],hull))
    for side in (-1,1):
        outline = [(223,side*188,1),(431,side*227,-16),(289,side*102,-16),(404,side*89,1)]
        parts.append(kit.loft("swept_antenna_vane",[outline,[(x,y,z-3) for x,y,z in outline]],panel))
        start,end=(431,side*227,-16),(431,side*477,-116)
        centre=tuple((a+b)/2 for a,b in zip(start,end))
        axis=tuple(b-a for a,b in zip(start,end))
        parts.append(kit.cylinder("long_vane_antenna",centre,2.2,sum(v*v for v in axis)**.5,axis,trim,segments=12))
        parts.append(kit.sphere("dorsal_sensor",(-15,side*95,72),26,panel,scale=(1.7,.5,.2)))
        parts.append(kit.cylinder("sensor_socket",(60,side*67,63),12,5,material=trim))
        parts.append(kit.sphere("sensor_dome",(60,side*67,66),10,panel,scale=(1,1,.45)))
        # Low lamps follow the wedge's canted side instead of protruding as a cabin.
        parts.append(kit.loft("flank_window_frame",[
            [(x,side*(w-1),0),(x,side*(w-20),11),
             (x,side*(w-18),13),(x,side*(w+1),2)]
            for x,w in ((80,225),(160,204))],trim))
        parts.append(kit.loft("flank_window",[
            [(x,side*(w-2),3),(x,side*(w-12),9),
             (x,side*(w-11),10),(x,side*(w-1),4)]
            for x,w in ((87,223),(153,206))],glass))
    parts.append(kit.loft("dorsal_cargo_panel",[
        [(x,-47,z-1),(x,47,z-1),(x,47,z+2),(x,-47,z+2)]
        for x,z in ((110,53.4),(270,24.8))],dark))
    parts.append(kit.box("dorsal_signal_strip",(111,0,56),(3,83,2),glass))
    parts.append(kit.loft("communications_mast",[
        [(-345,109,46),(-305,109,46),(-305,161,46),(-345,161,46)],
        [(-337,123,160),(-305,123,160),(-305,147,160),(-337,147,160)]],panel))
    parts.append(kit.box("mast_dark_band",(-321,135,146),(34,26,7),dark))
    for y in (-120,0,120):
        parts.append(kit.cylinder("drive_socket",(-414,y,-2),21,14,(-1,0,0),dark))
        parts.append(kit.cylinder("drive_rim",(-432,y,-2),21,8,(-1,0,0),trim,cap_end=False))
        parts.append(kit.cylinder("drive_glow",(-434,y,-2),16,2,(-1,0,0),exhaust))
    return kit.finalize(parts,length)
