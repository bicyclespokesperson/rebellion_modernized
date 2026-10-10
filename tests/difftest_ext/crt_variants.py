#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["unicorn>=2.1", "pefile", "capstone>=5"]
# ///
"""Input sweeps for the reconstructed CRT pieces (src/game/crt): the game only ever feeds them a few values, but the
reconstruction must agree with the original machine code on every value that does not take the error-reporting path.

Each case replays a real capture of the function (or, for acos, of a function with the same x87 convention) in the
emulator with other arguments: `--fpu` replaces the x87 stack, `--fpcw` the control word, `--poke` memory.
Cases that make the original report an error (domain/overflow/underflow: it sets errno and calls _matherr, which the
reconstruction does not) are left out on purpose; see docs/library_code.md.

Fixtures are captures of 0x6197b0 (_CIpow), 0x619780 (__ftol), 0x619610 (floor), 0x61a0c0 (qsort), 0x619600 (atoi):
take them with REB_CAPTURE over ui_tour (volume sliders: floor, _CIpow) and new_game_rebel (qsort, rand, atoi).

  crt_variants.py [--integration] [--only SUBSTRING]
"""
from __future__ import annotations

import argparse
import contextlib
import importlib.util
import io
import random
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOLS = ROOT / "tools"
sys.path.insert(0, str(TOOLS))
from paths import MAIN  # noqa: E402

_spec = importlib.util.spec_from_file_location("difftest", TOOLS / "difftest.py")
d = importlib.util.module_from_spec(_spec)
sys.modules["difftest"] = d
_spec.loader.exec_module(d)

NORMAL_CW = 0x27F
CONTROL_WORDS = (0x27F, 0x37F, 0x73F, 0xF7F)  # 53/64-bit precision, round down, round up + truncate-all


@dataclass
class Case:
    name: str
    fixture: int
    entry: int | None = None
    fpu: str = ""
    fpcw: int | None = None
    pokes: list[str] = field(default_factory=list)


def hex_double(value: float) -> str:
    return struct.pack("<d", value).hex()


def pow_cases() -> list[Case]:
    pairs = [(10.0, 0.5), (10.0, 2.5), (2.0, 10.0), (2.0, -3.0), (0.5, 3.0), (1.7, 3.3), (123456.789, 1 / 3),
             (1e-5, 2.0), (10.0, -2.0), (10.0, 0.0), (10.0, -0.25), (-2.0, 3.0), (-2.0, 2.0), (-3.0, -3.0),
             (0.0, 5.0), (-0.0, 3.0), (-0.0, 2.0), (5.0, 0.0), (0.0, 0.0), (1.0, 1e10), (1e-310, 0.5), (3.0, 1e-310),
             (10.0, 123.456), (10.0, -300.5), (7.0, 0.1), (1.0000001, 1e6), (0.3, 7.0), (100.0, 0.5), (65536.0, 0.25)]
    pairs += [(10.0, v / 2000.0 * 1.0) for v in range(-10000, 1, 1250)]
    return [Case(f"pow({b!r}, {e!r}) cw={cw:#x}", 0x6197B0, fpu=f"{e!r},{b!r}", fpcw=cw)
            for b, e in pairs for cw in CONTROL_WORDS[:2]]


def acos_cases() -> list[Case]:
    values = [0.0, -0.0, 0.5, -0.5, 0.123456789, -0.99, 0.9999999, 1.0, -1.0, 1e-300, 0.25, -0.75, 0.7071067811865476]
    return [Case(f"acos({x!r}) cw={cw:#x}", 0x6197B0, entry=0x61A340, fpu=repr(x), fpcw=cw)
            for x in values for cw in CONTROL_WORDS[:2]]


def ftol_cases() -> list[Case]:
    values = [0.0, -0.0, 0.5, -0.5, 1.5, -1.5, 2.5, 0.9999999, -0.9999999, 1e9, -1e9, 2147483647.5, 2147483648.0, 3e9,
              -3e9, 4503599627370495.5, 1e18, 9.3e18, -9.3e18, 123456.789, float("inf"), float("-inf"), 7.99999999999999]
    return [Case(f"ftol({x!r}) cw={cw:#x}", 0x619780, fpu=repr(x), fpcw=cw) for x in values for cw in CONTROL_WORDS]


