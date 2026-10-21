"""Bulk transport: narrow slab sides, low triangular prow and an exposed machinery well."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 7
NAME = "Bulk Transport"
ENCYCLOPEDIA = "EDATA.049"
LENGTH = 1047.0
VIEW = {"yaw": (15, 85), "pitch": (5, 45)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("bulk_transport_hull",palette.light,25,contrast=.09,seam=.6)
    plate = kit.plated("bulk_transport_panels",kit.mix(palette.light,palette.accent,.15),35,contrast=.08)
    dark = kit.plain("bulk_transport_recess",kit.mix(palette.dark,palette.mid,.2),metallic=.35)
    metal = kit.plated("bulk_transport_machinery",palette.mid,15,contrast=.12)
    glow = kit.emissive("bulk_transport_exhaust",(.22,.45,1,1),3)
    outline = [(-524,78),(-410,78),(-184,120),(-142,21),(156,21),(226,92),(425,-7),(405,-28),(524,-102),(156,-120),(-340,-120),(-396,-50),(-524,-50)]
    verts: list[kit.Vec3] = [(x,side*(66 if x==524 else 83),z) for side in (-1,1) for x,z in outline]
    n = len(outline)
    faces: list[tuple[int,...]] = [tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]
    faces.extend((i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n))
    parts: list[bpy.types.Object] = [kit.mesh_object("faceted_freighter",verts,faces,hull)]
    for side in (-1,1):
        for x,z0,z1 in [(-480,-48,78),(-390,-60,81),(-285,-118,100),(-180,-118,110),(-50,-118,21),(80,-118,21),(180,-115,45),(285,-108,58),(390,-100,10)]:
            parts.append(kit.box("side_panel_divider",(x,side*83.5,(z0+z1)/2),(2,1.5,z1-z0),dark))
        parts.append(kit.box("longitudinal_rub_rail",(-70,side*86,-22),(810,7,6),dark))
        for x in range(-125,153,28):
            parts.append(kit.box("cooling_fin",(x,side*47,44),(8,49,42),plate))
    parts.append(kit.cylinder("raised_feed_pipe",(-2,0,72),16,383,(1,0,0),metal))
    for x in range(-165,210,38):
        parts.append(kit.cylinder("pipe_collar",(x,0,72),19,7,(1,0,0),hull))
    parts.append(kit.box("pipe_foot",(-172,0,60),(30,42,45),metal))
    parts.append(kit.box("pipe_forward_foot",(195,0,56),(35,42,48),metal))
    for x in range(-110,145,28):
        parts.append(kit.box("well_floor_recess",(x,0,22),(15,65,2),dark))
    for side in (-1,1):
        for a,b in [((425,-7),(156,-120)),((226,92),(156,21)),((156,21),(156,-120)),((156,21),(-142,21)),((-142,21),(-340,-120)),((-142,21),(-184,120))]:
            dx,dz=b[0]-a[0],b[1]-a[1]
            parts.append(kit.cylinder("armour_panel_seam",((a[0]+b[0])/2,side*84,(a[1]+b[1])/2),.9,(dx*dx+dz*dz)**.5,(dx,0,dz),dark,segments=8))
        for offset in (0,1):
            a,b = (226,92),(425,-7)
            y = side*(48 if offset==0 else 68)
            verts = [(x,y+dy,z+1.5) for (x,z),dy in [(a,-3),(b,-3),(b,3),(a,3)]]
            parts.append(kit.mesh_object("prow_armour_rail",verts,[(0,1,2,3)],dark))
    for y0,y1 in [(-81,-4),(4,81)]:
        points=[(226,y0,93),(425,y0,-6),(425,y1,-6),(226,y1,93)]
        parts.append(kit.mesh_object("dark_dorsal_armour",points,[(0,1,2,3)],dark))
    parts.append(kit.loft("ventral_cargo_keel", [[(x,-48,-110),(x,48,-110),(x,40,lo),(x,-40,lo)] for x,lo in [(380,-121),(300,-146),(150,-138),(95,-120)]],metal))
    cabin = [(426,27,-41),(463,23,-64)]
    parts.append(kit.loft("forward_command_cabin", [[(x,-w,z+11),(x,w,z+11),(x,w,z-1),(x,-w,z-1)] for x,w,z in cabin],metal))
    command_glass = kit.emissive("bulk_transport_glass",kit.linear("#346b3c"),.4)
    parts.append(kit.mesh_object("command_viewport",[(463,-18,-60),(463,18,-60),(463,18,-56),(463,-18,-56)],[(0,1,2,3)],command_glass))
    for y in (-46,0,46):
        parts.append(kit.cylinder("engine_socket",(-523,y,8),21,8,(-1,0,0),dark))
        parts.append(kit.cylinder("engine_rim",(-529,y,8),18,6,(-1,0,0),metal,cap_end=False))
        parts.append(kit.cylinder("engine_light",(-530,y,8),14,2,(-1,0,0),glow))
    return kit.finalize(parts,length)
