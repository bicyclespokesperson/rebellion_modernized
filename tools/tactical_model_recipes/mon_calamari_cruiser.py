"""Mon Calamari cruiser (MC80 Liberty type): a lofted, flattened organic hull with a raised dorsal terrace, swept
wings ending in nacelle pods, a lateral rim and scattered blisters, unioned into one shell by voxel remesh and
relaxed into fillets; bronze engine pods at the stern.

Authored in design units with the bow tip at x = +865 and the stern at x = -928; the assembled model is scaled to
the requested length. Blisters, pods and rim are deliberately not mirrored: Mon Calamari ships are never quite
symmetric.
"""
from __future__ import annotations

import math
import random

import _kit as kit

TYPE_INDEX = 0
NAME = "Mon Calamari Cruiser"
ENCYCLOPEDIA = "EDATA.042"
# Class length from tools/tactical_sprites_bpy.py (original mesh units x 800 m).
LENGTH = 1750.0
# The encyclopedia render looks at the port bow from slightly above (degrees; yaw 0 is dead ahead).
VIEW = {"yaw": (0, 90), "pitch": (0, 40)}

SEED = 1983
EXPONENT = 2.0
STATIONS = 96
RING = 64
VOXEL = 5.0

HALF_WIDTH = kit.profile([(865, 0), (845, 40), (780, 95), (650, 160), (450, 215), (200, 248), (0, 258),
                          (-250, 250), (-500, 228), (-700, 196), (-840, 140), (-905, 76), (-928, 0)])
TOP = kit.profile([(865, 0), (845, 22), (780, 54), (650, 88), (450, 110), (250, 116), (0, 114), (-300, 112),
                   (-600, 106), (-760, 92), (-860, 62), (-910, 30), (-928, 0)])
BOTTOM = kit.profile([(865, 0), (845, 24), (780, 58), (650, 96), (450, 118), (250, 122), (0, 118), (-300, 110),
                      (-600, 100), (-760, 86), (-860, 56), (-910, 26), (-928, 0)])
# Raised dorsal terrace: its flat top and steep flanks give the stepped upper hull of the encyclopedia render.
TERRACE_WIDTH = kit.profile([(720, 0), (640, 70), (400, 128), (0, 150), (-400, 140), (-700, 110), (-860, 50),
                             (-900, 0)])
TERRACE_RISE = kit.profile([(720, 0), (600, 14), (400, 24), (0, 26), (-400, 24), (-700, 18), (-900, 0)])
TERRACE_EXPONENT = 4.0

# Wing (port side), from root to tip: span y, leading-edge x, trailing-edge x, thickness. The planform follows the
# original mesh: the leading edge leaves the hull at its widest point and sweeps shallowly back to the pod.
WING = ((150.0, 220.0, -700.0, 64.0), (300.0, 110.0, -600.0, 52.0), (380.0, 10.0, -555.0, 44.0),
        (450.0, -250.0, -530.0, 36.0), (496.0, -400.0, -520.0, 30.0))
# Nacelle pods at the wing tips: centre and semi-axes; the starboard pod sits a little further aft.
PODS = (((-465.0, 500.0, 2.0), (215.0, 40.0, 38.0)), ((-485.0, -496.0, 2.0), (205.0, 39.0, 37.0)))


def hull_rings() -> list[list[tuple[float, float, float]]]:
    xs = [865 - (865 + 928) * i / (STATIONS - 1) for i in range(1, STATIONS - 1)]
    return [kit.superellipse_ring(x, HALF_WIDTH(x), TOP(x), BOTTOM(x), EXPONENT, RING) for x in xs]


def terrace_rings() -> list[list[tuple[float, float, float]]]:
    xs = [720 - 1620 * i / 63 for i in range(1, 63)]
    return [kit.superellipse_ring(x, TERRACE_WIDTH(x), TOP(x) + TERRACE_RISE(x), 10.0, TERRACE_EXPONENT, RING)
            for x in xs]


def top_surface(x: float, y: float) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    """Point and outward normal on the upper surface (hull or dorsal terrace, whichever is higher)."""
    width, height = HALF_WIDTH(x), TOP(x)
    z = kit.surface_height(width, height, EXPONENT, y)
    normal = kit.section_normal(width, height, EXPONENT, y, max(z, 1e-3))
    t_width, t_height = TERRACE_WIDTH(x), TOP(x) + TERRACE_RISE(x)
    if abs(y) < t_width:
        tz = kit.surface_height(t_width, t_height, TERRACE_EXPONENT, y)
        if tz > z:
            z, normal = tz, kit.section_normal(t_width, t_height, TERRACE_EXPONENT, y, max(tz, 1e-3))
    return (x, y, z), normal


def bottom_surface(x: float, y: float) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
    width, depth = HALF_WIDTH(x), BOTTOM(x)
    z = -kit.surface_height(width, depth, EXPONENT, y)
    return (x, y, z), kit.section_normal(width, depth, EXPONENT, y, min(z, -1e-3))


def wing(side: int) -> object:
    sections = []
    for span, lead, trail, thickness in WING:
        centre, chord = (lead + trail) / 2, (lead - trail) / 2
        sections.append([(centre + chord * math.cos(a), side * span, thickness / 2 * math.sin(a))
                         for a in (2 * math.pi * i / 32 for i in range(32))])
    return kit.loft(f"wing{side}", sections)


def rim(side: int) -> object:
    sections = []
    for i in range(48):
        x = 760 - 1620 * i / 47
        radius = 20 * max(0.0, math.sin(math.pi * i / 47)) ** 0.5 + 2
        y = side * (HALF_WIDTH(x) - 6)
        sections.append([(x, y + radius * math.cos(a), -6 + radius * math.sin(a))
                         for a in (2 * math.pi * k / 16 for k in range(16))])
    return kit.loft(f"rim{side}", sections)


