#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# dependencies = ["pillow", "numpy"]
# ///
"""Per-screenshot pixel comparison of two runs (real-Win32 standalone vs minwin_impl): differing pixel count, clusters of differing
pixels (bounding boxes) and a diff image per screenshot.

Both runs are 8-bit Xvfb screenshots (REB_XVFB_DEPTH=8). Wine's 8-bit display and SDL-on-Wine map a palette entry to different X pixels
(an entry that is not one of the 20 static colours is matched differently), so plain RGB differences contain colour-conversion noise.
When the impl run dumped its frame (`REB_MINWIN_DUMP=Z:/out` in the environment of tools/run_game.sh: `<shot>.idx` = 640x480 palette
indices + 256 RGBA entries) the tool also checks palette-index equivalence: every impl index must map to one reference colour (the most
common one); reference pixels that deviate from that mapping are the "index px" (real differences in what was drawn).

  tools/pixel_compare.py work/run/ref_smoke work/run/impl_smoke --out work/run/cmp_smoke
"""
from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from PIL import Image

WIDTH, HEIGHT = 640, 480
CELL = 8


def load_rgb(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.uint8)


def load_index_dump(path: Path) -> np.ndarray | None:
    if not path.exists():
        return None
    raw = path.read_bytes()
    if len(raw) < WIDTH * HEIGHT:
        return None
    return np.frombuffer(raw[: WIDTH * HEIGHT], dtype=np.uint8).reshape(HEIGHT, WIDTH)


def packed(image: np.ndarray) -> np.ndarray:
    return (image[..., 0].astype(np.uint32) << 16) | (image[..., 1].astype(np.uint32) << 8) | image[..., 2]


def index_deviations(reference: np.ndarray, frame: np.ndarray) -> np.ndarray:
    """Mask of reference pixels whose colour is not the majority colour the reference shows for that impl palette index."""
    colours = packed(reference)
    mask = np.zeros(frame.shape, dtype=bool)
    for index in np.unique(frame):
        where = frame == index
        values, counts = np.unique(colours[where], return_counts=True)
        mask |= where & (colours != values[np.argmax(counts)])
    return mask


def clusters(mask: np.ndarray) -> list[tuple[int, int, int, int, int]]:
    """Connected groups (8-neighbourhood on an 8x8 cell grid) of differing pixels: (x0, y0, x1, y1, pixels), biggest first."""
    rows, cols = (HEIGHT + CELL - 1) // CELL, (WIDTH + CELL - 1) // CELL
    padded = np.zeros((rows * CELL, cols * CELL), dtype=bool)
    padded[: mask.shape[0], : mask.shape[1]] = mask
    cells = padded.reshape(rows, CELL, cols, CELL).any(axis=(1, 3))
    seen = np.zeros_like(cells)
    found = []
    for r in range(rows):
        for c in range(cols):
            if not cells[r, c] or seen[r, c]:
                continue
            stack, members = [(r, c)], []
            seen[r, c] = True
            while stack:
                cr, cc = stack.pop()
                members.append((cr, cc))
                for dr in (-1, 0, 1):
                    for dc in (-1, 0, 1):
                        nr, nc = cr + dr, cc + dc
                        if 0 <= nr < rows and 0 <= nc < cols and cells[nr, nc] and not seen[nr, nc]:
                            seen[nr, nc] = True
                            stack.append((nr, nc))
            r0, r1 = min(m[0] for m in members), max(m[0] for m in members)
            c0, c1 = min(m[1] for m in members), max(m[1] for m in members)
            region = mask[r0 * CELL : (r1 + 1) * CELL, c0 * CELL : (c1 + 1) * CELL]
            ys, xs = np.nonzero(region)
            found.append((c0 * CELL + int(xs.min()), r0 * CELL + int(ys.min()), c0 * CELL + int(xs.max()) + 1, r0 * CELL + int(ys.max()) + 1, int(region.sum())))
    return sorted(found, key=lambda f: -f[4])


def write_diff_image(reference: np.ndarray, impl: np.ndarray, mask: np.ndarray, path: Path) -> None:
    dimmed = (impl.astype(np.uint16) // 3).astype(np.uint8)
    dimmed[mask] = (255, 0, 0)
    side = np.concatenate([reference, impl, dimmed], axis=1)
    Image.fromarray(side).save(path)


def describe(boxes: list[tuple[int, int, int, int, int]], limit: int = 4) -> str:
    parts = [f"({x0},{y0})-({x1},{y1}) {n}px" for x0, y0, x1, y1, n in boxes[:limit]]
    if len(boxes) > limit:
        parts.append(f"+{len(boxes) - limit} more")
    return "; ".join(parts)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("reference", type=Path)
    ap.add_argument("impl", type=Path)
    ap.add_argument("--out", type=Path, help="directory for the diff images (reference | impl | diff side by side)")
    args = ap.parse_args()
    if args.out:
        args.out.mkdir(parents=True, exist_ok=True)
    print("| shot | rgb px | index px | regions (of the index diff when available; bounding box, px) |\n|---|---|---|---|")
    identical = total = 0
    for shot in sorted(args.reference.glob("*.png")):
        other = args.impl / shot.name
        if not other.exists():
            print(f"| {shot.stem} | missing | | |")
            continue
        total += 1
        reference, impl = load_rgb(shot), load_rgb(other)
        mask = (reference != impl).any(axis=2)
        count = int(mask.sum())
        identical += count == 0
        frame = load_index_dump(args.impl / (shot.stem + ".idx"))
        index_mask = index_deviations(reference, frame) if frame is not None else None
        index_text = "-" if index_mask is None else str(int(index_mask.sum()))
        shown = mask if index_mask is None else index_mask
        print(f"| {shot.stem} | {count} | {index_text} | {describe(clusters(shown)) if shown.any() else ''} |")
        if args.out and index_mask is not None:
            mask = index_mask
        if args.out and mask.any():
            write_diff_image(reference, impl, mask, args.out / (shot.stem + "_diff.png"))
    print(f"\n{identical} of {total} identical (rgb)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
