"""Medium transport: shallow faceted cargo shell and narrow raised drive spine."""
from __future__ import annotations

import math
from collections.abc import Callable

import bpy
from mathutils import Vector
import _kit as kit

TYPE_INDEX = 6
NAME = "Medium Transport"
ENCYCLOPEDIA = "EDATA.048"
LENGTH = 1193.0
VIEW = {"yaw": (0,85), "pitch": (0,30)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull=kit.plated("medium_transport_shell",palette.light,24,contrast=.08,seam=.65)
    metal=kit.plated("medium_transport_machinery",palette.mid,14,contrast=.13)
    dark=kit.plain("medium_transport_recess",kit.mix(palette.dark,palette.mid,.2),metallic=.4)
    glow=kit.emissive("medium_transport_exhaust",(.2,.44,1,1),3)
    sections=[(597,108,27,27),(440,165,92,-84),(251,229,105,-84),(28,205,115,-122),(-172,185,114,-93),(-251,175,114,-88),(-420,85,114,-83),(-533,60,114,-30),(-597,55,12,12)]
    rings=[]
    for x,w,hi,lo in sections:
        middle=(hi+lo)/2
        rings.append([(px,py,pz+middle) for px,py,pz in kit.superellipse_ring(x,w,(hi-lo)/2,(hi-lo)/2,1.4 if x < -200 else 1.8,16)])
    parts: list[bpy.types.Object]=[kit.loft("faceted_cargo_shell",rings,hull)]
    lower=kit.plated("medium_transport_ventral_plating",kit.mix(palette.mid,palette.dark,.35),18,contrast=.1)
    parts[0].data.materials.append(lower)
    for face in parts[0].data.polygons:
        if face.center.z < 0:
            face.material_index=1
    bpy.context.view_layer.update()

    def roof_point(x: float,y: float,offset: float=.5) -> kit.Vec3:
        hit,point,normal,_=parts[0].ray_cast(Vector((x,y,400)),Vector((0,0,-1)))
        if not hit:
            raise ValueError("roof detail must intersect the cargo shell")
        return tuple(point+Vector((0,0,offset)))

    def side_point(x: float,z: float,side: int,offset: float=.6) -> kit.Vec3:
        hit,point,normal,_=parts[0].ray_cast(Vector((x,side*600,z)),Vector((0,-side,0)))
        if not hit:
            raise ValueError("freight detail must intersect the cargo shell")
        return tuple(point+Vector((0,side*offset,0)))

    def surface_panel(name: str,x0: float,x1: float,a0: float,a1: float,
                      point: Callable[[float,float],kit.Vec3],material: bpy.types.Material) -> bpy.types.Object:
        nx=max(1,math.ceil((x1-x0)/5))
        na=max(1,math.ceil((a1-a0)/4))
        vertices=[point(x0+(x1-x0)*i/nx,a0+(a1-a0)*j/na) for j in range(na+1) for i in range(nx+1)]
        faces=[]
        for j in range(na):
            for i in range(nx):
                start=j*(nx+1)+i
                faces.append((start,start+1,start+nx+2,start+nx+1))
        return kit.mesh_object(name,vertices,faces,material)

    freight=kit.plated("medium_transport_freight_frames",kit.mix(palette.mid,palette.accent,.4),10,contrast=.08)
    for side in (-1,1):
        for x in (-210,-140,-70,0,70,140,210,280,350):
            parts.append(surface_panel("recessed_freight_bay",x-23,x+23,-60,-20,
                                       lambda px,pz: side_point(px,pz,side,.8),dark))
            parts.append(surface_panel("ventral_freight_rib",x-28,x-24,-65,-14,
                                       lambda px,pz: side_point(px,pz,side,1),freight))
        for y in (side*40,side*80):
            parts.append(surface_panel("forward_roof_vent",286,314,y-4,y+4,roof_point,dark))
    for y in (-30,0,30):
        ridge=[]
        for x in (-550,-480,-400,-320,-230):
            left,right=roof_point(x,y-3,-1),roof_point(x,y+3,-1)
            height=3 if y==0 else 6
            ridge.append([left,right,(right[0],right[1],right[2]+height),(left[0],left[1],left[2]+height)])
        parts.append(kit.loft("raised_aft_roof_rib",ridge,hull))
    for side in (-1,1):
        parts.append(kit.cylinder("drive_pod",(-493,side*55,8),31,150,(1,0,0),metal))
        for x in (-550,-525,-500,-475,-450):
            parts.append(kit.cylinder("drive_collar",(x,side*55,8),33,6,(1,0,0),hull))
        parts.append(kit.cylinder("drive_socket",(-574,side*55,8),27,8,(-1,0,0),dark))
        parts.append(kit.cylinder("drive_rim",(-580,side*55,8),23,6,(-1,0,0),hull,cap_end=False))
        parts.append(kit.cylinder("drive_light",(-582,side*55,8),18,2,(-1,0,0),glow))
    parts.append(kit.loft("dorsal_drive_spine",[[(x,-w,z-8),(x,w,z-8),(x,w,z+8),(x,-w,z+8)] for x,w,z in [(-570,35,42),(-530,36,103),(-420,36,80),(-240,50,75)]],hull))
    parts.append(kit.cylinder("sensor_base",(-400,0,112),16,50,material=metal))
    parts.append(kit.sphere("sensor_cap",(-400,0,140),20,hull,scale=(1,1,.3)))
    for x in (-360,-300,-250):
        parts.append(kit.box("spine_service_panel",(x,0,80),(30,45,4),metal))
    return kit.finalize(parts,length)
