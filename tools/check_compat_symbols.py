#!/usr/bin/env -S uv run
"""Reject compatibility definitions that still export bare Windows API names."""
# /// script
# requires-python = ">=3.11"
# dependencies = []
# ///
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


COMMENT_PATTERN = re.compile(r"//[^\n]*|/\*.*?\*/", re.DOTALL)
NAMESPACE_PATTERN = re.compile(r"\bnamespace\s+reb::compat\s*\{")
VARIABLE_PATTERN = re.compile(r"\bextern\s+const\s+GUID\s+([A-Za-z_]\w*)\s*;")
DECLARATION_PATTERN = re.compile(r"\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*;")


def compatibility_api_names(header_dir: Path) -> set[str]:
    names: set[str] = set()
    headers = [header_dir] if header_dir.is_file() else sorted(header_dir.glob("*.h"))
    for header in headers:
        source = COMMENT_PATTERN.sub("", header.read_text(encoding="utf-8"))
        for namespace in NAMESPACE_PATTERN.finditer(source):
            depth = 1
            end = namespace.end()
            while depth and end < len(source):
                depth += (source[end] == "{") - (source[end] == "}")
                end += 1
            if depth:
                raise ValueError(f"Unclosed reb::compat namespace in {header}")
            body = source[namespace.end():end - 1]
            names.update(DECLARATION_PATTERN.findall(body))
            names.update(VARIABLE_PATTERN.findall(body))
    if not names:
        raise ValueError(f"No reb::compat declarations found in {header_dir}")
    return names


def undecorate_c_symbol(symbol: str) -> str:
    return re.sub(r"@\d+$", "", symbol.removeprefix("_"))


def conflicting_symbols(output: str, api_names: set[str]) -> set[str]:
    conflicts: set[str] = set()
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 2 or fields[1] in {"U", "w", "v"}:
            continue
        if undecorate_c_symbol(fields[0]) in api_names:
            conflicts.add(fields[0])
    return conflicts


def check_objects(nm: str, objects: list[Path], api_names: set[str]) -> list[str]:
    violations: list[str] = []
    for obj in objects:
        result = subprocess.run([nm, "-g", "-P", str(obj)], check=True, capture_output=True, text=True)
        violations.extend(f"{obj}: {symbol}" for symbol in sorted(conflicting_symbols(result.stdout, api_names)))
    return violations


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nm", required=True, help="llvm-nm or a POSIX-compatible nm executable")
    parser.add_argument("--headers", type=Path, nargs="+", required=True, help="compatibility header files or directories")
    parser.add_argument("--objects-file", type=Path, required=True, help="file containing one object path per line")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        api_names = set().union(*(compatibility_api_names(path) for path in args.headers))
        objects = [Path(line) for line in args.objects_file.read_text(encoding="utf-8").splitlines() if line]
        if not objects:
            raise ValueError(f"No objects listed in {args.objects_file}")
        violations = check_objects(args.nm, objects, api_names)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Compatibility symbol check failed: {error}", file=sys.stderr)
        if isinstance(error, subprocess.CalledProcessError) and error.stderr:
            print(error.stderr, file=sys.stderr)
        return 1
    if violations:
        print("Compatibility functions must use reb::compat C++ linkage:", file=sys.stderr)
        print("\n".join(violations), file=sys.stderr)
        return 1
    print(f"Checked {len(objects)} objects against {len(api_names)} compatibility API names")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
