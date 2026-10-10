"""LLVM tools and symbol spelling shared by the native data generators."""
from __future__ import annotations

import shutil
import subprocess
import sys


def llvm_tool(name: str) -> str:
    for candidate in (f"{name}-19", name):
        if path := shutil.which(candidate):
            return path
    if sys.platform == "darwin":
        return subprocess.check_output(["xcrun", "--find", name], text=True).strip()
    raise FileNotFoundError(f"Install LLVM tools: {name} is missing")


def source_symbol(name: str) -> str:
    return name.removeprefix("_") if sys.platform == "darwin" else name


def linker_symbol(name: str) -> str:
    return f"_{name}" if sys.platform == "darwin" else name
