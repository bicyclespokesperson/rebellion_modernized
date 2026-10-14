from __future__ import annotations

import hashlib
import struct
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.build_hd_pack import bitmap_file, fnv1a64, indexed_lanczos, merge_asset_records, remap_key_cells, scale2x, write_asset


def synthetic_dib(width: int = 2, height: int = 2) -> bytes:
    stride = ((width + 3) // 4) * 4
    palette = bytearray(256 * 4)
    palette[1 * 4:1 * 4 + 4] = bytes((0, 0, 255, 0))
    palette[3 * 4:3 * 4 + 4] = bytes((0, 0, 255, 0))
    palette[252 * 4:252 * 4 + 4] = bytes((255, 0, 0, 0))
    rows = [bytes((1, 252)) + bytes(stride - width), bytes((252, 3)) + bytes(stride - width)]
    header = struct.pack("<IiiHHIIiiII", 40, width, height, 1, 8, 0, stride * height, 0, 0, 256, 0)
    return header + palette + b"".join(rows)


def indexed_image(size: tuple[int, int], pixels: bytes, palette: list[int]) -> Image.Image:
    image = Image.frombytes("P", size, pixels)
    image.putpalette(palette)
    return image


def two_color_palette() -> list[int]:
    palette = [0, 0, 0] * 256
    palette[1 * 3:1 * 3 + 3] = [255, 0, 0]
    palette[3 * 3:3 * 3 + 3] = [255, 0, 0]
    palette[252 * 3:252 * 3 + 3] = [0, 0, 255]
    return palette


class BuildHdPackTests(unittest.TestCase):
    def test_keyed_lanczos_preserves_mask_and_avoids_key_color_fringes(self) -> None:
        source = indexed_image((2, 2), bytes((252, 1, 1, 252)), two_color_palette())
        enlarged = indexed_lanczos(source, 4, 252)
        enlarged = remap_key_cells(source, enlarged, 252, 4)
        output = enlarged.tobytes()
        palette = enlarged.getpalette()
        for y in range(2):
            for x in range(2):
                block = [output[(y * 4 + dy) * 8 + x * 4 + dx] for dy in range(4) for dx in range(4)]
                if source.getpixel((x, y)) == 252:
                    self.assertEqual(set(block), {252})
                else:
                    self.assertNotIn(252, block)
                    self.assertTrue(all(palette[index * 3:index * 3 + 3] == [255, 0, 0] for index in block))

    def test_duplicate_palette_rgb_keeps_index_roles(self) -> None:
        source = indexed_image((2, 1), bytes((252, 3)), two_color_palette())
        enlarged = remap_key_cells(source, indexed_lanczos(source, 4, 252), 252, 4)
        pixels = enlarged.tobytes()
        for y in range(4):
            self.assertEqual(set(pixels[y * 8:y * 8 + 4]), {252})
            self.assertEqual(set(pixels[y * 8 + 4:y * 8 + 8]), {3})
        self.assertEqual(source.getpalette()[3 * 3:3 * 3 + 3], source.getpalette()[1 * 3:1 * 3 + 3])

    def test_scale2x_preserves_shape_edges(self) -> None:
        palette = two_color_palette()
        source = indexed_image((3, 3), bytes((1, 1, 2, 1, 0, 2, 1, 2, 2)), palette)
        doubled = scale2x(source)
        self.assertEqual(doubled.size, (6, 6))
        self.assertEqual([doubled.getpixel((x, y)) for y in (2, 3) for x in (2, 3)], [1, 0, 0, 2])

    def test_pack_header_payload_and_filename_match_contract(self) -> None:
        dib = synthetic_dib()
        bitmap_file(dib)
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            record = write_asset(Path("synthetic.dll"), output, 42, 1033, dib, 4, "lanczos", "252")
            pack = (output / record["pack_file"]).read_bytes()
            width, height, scale, payload_size, source_hash = struct.unpack_from("<IIIIQ", pack, 8)
            self.assertEqual(pack[:8], b"REBHD01\0")
            self.assertEqual((width, height, scale), (2, 2, 4))
            self.assertEqual(payload_size, 64)
            self.assertEqual(len(pack), 32 + payload_size)
            self.assertEqual(source_hash, fnv1a64(dib))
            self.assertEqual(record["pack_file"], f"{source_hash:016x}.rhd")
            self.assertEqual(hashlib.sha256(pack).hexdigest(), record["pack_sha256"])
            with Image.open(record["accepted_png"]) as accepted:
                self.assertEqual(accepted.tobytes(), pack[32:])

    def test_malformed_dib_palette_or_pixels_are_rejected(self) -> None:
        dib = synthetic_dib()
        with self.assertRaises(ValueError):
            bitmap_file(dib[:-1])
        bad_palette = bytearray(dib)
        struct.pack_into("<I", bad_palette, 32, 257)
        with self.assertRaises(ValueError):
            bitmap_file(bytes(bad_palette))

    def test_manifest_merge_retains_other_pack_hashes_and_updates_same_hash(self) -> None:
        first = {"pack_file": "a.rhd", "module": "A.dll", "resource_id": 1, "language_id": 1033, "route": "nearest"}
        other = {"pack_file": "b.rhd", "module": "A.dll", "resource_id": 2, "language_id": 1033, "route": "scale2x"}
        updated = {**first, "route": "lanczos"}
        merged = merge_asset_records([first, other], [updated])
        self.assertEqual([record["pack_file"] for record in merged], ["a.rhd", "b.rhd"])
        self.assertEqual(merged[0]["route"], "lanczos")


if __name__ == "__main__":
    unittest.main()
