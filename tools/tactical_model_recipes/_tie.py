"""Attached cockpit, pylon and opaque solar-panel parts shared by the TIE family."""
from __future__ import annotations

from dataclasses import dataclass
import math

import bpy
import _kit as kit


@dataclass(frozen=True)
class Materials:
    armour: bpy.types.Material
    frame: bpy.types.Material
    solar: bpy.types.Material
    recess: bpy.types.Material
    glass: bpy.types.Material
    exhaust: bpy.types.Material


def materials(palette: kit.Palette, prefix: str) -> Materials:
    return Materials(
        kit.plated(prefix+"_armour",kit.mix(palette.light,palette.mid,.2),2.4,contrast=.08,seam=.7),
        kit.plain(prefix+"_frame",kit.mix(palette.light,palette.mid,.35),metallic=.45),
        kit.plated(prefix+"_solar",kit.linear("#11151f"),2.0,contrast=.08,seam=.6,metallic=.2),
        kit.plain(prefix+"_recess",kit.linear("#121720"),metallic=.25),
        kit.plain(prefix+"_viewport",kit.linear("#090d14"),metallic=.5,roughness=.15),
        kit.emissive(prefix+"_ion_exhaust",(.9,.08,.025,1),2.5))


def beam(name: str, first: kit.Vec3, second: kit.Vec3, radius: float,
         material: bpy.types.Material) -> bpy.types.Object:
    centre=tuple((a+b)/2 for a,b in zip(first,second))
    axis=tuple(b-a for a,b in zip(first,second))
    return kit.cylinder(name,centre,radius,math.sqrt(sum(v*v for v in axis)),axis,material,segments=8)


def cockpit(m: Materials, centre: kit.Vec3 = (0,0,0), radius: float = 9) -> list[bpy.types.Object]:
    x,y,z=centre
    parts=[kit.sphere("cockpit_shell",centre,radius,m.armour,segments=40,rings=24)]
    parts.append(kit.cylinder("viewport_rim",(x+radius-.9,y,z),radius*.73,3,(1,0,0),m.frame,segments=40,cap_end=False))
    parts.append(kit.cylinder("viewport_glass",(x+radius+.3,y,z),radius*.64,.2,(1,0,0),m.glass,segments=40))
    spoke_x=x+radius+.6
    for i in range(8):
        angle=2*math.pi*i/8
        start=(spoke_x,y+radius*.30*math.cos(angle),z+radius*.30*math.sin(angle))
        end=(spoke_x,y+radius*.63*math.cos(angle),z+radius*.63*math.sin(angle))
        parts.append(beam("viewport_radial",start,end,.16,m.frame))
    parts.append(kit.cylinder("viewport_central_rim",(spoke_x,y,z),radius*.32,.25,(1,0,0),m.frame,segments=8,cap_end=False))
    parts.append(kit.cylinder("viewport_central_glass",(spoke_x+.14,y,z),radius*.295,.1,(1,0,0),m.glass,segments=8))
    parts.append(kit.cylinder("top_hatch_rim",(x,y,z+radius-.2),radius*.32,1,material=m.frame))
    parts.append(kit.cylinder("top_hatch",(x,y,z+radius+.35),radius*.23,.2,material=m.recess))
    for offset in (-2.8,2.8):
        parts.append(kit.cylinder("ion_drive_rim",(x-radius+.4,y+offset,z),1.3,2,(-1,0,0),m.frame,cap_end=False))
        parts.append(kit.cylinder("ion_drive_light",(x-radius-.4,y+offset,z),.9,.1,(-1,0,0),m.exhaust))
    return parts


def transformed(x: float, tangent: float, radial: float, angle: float) -> kit.Vec3:
    return (x,radial*math.cos(angle)-tangent*math.sin(angle),
            radial*math.sin(angle)+tangent*math.cos(angle))


def panel(m: Materials, outline: list[tuple[float,float]], radial: float, angle: float,
          thickness: float = .65, hub_x: float = 0) -> list[bpy.types.Object]:
    rings=[[transformed(x,t,radial+depth,angle) for x,t in outline] for depth in (-thickness/2,thickness/2)]
    parts=[kit.loft("opaque_solar_panel",rings,m.solar)]
    for a,b in zip(outline,outline[1:]+outline[:1]):
        parts.append(beam("solar_edge_frame",transformed(*a,radial,angle),
                          transformed(*b,radial,angle),.45,m.frame))
    for face in (-1,1):
        surface_radial=radial+face*(thickness/2+.15)
        for x,t in outline:
            parts.append(beam("solar_cell_radial",transformed(hub_x,0,surface_radial,angle),
                              transformed(x,t,surface_radial,angle),.2,m.frame))
        parts.append(kit.cylinder("panel_hub",transformed(hub_x,0,surface_radial,angle),3.8,.5,
                                  (0,math.cos(angle),math.sin(angle)),m.frame,segments=16))
    return parts


def pylon(m: Materials, radial: float, angle: float) -> list[bpy.types.Object]:
    axis=(0,math.cos(angle),math.sin(angle))
    return [kit.cylinder("wing_pylon",transformed(0,0,(radial+5)/2,angle),3.2,radial-5,axis,m.armour,segments=16),
            kit.cylinder("wing_attachment_collar",transformed(0,0,radial-1,angle),4.7,4,axis,m.frame,segments=16)]
