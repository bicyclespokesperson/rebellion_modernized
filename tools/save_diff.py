#!/usr/bin/env python3
"""Compare two savegames byte for byte: sizes, number of differing bytes, and the differing runs (first 20).

  tools/save_diff.py a.005 b.005
"""
import sys
from pathlib import Path

a, b = Path(sys.argv[1]).read_bytes(), Path(sys.argv[2]).read_bytes()
print(f"sizes {len(a)} {len(b)} (delta {len(b) - len(a)})")
n = min(len(a), len(b))
runs = []
start = None
for i in range(n):
    if a[i] != b[i]:
        if start is None:
            start = i
    elif start is not None:
        runs.append((start, i))
        start = None
if start is not None:
    runs.append((start, n))
print(f"{sum(e - s for s, e in runs)} differing bytes in {len(runs)} runs within the common {n} bytes")
for s, e in runs[:20]:
    print(f"  {s:#x}..{e:#x} ({e - s}): {a[s:e][:16].hex()} vs {b[s:e][:16].hex()}")
