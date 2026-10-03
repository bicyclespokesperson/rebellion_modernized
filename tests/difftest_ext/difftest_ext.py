#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# dependencies = ["unicorn>=2.1", "pefile", "capstone>=5"]
# ///
"""Compatibility shim: the extensions that lived here (bump heap, game-file reads, --poke, --entry, GDI handles,
timer values, synthetic resources) are now part of tools/difftest.py. Kept so dib_variants.py works unchanged."""
from __future__ import annotations

import importlib.util
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[2] / "tools"
_spec = importlib.util.spec_from_file_location("difftest", TOOLS / "difftest.py")
d = importlib.util.module_from_spec(_spec)
sys.modules["difftest"] = d
_spec.loader.exec_module(d)

CFG = d.CFG
run_captures = d.run_captures

if __name__ == "__main__":
    sys.exit(d.main())