def floor_cases() -> list[Case]:
    values = [0.0, -0.0, 0.5, -0.5, 1.0, -1.0, 1.5, -1.5, 2.9999999999, -2.0000000001, 123456.789, -123456.789,
              1e15 + 0.5, 4503599627370495.5, 4503599627370496.5, 1e300, -1e300, 5e-324, -5e-324, 1e-10,
              float("inf"), float("-inf"), 99.99999999999999, -0.1]
    return [Case(f"floor({x!r}) cw={cw:#x}", 0x619610, pokes=[f"esp+4={hex_double(x)}"], fpcw=cw)
            for x in values for cw in CONTROL_WORDS]


def atoi_cases() -> list[Case]:
    texts = ["0", "123", "-45", "+7", "  12abc", "\t\n 99", "2147483647", "2147483648", "4294967295", "99999999999",
             "-2147483648", "-2147483649", "", "abc", "- 5", "1 2", "+-3", " -0", "007", "12345678901234567890", "\v\f\r1"]
    return [Case(f"atoi({t!r})", 0x619600, pokes=[f"[esp+4]={(t.encode() + b'\0').hex()}"]) for t in texts]


def qsort_cases() -> list[Case]:
    """Arrays of {pointer, distance} pairs (the one comparator the game uses): ties in distance and null entries show
    the order of equal elements, which differs between sort algorithms."""
    rng = random.Random(1998)
    cases = []
    for num in (0, 1, 2, 3, 5, 7, 8, 9, 10, 11, 16, 17, 25, 31, 40, 64):
        for spread in (3, 1000):
            pairs = []
            for i in range(num):
                pointer = 0 if rng.random() < 0.1 else 0x10000 + 4 * i
                pairs.append(struct.pack("<II", pointer, rng.randrange(spread)))
            data = b"".join(pairs) or bytes(8)
            cases.append(Case(f"qsort(n={num}, spread={spread})", 0x61A0C0,
                              pokes=[f"[esp+4]={data.hex()}", f"esp+8={num.to_bytes(4, 'little').hex()}",
                                     f"esp+0xc={(8).to_bytes(4, 'little').hex()}"]))
    return cases


def find_fixture(address: int) -> Path | None:
    name = f"{address:08x}_0.cap"
    for directory in sorted((ROOT / "work" / "run").glob("*/captures")):
        if (directory / name).exists():
            return directory / name
    for directory in (MAIN / "work" / "regression" / "caps" / f"{address:08x}",):
        for candidate in sorted(directory.glob("*_0.cap")):
            return candidate
    return None


def run_case(case: Case, args: argparse.Namespace, exe, imports) -> bool:
    fixture = find_fixture(case.fixture)
    if fixture is None:
        print(f"SKIP {case.name}: no capture of {case.fixture:#x}")
        return True
    d.CFG.__dict__.update(d.Config().__dict__)
    d.CFG.pokes = ";".join(case.pokes)
    d.CFG.entry = case.entry
    d.CFG.fpcw = case.fpcw
    d.CFG.fpu_stack = d.parse_fpu_stack(case.fpu) if case.fpu else None
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        ok = d.run_capture(fixture, args, exe, imports)
    text = out.getvalue()
    passed = ok and "PASS" in text
    if not passed:
        print(f"{case.name}\n{text}")
    return passed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--integration", action="store_true", help="replace every reimplemented function")
    parser.add_argument("--only", help="run only the cases whose name contains this text")
    parsed = parser.parse_args()
    cases = [*pow_cases(), *acos_cases(), *ftol_cases(), *floor_cases(), *atoi_cases(), *qsort_cases()]
    if parsed.only:
        cases = [c for c in cases if parsed.only in c.name]
    args = argparse.Namespace(dll=d.DLL_PATH, integration=parsed.integration, orig_only=False, max_insns=20_000_000)
    exe = d.Image.load(d.EXE_PATH, sections=(".text", ".rdata"))
    imports = d.Imports.load()
    results = [run_case(c, args, exe, imports) for c in cases]
    print(f"{sum(results)}/{len(results)} cases passed")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
