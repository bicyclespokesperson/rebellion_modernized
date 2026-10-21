"""Dauntless's broad armoured prow, tapering stern and supported ventral command fin."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 14
NAME = "Dauntless Cruiser"
ENCYCLOPEDIA = "EDATA.056"
LENGTH = 1600.0
VIEW = {"yaw": (30, 85), "pitch": (0, 30)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("dauntless_armour",palette.light,38,contrast=.1,seam=.7)
    panel = kit.plated("dauntless_panels",kit.mix(palette.light,palette.mid,.23),22,contrast=.09)
    dark = kit.plated("dauntless_machinery",kit.mix(palette.dark,palette.mid,.2),11,contrast=.16)
    trim = kit.plain("dauntless_trim",kit.mix(palette.mid,palette.light,.35),metallic=.4)
    glass = kit.emissive("dauntless_glazing",(.1,.35,.45,1),.9)
    exhaust = kit.emissive("dauntless_exhaust",(.15,.4,1,1),3)
    widths = kit.profile([(-800,1),(-791,50),(225,200),(253,256),(800,100)])
    tops = kit.profile([(-800,48),(-500,82),(0,140),(250,169),(500,110),(800,20)])
    bottoms = kit.profile([(-800,47),(-714,-25),(-300,-32),(308,-41),(661,-43),(800,-12)])
    def ring(x: float) -> list[kit.Vec3]:
        axis = 15.0
        shape = kit.superellipse_ring(x,widths(x),max(1,tops(x)-axis),max(1,axis-bottoms(x)),2.7,40)
        return [(px,py,pz+axis) for px,py,pz in shape]
    xs = sorted(set([-800,-714,-500,-300,0,225,241,253,286,308,661,726,800]+list(range(-760,801,30))))
    parts = [kit.loft("broad_armoured_shell",[ring(x) for x in xs],hull,smooth=True)]
    parts.append(kit.bevel(kit.box("ventral_forward_armour",(450,0,-40),(445,225,70),panel),12,3))
    for side in (-1,1):
        for x in (-340,-220,-100,20,140):
            w=widths(x)
            parts.append(kit.box("aft_flank_equipment",(x,side*w*.75,-10),(80,32,35),dark))
        for x in (110,390):
            w=widths(x)*.58
            z=15+kit.surface_height(widths(x),tops(x)-15,2.7,w)
            parts.append(kit.box("viewing_gallery_base",(x,side*w,z-5),(65,25,17),panel))
            parts.append(kit.box("viewing_gallery",(x+32,side*w,z),(3,25,8),glass))
    parts.append(kit.prism("aft_command_pedestal",[(-670,-19),(-503,-19),(-503,21),(-670,21)],65,114,panel))
    parts.append(kit.cylinder("aft_sensor_support",(-590,0,119),7,20,material=trim))
    parts.append(kit.sphere("aft_sensor_dish",(-590,0,131),43,panel,scale=(1,.65,.1)))
    parts.append(kit.loft("ventral_command_fin",[
        [(-714,-5,-24),(-604,-5,-23),(-575,-5,-23),(-543,-5,-67),(-543,-5,-129),(-670,-5,-270),(-656,-5,-85)],
        [(-714,5,-24),(-604,5,-23),(-575,5,-23),(-543,5,-67),(-543,5,-129),(-670,5,-270),(-656,5,-85)]],panel))
    parts.append(kit.sphere("dorsal_sensor",(225,0,169),40,panel,scale=(1,1,.12)))
    for x in (-100,250,470):
        lower,upper = ring(x-2),ring(x+2)
        parts.append(kit.loft("armour_section_seam",[
            [(px,py*1.004,(pz-15)*1.004+15) for px,py,pz in profile]
            for profile in (lower,upper)],panel,smooth=True))
    parts.append(kit.loft("prow_dark_window",[
        [(800,-76,-5),(800,76,-5),(800,76,15),(800,-76,15)],
        [(802,-76,-5),(802,76,-5),(802,76,15),(802,-76,15)]],dark))
    for y in (-73,73):
        parts.append(kit.cylinder("drive_housing",(-725,y,15),19,114,(-1,0,0),panel))
        parts.append(kit.cylinder("drive_socket",(-763,y,15),16,12,(-1,0,0),dark))
        parts.append(kit.cylinder("drive_rim",(-784,y,15),19,8,(-1,0,0),trim,cap_end=False))
        parts.append(kit.cylinder("drive_glow",(-786,y,15),14,2,(-1,0,0),exhaust))
    return kit.finalize(parts,length)
