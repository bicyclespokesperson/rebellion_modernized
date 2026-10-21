"""Strike Cruiser's ribbed oval armour shell, ventral machinery and compact dorsal bridge."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 15
NAME = "Strike Cruiser"
ENCYCLOPEDIA = "EDATA.057"
LENGTH = 889.0
VIEW = {"yaw": (30, 85), "pitch": (0, 30)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("strike_hull",palette.light,25,contrast=.09,seam=.7)
    armour = kit.plated("strike_armour",kit.mix(palette.light,palette.mid,.18),17,contrast=.07)
    trim = kit.plain("strike_ribs",kit.mix(palette.mid,palette.light,.45),metallic=.4)
    dark = kit.plated("strike_machinery",kit.mix(palette.dark,palette.mid,.15),9,contrast=.17)
    glass = kit.plain("strike_bridge_glass",kit.linear("#101d26"),roughness=.2,metallic=.5)
    exhaust = kit.emissive("strike_exhaust",(.14,.4,1,1),3)
    widths = kit.profile([(-445,.5),(-416,71),(-228,139),(247,69),(445,.5)])
    roofs = kit.profile([(-445,-3),(-416,15),(-247,64),(201,49),(445,-5)])
    bottoms = kit.profile([(-445,-4),(-410,-72),(-228,-89),(-50,-73),(302,-56),(445,-5)])
    def ring(x: float, extra: float = 0) -> list[kit.Vec3]:
        width = widths(x)+extra
        axis = (roofs(x)+bottoms(x))/2
        result = kit.superellipse_ring(x,width,roofs(x)-axis+extra,axis-bottoms(x)+extra,2.5,40)
        return [(px,py,pz+axis) for px,py,pz in result]
    xs = sorted(set([-445,-416,-247,-228,-50,201,247,302,445]+list(range(-400,441,20))))
    shell = kit.loft("oval_armour_shell",[ring(x) for x in xs],hull,smooth=True)
    shell.data.materials.append(dark)
    for polygon in shell.data.polygons:
        if sum(shell.data.vertices[i].co.z for i in polygon.vertices)/len(polygon.vertices) < -24:
            polygon.material_index = 1
    parts = [shell]
    for x in (-345,-235,-110,35,170,285):
        parts.append(kit.loft("transverse_armour_rib",[ring(x-2,1),ring(x+2,1)],trim,smooth=True))
    for side in (-1,1):
        for x in (-270,-150,-30,90):
            width = widths(x)
            parts.append(kit.sphere("ventral_machinery_pod",(x,side*width*.78,-52),30,armour,
                                    scale=(1.6,.65,.65)))
            parts.append(kit.box("lower_service_module",(x,side*width*.45,-64),(63,25,19),dark))
    parts.append(kit.prism("bridge_foundation",[(-130,-35),(-45,-35),(15,0),(-45,35),(-130,35)],40,65,armour))
    parts.append(kit.prism("command_pedestal",[(-112,-13),(-62,-18),(-28,0),(-62,18),(-112,13)],58,82,trim))
    parts.append(kit.sphere("command_roof",(-75,0,85),37,armour,scale=(1.1,1,.15)))
    parts.append(kit.box("bridge_viewport",(-75,0,82),(60,43,4),glass))
    for y in (-46,46):
        parts.append(kit.cylinder("drive_pod",(-398,y,-27),25,74,(-1,0,0),armour))
        parts.append(kit.cylinder("drive_socket",(-421,y,-27),21,12,(-1,0,0),dark))
        parts.append(kit.cylinder("drive_rim",(-436,y,-27),21,8,(-1,0,0),trim,cap_end=False))
        parts.append(kit.cylinder("drive_glow",(-438,y,-27),16,2,(-1,0,0),exhaust))
    return kit.finalize(parts,length)
