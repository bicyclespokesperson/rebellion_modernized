"""Victory Destroyer: flanged dagger hull, stepped decks and broad command tower.

The original mesh's atmospheric wing planform is retained, including its forward
notches and sharply tapered stern. The two Victory references share this geometry.
"""
from __future__ import annotations

import math
import random

import bpy

import _kit as kit

TYPE_INDEX = 19
NAME = "Victory Destroyer"
ENCYCLOPEDIA = "EDATA.061"
# Class length from tools/tactical_sprites_bpy.py (original mesh units x 800 m).
LENGTH = 1469.0
# The encyclopedia render looks at the port bow from slightly above (degrees; yaw 0 is dead ahead).
VIEW = {"yaw": (0, 90), "pitch": (0, 40)}

BOW, MAX_X, STERN = 735.0, -583.0, -735.0
HALF_BEAM, STERN_HALF_BEAM = 392.0, 60.0
BAND = 24.0
LIP = 8.0
TRENCH_DEPTH = 14.0
DORSAL_SLOPE = 0.29
VENTRAL_SLOPE = 0.28
SEED = 1977

# Raised dorsal plate: front x, rear x, front and rear half-widths, height above the deck.
PLATE = (45.0, -695.0, 74.0, 235.0, 10.0)
# Superstructure tiers, each a narrow forward block and a wider rear shoulder:
# (front x, shoulder x, rear x, front half-width, shoulder half-width, top z).
TIERS = (
    (-190.0, -400.0, -640.0, 155.0, 204.0, 151.0),
    (-310.0, -465.0, -635.0, 132.0, 173.0, 183.0),
    (-400.0, -520.0, -630.0, 105.0, 137.0, 213.0),
)
SLAB = 13.0
BRIDGE_FRONT, BRIDGE_REAR, BRIDGE_HALF_WIDTH = -437.0, -583.0, 130.0
BRIDGE_BOTTOM, BRIDGE_FRONT_TOP, BRIDGE_REAR_TOP = 264.0, 316.0, 328.0
DOMES = (-471.0, 105.0, 359.0, 24.0)


def half_beam(x: float) -> float:
    if x >= MAX_X:
        return HALF_BEAM * (BOW - x) / (BOW - MAX_X)
    return HALF_BEAM + (STERN_HALF_BEAM - HALF_BEAM) * (MAX_X - x) / (MAX_X - STERN)


def ramp(x: float) -> float:
    return min(1.0, max(0.0, (BOW - x) / (BOW - MAX_X)))


def ridge(x: float) -> float:
    return BAND + DORSAL_SLOPE * HALF_BEAM * ramp(x)


def keel(x: float) -> float:
    return -VENTRAL_SLOPE * HALF_BEAM * ramp(x)


def deck(x: float, y: float) -> float:
    w = half_beam(x)
    return BAND + (ridge(x) - BAND) * max(0.0, 1 - abs(y) / w) if w > 0 else BAND


def hull_section(x: float) -> list[tuple[float, float, float]]:
    w = half_beam(x)
    depth = min(TRENCH_DEPTH, 0.35 * w)
    port = [(w, BAND), (w, BAND - LIP), (w - depth, BAND - LIP), (w - depth, LIP), (w, LIP), (w, 0.0)]
    ring = [(x, 0.0, ridge(x))] + [(x, y, z) for y, z in port] + [(x, 0.0, keel(x))]
    ring += [(x, -y, z) for y, z in reversed(port)]
    return ring


def hull(material: bpy.types.Material, trench: bpy.types.Material) -> list[bpy.types.Object]:
    stations = [BOW - 6, 500, 250, 44, -200, -450, MAX_X, STERN]
    body = kit.loft("hull", [hull_section(x) for x in stations], material)
    body.data.materials.append(trench)
    for polygon in body.data.polygons:
        in_trench = abs(polygon.center.z - BAND / 2) < BAND / 2 - LIP + 0.5
        if in_trench and abs(polygon.normal.z) < 0.2 and abs(polygon.normal.x) < 0.9:
            polygon.material_index = 1
    return [kit.bevel(body, 2.5, 1, 25)]


