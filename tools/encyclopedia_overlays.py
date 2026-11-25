"""Redraw ship state overlays (damage fire, en-route engine trails) to fit HD encyclopedia cutouts.

CreateObjectPicture draws a per-record overlay under the ship picture: GOKRES `id + 0x2000`/`0x6000` when the ship
is damaged and `id + 0x1000`/`0x5000` when it is en route. The original overlays are positioned for the original
sprite, whose pose differs from the encyclopedia artwork, so they are regenerated from the HD silhouette instead.
"""
from __future__ import annotations

import hashlib

import cv2
import numpy as np
from PIL import Image

UNIT_RECORDS = [*range(0x640, 0x644), *range(0x680, 0x684), *range(0x740, 0x74f), *range(0x780, 0x78f)]
DAMAGE_OFFSET = 0x2000
ENROUTE_OFFSET = 0x1000
FIRE_RAMP = np.array([[0.0, 90, 10, 5], [0.12, 150, 25, 8], [0.3, 215, 60, 10], [0.5, 245, 135, 20],
                      [0.7, 252, 205, 50], [0.88, 255, 245, 170], [1.0, 255, 255, 225]])


def overlay_targets() -> dict[int, tuple[int, int]]:
    """Map overlay resource id to (base picture id, overlay offset)."""
    targets = {}
    for record in UNIT_RECORDS:
        for picture in (record, record + 0x4000):
            for offset in (DAMAGE_OFFSET, ENROUTE_OFFSET):
                targets[picture + offset] = (picture, offset)
    return targets


def opaque(image: Image.Image, key: int, size: tuple[int, int] | None = None) -> np.ndarray:
    pixels = np.asarray(image) != key
    if size and pixels.shape != (size[1], size[0]):
        padded = np.zeros((size[1], size[0]), dtype=bool)
        height, width = min(pixels.shape[0], size[1]), min(pixels.shape[1], size[0])
        padded[:height, :width] = pixels[:height, :width]
        pixels = padded
    return pixels


def seeded(*parts: object) -> np.random.Generator:
    digest = hashlib.sha256(repr(parts).encode()).digest()
    return np.random.default_rng(int.from_bytes(digest[:8], "little"))


def smooth_noise(shape: tuple[int, int], feature: float, rng: np.random.Generator) -> np.ndarray:
    cells = (max(2, int(shape[0] / feature) + 2), max(2, int(shape[1] / feature) + 2))
    coarse = rng.uniform(-1, 1, cells).astype(np.float32)
    return cv2.resize(coarse, (shape[1], shape[0]), interpolation=cv2.INTER_CUBIC)


def centroid(mask: np.ndarray) -> np.ndarray:
    ys, xs = np.nonzero(mask)
    return np.array([xs.mean(), ys.mean()])


def bbox(mask: np.ndarray) -> np.ndarray:
    ys, xs = np.nonzero(mask)
    return np.array([xs.min(), ys.min(), xs.max() + 1, ys.max() + 1], dtype=float)


def nearest_inside(mask: np.ndarray, point: np.ndarray) -> np.ndarray:
    ys, xs = np.nonzero(mask)
    index = int(np.argmin((xs - point[0]) ** 2 + (ys - point[1]) ** 2))
    return np.array([xs[index], ys[index]], dtype=float)


def silhouette_ratio(original: np.ndarray, hd: np.ndarray, scale: int) -> float:
    """Linear size of the HD silhouette relative to the scaled original one."""
    return float(np.sqrt(hd.sum() / (original.sum() * scale * scale)))


def fire_puffs(original: np.ndarray, overlay: np.ndarray, hd: np.ndarray, scale: int) -> list[tuple[np.ndarray, float]]:
    count, labels, stats, centers = cv2.connectedComponentsWithStats(overlay.astype(np.uint8), connectivity=8)
    source_box, target_box = bbox(original), bbox(hd)
    size = np.clip(silhouette_ratio(original, hd, scale), 0.65, 1.15)
    puffs = []
    for index in range(1, count):
        area = stats[index, cv2.CC_STAT_AREA]
        if area < 3:
            continue
        relative = (centers[index] - source_box[:2]) / np.maximum(source_box[2:] - source_box[:2], 1)
        target = target_box[:2] + relative * (target_box[2:] - target_box[:2])
        radius = np.sqrt(area / np.pi) * scale * size * 1.15
        puffs.append((nearest_inside(hd, target), float(radius)))
    return puffs


def ramp(values: np.ndarray, table: np.ndarray) -> np.ndarray:
    return np.stack([np.interp(values, table[:, 0], table[:, channel]) for channel in (1, 2, 3)], axis=-1)


def draw_fire(hd: np.ndarray, puffs: list[tuple[np.ndarray, float]], rng: np.random.Generator) -> np.ndarray:
    height, width = hd.shape
    ys, xs = np.mgrid[0:height, 0:width].astype(np.float32)
    heat = np.zeros(hd.shape, dtype=np.float32)
    for center, radius in puffs:
        coarse = smooth_noise(hd.shape, max(4.0, radius / 2.2), rng)
        fine = smooth_noise(hd.shape, max(2.0, radius / 6), rng)
        distance = np.hypot(xs - center[0], ys - center[1]) / radius
        heat = np.maximum(heat, 1 - distance + 0.38 * coarse + 0.14 * fine)
    rgba = np.zeros((height, width, 4), dtype=np.uint8)
    rgba[..., :3] = ramp(np.clip(heat, 0, 1), FIRE_RAMP)
    rgba[..., 3] = np.where(heat > 0, 255, 0)
    return rgba


