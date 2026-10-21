"""Interdictor's shallow dagger hull, four gravity-well spheres and low command bar.

The original mesh supplies the 1024:471 plan proportions; the encyclopedia supplies
its gravity generators, which the original geometry omitted entirely.
"""
from __future__ import annotations

import random

import _kit as kit

TYPE_INDEX = 17
NAME = "Interdictor Cruiser"
ENCYCLOPEDIA = "EDATA.059"
LENGTH = 1024.0
VIEW = {"yaw": (0, 90), "pitch": (0, 30)}

BOW, STERN, BEAM = 512.0, -512.0, 235.6
GRAVITY_WELLS = ((-65.0, 83.0, -17.0, 60.0), (-295.0, 130.0, -12.0, 73.0))


def half_beam(x: float) -> float:
    return BEAM * (BOW - x) / (BOW - STERN)


def deck(x: float, y: float) -> float:
    return 28.0 * (BOW - x) / 1024.0 * max(0.0, 1.0 - abs(y) / max(half_beam(x), 0.1))


def build(palette: kit.Palette, length: float) -> object:
    hull = kit.plated("interdictor_hull", palette.light, 60.0, contrast=0.06, seam=0.62)
    walls = kit.plated("interdictor_walls", kit.mix(palette.mid, palette.dark, 0.35), 12.0, contrast=0.16)
    domes = kit.plated("gravity_generators", palette.light, 38.0, contrast=0.035, seam=0.8)
    machinery = kit.plain("interdictor_machinery", palette.mid, roughness=0.65)
    glow = kit.emissive("interdictor_engines", (0.42, 0.66, 1.0, 1.0), 2.6)
    sections = []
    for x in (510.0, 350.0, 100.0, -100.0, -300.0, STERN):
        w = half_beam(x)
        ridge = 28.0 * (BOW - x) / 1024.0
        sections.append([(x, 0, ridge), (x, w, 0), (x, w, -12), (x, 0, -54 * (BOW - x) / 1024),
                         (x, -w, -12), (x, -w, 0)])
    body = kit.loft("dagger_hull", sections, hull)
    body.data.materials.append(walls)
    for polygon in body.data.polygons:
        if abs(polygon.normal.z) < 0.2:
            polygon.material_index = 1
    parts = [kit.bevel(body, 1.1, 1, 25)]
    for x, y, z, r in GRAVITY_WELLS:
        for side in (-1, 1):
            parts.append(kit.sphere(f"gravity_well_{x}_{side}", (x, side * y, z), r, domes))
    parts += [kit.box("aft_deck", (-400.0, 0, 27), (170, 90, 27), hull),
              kit.box("command_neck", (-425.0, 0, 57), (65, 38, 57), walls),
              kit.box("bridge_lower", (-360.0, 0, 81), (95, 233, 15), walls),
              kit.box("bridge_roof", (-366.0, 0, 94), (87, 218, 12), hull),
              kit.box("sensor_deck", (-406.0, 0, 105), (52, 80, 10), hull)]
    for y in (-30.0, 30.0):
        parts.append(kit.cylinder(f"antenna{y}", (-406, y, 120), 1.5, 25, material=machinery))
    for y in (-90.0, 0.0, 90.0):
        parts.append(kit.cylinder(f"nozzle{y}", (-514, y, -11), 18, 12, (-1, 0, 0), machinery, cap_end=False))
        parts.append(kit.cylinder(f"glow{y}", (-519, y, -11), 15, 2, (-1, 0, 0), glow))
    rng = random.Random(1979)
    placements = []
    for _ in range(120):
        x = rng.uniform(-490, -170)
        y = rng.uniform(-0.8, 0.8) * half_beam(x)
        if any(abs(x - cx) < r + 10 and abs(abs(y) - cy) < r + 10
               for cx, cy, _, r in GRAVITY_WELLS):
            continue
        placements.append(((x, y, deck(x, y) + 1), (rng.uniform(5, 18), rng.uniform(3, 8), 2), 0))
    parts.append(kit.boxes("deck_fittings", placements, machinery))
    rim = []
    for x in range(-490, 450, 12):
        for side in (-1, 1):
            rim.append(((float(x), side * (half_beam(x) - 1.0), -6.0),
                        (rng.uniform(4, 9), 2.5, rng.uniform(3, 7)), 0.0))
    parts.append(kit.boxes("rim_machinery", rim, hull))
    return kit.finalize(parts, length)
