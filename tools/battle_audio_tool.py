#!/usr/bin/env -S uv run
"""Inspect the original game's battle sounds: the sound manager's id table, the WAVE resources and their loudness.

The table subcommand disassembles the constructor of the original tactical sound manager (REBEXE.EXE 0x5bae60) and
replays its stores, which is how docs/battle_audio.md maps the manager's sound ids to WAVE resource ids.
Output goes under work/battle_audio (ignored); never commit what it extracts.
"""
# /// script
# requires-python = ">=3.11,<3.14"
# dependencies = ["pefile>=2023.2.7", "capstone>=5", "numpy>=2"]
# ///
from __future__ import annotations

import argparse
import re
import struct
import sys
import wave
from pathlib import Path

import capstone
import numpy as np
import pefile

REPO = Path(__file__).resolve().parent.parent
DEFAULT_INSTALL = REPO / "orig_game_files" / "Star Wars - Rebellion"
MODULES = ("TACTICAL.DLL", "VOICEFXA.DLL", "VOICEFXE.DLL")

MANAGER_CONSTRUCTOR = (0x5BAE60, 0x5BBCED)
SOUND_COUNT = 0x13D
FIRST_VOICE_ID = 0x20
VARIANT_COUNT_OFFSET = 0x50
CATEGORY_OFFSET = 0x544
WAVE_TABLE_OFFSET = 0xA38
LOOP_END = 0x5BAEEB  # the fill loop before this address is replayed by hand: every id defaults to 1 variant of 13000
MAX_VARIANTS = 4
OBJECT_SIZE = 0x1500


def wave_resources(dll: Path) -> dict[int, bytes]:
    pe = pefile.PE(str(dll))
    waves: dict[int, bytes] = {}
    for type_entry in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        if str(type_entry.name) != "WAVE":
            continue
        for name_entry in type_entry.directory.entries:
            for language_entry in name_entry.directory.entries:
                data = language_entry.data.struct
                waves[name_entry.id] = pe.get_data(data.OffsetToData, data.Size)
    return waves


class ManagerImage:
    """The constructor's stores replayed into a byte image of the manager object."""

    def __init__(self, exe: Path) -> None:
        pe = pefile.PE(str(exe))
        base = pe.OPTIONAL_HEADER.ImageBase
        start, end = MANAGER_CONSTRUCTOR
        code = pe.get_data(start - base, end - start + 1)
        self.memory = bytearray(OBJECT_SIZE)
        for sound in range(SOUND_COUNT):
            struct.pack_into("<I", self.memory, VARIANT_COUNT_OFFSET + 4 * sound, 1)
            struct.pack_into("<II", self.memory, WAVE_TABLE_OFFSET + 8 * sound, 0x32C832C8, 0x32C832C8)
        self.registers: dict[str, int | tuple[str, int] | None] = {
            name: None for name in ("eax", "ebx", "ecx", "edx", "edi", "ebp")
        }
        self.registers["ebx"] = 1
        self.replay(capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32), code, start)

    def value(self, operand: str) -> int | tuple[str, int] | None:
        if operand in self.registers:
            return self.registers[operand]
        try:
            return int(operand, 0)
        except ValueError:
            return None

    def store(self, destination: str, source: str) -> None:
        match = re.fullmatch(r"(byte|word|dword) ptr \[esi(?: \+ (0x[0-9a-f]+))?\]", destination)
        value = self.value(source)
        if match is None or not isinstance(value, int):
            return
        fmt = {"byte": "<B", "word": "<H", "dword": "<I"}[match.group(1)]
        offset = int(match.group(2), 16) if match.group(2) else 0
        struct.pack_into(fmt, self.memory, offset, value & ((1 << (8 * struct.calcsize(fmt))) - 1))

    def fill(self) -> None:
        destination, count, fill = self.registers["edi"], self.registers["ecx"], self.registers["eax"]
        assert isinstance(destination, tuple) and isinstance(count, int) and isinstance(fill, int)
        for index in range(count):
            struct.pack_into("<I", self.memory, destination[1] + 4 * index, fill)

    def replay(self, disassembler: capstone.Cs, code: bytes, start: int) -> None:
        for instruction in disassembler.disasm(code, start):
            if instruction.address < LOOP_END:
                continue
            mnemonic, operands = instruction.mnemonic, instruction.op_str
            if mnemonic == "mov":
                destination, source = (part.strip() for part in operands.split(",", 1))
                if destination in self.registers:
                    self.registers[destination] = self.value(source)
                else:
                    self.store(destination, source)
            elif mnemonic == "xor":
                left, right = (part.strip() for part in operands.split(","))
                if left == right and left in self.registers:
                    self.registers[left] = 0
            elif mnemonic == "lea":
                destination, source = (part.strip() for part in operands.split(",", 1))
                match = re.fullmatch(r"\[esi \+ (0x[0-9a-f]+)\]", source)
                if destination in self.registers:
                    self.registers[destination] = ("esi+", int(match.group(1), 16)) if match else None
            elif mnemonic.startswith("rep stos"):
                self.fill()
            elif mnemonic == "call":
                for name in ("eax", "ecx", "edx"):
                    self.registers[name] = None

    def sound(self, sound_id: int) -> tuple[int, int, list[int]]:
        variants = struct.unpack_from("<I", self.memory, VARIANT_COUNT_OFFSET + 4 * sound_id)[0]
        category = struct.unpack_from("<I", self.memory, CATEGORY_OFFSET + 4 * sound_id)[0]
        waves = struct.unpack_from(f"<{MAX_VARIANTS}H", self.memory, WAVE_TABLE_OFFSET + 8 * sound_id)
        return category, variants, list(waves[:variants])


