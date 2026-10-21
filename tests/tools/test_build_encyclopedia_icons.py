from __future__ import annotations

import sys
import tempfile
import unittest
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from tools.build_encyclopedia_icons import (
    apply_mask_recipe,
    brighten_dark_facility,
    cached_mask,
    clean_mask,
    facility_icon,
    facility_targets,
    fit_icon,
    forces_tab_icon,
    forces_tab_targets,
    portrait_crop,
    quantize_icon,
    refine_planet_mask,
)


def palette() -> list[int]:
    values = [0, 0, 0] * 256
    values[2 * 3:2 * 3 + 3] = [240, 30, 20]
    values[3 * 3:3 * 3 + 3] = [240, 30, 20]
    values[6 * 3:6 * 3 + 3] = [20, 30, 240]
    return values


class BuildEncyclopediaIconsTests(unittest.TestCase):
    def test_forces_tabs_cover_both_factions_and_leave_unknown_tabs_empty(self) -> None:
        targets = forces_tab_targets()
        self.assertEqual(len(targets), 46)
        for base, number in ((0x294a, 94), (0x294d, 116)):
            for state in range(3):
                self.assertEqual(targets[base + state], ("defense", state, number))
        for base, number in ((0x28ac, 94), (0x28de, 116)):
            for offset, state in ((0, 0), (7, 1), (3, 2)):
                self.assertEqual(targets[base + offset], ("fleet", state, number))
        self.assertEqual(targets[0x2936], ("defense", 0, 10))
        self.assertEqual(targets[0x2939], ("defense", 0, 11))
        for resource in (0x2942, 0x2949, 0x2950, 0x28a8, 0x28da):
            self.assertNotIn(resource, targets)

    def test_defense_tab_replacement_preserves_frame_and_empty_state_pattern(self) -> None:
        source = Image.new("P", (36, 33), 6)
        source.putpalette(palette())
        cutout = Image.new("RGBA", (20, 5), (240, 30, 20, 255))
        for state in range(3):
            result = forces_tab_icon(cutout, source, 253, "troops", None, "defense", state)
            expected = source.resize(result.size, Image.Resampling.NEAREST)
            bottom = 124 if state == 1 else 116
            for box in ((0, 0, 144, 20), (0, 20, 20, 132),
                        (124, 20, 144, 132), (20, bottom, 124, 132)):
                self.assertEqual(result.crop(box).tobytes(), expected.crop(box).tobytes())
            if state == 1:
                self.assertEqual(result.getpixel((20, bottom - 1)), 0)
            self.assertEqual(result.getpixel((20, 20)), 60 if state == 2 else 0)
            self.assertEqual(result.getpixel((24, 20)), 0)

    def test_fleet_tabs_clear_baked_art_but_keep_faction_frame_and_baseline(self) -> None:
        cutout = Image.new("RGBA", (20, 5), (240, 30, 20, 255))
        for width, frame in ((30, 249), (31, 250)):
            source = Image.new("P", (width, 29), 6)
            source.putpalette(palette())
            ImageDraw.Draw(source).line((3, 0, 27, 0), fill=frame)
            source.putpixel((1, 8), frame)
            result = forces_tab_icon(cutout, source, 252, "ships", None, "fleet", 0)
            self.assertEqual(result.size, (width * 4, 116))
            self.assertEqual(result.getpixel((12, 0)), frame)
            self.assertEqual(result.getpixel((4, 32)), frame)
            self.assertEqual(result.getpixel((0, 0)), 0)
            self.assertEqual(result.crop((0, 108, width * 4, 116)).tobytes(),
                             source.resize(result.size, Image.Resampling.NEAREST)
                             .crop((0, 108, width * 4, 116)).tobytes())

    def test_facility_window_mapping_covers_buildings_and_states(self) -> None:
        targets = facility_targets()
        self.assertEqual(len(targets), 39)
        self.assertEqual(targets[0x2329], ("row", 0, 1))
        self.assertNotIn(0x2349, targets)  # Transit is composed from a shared backdrop at runtime.
        self.assertEqual(targets[0x233c], ("row", 2, 7))
        self.assertEqual(targets[0x285e], ("tab", 2, 5))
        self.assertNotIn(0x232d, targets)  # Unworked mineral deposits, not a mine.

    def test_facility_tabs_preserve_frames_and_clear_old_art(self) -> None:
        source = Image.new("P", (36, 33), 6)
        source.putpalette(palette())
        cutout = Image.new("RGBA", (20, 5), (240, 30, 20, 255))
        for state in range(3):
            with self.subTest(state=state):
                result = facility_icon(cutout, source, 253, None, "tab", state)
                expected_frame = source.resize(result.size, Image.Resampling.NEAREST)
                for box in ((0, 0, 144, 20), (0, 20, 20, 132), (124, 20, 144, 132)):
                    self.assertEqual(result.crop(box).tobytes(), expected_frame.crop(box).tobytes())
                self.assertEqual(result.getpixel((20, 20)), 64 if state == 2 else 183)
                self.assertEqual(result.getpixel((20, 120)), 183 if state == 0 else 6)

    def test_facility_brightness_ignores_transparency_and_preserves_alpha(self) -> None:
        image = Image.new("RGBA", (20, 20), (255, 255, 255, 0))
        image.putpixel((5, 5), (50, 50, 50, 255))
        image.putpixel((6, 5), (50, 50, 50, 128))
        original = image.tobytes()

        result, settings = brighten_dark_facility(image, None)

        self.assertEqual(settings["mean_visible_luma"], 50)
        self.assertEqual(settings["gain"], 1.1)
        self.assertEqual(result.getpixel((5, 5)), (55, 55, 55, 255))
        self.assertEqual(result.getchannel("A").tobytes(), image.getchannel("A").tobytes())
        self.assertEqual(image.tobytes(), original)

    def test_facility_brightness_uses_the_requested_crop(self) -> None:
        image = Image.new("RGBA", (10, 10), (255, 255, 255, 255))
        ImageDraw.Draw(image).rectangle((0, 0, 1, 1), fill=(50, 50, 50, 255))

        _, full = brighten_dark_facility(image, None)
        _, cropped = brighten_dark_facility(image, [0, 0, 2, 2])

        self.assertEqual(full["gain"], 1.0)
        self.assertEqual(cropped["gain"], 1.1)

    def test_bright_and_empty_facilities_are_unchanged(self) -> None:
        for color in [(80, 80, 80, 255), (180, 180, 180, 255), (0, 0, 0, 0)]:
            with self.subTest(color=color):
                image = Image.new("RGBA", (10, 10), color)
                result, settings = brighten_dark_facility(image, None)
                self.assertEqual(result.tobytes(), image.tobytes())
                self.assertEqual(settings["gain"], 1.0)

    def test_planet_refinement_removes_outer_matte_without_hollowing_the_disc(self) -> None:
        mask = Image.new("L", (120, 120))
        ImageDraw.Draw(mask).ellipse((10, 10, 110, 110), fill=255)
        original = mask.tobytes()

        refined = refine_planet_mask(mask)

        self.assertEqual(mask.tobytes(), original)
        self.assertEqual(refined.getpixel((60, 12)), 0)
        self.assertEqual(refined.getpixel((60, 20)), 255)
        self.assertEqual(refined.getpixel((60, 60)), 255)
        self.assertEqual(refined.getpixel((60, 100)), 255)
        self.assertTrue(np.any((np.asarray(refined) > 0) & (np.asarray(refined) < 255)))

    def test_planet_refinement_preserves_rings_and_irregular_asteroids(self) -> None:
        ringed = Image.new("L", (160, 120))
        draw = ImageDraw.Draw(ringed)
        draw.ellipse((2, 45, 158, 75), outline=255, width=6)
        draw.ellipse((45, 25, 115, 95), fill=255)
        asteroid = Image.new("L", (120, 120))
        ImageDraw.Draw(asteroid).polygon([(20, 55), (40, 15), (60, 25), (78, 12),
                                         (95, 45), (85, 65), (105, 82), (65, 106),
                                         (35, 92), (15, 100)], fill=255)
        for mask in (ringed, asteroid, Image.new("L", (120, 120))):
            with self.subTest(bounds=mask.getbbox()):
                self.assertEqual(refine_planet_mask(mask).tobytes(), mask.tobytes())

    def test_mask_recipe_operations_apply_in_order_without_mutating_input(self) -> None:
        original = Image.new("L", (8, 8), 0)
        original.putpixel((0, 0), 255)
        before = original.copy()
        recipe = {
            "source_sha256": "source",
            "size": [8, 8],
            "label": "synthetic",
            "operations": [
                {"operation": "add", "points": [(1, 1), (4, 1), (4, 4), (1, 4)]},
                {"operation": "subtract", "points": [(2, 2), (3, 2), (3, 3), (2, 3)]},
                {"operation": "intersect", "points": [(1, 1), (5, 1), (5, 5), (1, 5)]},
            ],
        }

        result = apply_mask_recipe(original, recipe, "source")

        self.assertEqual(original, before)
        self.assertEqual(result.getpixel((0, 0)), 0)
        self.assertEqual(result.getpixel((1, 1)), 255)
        self.assertEqual(result.getpixel((2, 2)), 0)
        self.assertEqual(result.getpixel((4, 4)), 255)

    def test_mask_recipe_replace_discards_previous_mask(self) -> None:
        mask = Image.new("L", (8, 8), 255)
        recipe = {
            "source_sha256": "source",
            "size": [8, 8],
            "label": "synthetic",
            "operations": [
                {"operation": "replace", "points": [(2, 2), (4, 2), (4, 4), (2, 4)]},
            ],
        }

        result = apply_mask_recipe(mask, recipe, "source")

        self.assertEqual(mask.getpixel((0, 0)), 255)
        self.assertEqual(result.getpixel((0, 0)), 0)
        self.assertEqual(result.getpixel((3, 3)), 255)

    def test_mask_recipe_rejects_source_hash_and_size_mismatch(self) -> None:
        mask = Image.new("L", (8, 8))
        recipe = {"source_sha256": "expected", "size": [8, 8], "label": "synthetic", "operations": []}

        with self.assertRaisesRegex(ValueError, "differs from the reviewed recipe"):
            apply_mask_recipe(mask, recipe, "different")
        recipe["source_sha256"] = "expected"
        recipe["size"] = [9, 8]
        with self.assertRaisesRegex(ValueError, "differs from the reviewed recipe"):
            apply_mask_recipe(mask, recipe, "expected")

    def test_recipe_change_rerenders_from_raw_cached_mask_without_prediction(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            source = Image.new("RGB", (12, 12))
            calls: list[int] = []

            def predict(image: Image.Image) -> Image.Image:
                calls.append(1)
                result = Image.new("L", image.size)
                ImageDraw.Draw(result).rectangle((2, 2, 9, 9), fill=255)
                return result

            base_recipe = {"source_sha256": "source", "size": [12, 12], "label": "synthetic", "operations": []}
            first = cached_mask(source, "source", "facilities", base_recipe, directory, "env", False, predict)
            changed_recipe = {
                **base_recipe,
                "operations": [{"operation": "subtract", "points": [(2, 2), (4, 2), (4, 4), (2, 4)]}],
            }
            second = cached_mask(source, "source", "facilities", changed_recipe, directory, "env", False, predict)

            self.assertEqual(len(calls), 1)
            self.assertEqual(first.getpixel((3, 3)), 255)
            self.assertEqual(second.getpixel((3, 3)), 0)
            self.assertEqual(second.getpixel((8, 8)), 255)

    def test_raw_cache_invalidates_when_source_hash_or_environment_changes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            source = Image.new("RGB", (12, 12))
            calls: list[int] = []

            def predict(image: Image.Image) -> Image.Image:
                calls.append(1)
                result = Image.new("L", image.size)
                ImageDraw.Draw(result).rectangle((2, 2, 9, 9), fill=255)
                return result

            cached_mask(source, "source-a", "facilities", None, directory, "env-a", False, predict)
            cached_mask(source, "source-b", "facilities", None, directory, "env-a", False, predict)
            cached_mask(source, "source-b", "facilities", None, directory, "env-b", False, predict)

            self.assertEqual(len(calls), 3)

    def test_force_reruns_prediction_even_with_valid_raw_cache(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            source = Image.new("RGB", (12, 12))
            calls: list[int] = []

            def predict(image: Image.Image) -> Image.Image:
                calls.append(1)
                result = Image.new("L", image.size)
                ImageDraw.Draw(result).rectangle((2, 2, 9, 9), fill=255)
                return result

            cached_mask(source, "source", "facilities", None, directory, "env", False, predict)
            cached_mask(source, "source", "facilities", None, directory, "env", True, predict)

            self.assertEqual(len(calls), 2)

    def test_character_mask_discards_left_emblem_and_small_background_islands(self) -> None:
        mask = Image.new("L", (100, 100))
        draw = ImageDraw.Draw(mask)
        draw.rectangle((4, 10, 24, 66), fill=255)  # disconnected faction emblem
        draw.ellipse((27, 8, 63, 52), fill=255)  # face reaches into the former crop zone
        draw.rectangle((22, 50, 72, 95), fill=255)  # connected shoulders and torso
        draw.rectangle((86, 14, 88, 16), fill=255)  # small disconnected noise

        cleaned = np.asarray(clean_mask(mask, "characters"))

        self.assertEqual(int(cleaned[:, :30].max()), 0)
        self.assertEqual(int(cleaned[30, 31]), 255)
        self.assertEqual(int(cleaned[30, 45]), 255)
        self.assertEqual(int(cleaned[15, 87]), 0)

    def test_nonportrait_mask_keeps_separate_major_parts(self) -> None:
        mask = Image.new("L", (40, 30))
        draw = ImageDraw.Draw(mask)
        draw.rectangle((2, 3, 17, 18), fill=255)
        draw.rectangle((23, 6, 34, 17), fill=255)
        draw.rectangle((0, 28, 1, 29), fill=255)

        cleaned = np.asarray(clean_mask(mask, "facilities"))

        self.assertEqual(int(cleaned[8, 8]), 255)
        self.assertEqual(int(cleaned[10, 28]), 255)
        self.assertEqual(int(cleaned[28, 0]), 0)

    def test_portrait_crop_keeps_full_face_and_drops_lower_torso(self) -> None:
        cutout = Image.new("RGBA", (100, 140))
        alpha = Image.new("L", cutout.size)
        draw = ImageDraw.Draw(alpha)
        draw.ellipse((34, 8, 66, 48), fill=255)
        draw.rectangle((24, 48, 76, 139), fill=255)
        cutout.putalpha(alpha)

        crop = portrait_crop(cutout)
        left, top, right, bottom = crop
        cropped_alpha = np.asarray(cutout.getchannel("A").crop(crop))

        self.assertLessEqual(left, 34)
        self.assertLessEqual(top, 8)
        self.assertGreaterEqual(right, 66)
        self.assertGreaterEqual(bottom, 48)
        self.assertEqual(int(cropped_alpha[8 - top, 50 - left]), 255)
        self.assertLess(bottom, 139)

    def test_empty_mask_is_rejected_so_builder_can_retain_original_icon(self) -> None:
        with self.assertRaisesRegex(ValueError, "no foreground"):
            clean_mask(Image.new("L", (32, 24)), "facilities")

    def test_fit_icon_preserves_source_key_bounds_and_outputs_scale_four_canvas(self) -> None:
        source = Image.new("P", (4, 3), 0)
        source.putpixel((1, 1), 1)
        source.putpixel((2, 1), 1)
        cutout = Image.new("RGBA", (8, 8), (255, 0, 0, 255))

        fitted = fit_icon(cutout, source, 0, "facilities", None)

        self.assertEqual(fitted.size, (16, 12))
        alpha = np.asarray(fitted.getchannel("A"))
        self.assertEqual(np.count_nonzero(alpha), 4 * 4)
        self.assertTrue(np.all(alpha[:, :4] == 0))
        self.assertTrue(np.all(alpha[:, 12:] == 0))

    def test_quantization_never_uses_key_and_preserves_duplicate_palette_index_roles(self) -> None:
        pixels = Image.new("RGBA", (3, 1))
        pixels.putdata([(240, 30, 20, 255), (0, 0, 0, 0), (20, 30, 240, 255)])

        result = quantize_icon(pixels, palette(), 7)

        self.assertEqual(result.mode, "P")
        self.assertEqual(result.getpixel((0, 0)), 2)
        self.assertEqual(result.getpixel((1, 0)), 7)
        self.assertEqual(result.getpixel((2, 0)), 6)
        self.assertEqual(result.getpalette()[2 * 3:2 * 3 + 3], result.getpalette()[3 * 3:3 * 3 + 3])


if __name__ == "__main__":
    unittest.main()
