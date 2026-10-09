#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# ///
"""Entry-poke cases for the iostream library functions no scenario reaches (src/game/iostream).

The game never parses text with `>>` or formats numbers with `<<` in any scenario (those paths belong to the data
builders), and a few streambuf / strstream entry points are only used by multiplayer code. Each case borrows a real
capture of a library call that has the right kind of `this` (an istream on an ifstream, an ostream on a strstream, a
strstreambuf, raw object memory), pokes the stream state and the arguments the case needs, and lets
`tools/difftest.py --entry` run the original and the reconstruction on it.

The fixtures come from captures of these library functions (take them with REB_CAPTURE over load_game and
save_game_slot, see the commit message of the iostream reconstruction): istream_read 0x6173b0, ostream_operator_ls_long
0x616e10, strstreambuf_str 0x615680, strstream_ctor 0x615db0, strstreambuf_ctor 0x6154e0. They are looked up under
work/run/*/captures of this checkout and in the regression store.

  iostream_variants.py [--integration] [--only SUBSTRING]
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from paths import MAIN  # noqa: E402

DIFFTEST = ROOT / "tools" / "difftest.py"

# Expressions for a stream whose `this` (ECX) is the istream/ostream part: the ios virtual base and its streambuf.
IOS = "ecx+[[ecx]+4]"
BP = f"[{IOS}+4]"
BASE = f"[{BP}+0x10]"


def ios(offset: int) -> str:
    return f"{IOS}+{offset:#x}"


def bp(offset: int) -> str:
    return f"{BP}+{offset:#x}"


def dword(value: int) -> str:
    return (value & 0xFFFFFFFF).to_bytes(4, "little").hex()


def arg(index: int) -> str:
    return f"esp+{4 * index}"


def text_bytes(text: bytes) -> str:
    return (text + b"\0").hex()


@dataclass
class Case:
    name: str
    entry: int
    fixture: int
    pokes: list[str] = field(default_factory=list)


def istream_pokes(text: bytes, flags: int, state: int = 0) -> list[str]:
    """An ifstream-based istream whose get area holds `text`."""
    return [
        f"{BASE}={text_bytes(text)}",
        f"{ios(8)}={dword(state)}",
        f"{ios(0x24)}={dword(flags)}",
        f"{ios(0x20)}={dword(0)}",
        f"{ios(0x30)}={dword(0)}",
        f"{bp(8)}={dword(0)}",
        f"{bp(0xc)}={dword(-1)}",
        f"{bp(0x24)}=@{BASE}",
        f"{bp(0x28)}=@{BASE}",
        f"{bp(0x2c)}=@{BASE}+{len(text)}",
        f"ecx+8={dword(0)}",
    ]


def ostream_pokes(flags: int, width: int, fill: int = 0x20, state: int = 0) -> list[str]:
    return [
        f"{ios(8)}={dword(state)}",
        f"{ios(0x24)}={dword(flags)}",
        f"{ios(0x2c)}={fill:02x}",
        f"{ios(0x30)}={dword(width)}",
        f"{ios(0x20)}={dword(0)}",
    ]


SKIPWS = 1
LEFT, RIGHT, INTERNAL = 2, 4, 8
DEC, OCT, HEX = 0x10, 0x20, 0x40
SHOWBASE, SHOWPOS, UPPER = 0x80, 0x400, 0x200

READ_FIXTURE = 0x6173B0
OUT_FIXTURE = 0x616E10
STRSTREAMBUF_FIXTURE = 0x615680
STRSTREAM_CTOR_FIXTURE = 0x615DB0
STRSTREAMBUF_CTOR_FIXTURE = 0x6154E0

NUMBER_TEXTS = [b"123 456", b"  -45x", b"0x1fz", b"0777", b"08", b"+7", b"99999999999", b"abc", b"", b"-", b"0x",
                b"12345678901234567890", b"65535", b"70000", b"-40000", b"4294967295", b"\n\t 9"]
STRING_TEXTS = [b"hello world", b"  spaced", b"", b"x", b"abcdefghij"]
INPUT_FLAG_SETS = [SKIPWS, SKIPWS | DEC, SKIPWS | HEX, SKIPWS | OCT, 0]
NUMBER_ENTRIES = {"rs_long": 0x617610, "rs_ulong": 0x617760, "rs_short": 0x617900, "rs_ushort": 0x6176A0}
OUTPUT_VALUES = [0, 5, -5, 255, 0x7FFFFFFF, -1]
OUTPUT_FLAG_SETS = [DEC, DEC | SHOWPOS, OCT, OCT | SHOWBASE, HEX, HEX | SHOWBASE, HEX | SHOWBASE | UPPER, 0]
OUTPUT_ENTRIES = {"ls_int": 0x617540, "ls_long": 0x616E10, "ls_ulong": 0x617480}
IMAGE_STRINGS = (0x6AC1C0, 0x6AC1C4, 0x6B120C)  # "%ld", "%lu", ""


def input_cases() -> list[Case]:
    cases: list[Case] = []
    for entry_name, entry in NUMBER_ENTRIES.items():
        for flags in INPUT_FLAG_SETS:
            for text in NUMBER_TEXTS:
                cases.append(Case(f"{entry_name} flags={flags:#x} {text!r}", entry, READ_FIXTURE, istream_pokes(text, flags)))
    for text in NUMBER_TEXTS[:6]:
        for flags in (SKIPWS | DEC, SKIPWS):
            cases.append(Case(f"getint flags={flags:#x} {text!r}", 0x618510, READ_FIXTURE, istream_pokes(text, flags)))
    for width in (0, 1, 3, 6):
        for text in STRING_TEXTS:
            cases.append(Case(f"rs_charptr width={width} {text!r}", 0x617B90,
                              READ_FIXTURE, istream_pokes(text, SKIPWS) + [f"{ios(0x30)}={dword(width)}"]))
    for limit in (0, 1, 2, 5, 40):
        for delimiter in (ord("\n"), ord(" ")):
            for text in (b"one two\nthree", b"", b"nodelim"):
                for fgline in (0, 1):
                    cases.append(Case(f"getline limit={limit} delim={delimiter} fgline={fgline} {text!r}", 0x617800, READ_FIXTURE,
                                      istream_pokes(text, SKIPWS) + [f"{arg(2)}={dword(limit)}", f"{arg(3)}={dword(delimiter)}",
                                                                      f"ecx+4={dword(fgline)}"]))
    for text in (b"abc", b""):
        cases.append(Case(f"get {text!r}", 0x617330, READ_FIXTURE, istream_pokes(text, SKIPWS)))
        cases.append(Case(f"eatwhite {text!r}", 0x617CB0, READ_FIXTURE, istream_pokes(b"   " + text, SKIPWS)))
    for need in (0, 1, 4):
        for text in (b"  abc", b""):
            for flags in (SKIPWS, 0):
                cases.append(Case(f"ipfx need={need} flags={flags} {text!r}", 0x617A90, READ_FIXTURE,
                                  istream_pokes(text, flags) + [f"{arg(1)}={dword(need)}"]))
    cases.append(Case("ipfx bad state", 0x617A90, READ_FIXTURE, istream_pokes(b"x", SKIPWS, state=4) + [f"{arg(1)}={dword(1)}"]))
    for position in (0, 3, 99):
        cases.append(Case(f"seekg_pos {position}", 0x616C30, READ_FIXTURE, [f"{arg(1)}={dword(position)}"]))
    return cases


def output_cases() -> list[Case]:
    cases: list[Case] = []
    for entry_name, entry in OUTPUT_ENTRIES.items():
        for flags in OUTPUT_FLAG_SETS:
            for value in OUTPUT_VALUES:
                for width in (0, 9):
                    for justify in (RIGHT, LEFT, INTERNAL):
                        if width == 0 and justify != RIGHT:
                            continue
                        cases.append(Case(f"{entry_name} flags={flags:#x} value={value} width={width} just={justify}", entry, OUT_FIXTURE,
                                          ostream_pokes(flags | justify, width) + [f"{arg(1)}={dword(value)}"]))
    for prefix in IMAGE_STRINGS:
        for text in IMAGE_STRINGS:
            for flags in (RIGHT, LEFT, INTERNAL, 0):
                for width in (0, 2, 7):
                    cases.append(Case(f"outstr {prefix:#x} {text:#x} flags={flags} width={width}", 0x6162B0, OUT_FIXTURE,
                                      ostream_pokes(flags, width, fill=0x2A) + [f"{arg(1)}={dword(prefix)}", f"{arg(2)}={dword(text)}"]))
    for width in (0, 4):
        cases.append(Case(f"ls_char width={width}", 0x615F00, OUT_FIXTURE, ostream_pokes(RIGHT, width) + [f"{arg(1)}={dword(ord('A'))}"]))
    cases.append(Case("flush", 0x616140, 0x616A40))
    for direction in (0, 1, 2):
        for offset in (0, 2, -1):
            cases.append(Case(f"seekp_off off={offset} dir={direction}", 0x6169A0, 0x616900,
                              [f"{arg(1)}={dword(offset)}", f"{arg(2)}={dword(direction)}"]))
    cases.append(Case("ls_streambuf from own buffer", 0x616AE0, 0x616110, [f"{arg(1)}=@{BP}"]))
    return cases


def streambuf_cases() -> list[Case]:
    """strstreambuf / streambuf state machines, driven from a captured strstreambuf (ECX = the buffer)."""
    base, ebuf = "[ecx+0x10]", "[ecx+0x14]"
    cases: list[Case] = []
    get_areas = [(0, 0), (0, 3), (1, 5), (2, 2), (5, 5), (3, 1)]  # (gptr offset, egptr offset) from base
    for entry_name, entry in (("underflow", 0x615860), ("snextc", 0x6183E0), ("sbumpc", 0x618440), ("stossc", 0x618490),
                              ("sgetc", 0x6184E0)):
        for unbuffered in (0, 1):
            for lastc in (-1, 0x41):
                for gptr, egptr in get_areas:
                    for pptr_extra in (0, 4):
                        cases.append(Case(f"{entry_name} unbuf={unbuffered} lastc={lastc} get={gptr}/{egptr} put+{pptr_extra}", entry,
                                          STRSTREAMBUF_FIXTURE,
                                          [f"ecx+8={dword(unbuffered)}", f"ecx+0xc={dword(lastc)}", f"ecx+0x24=@{base}",
                                           f"ecx+0x28=@{base}+{gptr}", f"ecx+0x2c=@{base}+{egptr}", f"ecx+0x18=@{base}",
                                           f"ecx+0x1c=@{base}+{pptr_extra}", f"ecx+0x20=@{ebuf}"]))
    for dynamic in (0, 1):
        for next_size in (1, 2, 40):
            for epptr in (0, 1):
                cases.append(Case(f"doallocate dyn={dynamic} next={next_size} epptr={epptr}", 0x615690, STRSTREAMBUF_FIXTURE,
                                  [f"ecx+0x4c={dword(dynamic)}", f"ecx+0x50={dword(next_size)}", f"ecx+0x5c={dword(0)}", f"ecx+0x60={dword(0)}",
                                   f"ecx+0x18=@{base}", f"ecx+0x1c=@{base}+2", f"ecx+0x20=@{ebuf}" if epptr else f"ecx+0x20={dword(0)}",
                                   f"ecx+0x24=@{base}", f"ecx+0x28=@{base}+1", f"ecx+0x2c=@{base}+3"]))
    cases.append(Case("streambuf doallocate", 0x617FE0, STRSTREAMBUF_FIXTURE))
    for size in (0, 7):
        cases.append(Case(f"strstreambuf_setbuf size={size}", 0x6157C0, STRSTREAMBUF_FIXTURE, [f"{arg(2)}={dword(size)}"]))
        cases.append(Case(f"streambuf_setbuf size={size}", 0x617E10, STRSTREAMBUF_FIXTURE,
                          [f"{arg(1)}=@{base}", f"{arg(2)}={dword(size)}", f"ecx+0x10={dword(0)}"]))
    for mode in (1, 2, 3):
        for direction in (0, 1, 2, 5):
            for offset in (0, 2, -1, 40):
                for dynamic in (0, 1):
                    cases.append(Case(f"strstreambuf_seekoff mode={mode} dir={direction} off={offset} dyn={dynamic}", 0x6158C0,
                                      STRSTREAMBUF_FIXTURE,
                                      [f"{arg(1)}={dword(offset)}", f"{arg(2)}={dword(direction)}", f"{arg(3)}={dword(mode)}",
                                       f"ecx+0x4c={dword(dynamic)}", f"ecx+0x24=@{base}", f"ecx+0x28=@{base}+1", f"ecx+0x2c=@{base}+4",
                                       f"ecx+0x18=@{base}", f"ecx+0x1c=@{base}+2", f"ecx+0x20=@{base}+6"]))
    cases.append(Case("streambuf_seekoff", 0x618060, STRSTREAMBUF_FIXTURE))
    for char in (0x41, 0xE9):
        for gptr in (0, 2):
            for unbuffered in (0, 1):
                cases.append(Case(f"pbackfail c={char:#x} gptr={gptr} unbuf={unbuffered}", 0x618090, STRSTREAMBUF_FIXTURE,
                                  [f"{arg(1)}={dword(char)}", f"ecx+8={dword(unbuffered)}", f"ecx+0x24=@{base}", f"ecx+0x28=@{base}+{gptr}",
                                   f"ecx+0x2c=@{base}+4"]))
    return cases


def stream_ctor_cases() -> list[Case]:
    text = 0x6AC1C0
    cases = [
        Case("strstreambuf_ctor_buf size=0", 0x615530, STRSTREAMBUF_CTOR_FIXTURE,
             [f"{arg(1)}={dword(text)}", f"{arg(2)}={dword(0)}", f"{arg(3)}={dword(0)}"]),
        Case("strstreambuf_ctor_buf size=2", 0x615530, STRSTREAMBUF_CTOR_FIXTURE,
             [f"{arg(1)}={dword(text)}", f"{arg(2)}={dword(2)}", f"{arg(3)}={dword(0)}"]),
        Case("strstreambuf_ctor_buf size<0 start", 0x615530, STRSTREAMBUF_CTOR_FIXTURE,
             [f"{arg(1)}={dword(text)}", f"{arg(2)}={dword(-1)}", f"{arg(3)}={dword(text)}"]),
    ]
    flag = 1  # flag 0 expects an already constructed virtual base, which raw object memory does not have
    for _ in (0,):
        cases += [
            Case(f"istrstream_ctor_str flag={flag}", 0x6159E0, STRSTREAM_CTOR_FIXTURE, [f"{arg(1)}={dword(text)}", f"{arg(2)}={dword(flag)}"]),
            Case(f"istrstream_ctor_str_len flag={flag}", 0x615AD0, STRSTREAM_CTOR_FIXTURE,
                 [f"{arg(1)}={dword(text)}", f"{arg(2)}={dword(3)}", f"{arg(3)}={dword(flag)}"]),
        ]
        for mode in (0, 1, 2, 4, 8):
            cases.append(Case(f"ostrstream_ctor_buf mode={mode} flag={flag}", 0x615C90, STRSTREAM_CTOR_FIXTURE,
                              [f"{arg(1)}={dword(text)}", f"{arg(2)}={dword(8)}", f"{arg(3)}={dword(mode)}", f"{arg(4)}={dword(flag)}"]))
    return cases


def find_fixture(address: int) -> Path | None:
    name = f"{address:08x}_0.cap"
    for directory in sorted((ROOT / "work" / "run").glob("*/captures")):
        if (directory / name).exists():
            return directory / name
    for directory in ((MAIN / "work" / "regression" / "caps" / f"{address:08x}"),):
        for candidate in sorted(directory.glob("*_0.cap")):
            return candidate
    return None


def run_case(case: Case, integration: bool) -> bool:
    fixture = find_fixture(case.fixture)
    if fixture is None:
        print(f"SKIP {case.name}: no capture of {case.fixture:#x}")
        return True
    command = [str(DIFFTEST), "--entry", hex(case.entry)]
    if case.pokes:
        command += ["--poke", ";".join(case.pokes)]
    if integration:
        command.append("--integration")
    result = subprocess.run([*command, str(fixture)], capture_output=True, text=True)
    first = next((line for line in result.stdout.splitlines() if line.startswith(("PASS", "FAIL", "SKIP"))), "")
    if not first.startswith("PASS"):
        print(f"{first or 'NO RESULT'}  <- {case.name}\n{result.stdout[-1500:]}{result.stderr[-500:]}")
        return first.startswith("SKIP")
    return True


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--integration", action="store_true", help="replace every reimplemented function, not just the entry")
    parser.add_argument("--only", help="run only the cases whose name contains this text")
    parser.add_argument("-j", "--jobs", type=int, default=8)
    args = parser.parse_args()
    cases = [*input_cases(), *output_cases(), *streambuf_cases(), *stream_ctor_cases()]
    if args.only:
        cases = [c for c in cases if args.only in c.name]
    from concurrent.futures import ThreadPoolExecutor

    with ThreadPoolExecutor(args.jobs) as pool:
        results = list(pool.map(lambda c: run_case(c, args.integration), cases))
    print(f"{sum(results)}/{len(results)} cases passed")
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