def axes(direction: np.ndarray, shape: tuple[int, int]) -> tuple[np.ndarray, np.ndarray]:
    ys, xs = np.mgrid[0:shape[0], 0:shape[1]].astype(np.float32)
    return xs * direction[0] + ys * direction[1], ys * direction[0] - xs * direction[1]


def rear_point(ship: np.ndarray, along: np.ndarray, across: np.ndarray, band: float, tolerance: float) -> np.ndarray:
    """Rearmost ship pixel near the given across coordinate, as (along, across)."""
    near = ship & (np.abs(across - band) <= tolerance)
    if not near.any():
        near = ship & (np.abs(across - band) <= np.abs(across[ship] - band).min() + 1)
    index = np.argmax(np.where(near, along, -np.inf))
    return np.array([along.flat[index], across.flat[index]])


def streak_layer(rgb: np.ndarray, component: np.ndarray, factor: float, offset: np.ndarray,
                 shape: tuple[int, int]) -> np.ndarray:
    """Smoothly scale a streak by `factor` and place it so logical point p lands at p * factor + offset."""
    ys, xs = np.nonzero(component)
    top, bottom, left, right = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    alpha = component[top:bottom, left:right].astype(np.float32)
    colour = rgb[top:bottom, left:right].astype(np.float32) * alpha[..., None]
    size = (max(1, round((right - left) * factor)), max(1, round((bottom - top) * factor)))
    alpha = cv2.resize(alpha, size, interpolation=cv2.INTER_LINEAR)
    colour = cv2.resize(colour, size, interpolation=cv2.INTER_LINEAR) / np.maximum(alpha, 1e-6)[..., None]
    x0, y0 = (np.array([left, top]) * factor + offset).round().astype(int)
    layer = np.zeros((*shape, 4), dtype=np.uint8)
    sx0, sy0 = max(0, -x0), max(0, -y0)
    dx0, dy0 = max(0, x0), max(0, y0)
    width = min(size[0] - sx0, shape[1] - dx0)
    height = min(size[1] - sy0, shape[0] - dy0)
    if width <= 0 or height <= 0:
        return layer
    patch = slice(sy0, sy0 + height), slice(sx0, sx0 + width)
    target = slice(dy0, dy0 + height), slice(dx0, dx0 + width)
    layer[target][..., :3] = np.clip(colour[patch], 0, 255)
    layer[target][..., 3] = np.where(alpha[patch] >= 0.5, 255, 0)
    return layer


def draw_trail(original: np.ndarray, overlay: np.ndarray, overlay_rgb: np.ndarray, hd: np.ndarray,
               scale: int) -> np.ndarray:
    heading = centroid(overlay) - centroid(original)
    direction = heading / max(np.linalg.norm(heading), 1e-6)
    factor = scale * float(np.clip(silhouette_ratio(original, hd, scale), 0.6, 1.2))
    along, across = axes(direction, original.shape)
    hd_along, hd_across = axes(direction, hd.shape)
    source_span = np.array([across[original].min(), across[original].max()])
    target_span = np.array([hd_across[hd].min(), hd_across[hd].max()])
    count, labels = cv2.connectedComponents(overlay.astype(np.uint8), connectivity=8)
    rgba = np.zeros((*hd.shape, 4), dtype=np.uint8)
    for index in range(1, count):
        component = labels == index
        if component.sum() < 3:
            continue
        root_along = along[component].min()
        root_across = across[component & (along <= root_along + 1.5)].mean()
        source_rear = rear_point(original, along, across, root_across, 1.5)
        relative = (root_across - source_span[0]) / max(source_span[1] - source_span[0], 1)
        band = target_span[0] + relative * (target_span[1] - target_span[0])
        target_rear = rear_point(hd, hd_along, hd_across, band, 1.5 * scale)
        target_root = np.array([target_rear[0] + (root_along - source_rear[0]) * factor, target_rear[1]])
        root_xy = target_root[0] * direction + target_root[1] * np.array([-direction[1], direction[0]])
        source_root_xy = root_along * direction + root_across * np.array([-direction[1], direction[0]])
        layer = streak_layer(overlay_rgb, component, factor, root_xy - source_root_xy * factor, hd.shape)
        rgba = np.where(layer[..., 3:] > 0, layer, rgba)
    return rgba


def overlay_rgba(kind: int, original: np.ndarray, hd: np.ndarray, overlay: Image.Image, overlay_key: int,
                 scale: int, seed: object) -> np.ndarray | None:
    """Draw an overlay fitted to `hd` (the HD ship silhouette) from the original overlay and ship silhouette."""
    height, width = original.shape
    painted = opaque(overlay, overlay_key, (width, height))
    if not original.any() or not hd.any() or not painted.any():
        return None
    if kind == DAMAGE_OFFSET:
        puffs = fire_puffs(original, painted, hd, scale)
        return draw_fire(hd, puffs, seeded("fire", seed)) if puffs else None
    overlay_rgb = np.zeros((height, width, 3), dtype=np.uint8)
    rows, columns = min(height, overlay.height), min(width, overlay.width)
    overlay_rgb[:rows, :columns] = np.asarray(overlay.convert("RGB"))[:rows, :columns]
    return draw_trail(original, painted, overlay_rgb, hd, scale)


def crop_to(rgba: np.ndarray, overlay: Image.Image, scale: int) -> Image.Image:
    width, height = overlay.width * scale, overlay.height * scale
    canvas = np.zeros((height, width, 4), dtype=np.uint8)
    rows, columns = min(height, rgba.shape[0]), min(width, rgba.shape[1])
    canvas[:rows, :columns] = rgba[:rows, :columns]
    return Image.fromarray(canvas, "RGBA")
