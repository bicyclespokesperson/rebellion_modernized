"""Super Star Destroyer: long dagger, recessed stern and densely terraced city spine.

The shoulder and inset stern follow the original's distinctive plan silhouette;
its compressed game length is retained rather than the film's canonical scale.
"""
from __future__ import annotations

import random

import _kit as kit

TYPE_INDEX = 21
NAME = "Super Star Destroyer"
ENCYCLOPEDIA = "EDATA.063"
LENGTH = 4795.0
VIEW = {"yaw": (0, 90), "pitch": (0, 40)}

PROFILES = (
    (-2390.0, 1.0, 114.0, 16.0, 16.0),
    (-2357.0, 227.0, 112.0, 39.0, 17.0),
    (-1544.0, 353.0, 93.0, 38.0, 17.0),
    (-1232.0, 402.0, 89.0, 37.0, 16.0),
    (-1082.0, 813.0, 48.0, -70.0, -96.0),
    (2397.0, 0.5, -69.0, -69.0, -96.0),
)


def profile(x: float) -> tuple[float, float, float, float]:
    for a, b in zip(PROFILES, PROFILES[1:]):
        if x <= b[0]:
            t = max(0.0, (x - a[0]) / (b[0] - a[0]))
            return tuple(a[i] + t * (b[i] - a[i]) for i in range(1, 5))
    return PROFILES[-1][1:]


def deck(x: float, y: float) -> float:
    width, ridge, edge, _ = profile(x)
    return edge + (ridge - edge) * max(0.0, 1.0 - abs(y) / width)


def build(palette: kit.Palette, length: float) -> object:
    hull = kit.plated("ssd_hull", kit.scaled(palette.light, 0.94), 110.0, contrast=0.07, seam=0.6)
    city = kit.plated("ssd_city", kit.mix(palette.light, palette.mid, 0.55), 22.0, contrast=0.2)
    shadow = kit.plain("ssd_trench", kit.mix(palette.dark, palette.mid, 0.35))
    glow = kit.emissive("ssd_engines", (0.42, 0.66, 1.0, 1.0), 2.6)
    parts = []
    rings = []
    for x, width, ridge, edge, lower in PROFILES:
        keel = lower - (58.0 if x == -1082.0 else 0.0)
        rings.append([(x, 0.0, ridge), (x, width, edge), (x, width, lower),
                      (x, 0.0, keel), (x, -width, lower), (x, -width, edge)])
    parts.append(kit.bevel(kit.loft("dagger_hull", rings, hull), 1.5, 1, 25))
    # Repeated narrow terraces read as the extensive city rather than an ISD's three large slabs.
    for index, (front, rear, width, top) in enumerate((
        (1050.0, -2310.0, 45.0, 20.0), (450.0, -2250.0, 90.0, 35.0),
        (-150.0, -2140.0, 125.0, 49.0), (-650.0, -2010.0, 145.0, 62.0),
        (-1040.0, -1900.0, 118.0, 77.0), (-1060.0, -1520.0, 49.0, 105.0))):
        rings = []
        for x, w in ((front, width * 0.22), (rear, width)):
            z = min(top, deck(x, w) + 5.0 + index * 6.0)
            rings.append([(x, -w, z), (x, w, z), (x, w, z - 8.0), (x, -w, z - 8.0)])
        parts.append(kit.loft(f"city_terrace{index}", rings, city))
    parts.append(kit.box("tower_neck", (-820.0, 0.0, 63.0), (90.0, 50.0, 65.0), city))
    parts.append(kit.bevel(kit.box("command_bridge", (-798.0, 0.0, 105.0), (63.0, 90.0, 23.0), hull), 2.0, 1, 25))
    rng = random.Random(1980)
    placements = []
    for _ in range(1600):
        x = rng.uniform(-2270.0, 900.0)
        width, _, _, _ = profile(x)
        y = rng.uniform(-0.38, 0.38) * width
        size = (rng.uniform(10.0, 48.0), rng.uniform(6.0, 20.0), rng.uniform(3.0, 13.0))
        z = deck(x, y)
        for front, rear, terrace_width, top in ((1050, -2310, 45, 20), (450, -2250, 90, 35),
                                               (-150, -2140, 125, 49), (-650, -2010, 145, 62),
                                               (-1040, -1900, 118, 77)):
            if rear < x < front and abs(y) < terrace_width * (0.22 + 0.78 * (front - x) / (front - rear)):
                z = max(z, min(top, deck(x, y) + 9.0))
        placements.append(((x, y, z + size[2] / 2), size, 0.0))
    parts.append(kit.boxes("city_detail", placements, city))
    for index, y in enumerate((-165.0, -110.0, -55.0, 0.0, 55.0, 110.0, 165.0)):
        x = -2368.0 + abs(y) * 0.1
        parts.append(kit.cylinder(f"engine{index}", (x, y, 50.0), 18.0, 24.0, (-1, 0, 0), shadow, cap_end=False))
        parts.append(kit.cylinder(f"engine_glow{index}", (x - 10, y, 50.0), 14.0, 2.0, (-1, 0, 0), glow))
    return kit.finalize(parts, length)
