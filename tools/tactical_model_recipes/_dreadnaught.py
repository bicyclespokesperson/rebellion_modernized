"""Shared Dreadnaught hull: six reference-visible flank blisters mirrored to twelve for other headings."""
from __future__ import annotations

import bpy
import _kit as kit


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("dreadnaught_hull",palette.light,23,contrast=.08,seam=.65)
    plates = kit.plated("dreadnaught_armour",kit.mix(palette.light,palette.mid,.18),32,contrast=.07)
    dark = kit.plain("dreadnaught_recess",kit.mix(palette.dark,palette.mid,.2),metallic=.4)
    bronze = kit.plain("dreadnaught_sensors",kit.mix(palette.mid,palette.accent,.4),metallic=.4)
    foreplate = kit.plated("dreadnaught_foreplate",kit.mix(palette.light,palette.accent,.4),32,contrast=.07)
    belt = kit.plated("dreadnaught_ventral_belt",kit.mix(palette.dark,palette.mid,.5),25,contrast=.08)
    glow = kit.emissive("dreadnaught_exhaust",(.23,.46,1,1),3)
    sections = [(520,59,-15,59,-15,59,-15),(479,40,34,90,-20,58,-28),(403,57,68,111,-20,56,-39),(348,57,68,120,-20,56,-48),(178,55,68,88,-20,55,-80),(-221,0,47,88,-20,55,-81),(-301,57,86,110,-20,57,-100),(-327,57,90,110,-20,57,-105),(-528,57,110,110,-20,57,-85)]
    rings = [[(x,-wt,zt),(x,wt,zt),(x,wm,zm),(x,wb,zb),(x,-wb,zb),(x,-wm,zm)] for x,wt,zt,wm,zm,wb,zb in sections]
    parts: list[bpy.types.Object] = [kit.loft("armoured_hull",rings,hull)]
    parts[0].data.materials.append(foreplate)
    for face in parts[0].data.polygons:
        if face.center.x > 400:
            face.material_index = 1

    def section_value(x: float, field: int) -> float:
        ordered = sorted(sections)
        if x <= ordered[0][0]:
            return ordered[0][field]
        for left,right in zip(ordered,ordered[1:]):
            if x <= right[0]:
                t=(x-left[0])/(right[0]-left[0])
                return left[field]*(1-t)+right[field]*t
        return ordered[-1][field]

    def roof(x: float) -> float:
        return section_value(x,2)

    def roof_at(x: float,y: float) -> float:
        wt,zt,wm,zm=(section_value(x,i) for i in (1,2,3,4))
        if abs(y) <= wt:
            return zt
        return zt+(abs(y)-wt)*(zm-zt)/(wm-wt)

    def side_width(x: float,z: float) -> float:
        wt,zt,wm,zm,wb,zb = (section_value(x,i) for i in range(1,7))
        if z >= zm:
            t=(z-zm)/(zt-zm)
            return wm*(1-t)+wt*t
        t=(z-zb)/(zm-zb)
        return wb*(1-t)+wm*t

    def side_patch(name: str,x0: float,x1: float,z0: float,z1: float,side: int,
                   material: bpy.types.Material,offset: float=1) -> bpy.types.Object:
        levels=[z0]+([-20.] if z0 < -20 < z1 else [])+[z1]
        points=[(x,side*(side_width(x,z)+offset),z) for z in levels for x in (x0,x1)]
        faces=[(i,i+1,i+3,i+2) for i in range(0,len(points)-2,2)]
        return kit.mesh_object(name,points,faces,material)
    for side in (-1,1):
        for x,r,across in [(160,34,60),(52,29,60),(-62,38,60),(105,29,75),(-18,38,75),(-142,30,75)]:
            y=side*across
            wt,zt,wm,zm=(section_value(x,i) for i in (1,2,3,4))
            z=zt+(across-wt)*(zm-zt)/(wm-wt)
            normal=(0,side*(zt-zm),wm-wt)
            parts.append(kit.sphere("flank_blister_foundation",(x,y,z-1),r,dark,scale=(1,.73,.06),up=normal))
            parts.append(kit.sphere("flank_reactor_blister",(x,y,z-1),r,plates,scale=(1,.73,.20),up=normal))
        for x0,x1 in zip((-205,-100,0,100,178,260),(-100,0,100,178,260,340)):
            points=[]
            for x in (x0,x1):
                for z in (section_value(x,6)+5,-30):
                    points.append((x,side*(side_width(x,z)+.7),z))
            parts.append(kit.mesh_object("ventral_belt",points,[(0,2,3,1)],belt))
        for x in (-260,-290,-320):
            parts.append(side_patch("stern_heat_vent",x-7,x+7,-44,20,side,dark))
            parts.append(side_patch("stern_vent_shield",x+3,x+8,-40,24,side,plates,2))
        for x in range(-175,400,65):
            parts.append(side_patch("equatorial_armour_band",x-2,x+2,-30,-14,side,dark))
    for x0,x1 in ((405,445),(445,478)):
        points=[(x,y,roof(x)+.5) for x,y in ((x0,-28),(x1,-28),(x1,28),(x0,28))]
        parts.append(kit.mesh_object("warm_prow_armour",points,[(0,1,2,3)],foreplate))
    bridge_sections=[]
    for x,w in ((-238,36),(-290,40)):
        bridge_sections.append([(x,-w,roof_at(x,-w)-3),(x,w,roof_at(x,w)-3),(x,w,roof(x)+10),(x,-w,roof(x)+10)])
    parts.append(kit.loft("rectangular_bridge_canopy",bridge_sections,hull))
    bridge_points=[(-237.3,y,z) for y,z in ((-30,46),(30,46),(30,62),(-30,62))]
    parts.append(kit.mesh_object("bridge_viewport",bridge_points,[(0,1,2,3)],dark))
    parts.append(kit.loft("aft_command_roof", [[(x,-42,roof(x)-1),(x,42,roof(x)-1),(x,42,roof(x)+5),(x,-42,roof(x)+5)] for x in (-465,-335)],plates))
    parts.append(kit.box("aft_command_glazing",(-334,0,roof(-335)+2),(2,68,4),dark))
    for sign in (-1,1):
        surface = roof(-123) if sign > 0 else section_value(-123,6)
        tip = 83 if sign > 0 else -118
        parts.append(kit.cylinder("sensor_stalk",(-123,0,(surface+tip)/2),3,abs(tip-surface),material=bronze))
        parts.append(kit.cylinder("sensor_dish",(-123,0,tip),18,5,(0,1,0),bronze))
    for y,z in [(-42,56),(42,56),(-42,-30),(42,-30)]:
        parts.append(kit.cylinder("engine_socket",(-526,y,z),25,8,(-1,0,0),dark))
        parts.append(kit.cylinder("engine_rim",(-532,y,z),21,6,(-1,0,0),plates,cap_end=False))
        parts.append(kit.cylinder("engine_light",(-533,y,z),17,2,(-1,0,0),glow))
    return kit.finalize(parts,length)
