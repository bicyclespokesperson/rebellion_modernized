#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["unicorn>=2.1", "pefile", "capstone>=5"]
# ///
"""Entry-poke cases for the 3D battle's character / special-force records (0x5bd7d0-0x5be080, 0x5bdc30).

No scenario reaches these functions (the 3D battle's object manager synchronises them), so each case borrows a real
capture of a strategic Read / Write / Checksum function from the regression store: ecx is a 0x7c-byte object and
[esp+4] a real stream or Crc32. The record under test is poked into that object (a tree: the tree header into the
object and the nodes on the stack), and difftest `--entry` runs the original and the reconstruction on it.

  tactical_record_variants.py
"""
from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from paths import MAIN  # noqa: E402

STORE = MAIN / "work" / "regression" / "caps"
DIFFTEST = ROOT / "tools" / "difftest.py"
TEXT_ADDRESS = 0x6ABD4C  # a string literal of the image, used as a record's name

VT_CHARACTER = 0x66BAC0
VT_SPECIAL_FORCE = 0x66BAD0
VT_CHANNEL_A = 0x66BACC
VT_CHANNEL_B = 0x66BAC8
VT_CHARACTER_TREE = 0x66C380
VT_SPECIAL_FORCE_TREE = 0x66C370
NODE_EXPRESSIONS = ("esp-0x1000", "esp-0x1200")

STREAM_BASES = {"write": "00503d30", "checksum": "00503d60", "read": "00435db0"}


def string_vptr() -> int:
    for line in (ROOT / "symbols" / "symbols.csv").read_text().splitlines():
        parts = line.split(",")
        if len(parts) > 2 and parts[2] == "String::vftable":
            return int(parts[0], 16)
    raise SystemExit("symbols.csv has no String::vftable")


def pack(image: bytearray, offset: int, fmt: str, value: int | float) -> None:
    if isinstance(value, int):
        value &= (1 << (8 * struct.calcsize(fmt))) - 1
    struct.pack_into(fmt, image, offset, value)


def record_image(vptr: int, size: int, id_: int, data_a: int, data_b: int, family: int, object_id: int, sprite: int,
                 text: int) -> bytearray:
    image = bytearray(size)
    for offset, fmt, value in ((0x00, "<I", vptr), (0x18, "<I", id_), (0x1C, "<I", VT_CHANNEL_A), (0x20, "<I", data_a),
                               (0x24, "<I", VT_CHANNEL_B), (0x28, "<I", data_b), (0x2C, "<I", family),
                               (0x30, "<I", object_id), (0x34, "<H", sprite), (0x38, "<I", string_vptr()),
                               (0x3C, "<I", text), (0x40, "<I", 1 if text else 0)):
        pack(image, offset, fmt, value)
    return image


def character_image(id_: int, data_a: int, data_b: int, family: int, object_id: int, sprite: int, text: int,
                    fields: tuple[int, int, float, float, int, int, int, int]) -> bytes:
    image = record_image(VT_CHARACTER, 0x64, id_, data_a, data_b, family, object_id, sprite, text)
    not_alliance, major, scale_a, scale_b, first, second, ship, injured = fields
    for offset, fmt, value in ((0x44, "<I", not_alliance), (0x48, "<I", major), (0x4C, "<f", scale_a),
                               (0x50, "<f", scale_b), (0x54, "<I", first), (0x58, "<I", second), (0x5C, "<I", ship),
                               (0x60, "<I", injured)):
        pack(image, offset, fmt, value)
    return bytes(image)


def special_force_image(id_: int, data_a: int, data_b: int, family: int, object_id: int, sprite: int, text: int,
                        not_alliance: int, ship: int) -> bytes:
    image = record_image(VT_SPECIAL_FORCE, 0x4C, id_, data_a, data_b, family, object_id, sprite, text)
    pack(image, 0x44, "<I", not_alliance)
    pack(image, 0x48, "<I", ship)
    return bytes(image)


def poke(where: str, data: bytes) -> str:
    return f"{where}={data.hex()}"


def run(entry: int, base: Path, pokes: list[str], label: str) -> bool:
    command = [str(DIFFTEST), "--entry", hex(entry), "--poke", ";".join(pokes), str(base)]
    output = subprocess.run(command, capture_output=True, text=True).stdout
    passed = any(line.startswith("PASS") for line in output.splitlines()) and "FAIL" not in output
    print(("PASS " if passed else "FAIL ") + label)
    if not passed:
        print("\n".join(output.splitlines()[:25]))
    return passed


def base_captures() -> dict[str, Path]:
    return {kind: sorted((STORE / address).glob("*.cap"))[0] for kind, address in STREAM_BASES.items()}


