#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["unicorn>=2.1", "pefile", "capstone>=5"]
# ///
"""Variant matrix for the DibBitmap / Stopwatch reconstruction.

The game only ever draws a few shapes of bitmap, so each real capture from the regression store (see
tools/capture_store.py) is replayed many times with parts of the machine state overwritten (difftest_ext --poke):
negative/zero/overlong coordinates, other bit depths and heights, RLE or unloaded sources, ... Both the original
and the reconstruction see the same modified state; only their disagreement matters. A SKIP means the original
itself faulted (it reads/writes outside the snapshot for such inputs).

  dib_variants.py [function-name-substring ...]      e.g. dib_variants.py Blit Stopwatch
"""
from __future__ import annotations

import contextlib
import io
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import difftest_ext as ext  # noqa: E402

d = ext.d
STORE = d.MAIN / "work" / "regression" / "caps"


def le(value: int, size: int = 4) -> str:
    return (value & ((1 << (8 * size)) - 1)).to_bytes(size, "little").hex()


def stack_args(**named: int) -> str:
    """esp-relative pokes: a1 is the first stack argument (esp+4), a2 the next, ..."""
    return ";".join(f"esp+{4 * int(k[1:]):#x}={le(v)}" for k, v in named.items())


def join(*parts: str) -> str:
    return ";".join(p for p in parts if p)


SRC_HEADER = "[ecx]"
DEST_HEADER = "[[esp+4]]"
GEOMETRY_CASES = [  # destX, destY, srcX, srcY, width, height  (for the 7-argument blits)
    (0, 0, 0, 0, 0, 0), (-5, 0, 0, 0, 0, 0), (0, -5, 0, 0, 0, 0), (-5, -15, 0, 0, 0, 0), (0, 0, -4, 0, 0, 0),
    (0, 0, 0, -4, 0, 0), (0, 0, -4, -4, 0, 0), (0, 0, -100, 0, 0, 0), (0, 0, 0, -100, 0, 0), (5, 0, -4, 0, 3, 0),
    (0, 5, 0, -4, 0, 3), (0, 0, 0, 0, 10, 7), (0, 0, 0, 0, 1000, 1000), (30, 20, 0, 0, 10, 7), (20, 10, 0, 0, 64, 64),
    (0, 0, 700, 0, 0, 0), (0, 0, 0, 500, 0, 0), (0, 0, 0, 480, 0, 0), (0, 0, 640, 0, 0, 0),
]
BLIT_STATE_CASES = [
    "", f"{DEST_HEADER}+0xe=1800", f"{DEST_HEADER}+0x10=01000000", "[esp+4]=00000000", f"{SRC_HEADER}+0xe=0400",
    f"{SRC_HEADER}+0xe=0100", f"{SRC_HEADER}+0xe=2000", f"{SRC_HEADER}+0xe=1800", f"{SRC_HEADER}+8=20feffff",
    f"{DEST_HEADER}+8=d0ffffff", f"{SRC_HEADER}+8=20feffff;{DEST_HEADER}+8=d0ffffff", f"{SRC_HEADER}+0x10=01000000",
    f"{SRC_HEADER}=00000000",
]


def blit_variants(prefix: str = "", keyed: bool = False) -> list[tuple[dict, str]]:
    """Variants for Blit (a1=dest, a2..a7 geometry) or BlitKeyed (a4 = colour key, geometry shifted by one)."""
    names = ["a2", "a3", "a4", "a5", "a6", "a7"] if not keyed else ["a2", "a3", "a5", "a6", "a7", "a8"]
    out = []
    for case in GEOMETRY_CASES:
        out.append(({}, join(prefix, stack_args(**dict(zip(names, case))))))
    for state in BLIT_STATE_CASES:
        out.append(({}, join(prefix, state)))
        out.append(({}, join(prefix, state, stack_args(**dict(zip(names, (3, 2, 5, 4, 20, 9)))))))
    return out


def remap_variants() -> list[tuple[dict, str]]:
    table = "ecx+0x24=@[ecx+4]"
    return ([({}, table), ({}, "ecx+0x24=00000000")] + blit_variants(table))


