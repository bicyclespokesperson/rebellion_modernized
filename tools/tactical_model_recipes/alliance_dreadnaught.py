"""Alliance Dreadnaught in the encyclopedia's warmer naval grey."""
from __future__ import annotations

import bpy
import _kit as kit
import _dreadnaught

TYPE_INDEX = 9
NAME = "Alliance Dreadnaught"
ENCYCLOPEDIA = "EDATA.051"
LENGTH = 1056.0
VIEW = {"yaw": (0,85), "pitch": (0,25)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    return _dreadnaught.build(palette,length)
