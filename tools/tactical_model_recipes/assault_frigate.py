"""Assault Frigate: armoured Dreadnaught prow, exposed spar and tall radiator tail."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 2
NAME = "Assault Frigate"
ENCYCLOPEDIA = "EDATA.044"
LENGTH = 1138.0
VIEW = {"yaw": (0,85), "pitch": (0,30)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull = kit.plated("assault_frigate_armour",palette.light,24,contrast=.08,seam=.65)
    metal = kit.plated("assault_frigate_machinery",palette.mid,12,contrast=.15)
    dark = kit.plain("assault_frigate_recess",kit.mix(palette.dark,palette.mid,.15),metallic=.4)
    glow = kit.emissive("assault_frigate_exhaust",(.2,.45,1,1),3)
    sections = [(569,54,44,54,44,54,44),(447,61,132,120,45,51,-28),(374,61,132,139,45,51,-28),(261,61,132,100,45,51,-28),(136,34,68,61,45,34,20)]
    parts: list[bpy.types.Object] = [kit.loft("armoured_forward_shell",[[(x,-wt,zt),(x,wt,zt),(x,wm,zm),(x,wb,zb),(x,-wb,zb),(x,-wm,zm)] for x,wt,zt,wm,zm,wb,zb in sections],hull)]
    belly=kit.plated("assault_frigate_dark_bowl",kit.mix(palette.mid,palette.dark,.45),22,contrast=.09)
    parts[0].data.materials.append(belly)
    for face in parts[0].data.polygons:
        if face.center.z < 40:
            face.material_index=1
    parts.append(kit.loft("exposed_spine",[[(px,py,pz+(hi+lo)/2) for px,py,pz in kit.superellipse_ring(x,w,(hi-lo)/2,(hi-lo)/2,2,12)] for x,w,hi,lo in [(180,34,87,20),(-245,36,88,20),(-415,28,76,32),(-569,1,65,33)]],metal))
    fin_outline = [(-245,88),(-414,254),(-483,236),(-543,33),(-509,-219),(-416,-254),(-245,20)]
    verts: list[kit.Vec3] = [(x,y,z) for y in (-5,5) for x,z in fin_outline]
    n=len(fin_outline)
    faces: list[tuple[int,...]] = [tuple(range(n-1,-1,-1)),tuple(range(n,2*n))]
    faces.extend((i,(i+1)%n,(i+1)%n+n,i+n) for i in range(n))
    parts.append(kit.mesh_object("vertical_radiator_fin",verts,faces,hull))
    for side in (-1,1):
        for x,r,z in [(125,35,84),(-15,29,75),(-155,37,78),(-285,28,67)]:
            parts.append(kit.sphere("machinery_blister",(x,side*26,z),r,hull,scale=(1,.8,.65)))
        for z,r in [(-175,27),(-84,38),(-8,24),(38,20),(123,28),(193,30)]:
            parts.append(kit.sphere("tail_drive_pod",(-445,side*10,z),r,hull,scale=(1.4,.65,.57)))
            parts.append(kit.cylinder("tail_exhaust_socket",(-445-r*1.25,side*10,z),r*.5,6,(-1,0,0),dark))
            parts.append(kit.cylinder("tail_exhaust",(-445-r*1.25-4,side*10,z),r*.38,3,(-1,0,0),glow))
        for i,x in enumerate((-350,-312,-271,-235,-192,-150,-117,-78,-35,10,46,83,116)):
            parts.append(kit.box("spine_rib",(x,side*30,53),(5+(i%3)*2,10,22+(i%4)*5),hull))
        for i,x in enumerate((-320,-230,-105,25)):
            parts.append(kit.box("machinery_pack",(x,side*29,76),(22+i*3,22,14),metal))
            parts.append(kit.cylinder("service_drum",(x+10,side*35,58),8,23,(1,0,0),dark))
        for x in (-300,-160,-20):
            parts.append(kit.cylinder("diagonal_feed",(x,side*34,54),3,66,(1,0,.4),metal,segments=12))
        parts.append(kit.cylinder("external_feed_pipe",(-150,side*34,43),4,390,(1,0,0),dark))
        keel = [(415,44),(412,-161),(311,-146),(202,-68),(183,6)]
        verts = [(x,side*3,z) for x,z in keel]
        parts.append(kit.mesh_object("forward_ventral_keel",verts,[tuple(range(len(verts)))],hull))
    for x in (300,360,420):
        parts.append(kit.box("forward_roof_panel",(x,0,133),(38,88,3),hull))
    return kit.finalize(parts,length)
