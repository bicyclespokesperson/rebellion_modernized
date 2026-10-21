#!/usr/bin/env -S uv run
"""Build remodelled tactical ship classes in Blender and validate them against the encyclopedia renders.

Each recipe in tools/tactical_model_recipes/ builds one class from primitives. For every class this tool samples
hull colours from the encyclopedia render, has Blender build and save the model, fits a camera to the encyclopedia
view by maximising silhouette intersection-over-union, renders the model from that camera and from the game's
views, and writes contact sheets and a JSON report. Models and renders derive from licensed art and stay in work/.
"""
# /// script
# requires-python = ">=3.11,<3.14"
# dependencies = ["numpy>=2", "opencv-python-headless>=4.10", "pillow>=11", "scipy>=1.14"]
# ///
from __future__ import annotations

import argparse
import ast
import json
import math
import os
import shutil
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw
from scipy.optimize import minimize

REPO = Path(__file__).resolve().parent.parent
RECIPE_DIR = REPO / "tools" / "tactical_model_recipes"
BLENDER_MAIN = RECIPE_DIR / "_blender_main.py"
IOU_TARGET = 0.85
SUPERSAMPLE = 2
FIT_TRIANGLES = 6000
HEADINGS = 64
ANGLED_PITCH = math.radians(50.0)
SPRITE_LENGTHS = (200, 50)
FRAME_MARGIN = 1.28
HERO_SIZE = (1200, 600)


@dataclass(frozen=True)
class Recipe:
    slug: str
    path: Path
    type_index: int
    name: str
    encyclopedia: str
    length: float
    view: tuple[tuple[float, float], tuple[float, float]]
    fit: str = "length"
    roll_seeds: tuple[float, ...] = (0.0,)

    @property
    def stem(self) -> str:
        return f"{self.type_index}_{self.slug}"


def recipe_constants(path: Path) -> dict[str, object]:
    constants: dict[str, object] = {}
    for node in ast.parse(path.read_text()).body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name):
            name = node.targets[0].id
            if name.isupper():
                try:
                    constants[name] = ast.literal_eval(node.value)
                except ValueError:
                    pass
    return constants


def load_recipes() -> list[Recipe]:
    """Class recipes are the modules in RECIPE_DIR that declare TYPE_INDEX and ENCYCLOPEDIA; the folder also
    holds the sprite renderer's procedural recipes (fighters, Death Star), which this tool leaves alone."""
    recipes = []
    for path in sorted(RECIPE_DIR.glob("[!_]*.py")):
        c = recipe_constants(path)
        if not {"TYPE_INDEX", "ENCYCLOPEDIA"} <= c.keys():
            continue
        view = c.get("VIEW", {})
        bounds = (tuple(view.get("yaw", (0, 355))), tuple(view.get("pitch", (-60, 75))))
        fit = str(c.get("FIT", "length"))
        if fit not in ("length", "size"):
            raise ValueError(f"{path}: FIT must be 'length' or 'size'")
        roll_seeds = tuple(float(roll) for roll in view.get("roll_seeds", (0.0,)))
        if not roll_seeds or not all(math.isfinite(roll) for roll in roll_seeds):
            raise ValueError(f"{path}: VIEW roll_seeds must contain finite angles in degrees")
        recipes.append(Recipe(path.stem, path, int(c["TYPE_INDEX"]), str(c["NAME"]), str(c["ENCYCLOPEDIA"]),
                              float(c["LENGTH"]), bounds, fit, roll_seeds))
    return recipes


def select_recipes(recipes: list[Recipe], wanted: list[str]) -> list[Recipe]:
    if not wanted:
        return recipes
    by_key = {r.slug: r for r in recipes} | {str(r.type_index): r for r in recipes}
    missing = [w for w in wanted if w not in by_key]
    if missing:
        raise SystemExit(f"unknown class {missing}; known: {sorted(r.slug for r in recipes)}")
    return [by_key[w] for w in wanted]


