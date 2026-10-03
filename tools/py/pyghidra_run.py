"""Run a script against the Rebellion Ghidra project with `program` bound. Usage: ghidra_py <script.py> [args]"""
import os
import runpy
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
os.environ.setdefault("GHIDRA_INSTALL_DIR", str(ROOT / "tools" / "ghidra"))

import pyghidra

pyghidra.start(install_dir=Path(os.environ["GHIDRA_INSTALL_DIR"]))
from ghidra.base.project import GhidraProject  # noqa: E402

project = GhidraProject.openProject(str(ROOT / "ghidra_proj"), "Rebellion", False)
program = project.openProgram("/", "REBEXE.EXE", os.environ.get("GHIDRA_READONLY") == "1")
try:
    script = sys.argv[1]
    sys.argv = sys.argv[1:]
    runpy.run_path(script, init_globals={"program": program, "project": project, "ROOT": ROOT}, run_name="__main__")
finally:
    project.close()
