"""Victory II: the encyclopedia and original mesh retain the Victory hull and atmospheric fins."""
from __future__ import annotations

import bpy

import _kit as kit
from victory_destroyer import build as build_victory

TYPE_INDEX = 24
NAME = "Victory II Star Destroyer"
ENCYCLOPEDIA = "EDATA.067"
LENGTH = 1469.0
VIEW = {"yaw": (0, 90), "pitch": (0, 40)}


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    return build_victory(palette, length)