@dataclass(frozen=True)
class Camera:
    """Pinhole camera orbiting the model: yaw/pitch place it, roll turns it about its view axis, and the look-at
    point is offset in the image plane by `shift` model units."""
    yaw: float
    pitch: float
    roll: float
    distance: float
    focal_px: float
    shift: tuple[float, float]

    def basis(self) -> tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
        to_camera = np.array([math.cos(self.pitch) * math.cos(self.yaw), math.cos(self.pitch) * math.sin(self.yaw),
                              math.sin(self.pitch)])
        forward = -to_camera
        right0 = np.cross(forward, (0.0, 0.0, 1.0))
        right0 /= np.linalg.norm(right0)
        up0 = np.cross(right0, forward)
        c, s = math.cos(self.roll), math.sin(self.roll)
        right, up = c * right0 + s * up0, -s * right0 + c * up0
        position = self.shift[0] * right + self.shift[1] * up + self.distance * to_camera
        return right, up, forward, position

    def project(self, points: np.ndarray, width: int, height: int) -> tuple[np.ndarray, np.ndarray]:
        right, up, forward, position = self.basis()
        rel = points - position
        depth = rel @ forward
        x = width / 2 + self.focal_px * (rel @ right) / depth
        y = height / 2 - self.focal_px * (rel @ up) / depth
        return np.stack([x, y], axis=-1), depth

    def blender(self, width: int) -> dict[str, object]:
        right, up, forward, position = self.basis()
        matrix = np.eye(4)
        matrix[:3, 0], matrix[:3, 1], matrix[:3, 2], matrix[:3, 3] = right, up, -forward, position
        sensor = 36.0
        return {"type": "PERSP", "matrix": matrix.tolist(), "lens": self.focal_px * sensor / width, "sensor": sensor,
                "clip": [self.distance * 0.05, self.distance * 4.0]}


PARAM_STEPS = np.array([math.radians(8), math.radians(5), math.radians(3), 0.05, 0.08, 15.0, 15.0])


def camera_from_vector(v: np.ndarray, length: float) -> Camera:
    yaw, pitch, roll, log_scale, perspective, sx, sy = v
    distance = length / float(np.clip(perspective, 0.02, 3.0))
    return Camera(float(yaw), float(np.clip(pitch, -1.45, 1.45)), float(roll), distance,
                  math.exp(log_scale) * distance, (float(sx), float(sy)))


def coverage(tris: np.ndarray, camera: Camera, width: int, height: int) -> np.ndarray:
    pts, depth = fitted_scaled(camera, SUPERSAMPLE).project(tris.reshape(-1, 3), width * SUPERSAMPLE,
                                                           height * SUPERSAMPLE)
    pts, depth = pts.reshape(-1, 3, 2), depth.reshape(-1, 3)
    visible = (depth > camera.distance * 0.01).all(axis=1)
    fixed = np.round(pts[visible] * 16).astype(np.int32)
    canvas = np.zeros((height * SUPERSAMPLE, width * SUPERSAMPLE), np.uint8)
    for tri in fixed:
        cv2.fillConvexPoly(canvas, tri, 1, lineType=cv2.LINE_8, shift=4)
    return canvas.reshape(height, SUPERSAMPLE, width, SUPERSAMPLE).mean(axis=(1, 3))


def soft_iou(cover: np.ndarray, target: np.ndarray) -> float:
    return float(np.minimum(cover, target).sum() / max(np.maximum(cover, target).sum(), 1e-9))


def binary_iou(a: np.ndarray, b: np.ndarray) -> float:
    return float((a & b).sum() / max((a | b).sum(), 1))


def initial_vector(tris: np.ndarray, target: np.ndarray, length: float, yaw: float, pitch: float,
                   roll: float = 0.0) -> np.ndarray:
    height, width = target.shape
    initial_scale = min(0.1, min(width, height) * 0.5 / length)
    probe = np.array([yaw, pitch, roll, math.log(initial_scale), 0.4, 0.0, 0.0])
    cover = coverage(tris, camera_from_vector(probe, length), width, height)
    if cover.sum() < 1:
        return probe
    probe[3] += 0.5 * math.log(target.sum() / cover.sum())
    cover = coverage(tris, camera_from_vector(probe, length), width, height)
    ys, xs = np.indices(target.shape)
    dx = (xs * target).sum() / target.sum() - (xs * cover).sum() / max(cover.sum(), 1e-9)
    dy = (ys * target).sum() / target.sum() - (ys * cover).sum() / max(cover.sum(), 1e-9)
    pixels_per_unit = math.exp(probe[3])
    probe[5], probe[6] = -dx / pixels_per_unit, dy / pixels_per_unit
    return probe


@dataclass(frozen=True)
class Fit:
    camera: Camera
    vector: np.ndarray
    iou: float
    evaluations: int