def dorsal_plate(material: bpy.types.Material) -> bpy.types.Object:
    front, rear, w_front, w_rear, lift = PLATE

    def section(x: float) -> list[tuple[float, float, float]]:
        t = (front - x) / (front - rear)
        w = min(w_front + (w_rear - w_front) * t, half_beam(x) - 20)
        return [(x, 0.0, ridge(x) + lift), (x, w, deck(x, w) + lift), (x, w, BAND),
                (x, -w, BAND), (x, -w, deck(x, w) + lift)]

    stations = [front, 0.0, -200.0, -400.0, MAX_X, rear]
    return kit.bevel(kit.loft("dorsal_plate", [section(x) for x in stations], material), 1.5, 1, 20)


def block(name: str, front: float, rear: float, half_width: float, base: float, top: float, slab_mat: bpy.types.Material, wall_mat: bpy.types.Material,
          inset: float = 7.0) -> list[bpy.types.Object]:
    outline = [(rear, -half_width), (front, -half_width), (front, half_width), (rear, half_width)]
    walls = [(rear + inset, -half_width + inset), (front - inset, -half_width + inset),
             (front - inset, half_width - inset), (rear + inset, half_width - inset)]
    wall = kit.prism(f"{name}_wall", walls, base, top - SLAB, wall_mat)
    slab = kit.prism(f"{name}_slab", outline, top - SLAB, top, slab_mat, top_inset=0.01)
    return [wall, kit.bevel(slab, 1.5, 1, 30)]


def superstructure(slab_mat: bpy.types.Material, wall_mat: bpy.types.Material) -> list[bpy.types.Object]:
    parts = []
    base = BAND
    for i, (front, shoulder, rear, w_front, w_shoulder, top) in enumerate(TIERS):
        parts += block(f"tier{i}", front, rear, w_front, base, top, slab_mat, wall_mat)
        parts += block(f"tier{i}_shoulder", shoulder, rear, w_shoulder, base, top - 4, slab_mat, wall_mat)
        base = top - 3
    return parts


