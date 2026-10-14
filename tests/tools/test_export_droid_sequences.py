from __future__ import annotations

import struct
import sys
import unittest
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[2]))
from tools.export_droid_sequences import decode_delta, runtime_bilinear


def palette() -> list[int]:
    values = [0, 0, 0] * 256
    values[5 * 3:5 * 3 + 3] = [240, 210, 160]
    values[6 * 3:6 * 3 + 3] = [240, 210, 160]
    values[7 * 3:7 * 3 + 3] = [20, 40, 80]
    values[8 * 3:8 * 3 + 3] = [244, 210, 160]
    return values


def delta_resource(width: int, height: int, rows: list[bytes], offsets: list[int] | None = None) -> bytes:
    payload = b"".join(rows)
    row_offsets = offsets if offsets is not None else [sum(len(row) for row in rows[:index]) for index in range(height)]
    header = struct.pack("<hhIBhhhh", width, height, len(payload), 0, 0, 0, width, height)
    return header + struct.pack(f"<{height}i", *row_offsets) + payload


class DroidSequenceExporterTests(unittest.TestCase):
    def test_delta_runs_apply_cumulatively_with_row_order_and_uint8_wrap(self) -> None:
        image = Image.frombytes("P", (3, 2), bytes((10, 20, 30, 1, 2, 3)))
        image.putpalette(palette())
        delta = delta_resource(3, 2, [bytes((1, 2, 1, 255)), bytes((0, 3, 2, 3, 4))])
        decode_delta(delta, image)
        self.assertEqual(image.tobytes(), bytes((10, 21, 29, 3, 5, 7)))

    def test_delta_rejects_rows_that_exceed_bitmap_bounds(self) -> None:
        image = Image.frombytes("P", (2, 1), bytes((1, 2)))
        image.putpalette(palette())
        with self.assertRaisesRegex(ValueError, "exceeds bitmap width"):
            decode_delta(delta_resource(2, 1, [bytes((3,))]), image)

    def test_delta_rejects_out_of_range_row_offsets(self) -> None:
        image = Image.frombytes("P", (2, 1), bytes((1, 2)))
        image.putpalette(palette())
        with self.assertRaisesRegex(ValueError, "points outside"):
            decode_delta(delta_resource(2, 1, [bytes((0, 2, 1, 1))], [9]), image)

    def test_runtime_bilinear_preserves_key_mask_and_duplicate_center_index(self) -> None:
        source = Image.frombytes("P", (2, 1), bytes((5, 0)))
        source.putpalette(palette())
        enlarged = runtime_bilinear(source, 4, 0)
        output = enlarged.tobytes()
        for y in range(4):
            self.assertEqual(set(output[y * 8:y * 8 + 4]), {5})
            self.assertEqual(set(output[y * 8 + 4:y * 8 + 8]), {0})

    def test_runtime_bilinear_flat_color_favors_center_over_quantized_lut_neighbor(self) -> None:
        source = Image.frombytes("P", (1, 1), bytes((5,)))
        source.putpalette(palette())
        enlarged = runtime_bilinear(source, 4, 0)
        self.assertEqual(set(enlarged.tobytes()), {5})


if __name__ == "__main__":
    unittest.main()
