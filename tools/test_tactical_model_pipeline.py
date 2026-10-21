#!/usr/bin/env -S uv run
"""Check remodel selection and sizing without licensed game assets."""
# /// script
# requires-python = ">=3.11,<3.14"
# dependencies = ["pefile>=2023.2.7", "pillow>=11", "numpy>=2"]
# ///
from __future__ import annotations

import argparse
import math
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def check_selection() -> None:
    from build_tactical_sprites import ClassSpec, FIGHTER, STATION, procedural_class, sha256

    with tempfile.TemporaryDirectory() as directory:
        models = Path(directory)
        for index, slug, kind, size, module in (
            (29, "xwing", FIGHTER, 100.0, "fighter_proxies.py"),
            (37, "death_star", STATION, 9600.0, "death_star.py"),
        ):
            def spec(folder: Path | None) -> ClassSpec:
                return procedural_class(index, slug, slug, kind, size, module, {module: "fallback"}, folder)

            fallback = spec(models)
            assert fallback.source["kind"] == "recipe"
            assert fallback.hashes == {"recipe": "fallback"}
            model = models / f"{index}_{slug}.blend"
            model.write_bytes(b"synthetic model fingerprint")
            override = spec(models)
            assert override.source == {"kind": "blend", "path": str(model.resolve())}
            assert override.hashes == {"blend": sha256(model.read_bytes())}
            assert override.fit == "size" and override.size_m == size
            assert spec(None).source["kind"] == "recipe"
            duplicate = models / f"{index}_duplicate.blend"
            duplicate.touch()
            try:
                spec(models)
            except SystemExit:
                pass
            else:
                raise AssertionError("ambiguous overrides must be rejected")
            duplicate.unlink()


def check_blender_sizing() -> None:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import bpy
    from tactical_model_recipes import _kit as kit
    from tactical_sprites_bpy import normalise

    for fit, expected in (("length", (60.0, 120.0, 180.0)), ("size", (20.0, 40.0, 60.0))):
        kit.reset_scene()
        bpy.ops.mesh.primitive_cube_add(size=2, location=(3, 4, 5))
        cube = bpy.context.object
        cube.scale = (1, 2, 3)
        bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
        ship = kit.finalize([cube], 60) if fit == "length" else kit.finalize([cube], 60, fit)
        factor, extent, _ = normalise(ship, 60, fit)
        assert math.isclose(factor, 1.0, rel_tol=1e-6), (fit, factor)
        assert all(math.isclose(a, b, rel_tol=1e-6) for a, b in zip(extent, expected)), (fit, extent)
        for axis in range(3):
            coords = [vertex.co[axis] for vertex in ship.data.vertices]
            assert abs(min(coords) + max(coords)) < 1e-5
    print("Blender model and sprite sizing passed")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blender", default=shutil.which("blender") or "blender")
    parser.add_argument("--inside-blender", action="store_true")
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    args = parser.parse_args(argv)
    if args.inside_blender:
        check_blender_sizing()
        return
    check_selection()
    subprocess.run([args.blender, "--background", "--factory-startup", "--python-exit-code", "1",
                    "--python", str(Path(__file__).resolve()), "--", "--inside-blender"], check=True)
    print("Remodel selection and sizing passed")


if __name__ == "__main__":
    main()