DRAW_TO_DC = [  # a1=dc a2=mode a3=rop a4..a9 = destX destY srcX srcY width height
    "", "ecx+0xc=10", f"ecx+0xc=10;{stack_args(a4=-16)}", f"ecx+0xc=10;{stack_args(a5=-16, a7=-5)}", "esp+8=00000000",
    f"esp+8=00000000;{stack_args(a6=-16, a7=-16)}", "esp+8=10000000", "ecx+0x20=00000000", "ecx+0x20=00000000;ecx+0xc=10",
    stack_args(a6=-1), stack_args(a7=-5), stack_args(a4=-16), stack_args(a5=-16), stack_args(a6=-16, a4=-16),
    stack_args(a8=0, a9=0), stack_args(a8=16, a9=8), stack_args(a8=16, a9=8, a6=8, a7=4), "ecx+0xc=08", "ecx+0xc=00",
]
DRAW_KEYED = [
    "", f"{SRC_HEADER}+0xe=1800", f"{SRC_HEADER}=00000000", f"{DEST_HEADER}+0xe=1800", f"{SRC_HEADER}+0x10=01000000",
    stack_args(a2=-3, a3=-3), stack_args(a2=500, a3=500), f"{DEST_HEADER}+0x10=01000000",
]
FILL_RECT = [  # a1=x a2=y a3=width a4=height a5=value
    "", stack_args(a1=10, a2=10, a3=20, a4=20, a5=5), stack_args(a1=0, a2=0, a3=1000, a4=1000, a5=7),
    stack_args(a1=60, a2=0, a3=20, a4=20, a5=9), stack_args(a1=0, a2=100, a3=20, a4=50, a5=3),
    stack_args(a1=0, a2=0, a3=0, a4=10, a5=1), stack_args(a1=0, a2=0, a3=10, a4=0, a5=1),
    stack_args(a1=-5, a2=0, a3=20, a4=20, a5=2), stack_args(a1=0, a2=-5, a3=20, a4=20, a5=2),
    stack_args(a1=66, a2=115, a3=1, a4=1, a5=255), stack_args(a1=66, a2=115, a3=5, a4=5, a5=255),
    stack_args(a1=0, a2=0, a3=-3, a4=5, a5=4), stack_args(a1=10, a2=10, a3=5, a4=-3, a5=4), f"{SRC_HEADER}+0x10=01000000",
]
APPLY_REMAP = [  # a1=x a2=y a3=width a4=height (shorts)
    "", stack_args(a1=16, a2=8, a3=32, a4=16), stack_args(a1=100, a2=100, a3=100, a4=30),
    stack_args(a1=100, a2=100, a3=30, a4=100), stack_args(a1=-16, a2=0, a3=20, a4=20), stack_args(a1=0, a2=-8, a3=20, a4=20),
    stack_args(a1=150, a2=150, a3=10, a4=10), stack_args(a1=0, a2=0, a3=0, a4=0), stack_args(a1=0, a2=0, a3=1, a4=1),
    stack_args(a1=159, a2=159, a3=1, a4=1), stack_args(a1=0, a2=0, a3=-3, a4=5), stack_args(a1=10, a2=10, a3=5, a4=-3),
    stack_args(a1=159, a2=0, a3=5, a4=5), stack_args(a1=0, a2=159, a3=5, a4=5), f"{SRC_HEADER}+0x10=01000000",
    "ecx+0x24=00000000",
]
OPAQUE = ["", f"{SRC_HEADER}+8=dcffffff;{stack_args(a1=5, a2=5)}"] + [
    stack_args(a1=x, a2=y) for x, y in
    [(0, 5), (5, 0), (39, 35), (40, 10), (10, 36), (-1, 5), (5, -1), (20, 20), (1, 1), (39, 1), (1, 35), (38, 34), (100, 100)]
]
DTOR_KINDS = ["", "ecx+0xc=02", "ecx+0xc=0a", "ecx+0xc=10", "ecx+0xc=18", "ecx+0xc=04", "ecx+0xc=01", "ecx+0xc=00",
              "ecx+0xc=10;ecx+0x1c=00000000", "ecx+0xc=02;ecx+0x14=00000000", "ecx+0x20=00000000", "[ecx]=00000000"]