def blister_radius(rng: random.Random) -> float:
    roll = rng.random()
    return rng.uniform(16, 26) if roll < 0.55 else rng.uniform(26, 40) if roll < 0.9 else rng.uniform(40, 58)


def dome(name: str, point, normal, radius: float, stretch: float, material) -> list:
    """A shallow button dome on a slightly wider collar, both standing on the hull along its normal."""
    foot = tuple(p - n * radius * 0.08 for p, n in zip(point, normal))
    collar_centre = tuple(f + n * radius * 0.05 for f, n in zip(foot, normal))
    return [kit.sphere(name, foot, radius, material, scale=(stretch, 1.0, 0.3), segments=24, rings=10, up=normal),
            kit.cylinder(f"{name}_collar", collar_centre, radius * 1.08, radius * 0.16, normal, material, segments=24)]


def blisters(rng: random.Random, material) -> list:
    """Crisp domes, added after the remesh so they keep a sharp foot like the encyclopedia render's, over the
    upper hull out to the rim, denser along the terrace, placed independently on each side, plus a sparser
    scattering underneath."""
    parts = []
    for i in range(95):
        x = rng.uniform(-860, 600)
        reach = TERRACE_WIDTH(x) if rng.random() < 0.4 else HALF_WIDTH(x) * 0.94
        point, normal = top_surface(x, rng.uniform(-1, 1) * reach)
        radius = blister_radius(rng) * (0.7 if x > 350 else 1.0)
        parts += dome(f"dome{i}", point, normal, radius, rng.uniform(1.0, 1.4), material)
    for i in range(28):
        x = rng.uniform(-800, 520)
        point, normal = bottom_surface(x, rng.uniform(-0.95, 0.95) * HALF_WIDTH(x))
        parts += dome(f"keel_dome{i}", point, normal, rng.uniform(16, 34), 1.2, material)
    return parts


def flank_ridges(rng: random.Random) -> list:
    """Long low ridges along the upper flanks, different on each side."""
    parts = []
    for side, spans in ((1, ((0.9, 560, -760), (0.78, 380, -420))), (-1, ((0.88, 600, -700), (0.74, 200, -820)))):
        for k, (fraction, start, end) in enumerate(spans):
            sections = []
            for i in range(40):
                x = start + (end - start) * i / 39
                radius = 9 * max(0.0, math.sin(math.pi * i / 39)) ** 0.4 + 1
                point, normal = top_surface(x, side * fraction * HALF_WIDTH(x))
                cy, cz = point[1] - normal[1] * 3, point[2] - normal[2] * 3
                sections.append([(x, cy + radius * math.cos(a), cz + radius * math.sin(a))
                                 for a in (2 * math.pi * j / 12 for j in range(12))])
            parts.append(kit.loft(f"ridge{side}{k}", sections))
    return parts


def shell(rng: random.Random, material) -> object:
    parts = [kit.loft("hull", hull_rings()), kit.loft("terrace", terrace_rings())]
    parts += [wing(1), wing(-1), rim(1), rim(-1)]
    parts += [kit.sphere(f"pod{i}", centre, 1.0, scale=axes, segments=40, rings=20)
              for i, (centre, axes) in enumerate(PODS)]
    parts += flank_ridges(rng)
    body = kit.join(parts, "shell")
    kit.remesh_smooth(body, VOXEL, smooth_iterations=6, smooth_factor=0.5)
    body.data.materials.clear()
    body.data.materials.append(material)
    for polygon in body.data.polygons:
        polygon.use_smooth = True
    return kit.join([body] + blisters(rng, material), "shell")


def engines(housing, glow) -> list:
    parts = []
    for i, (y, z, r) in enumerate(((0.0, 18.0, 40.0), (78.0, 26.0, 30.0), (-74.0, 22.0, 32.0), (40.0, -30.0, 24.0),
                                   (-38.0, -28.0, 24.0))):
        x = -905.0 + abs(y) * 0.5
        parts.append(kit.sphere(f"engine{i}", (x, y, z), r, housing, scale=(1.5, 1.0, 1.0), segments=24, rings=12))
        parts.append(kit.cylinder(f"engine_glow{i}", (x - r * 1.45, y, z), r * 0.62, 3.0, (-1, 0, 0), glow))
    for i, (x, y) in enumerate(((-700.0, 40.0), (-760.0, -55.0))):
        parts.append(kit.sphere(f"dorsal_engine{i}", (x, y, top_surface(x, y)[0][2] + 12), 26.0, housing,
                                scale=(1.6, 1.0, 0.9), segments=24, rings=12))
    return parts


def build(palette: kit.Palette, length: float):
    hues = palette.tints[3:7]
    level = sum(kit.luminance(t) for t in hues) / len(hues)
    mauve, lavender, khaki, pale = (kit.saturated(kit.mix(kit.relit(t, level), t, 0.5), 1.7) for t in hues)
    skin = kit.organic("moncal_skin", [(0.3, mauve), (0.42, lavender), (0.52, pale), (0.6, lavender), (0.72, khaki)],
                       110.0, mottle_size=45.0, port_size=5.0, port_density=0.12, belly=(-90.0, 10.0, 0.6))
    bronze = kit.plain("moncal_bronze", kit.mix(palette.accent, (0.32, 0.17, 0.05, 1.0), 0.6), roughness=0.4,
                       metallic=0.6)
    glow = kit.emissive("moncal_engine_glow", (0.55, 0.75, 1.0, 1.0), 2.6)
    rng = random.Random(SEED)
    parts = [shell(rng, skin)] + engines(bronze, glow)
    return kit.finalize(parts, length)
