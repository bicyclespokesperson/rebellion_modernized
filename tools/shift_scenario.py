#!/usr/bin/env python3
"""Prints a scenario with every action at or after <from_second> moved by <delta> seconds.

  tools/shift_scenario.py tests/scenarios/long_alliance.txt 300 -180 > work/run/shifted.txt
"""
import sys
from pathlib import Path

path, start, delta = Path(sys.argv[1]), int(sys.argv[2]), int(sys.argv[3])
for line in path.read_text().splitlines():
    head, _, rest = line.partition(" ")
    if head.isdigit() and int(head) >= start:
        print(f"{int(head) + delta} {rest}")
    else:
        print(line)