def command_table(args: argparse.Namespace) -> None:
    image = ManagerImage(args.install_dir / "REBEXE.EXE")
    print("sound id\tcategory\tvariants\tWAVE ids")
    for sound_id in range(SOUND_COUNT):
        category, variants, waves = image.sound(sound_id)
        print(f"{sound_id}\t{category}\t{variants}\t{' '.join(map(str, waves))}")


def command_extract(args: argparse.Namespace) -> None:
    for module in MODULES:
        target = args.out / module.split(".")[0].lower()
        target.mkdir(parents=True, exist_ok=True)
        for wave_id, data in wave_resources(args.install_dir / module).items():
            (target / f"{wave_id}.wav").write_bytes(data)
        print(f"extracted {module} to {target}")


def to_mono_float(path: Path) -> tuple[np.ndarray, int, int, int]:
    with wave.open(str(path)) as reader:
        channels, width, rate = reader.getnchannels(), reader.getsampwidth(), reader.getframerate()
        raw = reader.readframes(reader.getnframes())
    if width == 1:
        samples = (np.frombuffer(raw, np.uint8).astype(np.float32) - 128) / 128
    else:
        samples = np.frombuffer(raw, "<i2").astype(np.float32) / 32768
    if channels > 1:
        samples = samples.reshape(-1, channels).mean(axis=1)
    return samples, rate, channels, width * 8


def decibels(amplitude: float) -> float:
    return 20 * float(np.log10(max(amplitude, 1e-6)))


def command_measure(args: argparse.Namespace) -> None:
    print("module\twave\trate\tchannels\tbits\tseconds\tpeak_dbfs\trms_dbfs")
    for module in MODULES:
        for path in sorted((args.out / module.split(".")[0].lower()).glob("*.wav"), key=lambda p: int(p.stem)):
            samples, rate, channels, bits = to_mono_float(path)
            peak = decibels(float(np.abs(samples).max()))
            rms = decibels(float(np.sqrt((samples**2).mean())))
            print(f"{module}\t{path.stem}\t{rate}\t{channels}\t{bits}\t{len(samples) / rate:.2f}\t{peak:.1f}\t{rms:.1f}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--install-dir", type=Path, default=DEFAULT_INSTALL)
    parser.add_argument("--out", type=Path, default=REPO / "work" / "battle_audio" / "wav")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("table", help="print the sound manager's sound id -> WAVE id table").set_defaults(
        run=command_table)
    commands.add_parser("extract", help="write every WAVE of the three modules under --out").set_defaults(
        run=command_extract)
    commands.add_parser("measure", help="print format, duration, peak and RMS of the extracted WAVEs").set_defaults(
        run=command_measure)
    args = parser.parse_args()
    args.run(args)
    return 0


if __name__ == "__main__":
    sys.exit(main())