def tower(hull_mat: bpy.types.Material, wall_mat: bpy.types.Material, dome_mat: bpy.types.Material,
          window_mat: bpy.types.Material) -> list[bpy.types.Object]:
    top_tier = TIERS[-1][-1]
    neck_bottom = [(-591.0, -96.0), (-451.0, -84.0), (-451.0, 84.0), (-591.0, 96.0)]
    neck_top = [(-567.0, -76.0), (-467.0, -70.0), (-467.0, 70.0), (-567.0, 76.0)]
    neck = kit.loft("neck", [[(x, y, top_tier - 2) for x, y in neck_bottom],
                             [(x, y, BRIDGE_BOTTOM + 2) for x, y in neck_top]], hull_mat)

    def bridge_section(y: float, shrink: float) -> list[tuple[float, float, float]]:
        profile = [(BRIDGE_FRONT - 8 - shrink, BRIDGE_BOTTOM), (BRIDGE_FRONT - shrink, BRIDGE_BOTTOM + 30),
                   (BRIDGE_FRONT - 6 - shrink, BRIDGE_FRONT_TOP), (BRIDGE_REAR + 8 + shrink, BRIDGE_REAR_TOP),
                   (BRIDGE_REAR + shrink, BRIDGE_BOTTOM + 26), (BRIDGE_REAR + 10 + shrink, BRIDGE_BOTTOM)]
        return [(x, y, z) for x, z in profile]

    w = BRIDGE_HALF_WIDTH
    bridge = kit.loft("bridge", [bridge_section(-w, 16), bridge_section(-w + 18, 0), bridge_section(w - 18, 0),
                                 bridge_section(w, 16)], hull_mat)
    strip = ((-w + 24, BRIDGE_BOTTOM + 24), (-w + 24, BRIDGE_BOTTOM + 34), (w - 24, BRIDGE_BOTTOM + 34),
             (w - 24, BRIDGE_BOTTOM + 24))
    windows = kit.loft("bridge_windows", [[(x, y, z) for y, z in strip]
                                          for x in (BRIDGE_FRONT + 0.5, BRIDGE_FRONT - 3.5)], window_mat)
    bridge.data.materials.append(wall_mat)
    for polygon in bridge.data.polygons:
        if abs(polygon.normal.x) > 0.5:
            polygon.material_index = 1
    parts = [neck, kit.bevel(bridge, 1.5, 1, 30), windows]
    dome_x, dome_y, dome_z, dome_r = DOMES
    for side in (-1, 1):
        parts.append(kit.cylinder(f"dome_stalk{side}", (dome_x, side * dome_y, dome_z - dome_r - 6), 9.0, 22.0,
                                  material=hull_mat))
        parts.append(kit.sphere(f"dome{side}", (dome_x, side * dome_y, dome_z), dome_r, dome_mat))
    parts.append(kit.box("sensor_block", (-510.0, 0.0, BRIDGE_REAR_TOP + 8), (56.0, 96.0, 26.0), hull_mat))
    parts.append(kit.box("sensor_deck", (-515.0, 0.0, BRIDGE_REAR_TOP + 24), (40.0, 70.0, 12.0), hull_mat))
    parts.append(kit.box("sensor_mast", (-495.0, 0.0, BRIDGE_REAR_TOP + 34), (10.0, 120.0, 6.0), hull_mat))
    parts.append(kit.box("sensor_post", (-495.0, 0.0, BRIDGE_REAR_TOP + 27), (8.0, 8.0, 14.0), hull_mat))
    parts.append(kit.box("communications_mast", (-510.0, 0.0, 393.0), (5.0, 5.0, 100.0), hull_mat))
    parts.append(kit.box("bridge_sensor_boom", (-350.0, 0.0, 288.0), (190.0, 4.0, 4.0), hull_mat))
    return parts


def engines(housing: bpy.types.Material, glow: bpy.types.Material) -> list[bpy.types.Object]:
    parts = []
    for i, (y, radius) in enumerate(((-66.0, 35.0), (66.0, 35.0), (0.0, 22.0))):
        x = -716.0 if y else -738.0
        parts.append(kit.cylinder(f"nozzle{i}", (x, y, 35.0), radius, 24.0,
                                  (-1, 0, 0), housing, cap_end=False))
        parts.append(kit.cylinder(f"glow{i}", (x - 10, y, 35.0), radius * 0.88, 2.0,
                                  (-1, 0, 0), glow))
    return parts


def atmospheric_fins(material: bpy.types.Material) -> list[bpy.types.Object]:
    parts = []
    for side in (-1, 1):
        outline = [(44.0, side * 208), (44.0, side * 270), (-450.0, side * 419),
                   (-583.0, side * 392), (-450.0, side * 352)]
        if side < 0:
            outline.reverse()
        parts.append(kit.bevel(kit.prism(f"atmospheric_fin{side}", outline, 3.0, 14.0, material), 1.0, 1, 30))
    return parts


def trench_greebles(rng: random.Random) -> list[kit.Placement]:
    blocks = []
    for _ in range(1100):
        x = rng.uniform(STERN + 8, BOW - 60)
        w = half_beam(x)
        depth = min(TRENCH_DEPTH, 0.35 * w)
        side = rng.choice((-1, 1))
        size = (rng.uniform(5, 30), rng.uniform(2, depth - 1), rng.uniform(3, BAND - 2 * LIP - 2))
        if depth >= 4:
            z = rng.uniform(LIP + size[2] / 2, BAND - LIP - size[2] / 2)
            blocks.append(((x, side * (w - depth + size[1] / 2), z), size, -side * math.atan(half_beam(x - 1) - w)))
    return blocks


