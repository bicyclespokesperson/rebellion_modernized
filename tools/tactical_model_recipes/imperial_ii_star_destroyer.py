"""Imperial II: the encyclopedia shares the Imperial hull, with its own sampled plating palette.

EDATA.068 repeats EDATA.062's tiers and engines. Its fitted bridge sits 45 design units
forward; the hull retains the remodel's deliberate square stern.
"""
from __future__ import annotations

import bpy
from mathutils import Matrix

import _kit as kit
import imperial_star_destroyer

TYPE_INDEX = 25
NAME = "Imperial II Star Destroyer"
ENCYCLOPEDIA = "EDATA.068"
LENGTH = 1602.0
VIEW = {"yaw": (0, 90), "pitch": (0, 40)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    parts = imperial_star_destroyer.build_parts(palette)
    for part in parts:
        if part.name.startswith(("neck", "bridge", "dome", "sensor")):
            part.data.transform(Matrix.Translation((45.0, 0.0, 0.0)))
    return kit.finalize(parts, length)
