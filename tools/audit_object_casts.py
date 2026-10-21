#!/usr/bin/env -S uv run
"""Inventory object-pointer casts using the native compilation database."""
# /// script
# requires-python = ">=3.11"
# dependencies = ["libclang==18.1.1"]
# ///
from __future__ import annotations

import argparse
from concurrent.futures import ProcessPoolExecutor
from dataclasses import asdict, dataclass
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys
from typing import Any

from clang import cindex

CK = cindex.CursorKind
TK = cindex.TypeKind
CASTS = {CK.CXX_REINTERPRET_CAST_EXPR, CK.CXX_STATIC_CAST_EXPR, CK.CSTYLE_CAST_EXPR}
RECORDS = {CK.CLASS_DECL, CK.STRUCT_DECL, CK.CLASS_TEMPLATE, CK.CLASS_TEMPLATE_PARTIAL_SPECIALIZATION}
POINTERS = {TK.POINTER, TK.LVALUEREFERENCE, TK.RVALUEREFERENCE}
FUNCTIONS = {CK.CXX_METHOD, CK.FUNCTION_DECL, CK.CONSTRUCTOR, CK.DESTRUCTOR, CK.FUNCTION_TEMPLATE}
GATED_CATEGORIES = {"unrelated-record", "inheritance-reinterpret", "pointer-storage-cast", "incomplete-record-review"}


@dataclass(frozen=True)
class CastSite:
    file: str
    line: int
    column: int
    scope: str
    category: str
    cast: str
    source: str
    target: str
    expression: str

    def key(self) -> str:
        identity = [self.file, self.scope, self.category, self.cast, self.source, self.target, self.expression]
        return hashlib.sha256(json.dumps(identity, separators=(",", ":")).encode()).hexdigest()[:20]


def record_type(value: cindex.Type) -> tuple[cindex.Cursor, int] | None:
    value = value.get_canonical()
    depth = 0
    while value.kind in POINTERS:
        depth += 1
        value = value.get_pointee().get_canonical()
    if depth == 0:
        return None
    declaration = value.get_declaration()
    if declaration.kind not in RECORDS:
        return None
    return declaration.get_definition() or declaration, depth


def ancestry(record: cindex.Cursor, cache: dict[str, set[str]]) -> set[str]:
    key = record.get_usr()
    if key not in cache:
        cache[key] = {key}
        for child in record.get_children():
            if child.kind == CK.CXX_BASE_SPECIFIER:
                base = child.type.get_canonical().get_declaration()
                cache[key].update(ancestry(base.get_definition() or base, cache))
    return cache[key]


def scan_translation_unit(filename: str, arguments: list[str], root: str, downcasts: bool) -> dict[str, Any]:
    translation_unit = cindex.Index.create().parse(filename, args=arguments)
    errors = [str(item) for item in translation_unit.diagnostics if item.severity >= cindex.Diagnostic.Error]
    if errors:
        return {"file": filename, "errors": errors, "sites": []}
    sites: list[dict[str, Any]] = []
    cache: dict[str, set[str]] = {}
    source_root = Path(root) / "src"

    def visit(node: cindex.Cursor, scope: str = "") -> None:
        location = node.location
        if location.file and not Path(location.file.name).is_relative_to(source_root):
            return
        if node.kind in FUNCTIONS:
            parent = node.semantic_parent
            scope = f"{parent.spelling}::{node.displayname}" if parent and parent.spelling else node.displayname
        if node.kind in CASTS and location.file:
            children = [child for child in node.get_children() if child.kind.is_expression()]
            if children:
                operand = children[-1]
                source_info = record_type(operand.type)
                target_info = record_type(node.type)
                if source_info and target_info and (source_info[0].get_usr() != target_info[0].get_usr()
                                                     or source_info[1] != target_info[1]):
                    source, source_depth = source_info
                    target, target_depth = target_info
                    source_bases = ancestry(source, cache)
                    target_bases = ancestry(target, cache)
                    category = ""
                    if source_depth != target_depth or max(source_depth, target_depth) > 1:
                        category = "pointer-storage-cast"
                    elif target.get_usr() in source_bases:
                        if node.kind == CK.CXX_REINTERPRET_CAST_EXPR:
                            category = "inheritance-reinterpret"
                    elif source.get_usr() in target_bases:
                        if node.kind == CK.CXX_REINTERPRET_CAST_EXPR:
                            category = "inheritance-reinterpret"
                        elif downcasts:
                            category = "downcast-review"
                    elif not source.is_definition() or not target.is_definition():
                        category = "incomplete-record-review"
                    else:
                        category = "unrelated-record"
                    if category:
                        expression = " ".join(token.spelling for token in node.get_tokens())
                        site = CastSite(str(Path(location.file.name).relative_to(root)), location.line,
                                        location.column, scope.replace(root, "."), category, node.kind.name,
                                        operand.type.get_canonical().spelling.replace(root, "."),
                                        node.type.get_canonical().spelling.replace(root, "."),
                                        expression.replace(root, "."))
                        sites.append(asdict(site))
        for child in node.get_children():
            visit(child, scope)

    for child in translation_unit.cursor.get_children():
        visit(child)
    return {"file": filename, "errors": [], "sites": sites}


