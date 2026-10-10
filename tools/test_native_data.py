#!/usr/bin/env -S uv run
"""Regression checks for native global layout parsing."""
# /// script
# dependencies = ["pefile", "libclang==18.1.1"]
# ///
from __future__ import annotations

import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from gen_native_data import compiler_system_includes, parse_unit
from native_symbols import llvm_tool


class NativeLayoutTests(unittest.TestCase):
    def test_missing_header_rejects_partial_layout(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "broken.cpp"
            source.write_text('#include "missing_native_header.h"\nextern int g_value;\n')
            with self.assertRaisesRegex(RuntimeError, "missing_native_header.h"):
                parse_unit(([str(source)], ["g_value"], self.resource_dir()))

    @unittest.skipUnless(sys.platform == "darwin", "Xcode C++ header discovery")
    def test_mac_cpp_headers_produce_complete_object_layout(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "layout.cpp"
            source.write_text('#include <algorithm>\n#include <cstdint>\n'
                              'struct Handler { virtual ~Handler(); uint64_t values[17]; };\n'
                              'extern Handler g_handler;\n')
            args = ["-std=c++23", *compiler_system_includes(llvm_tool("clang++")), str(source)]
            info = parse_unit((args, ["g_handler"], self.resource_dir()))["g_handler"]
            self.assertEqual(info["size"], 144)
            self.assertEqual(info["align"], 8)
            self.assertTrue(info["pointer_like"])

    @staticmethod
    def resource_dir() -> str:
        return subprocess.check_output([llvm_tool("clang"), "-print-resource-dir"], text=True).strip()


if __name__ == "__main__":
    unittest.main()