BITMAP_HEADER = (le(40) + le(16) + le(16) + le(1, 2) + le(8, 2) + le(0) + le(256) + le(0) * 2 + le(0) + le(0)
                 + b"".join(bytes([i, 255 - i, i // 2, 0]) for i in range(256)).hex() + bytes(range(256)).hex())


def ctor_variants() -> list[tuple[dict, str]]:
    return [({}, ""), ({}, stack_args(a2=0)), ({}, stack_args(a3=2)), ({}, stack_args(a3=4)), ({}, stack_args(a3=9)),
            ({}, stack_args(a3=1)), ({}, stack_args(a3=0x12))]


def heap_ctor_variants() -> list[tuple[dict, str]]:
    sizes = [(1, 1), (16, 16), (63, 7), (256, 2), (5, 3), (640, 480), (0, 0), (100, 1)]
    out = [({}, join(stack_args(a1=w, a2=h))) for w, h in sizes]
    out += [({}, join(stack_args(a1=w, a2=h), "[[esp+0xc]]+0xe=1800")) for w, h in sizes[:4]]
    out += [({}, join(stack_args(a1=w, a2=h), "[[esp+0xc]]+0xe=0400")) for w, h in sizes[:4]]
    return out


def stopwatch_cases() -> list[tuple[str, dict, str]]:
    low = lambda v: le(v)
    return [
        ("5fbbe0", {"qpc": 0x1000}, f"ecx+0x10={low(0x100)}{low(0)}"),
        ("5fbbe0", {"qpc": 0x100000010}, f"ecx+0x10={low(0xfffffff0)}{low(0)}"),
        ("5fbbe0", {"qpc": 0x300000020}, f"ecx+0x10={low(0x10)}{low(0)}"),
        ("5fbbe0", {"qpc": 0x20}, f"ecx+0x10={low(0x10)}{low(2)}"),
        ("5fbbe0", {"qpc": 0x200000005}, f"ecx+0x10={low(0xa)}{low(1)}"),
        ("5fbbe0", {"qpc": 0x7fffffff00000000}, f"ecx+0x10={low(1)}{low(0xffffffff)}"),
        ("5fbbe0", {"qpc": 0x1000}, f"ecx+8={low(0)};ecx+0x10={low(0x100)}"),
        ("5fbbe0", {"time_get_time": 0x500}, f"ecx+8={low(0)};ecx+0x10={low(0x100)}"),
        ("5fbbe0", {"time_get_time": 0x10}, f"ecx+8={low(0)};ecx+0x10={low(0xfffff000)}"),
        ("5fbbe0", {"time_get_time": 0xffffffff}, f"ecx+8={low(0)};ecx+0x10={low(0)}"),
        *[("5fbb20", {"qpf": qpf, "qpc": 0x555500000042}, "") for qpf in (0x3b9aca00, 0x1c9c380, 1, 0xffffffff, 0x123456789)],
        ("5fbb20", {}, ""),
        ("5fbbb0", {"qpc": 0x555500000042}, ""),
        ("5fbbb0", {"qpc": 0x555500000042}, f"ecx+8={low(0)}"),
        ("5fbbb0", {"time_get_time": 0x1234}, f"ecx+8={low(0)}"),
    ]


def first_capture(address: str) -> Path | None:
    caps = sorted((STORE / f"{int(address, 16):08x}").glob("dib_*"))
    return caps[0] if caps else None


def run_one(cap: Path, cfg: dict, poke: str, entry: int | None = None) -> str:
    ext.CFG.__init__(**{"pokes": poke, "entry": entry, **cfg})
    sink = io.StringIO()
    with contextlib.redirect_stdout(sink):
        ext.run_captures([cap])
    text = sink.getvalue()
    status = (text.split() or ["?"])[0]
    if status == "FAIL":
        print(f"FAIL {cap.name} entry={entry and hex(entry)} {cfg} poke={poke!r}\n{text}")
    return status


def run_group(label: str, address: str, variants: list[tuple[dict, str]], entry: int | None = None) -> tuple[int, int, int]:
    cap = first_capture(address)
    if cap is None:
        print(f"{label}: no capture for {address} in {STORE}")
        return 0, 0, 0
    counts = {"PASS": 0, "SKIP": 0, "FAIL": 0}
    for cfg, poke in variants:
        counts[run_one(cap, cfg, poke, entry)] += 1
    return counts["PASS"], counts["SKIP"], counts["FAIL"]


def main() -> int:
    wanted = [a.lower() for a in sys.argv[1:]]
    gdi = {"gdi_handles": True}
    groups: list[tuple[str, str, list[tuple[dict, str]], int | None]] = [
        ("Blit", "5fcc30", blit_variants(), None),
        ("BlitKeyed", "5fc710", blit_variants(keyed=True), None),
        ("BlitRemapDestination", "5fcc30", remap_variants(), 0x5fe140),
        ("BlitRemapped (+RLE source)", "5fcc30", remap_variants(), 0x5fe3d0),
        ("BlitRemappedOverlay", "5fcc30", remap_variants(), 0x5fe640),
        ("DrawToDc", "5fc140", [(gdi, p) for p in DRAW_TO_DC], None),
        ("DrawKeyed", "5fd0f0", [({}, p) for p in DRAW_KEYED], None),
        ("FillRect", "5fcfe0", [({}, p) for p in FILL_RECT], None),
        ("ApplyRemapTable", "5fe050", [({}, p) for p in APPLY_REMAP], None),
        ("IsPixelOpaque", "5fca00", [({}, p) for p in OPAQUE], None),
        ("~DibBitmap", "5fbfa0", [({"gdi_handles": True}, p) for p in DTOR_KINDS], None),
        ("DibBitmap(file/resource name)", "5fbc80", ctor_variants(), None),
        ("DibBitmap(file/resource name) + resource", "5fbc80", [({"lock_hex": BITMAP_HEADER}, p) for _, p in ctor_variants()], None),
        ("DibBitmap(resource id)", "5fbd20", [({"lock_hex": BITMAP_HEADER}, p) for p in ("", stack_args(a2=0x12345), stack_args(a3=0x10))], None),
        ("LoadResourceById", "5fc050", [({"lock_hex": BITMAP_HEADER}, p) for p in ("", stack_args(a1=0x1ffff))], None),
        ("LoadResourceByName", "5fc050", [({"lock_hex": BITMAP_HEADER}, p) for p in ("esp+4=@esp+0x40", "esp+4=00000000", "ecx+0x18=00000000")], 0x5fc6a0),
        ("DibBitmap(w,h,format)", "5fbda0", heap_ctor_variants(), None),
        ("DibBitmap(w,h,format,dc)", "5fbea0", [(gdi, p) for _, p in heap_ctor_variants()[:8]], None),
        ("CreatePaletteFromColorTable", "5fc3f0", [(gdi, ""), ({}, "")], None),
    ]
    failures = 0
    for label, address, variants, entry in groups:
        if wanted and not any(w in label.lower() for w in wanted):
            continue
        passed, skipped, failed = run_group(label, address, variants, entry)
        failures += failed
        print(f"{label:44} PASS {passed:3} SKIP {skipped:3} FAIL {failed:3}")
    if not wanted or any(w in "stopwatch" for w in wanted):
        counts = {"PASS": 0, "SKIP": 0, "FAIL": 0}
        for address, cfg, poke in stopwatch_cases():
            cap = first_capture(address)
            if cap:
                counts[run_one(cap, cfg, poke)] += 1
        failures += counts["FAIL"]
        print(f"{'Stopwatch':44} PASS {counts['PASS']:3} SKIP {counts['SKIP']:3} FAIL {counts['FAIL']:3}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