def shoulder_greebles(rng: random.Random) -> list[kit.Placement]:
    """Rows of low blocks parallel to the dorsal plate beside the superstructure, as on the studio model; the
    forward deck stays clean plating."""
    front, rear, w_front, w_rear, _ = PLATE
    angle = math.atan((w_rear - w_front) / (front - rear))
    blocks = []
    for side in (-1, 1):
        for row, offset in enumerate((18.0, 44.0, 70.0)):
            x = -768.0
            while x < -200.0 + row * 60:
                size_x = rng.uniform(18, 56)
                y = w_front + (w_rear - w_front) * (front - x) / (front - rear) + offset
                if y < half_beam(x) - 50:
                    size = (size_x, rng.uniform(12, 20), rng.uniform(4, 10))
                    blocks.append(((x + size_x / 2, side * y, deck(x, y) + size[2] / 2 - 1), size, -side * angle))
                x += size_x + rng.uniform(5, 22)
    return blocks


def tier_greebles(rng: random.Random) -> list[kit.Placement]:
    blocks = []
    for _ in range(120):
        front, _, rear, w_front, _, top = TIERS[rng.randrange(len(TIERS))]
        x = rng.uniform(rear + 12, front - 12)
        y = rng.uniform(-1, 1) * (w_front - 12)
        size = (rng.uniform(6, 22), rng.uniform(5, 16), rng.uniform(2, 6))
        blocks.append(((x, y, top + size[2] / 2 - 0.5), size, 0.0))
    return blocks


def turbolasers(housing: bpy.types.Material) -> list[bpy.types.Object]:
    parts = []
    for i, x in enumerate((-290.0, -360.0, -430.0, -500.0, -570.0)):
        for side in (-1, 1):
            y = side * min(half_beam(x) * 0.8, 330.0)
            z = deck(x, y)
            parts.append(kit.box(f"battery{i}{side}", (x, y, z + 7), (30.0, 24.0, 16.0), housing))
            for barrel in (-6.0, 6.0):
                parts.append(kit.box(f"barrel{i}{side}{barrel}", (x + 22, y + barrel, z + 10), (26.0, 3.0, 3.0),
                                     housing))
    return parts


def build(palette: kit.Palette, length: float) -> bpy.types.Object:
    hull_mat = kit.plated("victory_hull", kit.scaled(palette.light, 0.92), 90.0, contrast=0.06, seam=0.62, grime=0.1)
    plate_mat = kit.plated("victory_plate", kit.scaled(palette.light, 0.97), 45.0, contrast=0.06, seam=0.62)
    wall_mat = kit.plated("victory_walls", kit.mix(palette.dark, palette.mid, 0.5), 14.0, contrast=0.2, seam=0.35,
                          roughness=0.7)
    trench_mat = kit.plain("victory_trench", kit.mix(palette.dark, palette.mid, 0.35), roughness=0.75, metallic=0.2)
    greeble_mat = kit.plated("victory_greebles", kit.mix(palette.mid, palette.light, 0.45), 12.0, contrast=0.2)
    housing_mat = kit.plain("victory_housing", kit.mix(palette.dark, palette.mid, 0.6), roughness=0.5, metallic=0.4)
    dome_mat = kit.plain("victory_domes", kit.scaled(palette.light, 0.95), roughness=0.4, metallic=0.2)
    window_mat = kit.emissive("victory_windows", (0.45, 0.4, 0.3, 1.0), 0.6)
    glow_mat = kit.emissive("victory_engine_glow", (0.42, 0.66, 1.0, 1.0), 2.6)

    rng = random.Random(SEED)
    parts = hull(hull_mat, trench_mat)
    parts.append(dorsal_plate(plate_mat))
    parts += superstructure(plate_mat, wall_mat)
    parts += tower(plate_mat, wall_mat, dome_mat, window_mat)
    parts += engines(housing_mat, glow_mat)
    parts += atmospheric_fins(plate_mat)
    parts += turbolasers(housing_mat)
    parts.append(kit.boxes("greebles", trench_greebles(rng) + shoulder_greebles(rng) + tier_greebles(rng),
                           greeble_mat))
    return kit.finalize(parts, length)
