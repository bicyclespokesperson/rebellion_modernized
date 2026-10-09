#!/usr/bin/env python3
"""Compare Smacker dumps: <dir>/real.txt (the real SMACKW32.DLL under Wine) against ours32.txt / ours64.txt (minwin_impl/smack).
Per frame the fields number, newpal, paltype, palbefore, ret, pal, pic, audio and after must be identical; the reported rectangles
are compared separately (identical, or a superset of the DLL's). Exit status 1 on any difference."""
import re
import sys
from pathlib import Path

FIELDS = ["number", "newpal", "paltype", "palbefore", "ret", "pal", "pic", "audio", "after"]


def parse(path: Path) -> dict[str, list[dict[str, str]]]:
    movies: dict[str, list[dict[str, str]]] = {}
    current = None
    for line in path.read_text().splitlines():
        if line.startswith("== "):
            current = Path(line[3:].replace("\\", "/")).name
            movies[current] = []
        elif line.startswith("frame ") and current:
            fields = dict(re.findall(r"(\w+)=(\S*)", line))
            movies[current].append(fields)
        elif line.startswith(("movie", "buffer", "setpalette")) and current:
            movies[current].append({"header": line})
        elif line.startswith(("wait", "goto1")) and current:
            movies[current].append({"extra": line})
    return movies


def rect_of(text: str) -> list[tuple[int, int, int, int]]:
    return [tuple(int(v) for v in m.split(",")) for m in re.findall(r"\[([-\d,]+)\]", text)]  # type: ignore[misc]


def main() -> int:
    directory = Path(sys.argv[1])
    ref = parse(directory / "real.txt")
    failed = 0
    for name in sys.argv[2:] or ["ours32", "ours64"]:
        other_path = directory / f"{name}.txt"
        if not other_path.exists():
            continue
        other = parse(other_path)
        for movie, frames in ref.items():
            theirs = other.get(movie)
            if theirs is None:
                print(f"MISSING {name} {movie}")
                failed += 1
                continue
            bad = 0
            exact_rects = 0
            superset_rects = 0
            first = None
            for i, (a, b) in enumerate(zip(frames, theirs)):
                if "extra" in a:
                    if a["extra"] != b.get("extra"):
                        print(f"  differs: {a['extra']} / {b.get('extra')}")
                        bad += 1
                    continue
                if "header" in a:
                    if a["header"].split("setpalette")[0] != b["header"].split("setpalette")[0] and not a["header"].startswith("setpalette"):
                        print(f"  header differs: {a['header']} / {b['header']}")
                        bad += 1
                    continue
                diff = [f for f in FIELDS if a.get(f) != b.get(f) and not (f == "audio" and a.get(f, "").split(":")[0] == b.get(f, "").split(":")[0] == "0")]
                ra, rb = rect_of(a["rects"]), rect_of(b["rects"])
                if ra == rb:
                    exact_rects += 1
                elif len(rb) == len(ra) and all(x2 <= x1 and y2 <= y1 and x2 + w2 >= x1 + w1 and y2 + h2 >= y1 + h1
                                                 for (x1, y1, w1, h1), (x2, y2, w2, h2) in zip(ra, rb)):
                    superset_rects += 1
                else:
                    diff.append("rects")
                if diff:
                    bad += 1
                    if first is None:
                        first = (i, diff, a, b)
            n = len(frames) - 1
            status = "OK  " if bad == 0 else "FAIL"
            print(f"{status} {name} {movie}: {n} frames, {bad} differ, rects identical {exact_rects}, superset {superset_rects}")
            if first:
                i, diff, a, b = first
                print(f"     first difference at line {i}: {diff}")
                for f in diff:
                    print(f"       {f}: real {a.get(f)}  ours {b.get(f)}")
            failed += bad != 0
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
