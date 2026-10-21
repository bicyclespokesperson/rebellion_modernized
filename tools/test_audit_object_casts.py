#!/usr/bin/env -S uv run
"""Regression tests for the native object-cast audit (no game assets required)."""
# /// script
# requires-python = ">=3.11"
# dependencies = ["libclang==18.1.1"]
# ///
from __future__ import annotations

import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from audit_object_casts import CastSite, compile_arguments, scan_translation_unit


class ObjectCastAuditTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name).resolve()
        (self.root / "src").mkdir()
        self.source = self.root / "src" / "casts.cpp"

    def scan(self, code: str, downcasts: bool = False) -> list[CastSite]:
        self.source.write_text(code)
        result = scan_translation_unit(str(self.source), ["-std=c++23"], str(self.root), downcasts)
        self.assertEqual(result["errors"], [])
        return [CastSite(**site) for site in result["sites"]]

    def test_sibling_this_cast(self) -> None:
        sites = self.scan("""
            struct Base { virtual ~Base() = default; };
            struct A : Base { int value; };
            struct B : Base { int get() { return reinterpret_cast<A*>(this)->value; } };
        """)
        self.assertEqual([(site.source, site.target, site.category) for site in sites],
                         [("B *", "A *", "unrelated-record")])

    def test_multiple_inheritance_requires_pointer_adjustment(self) -> None:
        sites = self.scan("""
            struct First { int first; }; struct Second { int second; };
            struct Both : First, Second {};
            Second* wrong(Both* value) { return reinterpret_cast<Second*>(value); }
            Second* correct(Both* value) { return static_cast<Second*>(value); }
        """)
        self.assertEqual(len(sites), 1)
        self.assertEqual(sites[0].category, "inheritance-reinterpret")

    def test_pointer_output_alias_and_nested_cast(self) -> None:
        sites = self.scan("""
            struct Base {}; struct Derived : Base {}; struct Other {};
            Base** output(Derived** value) { return reinterpret_cast<Base**>(value); }
            Other** indirect(Other* value) { return reinterpret_cast<Other**>(value); }
            Other* nested(Base* value) {
              return static_cast<Other*>(reinterpret_cast<Other*>(value));
            }
        """)
        self.assertEqual(sorted(site.category for site in sites),
                         ["pointer-storage-cast", "pointer-storage-cast", "unrelated-record"])

    def test_downcasts_are_optional_review_items(self) -> None:
        code = "struct Base {}; struct Derived : Base {}; Derived* f(Base* p) { return static_cast<Derived*>(p); }"
        self.assertEqual(self.scan(code), [])
        self.assertEqual(self.scan(code, True)[0].category, "downcast-review")

    def test_opaque_types_require_review_but_raw_bytes_are_outside_scope(self) -> None:
        sites = self.scan("""
            struct Handle; struct Object {};
            Handle* handle(Object* p) { return reinterpret_cast<Handle*>(p); }
            void* erase(Object* p) { return static_cast<void*>(p); }
            char* bytes(Object* p) { return reinterpret_cast<char*>(p); }
        """)
        self.assertEqual([site.category for site in sites], ["incomplete-record-review"])

    def test_site_keys_survive_line_changes_and_checkout_paths(self) -> None:
        code = "struct A {}; struct B {}; B* f(A* p) { return reinterpret_cast<B*>(p); }"
        original = self.scan(code)[0]
        shifted = self.scan("\n\n" + code)[0]
        self.assertNotEqual(original.line, shifted.line)
        self.assertEqual(original.key(), shifted.key())
        anonymous = self.scan("struct {} a; struct B {}; B* f() { return reinterpret_cast<B*>(&a); }")[0]
        self.assertNotIn(str(self.root), anonymous.source)

    def test_compilation_database_arguments(self) -> None:
        entry = {"directory": "/checkout", "file": "/checkout/src/a.cpp",
                 "arguments": ["clang++", "-Iinclude", "-std=c++23", "-MD", "-MF", "a.d",
                               "-c", "/checkout/src/a.cpp", "-o", "a.o"]}
        self.assertEqual(compile_arguments(entry, ["-DTEST"]),
                         ["-Iinclude", "-std=c++23", "-working-directory", "/checkout", "-DTEST"])

    def check(self, baseline: dict[str, str]) -> subprocess.CompletedProcess[str]:
        (self.root / "compile_commands.json").write_text(json.dumps([
            {"directory": str(self.root), "file": str(self.source),
             "arguments": ["clang++", "-std=c++23", "-c", str(self.source)]}]))
        exceptions = self.root / "exceptions.json"
        exceptions.write_text(json.dumps(baseline))
        return subprocess.run([sys.executable, str(Path(__file__).with_name("audit_object_casts.py")),
                               "--root", str(self.root), "--build", str(self.root),
                               "--baseline", str(exceptions), "--check", "-j", "1"],
                              capture_output=True, text=True, check=False)

    def test_gate_rejects_unreviewed_and_stale_exceptions(self) -> None:
        site = self.scan("struct A {}; struct B {}; B* f(A* p) { return reinterpret_cast<B*>(p); }")[0]
        self.assertEqual(self.check({}).returncode, 1)
        self.assertEqual(self.check({site.key(): "Fixture verifies exact exception matching."}).returncode, 0)
        self.assertEqual(self.check({site.key(): "Reviewed", "stale": "Removed cast"}).returncode, 1)

    def test_parse_errors_fail_closed(self) -> None:
        self.source.write_text("struct Broken { invalid syntax;")
        result = self.check({})
        self.assertEqual(result.returncode, 2, result.stdout + result.stderr)
        self.assertIn("parse failed", result.stderr)


if __name__ == "__main__":
    unittest.main()
