"""Parallel armoured pilot and ordnance pods between folded solar wings."""
from __future__ import annotations

import math
import bpy
import _kit as kit
import _tie

TYPE_INDEX = 35
NAME = "TIE Bomber"
ENCYCLOPEDIA = "EDATA.040"
FIT = "size"
LENGTH = 62.4
VIEW = {"yaw": (-40, -5), "pitch": (0, 45)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    m = _tie.materials(palette, "tie_bomber")
    parts: list[bpy.types.Object] = []
    for y in (-10,10):
        parts.append(kit.cylinder("longitudinal_pod",(0,y,0),8.5,31,(1,0,0),m.armour,segments=32))
        for x in (-12,-2,10):
            parts.append(kit.cylinder("pod_armour_collar",(x,y,0),8.9,2,(1,0,0),m.frame,segments=32))
        parts.append(kit.cylinder("rear_drive_rim",(-16,y,0),5.5,3,(-1,0,0),m.frame,segments=32,cap_end=False))
        parts.append(kit.cylinder("rear_drive_glow",(-17,y,0),4.7,.2,(-1,0,0),m.exhaust,segments=32))
    cockpit = _tie.cockpit(m,(9,-10,0),8.5)
    parts.extend(cockpit)
    parts.append(kit.cylinder("ordnance_front_plate",(16,10,0),7,1,(1,0,0),m.frame,segments=32))
    parts.append(kit.cylinder("ordnance_bay",(16.7,10,0),2.7,.3,(1,0,0),m.recess,segments=16))
    parts.append(kit.cylinder("ordnance_bay_centre",(17,10,0),.9,.4,(1,0,0),m.armour,segments=16))
    parts.append(kit.box("pod_bridge",(0,0,0),(20,10,9),m.armour))
    for x in (2,5,8,11):
        parts.append(kit.box("cockpit_dorsal_rib",(x,-10,8.2),(1.2,7,1.3),m.frame))
    parts.append(kit.box("ordnance_chute",(8,10,-9.8),(7,6,4),m.armour))
    parts.append(kit.box("ordnance_chute_face",(10.8,10,-10.4),(1.6,4,2),m.frame))
    for part in parts:
        part.location.z += 3
    for side in (-1,1):
        def point(x: float,z: float,radial: float,offset: float=0) -> kit.Vec3:
            return x,side*(radial+offset),z
        central=[point(-20,-13.3,29),point(18,-13.3,29),point(18,13.3,29),point(-20,13.3,29)]
        parts.append(kit.loft("central_solar_panel",[central,[(x,y+side*.5,z) for x,y,z in central]],m.solar))
        for a,b in zip(central,central[1:]+central[:1]):
            parts.append(_tie.beam("solar_frame",a,b,.45,m.frame))
        parts.append(_tie.beam("panel_divider",point(0,-13.3,29,.6),point(0,13.3,29,.6),.35,m.frame))
        for direction in (-1,1):
            corners=[point(-20,direction*13.3,29),point(18,direction*13.3,29),
                     point(21,direction*18.9,19),point(-20,direction*18.9,19)]
            parts.append(kit.loft("inward_folded_solar_lip",[corners,[(x,y+side*.5,z) for x,y,z in corners]],m.solar))
            for a,b in zip(corners,corners[1:]+corners[:1]):
                parts.append(_tie.beam("fold_frame",a,b,.45,m.frame))
        parts.extend(_tie.pylon(m,29,0 if side==1 else math.pi))
    return kit.finalize(parts,length,FIT)
