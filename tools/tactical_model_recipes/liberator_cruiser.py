"""Liberator's twin swept flight decks, open prow and warm angular armour panels."""
from __future__ import annotations

import bpy
import _kit as kit

TYPE_INDEX = 13
NAME = "Liberator Cruiser"
ENCYCLOPEDIA = "EDATA.054"
LENGTH = 907.0
VIEW = {"yaw": (30, 85), "pitch": (0, 30)}
WING = [(-454,167,97),(-77,328,-96),(454,202,-97),(454,167,-70),(68,167,3)]
FACES = [(0,1,4),(1,2,3),(1,3,4)]


def wing_height(x: float, y: float) -> float:
    for face in FACES:
        a,b,c = [WING[i] for i in face]
        det=(b[1]-c[1])*(a[0]-c[0])+(c[0]-b[0])*(a[1]-c[1])
        u=((b[1]-c[1])*(x-c[0])+(c[0]-b[0])*(y-c[1]))/det
        v=((c[1]-a[1])*(x-c[0])+(a[0]-c[0])*(y-c[1]))/det
        if min(u,v,1-u-v)>=-.001:
            return u*a[2]+v*b[2]+(1-u-v)*c[2]
    raise ValueError(f"point outside flight deck: {x}, {y}")


def clipped_patch(outline: list[tuple[float,float]], triangle: tuple[int,int,int]) -> list[tuple[float,float]]:
    clip=[WING[i] for i in triangle]
    orientation=(clip[1][0]-clip[0][0])*(clip[2][1]-clip[0][1])-(clip[1][1]-clip[0][1])*(clip[2][0]-clip[0][0])
    result=outline
    for a,b in zip(clip,clip[1:]+clip[:1]):
        def distance(p: tuple[float,float]) -> float:
            return ((b[0]-a[0])*(p[1]-a[1])-(b[1]-a[1])*(p[0]-a[0]))*orientation
        incoming=result
        result=[]
        if not incoming:
            break
        previous=incoming[-1]
        before=distance(previous)
        for current in incoming:
            after=distance(current)
            if (before>=0)!=(after>=0):
                t=before/(before-after)
                result.append((previous[0]+t*(current[0]-previous[0]),previous[1]+t*(current[1]-previous[1])))
            if after>=0:
                result.append(current)
            previous,before=current,after
    return result


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull=kit.plated("liberator_hull",palette.light,27,contrast=.09,seam=.68)
    panel=kit.plated("liberator_panels",kit.mix(palette.light,palette.mid,.23),16,contrast=.1)
    dark=kit.plain("liberator_recess",kit.mix(palette.dark,palette.mid,.16),metallic=.3)
    orange=kit.plain("liberator_orange",kit.linear("#b36735"),metallic=.2)
    trim=kit.plain("liberator_trim",kit.mix(palette.mid,palette.light,.35),metallic=.4)
    glow=kit.emissive("liberator_exhaust",(.15,.4,1,1),3)
    parts=[kit.loft("aft_command_hull",[
        [(-363,-116,-8),(-363,116,-8),(-454,167,97),(-454,68,97),(-454,-68,97),(-454,-167,97)],
        [(-42,-140,-27),(-42,140,-27),(-42,167,23),(-42,81,57),(-42,-72,57),(-42,-167,23)],
        [(68,-150,-34),(68,150,-34),(68,167,3),(68,166,3),(68,-166,3),(68,-167,3)]],hull)]
    for side in (-1,1):
        vertices=[(x,side*y,z) for x,y,z in WING]
        vertices += [(x,side*y,z-4) for x,y,z in [(-363,116,-8),(-77,328,-96),
                                                        (454,202,-97),(454,159,-80),(68,150,-34)]]
        faces=FACES+[tuple(i+5 for i in reversed(face)) for face in FACES]
        faces += [(i,(i+1)%5,(i+1)%5+5,i+5) for i in range(5)]
        parts.append(kit.mesh_object("swept_flight_deck",vertices,faces,hull))
        for name,outline,material in (
            ("forward_inset_panel",[(240,185),(370,184),(255,215)],dark),
            ("aft_inset_panel",[(-310,180),(-160,180),(-110,260),(-190,275)],dark),
            ("deck_orange_chevron",[(70,230),(40,235),(130,274),(210,251),(185,246),(130,260)],orange),
            ("aft_armour_panel",[(-320,180),(-180,180),(-125,282)],panel)):
            for face in FACES:
                patch=clipped_patch(outline,face)
                if len(patch)>=3:
                    roof=[(x,side*y,wing_height(x,y)+.8) for x,y in patch]
                    parts.append(kit.loft(name,[[(x,y,z-2) for x,y,z in roof],roof],material))
        for x in (300,335,370):
            y=185
            z=wing_height(x,y)
            parts.append(kit.box("flight_deck_vent",(x,side*y,z+1),(12,16,3),dark))
    command=kit.plated("liberator_command_deck",kit.mix(palette.mid,palette.dark,.1),16,contrast=.08)
    for side in (-1,1):
        outline: list[kit.Vec3] = []
        for x,y in ((-310,90),(-50,90),(-50,138),(-310,138)):
            t=(x+454)/412
            inner=68+13*t
            roof=97-40*t
            edge=97-74*t
            z=roof-(y-inner)*(roof-edge)/(167-inner)
            outline.append((x,side*y,z+1))
        parts.append(kit.loft("command_deck_inset",[[(x,y,z-2) for x,y,z in outline],outline],command))
        parts.append(kit.box("front_viewport_frame",(69,side*80,-15),(3,124,20),hull))
        for y in (35,60,85,110,135):
            parts.append(kit.box("front_viewport",(71,side*y,-15),(3,17,11),dark))
    parts.append(kit.loft("raised_command_spine",[
        [(-454,10,96),(-454,68,96),(-454,68,97),(-454,10,97)],
        [(-100,26,32),(-100,51,32),(-100,51,97),(-100,26,97)],
        [(-80,9,29),(-80,68,29),(-80,68,61),(-80,9,61)]],panel))
    for x in (-340,-270,-200):
        parts.append(kit.box("command_roof_hatch",(x,38,98.3),(42,24,3),trim))
    for y in (-100,-35,35,100):
        parts.append(kit.cylinder("drive_socket",(-398,y,45),20,68,(-1,0,0),dark))
        parts.append(kit.cylinder("drive_rim",(-432,y,45),20,10,(-1,0,0),trim,cap_end=False))
        parts.append(kit.cylinder("drive_glow",(-434,y,45),15,2,(-1,0,0),glow))
    parts.append(kit.sphere("navigation_dome",(-380,38,98),23,hull,scale=(1,.8,.4)))
    ship=kit.finalize(parts,length)
    for vertex in ship.data.vertices:
        vertex.co.y *= .988
        vertex.co.z *= .993
    ship.data.update()
    return ship