def nelder_mead(loss, v0: np.ndarray, steps: np.ndarray, max_evaluations: int) -> tuple[np.ndarray, float]:
    simplex = np.vstack([np.zeros(len(v0)), np.eye(len(v0))])
    result = minimize(lambda z: loss(v0 + z * steps), np.zeros(len(v0)), method="Nelder-Mead",
                      options={"initial_simplex": simplex, "maxfev": max_evaluations, "xatol": 1e-3, "fatol": 1e-5})
    return v0 + result.x * steps, float(result.fun)


def out_of_view(v: np.ndarray, view: tuple[tuple[float, float], tuple[float, float]]) -> float:
    """Degrees outside the VIEW ranges; yaw is measured around the circle, so a range may cross 0."""
    (yaw_low, yaw_high), (pitch_low, pitch_high) = view
    past_low = (math.degrees(v[0]) - yaw_low) % 360
    span = yaw_high - yaw_low
    yaw_gap = 0.0 if past_low <= span else min(past_low - span, 360 - past_low)
    pitch = math.degrees(v[1])
    pitch_gap = max(0.0, pitch_low - pitch, pitch - pitch_high)
    return yaw_gap + pitch_gap


def fit_camera(tris: np.ndarray, target: np.ndarray, length: float,
               view: tuple[tuple[float, float], tuple[float, float]],
               roll_seeds: tuple[float, ...] = (0.0,)) -> Fit:
    """Coarse yaw/pitch grid with scale and offset matched to the mask, then Nelder-Mead from the ten best starts
    and a restart from the winner (Nelder-Mead collapses early on piecewise-flat IoU). A silhouette alone cannot
    tell a view from its mirror (from behind and below), so the recipe's VIEW ranges, in degrees, bound the
    search; perspective is bounded so the camera stays at least one model length away. Optional roll seeds
    let asymmetric ships start in different image orientations before scale and centroid matching."""
    height, width = target.shape
    target_f = target.astype(np.float64)
    evaluations = 0

    def loss(v: np.ndarray) -> float:
        nonlocal evaluations
        evaluations += 1
        penalty = 0.02 * out_of_view(v, view) + max(0.0, v[4] - 1.0)
        return 1.0 - soft_iou(coverage(tris, camera_from_vector(v, length), width, height), target_f) + penalty

    (yaw_low, yaw_high), (pitch_low, pitch_high) = view
    starts = []
    for roll in np.radians(roll_seeds):
        for yaw in np.radians(np.arange(yaw_low, yaw_high + 1e-6, 5)):
            for pitch in np.radians(np.arange(pitch_low, pitch_high + 1e-6, 5)):
                v = initial_vector(tris, target_f, length, yaw, pitch, roll)
                starts.append((loss(v), v))
    starts.sort(key=lambda s: s[0])
    best_loss, best_v = starts[0]
    for _, v0 in starts[:10]:
        v, value = nelder_mead(loss, v0, PARAM_STEPS, 900)
        if value < best_loss:
            best_loss, best_v = value, v
    for _ in range(2):
        best_v, best_loss = nelder_mead(loss, best_v, PARAM_STEPS * 0.5, 600)
    return Fit(camera_from_vector(best_v, length), best_v, 1.0 - best_loss, evaluations)


def proportion_hint(tris: np.ndarray, target: np.ndarray, length: float, fit: Fit) -> dict[str, float]:
    """Refit with the model's length and height free relative to its beam. Large factors mean the recipe's
    proportions, not the camera, limit the match; it is advice for the recipe author, never applied."""
    height, width = target.shape
    target_f = target.astype(np.float64)

    def loss(v: np.ndarray) -> float:
        stretched = tris * np.array([math.exp(v[7]), 1.0, math.exp(v[8])])
        return 1.0 - soft_iou(coverage(stretched, camera_from_vector(v[:7], length), width, height), target_f)

    v0 = np.concatenate([fit.vector, [0.0, 0.0]])
    steps = np.concatenate([PARAM_STEPS * 0.5, [0.03, 0.03]])
    v, value = nelder_mead(loss, v0, steps, 1500)
    v, value = nelder_mead(loss, v, steps * 0.5, 800)
    return {"length_factor": round(math.exp(v[7]), 3), "height_factor": round(math.exp(v[8]), 3),
            "iou": round(1.0 - value, 4)}


def load_mask(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("L")) > 127


def srgb_hex(rgb: np.ndarray) -> str:
    return "#" + "".join(f"{int(round(c)):02x}" for c in np.clip(rgb, 0, 255))