def compile_arguments(entry: dict[str, Any], extra_arguments: list[str]) -> list[str]:
    arguments = entry.get("arguments") or shlex.split(entry["command"])
    result: list[str] = []
    skip = False
    for argument in arguments[1:]:
        if skip:
            skip = False
        elif argument in {"-o", "-MF", "-MT", "-MQ"}:
            skip = True
        elif argument not in {"-c", "-MD", "-MMD", entry["file"]} and not (
                argument.endswith(".cpp") and (Path(entry["directory"]) / argument).resolve() == Path(entry["file"])):
            result.append(argument)
    return result + ["-working-directory", entry["directory"]] + extra_arguments


def host_arguments() -> list[str]:
    if sys.platform != "darwin":
        return []
    sdk = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
    resource = subprocess.check_output(["xcrun", "clang++", "-print-resource-dir"], text=True).strip()
    return ["-isysroot", sdk, "-resource-dir", resource]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=Path("build/native"))
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--file", action="append", default=[], help="scan matching source paths only")
    parser.add_argument("--include-downcasts", action="store_true", help="also inventory related static/C-style downcasts")
    parser.add_argument("--json", type=Path, help="write the full inventory, including parse errors")
    parser.add_argument("--baseline", type=Path, default=Path(__file__).with_name("object_cast_exceptions.json"),
                        help="reviewed exceptions: JSON object mapping stable keys to reasons")
    parser.add_argument("--check", action="store_true", help="fail for unreviewed object casts or stale exceptions")
    parser.add_argument("-j", "--jobs", type=int, default=min(8, os.cpu_count() or 1))
    options = parser.parse_args()
    if options.jobs < 1:
        parser.error("--jobs must be positive")
    root = options.root.resolve()
    entries = json.loads((options.build / "compile_commands.json").read_text())
    for entry in entries:
        entry["directory"] = str(Path(entry["directory"]).resolve())
        entry["file"] = str((Path(entry["directory"]) / entry["file"]).resolve())
    sources = {entry["file"]: entry for entry in entries if Path(entry["file"]).is_relative_to(root / "src")
               and entry["file"].endswith(".cpp") and (not options.file or any(part in entry["file"] for part in options.file))}
    if not sources:
        parser.error("no matching native source files")
    extra = host_arguments()
    with ProcessPoolExecutor(max_workers=options.jobs) as executor:
        futures = [executor.submit(scan_translation_unit, entry["file"], compile_arguments(entry, extra),
                                   str(root), options.include_downcasts) for entry in sources.values()]
        batches = [future.result() for future in futures]
    sites = sorted({CastSite(**site) for batch in batches for site in batch["sites"]},
                   key=lambda site: (site.file, site.line, site.column, site.category))
    errors = [batch for batch in batches if batch["errors"]]
    baseline = json.loads(options.baseline.read_text()) if options.baseline else {}
    if not isinstance(baseline, dict) or any(not isinstance(reason, str) or not reason.strip() for reason in baseline.values()):
        parser.error("baseline must map cast keys to nonempty review reasons")
    unreviewed = [site for site in sites if site.category in GATED_CATEGORIES
                  and site.key() not in baseline]
    stale = sorted(set(baseline) - {site.key() for site in sites})
    report = {"translation_units": len(sources), "sites": [dict(asdict(site), key=site.key()) for site in sites],
              "parse_errors": errors, "unreviewed": len(unreviewed), "stale_baseline": stale}
    if options.json:
        options.json.parent.mkdir(parents=True, exist_ok=True)
        options.json.write_text(json.dumps(report, indent=2) + "\n")
    for site in unreviewed if options.check else sites:
        print(f"{site.file}:{site.line}:{site.column}: {site.category}: {site.source} -> {site.target} [{site.key()}]")
        print(f"  {site.expression}")
    for batch in errors:
        print(f"parse failed: {batch['file']}", file=sys.stderr)
        for error in batch["errors"]:
            print(f"  {error}", file=sys.stderr)
    print(f"cast audit: {len(sites)} sites, {len(unreviewed)} unreviewed, {len(errors)} parse failures, "
          f"{len(stale)} stale exceptions ({len(sources)} translation units)")
    if errors:
        return 2
    return int(options.check and (bool(unreviewed) or bool(stale)))


if __name__ == "__main__":
    raise SystemExit(main())
