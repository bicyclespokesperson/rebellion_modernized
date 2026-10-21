from __future__ import annotations

import io
import sys
import unittest
import xml.etree.ElementTree as ET
from pathlib import Path

from PIL import Image
import resvg_py

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from build_transit_backdrops import FACILITY_TRANSIT_RESOURCES, facility_transit_svg, index_backdrop, transit_svg


class TransitBackdropTests(unittest.TestCase):
    def test_svg_is_self_contained_vector_art(self) -> None:
        for draw, view_box in ((transit_svg, "0 0 244 100"), (facility_transit_svg, "0 0 268 140")):
            svg = draw()
            root = ET.fromstring(svg)
            self.assertEqual(root.attrib["viewBox"], view_box)
            self.assertEqual(svg, draw())
            for element in root.iter():
                self.assertNotIn(element.tag.rsplit("}", 1)[-1], ("image", "script", "text"))
                for name, value in element.attrib.items():
                    if name.endswith("href"):
                        self.assertTrue(value.startswith("#"))

    def test_facility_backdrops_render_at_exact_resource_dimensions(self) -> None:
        for width, height in FACILITY_TRANSIT_RESOURCES.values():
            with self.subTest(size=(width, height)):
                rendered = resvg_py.svg_to_bytes(svg_string=facility_transit_svg(width, height),
                                                 width=width * 4, height=height * 4, skip_system_fonts=True)
                with Image.open(io.BytesIO(rendered)) as image:
                    self.assertEqual(image.size, (width * 4, height * 4))

    def test_facility_backdrop_uses_new_alpha_instead_of_old_pixelated_streak_mask(self) -> None:
        source = Image.new("P", (2, 2), 0)
        source.putpalette([value for index in range(256) for value in (index, index, index)])
        rgba = Image.new("RGBA", (8, 8))
        rgba.putpixel((5, 5), (200, 200, 200, 255))
        result = index_backdrop(rgba, source, 0, preserve_source_mask=False)
        self.assertEqual(result.getpixel((5, 5)), 200)
        self.assertEqual(result.getpixel((4, 5)), 0)
        self.assertEqual(sum(value != 0 for value in result.tobytes()), 1)

    def test_indexing_preserves_palette_and_exact_transparent_cells(self) -> None:
        for key in (0, 252):
            with self.subTest(key=key):
                palette = [value for index in range(256) for value in (index, index, index)]
                source = Image.new("P", (3, 2), 180)
                source.putpalette(palette)
                source.putpixel((0, 1), key)
                before = source.tobytes()
                result = index_backdrop(Image.new("RGBA", (12, 8), (key, key, key, 255)), source, key)
                self.assertEqual(result.getpalette(), palette)
                self.assertEqual(source.tobytes(), before)
                for y in range(8):
                    for x in range(12):
                        self.assertEqual(result.getpixel((x, y)) == key, x < 4 and y >= 4)


if __name__ == "__main__":
    unittest.main()