LUMA = np.array([0.2126, 0.7152, 0.0722])
TINTS = 7


def sample_palette(source: Path, mask: Path) -> dict[str, object]:
    """Hull colours from the lit encyclopedia render, as sRGB: luminance bands of the hull, its most saturated
    tenth as an accent, and `tints`, k-means clusters sorted dark to light (the darkest are mostly shadow; the
    middle ones carry hue variation such as Mon Calamari mottling)."""
    rgb = np.asarray(Image.open(source).convert("RGB")).astype(np.float64)
    inner = cv2.erode(load_mask(mask).astype(np.uint8), np.ones((5, 5), np.uint8)).astype(bool)
    pixels = rgb[inner]
    luma = pixels @ LUMA
    lo, hi = np.percentile(luma, (25, 75))
    saturation = pixels.max(axis=1) - pixels.min(axis=1)
    cv2.setRNGSeed(0)
    criteria = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_MAX_ITER, 50, 0.5)
    _, _, centres = cv2.kmeans(pixels.astype(np.float32), TINTS, None, criteria, 4, cv2.KMEANS_PP_CENTERS)
    return {
        "light": srgb_hex(np.median(pixels[luma >= hi], axis=0)),
        "mid": srgb_hex(np.median(pixels[(luma >= lo) & (luma <= hi)], axis=0)),
        "dark": srgb_hex(np.median(pixels[luma <= lo], axis=0)),
        "accent": srgb_hex(np.median(pixels[saturation >= np.percentile(saturation, 90)], axis=0)),
        "tints": [srgb_hex(c) for c in sorted(centres.astype(np.float64), key=lambda c: float(c @ LUMA))],
    }


def find_blender(explicit: Path | None) -> Path:
    candidates = [explicit, os.environ.get("BLENDER"), shutil.which("blender"),
                  "/Applications/Blender.app/Contents/MacOS/Blender"]
    for candidate in candidates:
        if candidate and Path(candidate).exists():
            return Path(candidate)
    raise SystemExit("Blender not found; pass --blender or set BLENDER")


def run_blender(blender: Path, job: dict[str, object], job_path: Path) -> float:
    job_path.write_text(json.dumps(job, indent=1))
    log_path = job_path.with_suffix(".log")
    started = time.monotonic()
    result = subprocess.run([str(blender), "--background", "--factory-startup", "--python-exit-code", "1",
                             "--python", str(BLENDER_MAIN), "--", str(job_path)],
                            capture_output=True, text=True, check=False)
    log_path.write_text(result.stdout + result.stderr)
    if result.returncode != 0 or "TACTICAL_MODEL_JOB_DONE" not in result.stdout:
        tail = "\n".join((result.stdout + result.stderr).splitlines()[-25:])
        raise SystemExit(f"Blender job {job_path.name} failed (exit {result.returncode}); log {log_path}\n{tail}")
    return time.monotonic() - started


def ortho_camera(elevation: float, extent: float) -> dict[str, object]:
    """Orthographic camera looking at the origin from the -Y side (screen right is +X) at `elevation` above the
    plane; at pi/2 it looks straight down with +X to the right and +Y up, as tools/tactical_sprites_bpy.py does."""
    c, s = math.cos(elevation), math.sin(elevation)
    to_camera = np.array([0.0, -c, s])
    forward = -to_camera
    right = np.array([1.0, 0.0, 0.0])
    up = np.cross(right, forward)
    matrix = np.eye(4)
    matrix[:3, 0], matrix[:3, 1], matrix[:3, 2], matrix[:3, 3] = right, up, -forward, to_camera * extent * 4
    return {"type": "ORTHO", "matrix": matrix.tolist(), "ortho_scale": extent, "clip": [extent * 0.5, extent * 8]}


def hero_camera(length: float, yaw_deg: float, fit: str = "length") -> Camera:
    focal_px = HERO_SIZE[1] * 1.1 if fit == "size" else HERO_SIZE[0] * 1.55
    return Camera(math.radians(yaw_deg), math.radians(24), 0.0, length * 2.5, focal_px, (0.0, 0.0))