def tree_pokes(vptr: int, images: list[bytes], count: int) -> list[str]:
    pokes = [poke("ecx+0", struct.pack("<III", vptr, 0, 1))]
    if count:
        pokes.append(f"ecx+4=@{NODE_EXPRESSIONS[0]}")
    for index in range(count):
        expression = NODE_EXPRESSIONS[index]
        pokes.append(poke(expression, images[index]))
        successor = f"@{NODE_EXPRESSIONS[index + 1]}" if index + 1 < count else "@0"
        pokes.append(f"{expression}+0x10={successor}")
    return pokes


CHARACTERS = [
    character_image(1000001, 0, 0, 0x30, 0x1234, 0x107, TEXT_ADDRESS, (1, 1, 0.35, 1.5, 0, 1, 0x42, 2)),
    character_image(1000002, 0xDEADBEEF, 0x12345678, 0xFFFFFFFF, 7, 0xFFFF, 0, (0, 0, -0.0, 3.25, 1, 0, -1, 0)),
]
SPECIAL_FORCES = [
    special_force_image(1000003, 0, 0, 0x31, 0x4321, 0x108, TEXT_ADDRESS, 1, 0x77),
    special_force_image(1000004, 0x11111111, 0x22222222, 0, 0, 0, 0, 0, -5),
]


def record_cases(bases: dict[str, Path]) -> list[bool]:
    results = []
    kinds = (("TacticalCharacterUnit", CHARACTERS, {"write": 0x5BD9A0, "checksum": 0x5BD7D0, "read": 0x5BD890}),
             ("TacticalSpecialForceUnit", SPECIAL_FORCES, {"write": 0x5BDE70, "checksum": 0x5BDD40, "read": 0x5BDDB0}))
    for name, images, entries in kinds:
        for variant, image in enumerate(images):
            for kind, entry in entries.items():
                results.append(run(entry, bases[kind], [poke("ecx+0", image)], f"{name}::{kind} variant {variant}"))
    return results


def tree_cases(bases: dict[str, Path]) -> list[bool]:
    results = []
    kinds = (("TacticalCharacterTree", VT_CHARACTER_TREE, CHARACTERS,
              {"write": 0x5BDBA0, "checksum": 0x5BDBF0, "read": 0x5BDAB0}),
             ("TacticalSpecialForceTree", VT_SPECIAL_FORCE_TREE, SPECIAL_FORCES,
              {"write": 0x5BE030, "checksum": 0x5BE080, "read": 0x5BDF40}))
    for count in (0, 1, 2):
        for name, vptr, images, entries in kinds:
            pokes = tree_pokes(vptr, images, count)
            for kind in ("write", "checksum"):
                results.append(run(entries[kind], bases[kind], pokes, f"{name}::{kind} with {count} records"))
            results.append(run(0x5BDF20, bases["write"], pokes, f"{name}::CountItems with {count} records"))
    for name, vptr, _, entries in kinds:
        results.append(run(entries["read"], bases["read"], tree_pokes(vptr, [], 0), f"{name}::read from the stream"))
    return results


def event_cases(bases: dict[str, Path]) -> list[bool]:
    results = []
    handle = 1000001
    for injured, hurt in ((0, 0), (1, 0), (0, 1), (5, 7)):
        for present in (True, False):
            image = character_image(handle if present else handle + 1, 0, 0, 1, 2, 3, 0,
                                    (1, 1, 0.5, 0.5, 0, 0, 0x11, 0xDEAD))
            arguments = [0, 0, handle, 0, 0, 0, 0x99, 0, injured, hurt, 0, 0]
            pokes = [
                "0x6bc4a0=@esp-0x1f00",
                poke("esp-0x1f00+0xe50", struct.pack("<III", VT_CHARACTER_TREE, 0, 1)),
                "esp-0x1f00+0xe54=@ecx",
                poke("ecx+0", image),
                poke("esp-0x400", struct.pack("<12I", *arguments)),
                poke("esp-0x300", struct.pack("<II", 0, 0)),
                "esp-0x2fc=@esp-0x400",
                "esp+4=@esp-0x300",
            ]
            label = f"TacticalCharacterStatusEvent flags {injured},{hurt} character {'found' if present else 'missing'}"
            results.append(run(0x5BDC30, bases["write"], pokes, label))
    results.append(run(0x5BDC30, bases["write"], ["esp+4=00000000"], "TacticalCharacterStatusEvent null event"))
    return results


def main() -> int:
    bases = base_captures()
    results = record_cases(bases) + tree_cases(bases) + event_cases(bases)
    print(f"{sum(results)}/{len(results)} cases passed")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
