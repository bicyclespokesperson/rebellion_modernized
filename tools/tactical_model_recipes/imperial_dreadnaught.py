"""Imperial Dreadnaught using the same hull and its cooler encyclopedia palette."""
from __future__ import annotations

import bpy
import _kit as kit
import _dreadnaught

TYPE_INDEX = 28
NAME = "Imperial Dreadnaught"
ENCYCLOPEDIA = "EDATA.071"
LENGTH = 1056.0
VIEW = {"yaw": (0,85), "pitch": (0,25)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    return _dreadnaught.build(palette,length)