def render_jobs(recipe: Recipe, fitted: Camera, review: Path, samples: int,
                headings: bool) -> list[dict[str, object]]:
    width, height = 400, 200
    sprite = 256
    extent = recipe.length * FRAME_MARGIN
    jobs: list[dict[str, object]] = [
        {"path": str(review / "fit_view.png"), "width": width * SUPERSAMPLE, "height": height * SUPERSAMPLE,
         "camera": fitted_scaled(fitted, SUPERSAMPLE).blender(width * SUPERSAMPLE), "samples": samples},
        {"path": str(review / "hero_front.png"), "width": HERO_SIZE[0], "height": HERO_SIZE[1],
         "camera": hero_camera(recipe.length, 35, recipe.fit).blender(HERO_SIZE[0]), "samples": samples * 2},
        {"path": str(review / "hero_rear.png"), "width": HERO_SIZE[0], "height": HERO_SIZE[1],
         "camera": hero_camera(recipe.length, 215, recipe.fit).blender(HERO_SIZE[0]), "samples": samples * 2},
        {"path": str(review / "top.png"), "width": sprite, "height": sprite,
         "camera": ortho_camera(math.pi / 2, extent), "samples": samples},
    ]
    if headings:
        jobs.append({"path": str(review / "angled" / "h{heading:02d}.png"), "width": sprite, "height": sprite,
                     "camera": ortho_camera(ANGLED_PITCH, extent), "samples": samples, "headings": HEADINGS})
    return jobs


def fitted_scaled(camera: Camera, factor: int) -> Camera:
    return Camera(camera.yaw, camera.pitch, camera.roll, camera.distance, camera.focal_px * factor, camera.shift)


def overlay(model: np.ndarray, target: np.ndarray) -> Image.Image:
    out = np.zeros(model.shape + (3,), np.uint8)
    out[model & target] = (200, 200, 200)
    out[model & ~target] = (235, 60, 60)
    out[~model & target] = (60, 170, 255)
    return Image.fromarray(out)


def edges_over(reference: Image.Image, model: Image.Image) -> Image.Image:
    grey = cv2.cvtColor(np.asarray(model), cv2.COLOR_RGB2GRAY)
    edges = cv2.Canny(grey, 40, 110) > 0
    out = (np.asarray(reference).astype(np.float64) * 0.75).astype(np.uint8)
    out[edges] = (255, 220, 40)
    return Image.fromarray(out)


def label(image: Image.Image, text: str) -> Image.Image:
    canvas = Image.new("RGB", (image.width, image.height + 18), (16, 16, 24))
    canvas.paste(image.convert("RGB"), (0, 18))
    ImageDraw.Draw(canvas).text((4, 3), text, fill=(255, 230, 120))
    return canvas


def on_black(path: Path, size: tuple[int, int] | None = None) -> Image.Image:
    image = Image.open(path).convert("RGBA")
    if size:
        image = image.resize(size, Image.LANCZOS)
    background = Image.new("RGBA", image.size, (8, 8, 14, 255))
    return Image.alpha_composite(background, image).convert("RGB")


