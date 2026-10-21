"""Twin hexagonal solar wings, spoke-framed cockpit and short armoured pylons."""
from __future__ import annotations

import math
import bpy
import _kit as kit
import _tie

TYPE_INDEX = 33
NAME = "TIE Fighter"
ENCYCLOPEDIA = "EDATA.038"
FIT = "size"
LENGTH = 60.0
VIEW = {"yaw": (-85, 85), "pitch": (-25, 30)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    materials = _tie.materials(palette, "tie_fighter")
    parts = _tie.cockpit(materials, radius=8)
    outline = [(-22.7,-27),(22.7,-27),(24.8,0),(22.7,27),(-22.7,27),(-24.8,0)]
    for angle in (0, math.pi):
        parts.extend(_tie.panel(materials, outline, 22, angle))
        parts.extend(_tie.pylon(materials, 22, angle))
    for y in (-2.8,2.8):
        parts.append(_tie.beam("chin_cannon",(6,y,-5),(11,y,-5),.38,materials.frame))
    return kit.finalize(parts, length, FIT)
