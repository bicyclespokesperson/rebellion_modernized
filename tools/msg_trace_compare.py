#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.10"
# ///
"""Compare two REB_MSG_TRACE logs (src/port/msg_trace.h): the standalone with the real user32 (REB_MSG_TRACE_SHIM) against the
standalone on minwin_impl, run through the same scenario.

Both logs hold one line per window-procedure entry: "<depth> <class> #<serial> <MESSAGE> <wParam> <lParam>". Values that differ by
construction are normalised first (device contexts, window handles of not-yet-seen windows). Message kinds whose number depends on
timing or on the host window system are dropped from the comparison ("noise", listed in the summary with their counts on both
sides); `--keep` brings a kind back, `--drop` adds one. Consecutive identical lines collapse into one.

  tools/msg_trace_compare.py work/run/ref_smoke/msgtrace.log work/run/impl_smoke/msgtrace.log
"""
from __future__ import annotations

import argparse
import difflib
import re
from collections import Counter
from pathlib import Path

DEFAULT_NOISE = {
    "WM_PAINT": "paint count depends on timing and on how much each build invalidates",
    "WM_ERASEBKGND": "follows the paint count",
    "WM_NCPAINT": "follows the paint count",
    "WM_TIMER": "timing",
    "0x7f": "WM_GETICON: sent by Wine's shell integration",
    "0x7c": "WM_STYLECHANGING: Wine-internal SetWindowLong on activation",
    "0x7d": "WM_STYLECHANGED: Wine-internal SetWindowLong on activation",
    "0x4e": "WM_NOTIFY: the real tooltip control sends TTN_* notifications; minwin_impl's tooltip is a stub",
    "0x310": "WM_PALETTEISCHANGING: when the palette is first realized is up to the GDI component",
    "0x281": "WM_IME_SETCONTEXT: Wine's IME integration",
    "0x282": "WM_IME_NOTIFY: Wine's IME integration",
}

LINE = re.compile(r"^(?P<depth>\d+) (?P<cls>\S+) #(?P<serial>\d+) (?P<msg>\S+) (?P<w>\S+) (?P<l>\S+)(?: @[0-9a-f]+)?$")


def parse(path: Path) -> list[tuple[str, str]]:
    """Returns (normalised line, message name) pairs."""
    entries = []
    for raw in path.read_text(errors="replace").splitlines():
        m = LINE.match(raw.strip())
        if not m:
            continue
        msg, w, l = m["msg"], m["w"], m["l"]
        if msg == "WM_ERASEBKGND":
            w = "hdc"
        elif msg == "0x55":
            w = "hwnd"
        elif msg == "WM_NCPAINT":
            w = "1"
        entries.append((f"{m['depth']} {m['cls']} #{m['serial']} {msg} {w} {l}", msg))
    return entries


def filtered(entries: list[tuple[str, str]], dropped: set[str]) -> tuple[list[str], Counter]:
    kept: list[str] = []
    counts: Counter = Counter()
    for line, msg in entries:
        if msg in dropped:
            counts[msg] += 1
            continue
        if kept and kept[-1] == line:
            continue
        kept.append(line)
    return kept, counts


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("reference", type=Path, help="log of the real-user32 build")
    parser.add_argument("candidate", type=Path, help="log of the minwin_impl build")
    parser.add_argument("--drop", action="append", default=[], help="additional message name to ignore (e.g. WM_MOUSEMOVE)")
    parser.add_argument("--keep", action="append", default=[], help="message name to compare although it is noise by default")
    parser.add_argument("--max-hunks", type=int, default=40)
    parser.add_argument("--context", type=int, default=2)
    args = parser.parse_args()

    dropped = (set(DEFAULT_NOISE) | set(args.drop)) - set(args.keep)
    ref_all, cand_all = parse(args.reference), parse(args.candidate)
    ref, ref_noise = filtered(ref_all, dropped)
    cand, cand_noise = filtered(cand_all, dropped)

    matcher = difflib.SequenceMatcher(None, ref, cand, autojunk=False)
    same = sum(block.size for block in matcher.get_matching_blocks())
    print(f"reference {args.reference}: {len(ref_all)} entries, {len(ref)} compared")
    print(f"candidate {args.candidate}: {len(cand_all)} entries, {len(cand)} compared")
    print(f"identical lines in order: {same} ({100.0 * same / max(len(ref), len(cand), 1):.1f}% of the longer log)")
    print("ignored message kinds (reference / candidate):")
    for name in sorted(set(ref_noise) | set(cand_noise)):
        why = DEFAULT_NOISE.get(name, "requested")
        print(f"  {name:16} {ref_noise[name]:6} / {cand_noise[name]:6}  {why}")

    hunks = 0
    for tag, i1, i2, j1, j2 in matcher.get_opcodes():
        if tag == "equal":
            continue
        hunks += 1
        if hunks > args.max_hunks:
            continue
        print(f"\n--- divergence {hunks}: reference lines {i1 + 1}-{i2}, candidate lines {j1 + 1}-{j2}")
        for line in ref[max(0, i1 - args.context):i1]:
            print(f"   {line}")
        for line in ref[i1:i2]:
            print(f" - {line}")
        for line in cand[j1:j2]:
            print(f" + {line}")
        for line in ref[i2:i2 + args.context]:
            print(f"   {line}")
    print(f"\n{hunks} divergent hunks" + (f" ({hunks - args.max_hunks} not shown)" if hunks > args.max_hunks else ""))


if __name__ == "__main__":
    main()