def grid(images: list[Image.Image], columns: int, pad: int = 4) -> Image.Image:
    w, h = images[0].size
    rows = math.ceil(len(images) / columns)
    sheet = Image.new("RGB", (columns * (w + pad) + pad, rows * (h + pad) + pad), (16, 16, 24))
    for i, image in enumerate(images):
        sheet.paste(image, (pad + (i % columns) * (w + pad), pad + (i // columns) * (h + pad)))
    return sheet


def stack(images: list[Image.Image], pad: int = 6) -> Image.Image:
    sheet = Image.new("RGB", (max(i.width for i in images) + 2 * pad, sum(i.height + pad for i in images) + pad),
                      (16, 16, 24))
    y = pad
    for image in images:
        sheet.paste(image, (pad, y))
        y += image.height + pad
    return sheet


def row(images: list[Image.Image], pad: int = 6) -> Image.Image:
    sheet = Image.new("RGB", (sum(i.width + pad for i in images) + pad, max(i.height for i in images) + 2 * pad),
                      (16, 16, 24))
    x = pad
    for image in images:
        sheet.paste(image, (x, pad))
        x += image.width + pad
    return sheet


def silhouette(path: Path, width: int, height: int) -> np.ndarray:
    alpha = np.asarray(Image.open(path).convert("RGBA"))[..., 3].astype(np.float64) / 255
    return alpha.reshape(height, SUPERSAMPLE, width, SUPERSAMPLE).mean(axis=(1, 3)) >= 0.5


def contact_sheets(recipe: Recipe, review: Path, source: Path, target: np.ndarray, blender_iou: float,
                   fit_iou: float, original: dict[str, object] | None, headings: bool) -> None:
    zoom = (800, 400)
    src = Image.open(source).convert("RGB").resize(zoom, Image.LANCZOS)
    model_view = on_black(review / "fit_view.png")
    model_mask = silhouette(review / "fit_view.png", 400, 200)
    over = overlay(model_mask, target).resize(zoom, Image.NEAREST)
    panels = [label(src, f"{recipe.encyclopedia} encyclopedia render"),
              label(model_view, f"{recipe.name}: model at fitted camera"),
              label(over, f"silhouette IoU {blender_iou:.3f} (fit {fit_iou:.3f}); "
                          "red model only, blue reference only"),
              label(edges_over(src, model_view), "model edges over the reference")]
    if original:
        original_over = Image.open(review / "original_overlay.png").resize(zoom, Image.NEAREST)
        panels.append(label(original_over, f"original {original['triangles']}-triangle mesh, same fit: "
                                           f"IoU {original['iou']:.3f}"))
        plan = Image.open(review / "plan_overlay.png")
        plan_panel = Image.new("RGB", zoom, (0, 0, 0))
        plan_panel.paste(plan, ((zoom[0] - plan.width) // 2, (zoom[1] - plan.height) // 2))
        panels.append(label(plan_panel, f"plan view: IoU {original['plan_iou_with_remodel']:.3f}; "
                                        "red remodel only, blue original only"))
    fit = grid(panels, 2)
    fit.save(review / "fit_sheet.png")
    row([on_black(review / "hero_front.png"), on_black(review / "hero_rear.png")]).save(review / "heroes.png")
    if not headings:
        return
    sizes = []
    for length_px in SPRITE_LENGTHS:
        side = round(length_px * FRAME_MARGIN)
        top = on_black(review / "top.png", (side, side))
        frames = [on_black(review / "angled" / f"h{h:02d}.png", (side, side)) for h in range(HEADINGS)]
        sizes.append(label(row([top, grid(frames[:16], 16, 2)]),
                           f"{length_px} px long: top-down and headings 0-15 at {math.degrees(ANGLED_PITCH):.0f} deg"))
    heroes = row([label(on_black(review / "hero_front.png"), f"{recipe.name}: front three-quarter"),
                  label(on_black(review / "hero_rear.png"), "rear three-quarter")])
    stack([heroes] + sizes).save(review / "preview_sheet.png")
    strip_side = round(SPRITE_LENGTHS[0] * FRAME_MARGIN)
    grid([on_black(review / "angled" / f"h{h:02d}.png", (strip_side, strip_side)) for h in range(HEADINGS)], 8, 2) \
        .save(review / "headings_200px.png")
    small_side = round(SPRITE_LENGTHS[1] * FRAME_MARGIN)
    grid([on_black(review / "angled" / f"h{h:02d}.png", (small_side, small_side)) for h in range(HEADINGS)], 16, 2) \
        .save(review / "headings_50px.png")


def original_mesh_triangles(extract_dir: Path, recipe: Recipe) -> np.ndarray | None:
    """The class's original TACTICAL.DLL mesh as extracted by tools/build_tactical_sprites.py, centred and scaled
    to the class length like the remodel, or None when that cache is absent."""
    found = sorted(extract_dir.glob(f"{recipe.type_index:02d}_*/mesh.json"))
    if not found:
        return None
    mesh = json.loads(found[0].read_text())
    vertices = np.array(mesh["vertices"], np.float64)
    vertices -= (vertices.min(axis=0) + vertices.max(axis=0)) / 2
    extent = np.ptp(vertices, axis=0)
    vertices *= recipe.length / (extent[0] if recipe.fit == "length" else extent.max())
    return vertices[np.array(mesh["faces"])]


PLAN_PX = 400


def plan_silhouette(tris: np.ndarray, length: float) -> np.ndarray:
    """Top-down silhouette, bow to the right and port up, the model's length spanning PLAN_PX pixels."""
    scale = PLAN_PX / length
    canvas = np.zeros((PLAN_PX + 40, PLAN_PX + 40), np.uint8)
    pts = np.stack([tris[..., 0] * scale + canvas.shape[1] / 2, canvas.shape[0] / 2 - tris[..., 1] * scale], -1)
    for tri in np.round(pts * 16).astype(np.int32):
        cv2.fillConvexPoly(canvas, tri, 1, lineType=cv2.LINE_8, shift=4)
    return canvas.astype(bool)


def baseline(extract_dir: Path, recipe: Recipe, tris: np.ndarray, target: np.ndarray,
             review: Path) -> dict[str, object] | None:
    """Compare with the original mesh: its encyclopedia IoU under the same search (a like-for-like bar for the
    remodel's), its proportions, and its plan-view overlap with the remodel. The encyclopedia never shows the plan,
    the view the game uses most, so the original mesh is the only reference for it."""
    original = original_mesh_triangles(extract_dir, recipe)
    if original is None:
        return None
    fit = fit_camera(original, target, recipe.length, recipe.view, recipe.roll_seeds)
    cover = coverage(original, fit.camera, *target.shape[::-1]) >= 0.5
    overlay(cover, target).save(review / "original_overlay.png")
    remodel_plan, original_plan = plan_silhouette(tris, recipe.length), plan_silhouette(original, recipe.length)
    overlay(remodel_plan, original_plan).save(review / "plan_overlay.png")
    extent = np.ptp(original.reshape(-1, 3), axis=0)
    return {"iou": round(binary_iou(cover, target), 4), "triangles": len(original),
            "plan_iou_with_remodel": round(binary_iou(remodel_plan, original_plan), 4),
            "dimensions": [round(float(e), 1) for e in extent]}


def lit_colours(source: Path, target: np.ndarray, render: Path) -> tuple[np.ndarray, np.ndarray]:
    """Mean sRGB colour of the lit half (brighter than its own median) of the reference and of the fitted-view
    render, over the pixels both cover. The lit side approximates hull colour under a white key light; shadows
    carry each renderer's ambient tint (the 1998 one is blue-violet), which must not leak into the palette."""
    reference = np.asarray(Image.open(source).convert("RGB")).astype(np.float64)
    model = np.asarray(Image.open(render).convert("RGBA").resize(target.shape[::-1], Image.LANCZOS))
    model = model.astype(np.float64)
    both = target & (model[..., 3] > 250)

    def lit_mean(pixels: np.ndarray) -> np.ndarray:
        luma = pixels @ LUMA
        return pixels[luma >= np.median(luma)].mean(axis=0)
    return lit_mean(reference[both]), lit_mean(model[..., :3][both])


def build_model(blender: Path, recipe: Recipe, palette: dict[str, object], blend: Path, review: Path) -> float:
    return run_blender(blender, {"recipe": str(recipe.path), "palette": palette, "length": recipe.length,
                                 "fit": recipe.fit,
                                 "blend": str(blend), "tris": str(review / "fit_tris.npz"),
                                 "max_tris": FIT_TRIANGLES}, review / "build_job.json")


def build_class(recipe: Recipe, args: argparse.Namespace, blender: Path) -> dict[str, object]:
    """Build, fit the camera, then calibrate colour: our lights differ from the 1998 renderer's, so the sampled
    palette is scaled per channel until the fitted view's lit colour matches the reference's, and the model is
    rebuilt with that gain before the final renders."""
    encyclopedia = args.encyclopedia_dir / recipe.encyclopedia
    source, mask_path = encyclopedia / "source.png", encyclopedia / "mask.png"
    if not (source.exists() and mask_path.exists()):
        raise SystemExit(f"{encyclopedia} lacks source.png/mask.png; run tools/build_encyclopedia_icons.py first")
    review = args.out_dir / "review" / recipe.stem
    (review / "angled").mkdir(parents=True, exist_ok=True)
    blend = args.out_dir / f"{recipe.stem}.blend"
    palette = sample_palette(source, mask_path) | {"gain": [1.0, 1.0, 1.0]}

    build_seconds = build_model(blender, recipe, palette, blend, review)
    with np.load(review / "fit_tris.npz") as data:
        tris, stats = data["tris"].astype(np.float64), json.loads(str(data["stats"]))
    target = load_mask(mask_path)
    started = time.monotonic()
    fit = fit_camera(tris, target, recipe.length, recipe.view, recipe.roll_seeds)
    hint = proportion_hint(tris, target, recipe.length, fit)
    original = baseline(args.original_dir, recipe, tris, target, review)
    fit_seconds = time.monotonic() - started
    camera, fit_iou = fit.camera, fit.iou

    renders = render_jobs(recipe, camera, review, args.samples, not args.quick)
    render_seconds = run_blender(blender, {"blend": str(blend), "renders": renders[:1]}, review / "render_job.json")
    reference_mean, uncalibrated_mean = lit_colours(source, target, review / "fit_view.png")
    palette["gain"] = [round(float(g), 3) for g in np.clip(reference_mean / uncalibrated_mean, 0.5, 3.0)]
    build_seconds += build_model(blender, recipe, palette, blend, review)
    render_seconds += run_blender(blender, {"blend": str(blend), "renders": renders}, review / "render_job.json")
    _, calibrated_mean = lit_colours(source, target, review / "fit_view.png")

    blender_iou = binary_iou(silhouette(review / "fit_view.png", 400, 200), target)
    contact_sheets(recipe, review, source, target, blender_iou, fit_iou, original, not args.quick)
    report = {
        "class": recipe.name, "type_index": recipe.type_index, "blend": str(blend), "encyclopedia": recipe.encyclopedia,
        "fit": recipe.fit, "size_m": recipe.length,
        "iou": round(blender_iou, 4), "fit_soft_iou": round(fit_iou, 4), "iou_target": IOU_TARGET,
        "passes": blender_iou >= IOU_TARGET, "palette": palette,
        "lit_colour": {"reference": srgb_hex(reference_mean), "uncalibrated": srgb_hex(uncalibrated_mean),
                       "calibrated": srgb_hex(calibrated_mean)},
        "mesh": stats,
        "camera": {"yaw_deg": round(math.degrees(camera.yaw), 2), "pitch_deg": round(math.degrees(camera.pitch), 2),
                   "roll_deg": round(math.degrees(camera.roll), 2), "distance": round(camera.distance, 1),
                   "focal_px_at_400": round(camera.focal_px, 1),
                   "fov_deg": round(math.degrees(2 * math.atan(200 / camera.focal_px)), 2),
                   "shift": [round(s, 1) for s in camera.shift]},
        "fit_evaluations": fit.evaluations, "proportion_hint": hint, "original_mesh": original,
        "seconds": {"build": round(build_seconds, 1), "fit": round(fit_seconds, 1), "render": round(render_seconds, 1)},
    }
    (review / "report.json").write_text(json.dumps(report, indent=1))
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("classes", nargs="*", help="recipe slugs or tactical type indices (default: all)")
    parser.add_argument("--encyclopedia-dir", type=Path, default=REPO / "work/hd/encyclopedia",
                        help="extracted encyclopedia renders (tools/build_encyclopedia_icons.py)")
    parser.add_argument("--out-dir", type=Path, default=REPO / "work/hd/tactical/models")
    parser.add_argument("--original-dir", type=Path, default=REPO / "work/hd/tactical/extract",
                        help="original meshes extracted by tools/build_tactical_sprites.py, for the baseline IoU")
    parser.add_argument("--blender", type=Path, help="Blender executable (default: $BLENDER, PATH, /Applications)")
    parser.add_argument("--samples", type=int, default=32, help="EEVEE samples per preview frame")
    parser.add_argument("--quick", action="store_true", help="skip the heading frames and preview sheets")
    parser.add_argument("--list", action="store_true", help="list the recipes and exit")
    args = parser.parse_args()
    recipes = select_recipes(load_recipes(), args.classes)
    if args.list:
        for r in recipes:
            print(f"{r.type_index:3d}  {r.slug:32s} {r.encyclopedia}  length {r.length:g}  {r.name}")
        return
    args.out_dir = args.out_dir.resolve()
    blender = find_blender(args.blender)
    failures = []
    for recipe in recipes:
        report = build_class(recipe, args, blender)
        verdict = "ok" if report["passes"] else f"below {IOU_TARGET}"
        print(f"{recipe.stem}: IoU {report['iou']:.3f} ({verdict}), camera {report['camera']}, "
              f"seconds {report['seconds']}")
        print(f"  proportion hint (length, height relative to beam): {report['proportion_hint']}")
        print(f"  lit colour {report['lit_colour']}, gain {report['palette']['gain']}")
        print(f"  original mesh: {report['original_mesh'] or 'not extracted (run tools/build_tactical_sprites.py)'}")
        print(f"  review: {args.out_dir / 'review' / recipe.stem}")
        if not report["passes"]:
            failures.append(recipe.stem)
    if failures:
        raise SystemExit(f"silhouette IoU below {IOU_TARGET}: {failures}")


if __name__ == "__main__":
    main()
