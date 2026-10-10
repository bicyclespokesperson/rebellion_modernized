#!/usr/bin/env python3
"""Find argument lists in src/game whose evaluation order matters.

The order in which the arguments of a call are evaluated is unspecified in C++. MSVC (and clang-cl, which builds the i686 binaries) evaluates
them right to left; clang for the Itanium ABI (the native Linux/macOS build) left to right. When two or more arguments contain calls (or
increments/assignments) and any of them has a side effect, the two builds behave differently: random draws, stream reads, iterator cursors,
id allocation, counters.

A call is *read-only* when its body is known (in any translation unit) and neither writes anything but its own locals nor calls anything that
isn't read-only (virtual calls, function pointers, allocation and functions without a body are never read-only). A site whose calls are all
read-only does not depend on the order and is not reported. Every other site is either rewritten (hoist the calls into named locals in
right-to-left order, then call) or reviewed and listed in tools/arg_order_allow.txt with the reason.

Runs in the rebellion-native container (clang.cindex) over a compile_commands.json of the game (tools/check_arg_order.sh does it end to end).
  python3 tools/find_arg_order.py build/native [--check] [--detail] [--changed-since REV]
    (default)   print every unclassified site with its calls
    --check     one verdict line; exit status 1 when there are unclassified sites (used by tools/integrate.sh)
    --all       also print the allowlisted and the read-only sites
"""
from __future__ import annotations

import argparse
import hashlib
import multiprocessing
import re
import sys
from ctypes import POINTER, byref, c_uint
from pathlib import Path

from clang import cindex

CK = cindex.CursorKind
FUNCTION_KINDS = {CK.FUNCTION_DECL, CK.CXX_METHOD, CK.CONSTRUCTOR, CK.DESTRUCTOR, CK.CONVERSION_FUNCTION}
ASSIGN_OPERATORS = {"=", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="}
READ_ONLY_EXTERNALS = {
    "strlen", "strcmp", "strncmp", "_stricmp", "_strnicmp", "stricmp", "strchr", "strrchr", "strstr", "memcmp", "abs", "labs", "toupper",
    "tolower", "isdigit", "isalpha", "isalnum", "isspace", "isupper", "islower", "sqrt", "sin", "cos", "atan2", "floor", "ceil", "fabs", "pow",
    "min", "max",
}
ALLOWLIST = Path(__file__).with_name("arg_order_allow.txt")
sys.setrecursionlimit(20000)


def overridden_usrs(method: "cindex.Cursor") -> list[str]:
    lib = cindex.conf.lib
    lib.clang_getOverriddenCursors.argtypes = [cindex.Cursor, POINTER(POINTER(cindex.Cursor)), POINTER(c_uint)]
    lib.clang_getOverriddenCursors.restype = None
    lib.clang_disposeOverriddenCursors.argtypes = [POINTER(cindex.Cursor)]
    lib.clang_disposeOverriddenCursors.restype = None
    cursors = POINTER(cindex.Cursor)()
    count = c_uint()
    lib.clang_getOverriddenCursors(method, byref(cursors), byref(count))
    found = [cursors[i].get_usr() for i in range(count.value)] if cursors else []
    if cursors:
        lib.clang_disposeOverriddenCursors(cursors)
    return found


def scope_name(function: "cindex.Cursor | None") -> str:
    parent = function.semantic_parent if function is not None else None
    if parent is not None and parent.kind in (CK.CLASS_DECL, CK.STRUCT_DECL, CK.NAMESPACE, CK.CLASS_TEMPLATE):
        return parent.spelling
    return ""


def usr(cursor: "cindex.Cursor") -> str:
    return cursor.get_usr() or f"{cursor.spelling}@{cursor.location.file}:{cursor.location.line}"


def strip(node: "cindex.Cursor") -> "cindex.Cursor":
    while node.kind in (CK.PAREN_EXPR, CK.UNEXPOSED_EXPR, CK.CSTYLE_CAST_EXPR) and len(list(node.get_children())) >= 1:
        children = list(node.get_children())
        node = children[-1] if node.kind == CK.CSTYLE_CAST_EXPR else children[0]
    return node


def is_local_target(node: "cindex.Cursor", in_constructor: bool) -> bool:
    """True when assigning to this expression only changes a local variable (or, in a constructor, the object being built)."""
    node = strip(node)
    if node.kind == CK.DECL_REF_EXPR:
        d = node.referenced
        if d is None or d.kind not in (CK.VAR_DECL, CK.PARM_DECL):
            return False
        if d.type.get_canonical().kind in (cindex.TypeKind.LVALUEREFERENCE, cindex.TypeKind.RVALUEREFERENCE):
            return False
        return d.kind == CK.PARM_DECL or (is_automatic(d) and d.storage_class in (cindex.StorageClass.NONE, cindex.StorageClass.AUTO))
    if node.kind == CK.ARRAY_SUBSCRIPT_EXPR:
        base = strip(next(iter(node.get_children())))
        return base.type.get_canonical().kind in (cindex.TypeKind.CONSTANTARRAY, cindex.TypeKind.INCOMPLETEARRAY) and is_local_target(base, in_constructor)
    if node.kind == CK.MEMBER_REF_EXPR:
        children = list(node.get_children())
        if not children:
            return in_constructor
        base = strip(children[0])
        if base.kind == CK.CXX_THIS_EXPR:
            return in_constructor
        if base.type.get_canonical().kind == cindex.TypeKind.POINTER:
            return False
        return is_local_target(base, in_constructor)
    return False


def is_automatic(var: "cindex.Cursor") -> bool:
    parent = var.semantic_parent
    return parent is not None and parent.kind in FUNCTION_KINDS


def assignment_target(node: "cindex.Cursor") -> "cindex.Cursor | None":
    """The assigned expression when this node is an assignment, increment or decrement (as written, not overloaded)."""
    if node.kind in (CK.BINARY_OPERATOR, CK.COMPOUND_ASSIGNMENT_OPERATOR):
        op = node.binary_operator.name if hasattr(node, "binary_operator") else ""
        text = operator_token(node)
        if text in ASSIGN_OPERATORS or str(op).endswith("Assign"):
            return next(iter(node.get_children()), None)
    if node.kind == CK.UNARY_OPERATOR:
        text = operator_token(node)
        if text in ("++", "--"):
            return next(iter(node.get_children()), None)
    return None


def operator_token(node: "cindex.Cursor") -> str:
    children = list(node.get_children())
    if not children:
        return ""
    first_end = children[0].extent.end.offset
    last_start = children[-1].extent.start.offset if len(children) > 1 else None
    for t in node.get_tokens():
        if t.kind != cindex.TokenKind.PUNCTUATION:
            continue
        o = t.extent.start.offset
        if node.kind == CK.UNARY_OPERATOR:
            if t.spelling in ("++", "--"):
                return t.spelling
        elif o >= first_end and (last_start is None or o < last_start):
            return t.spelling
    return ""


class Visitor:
    """One translation unit: function facts (for the read-only fixpoint) and candidate sites."""

    def __init__(self, repo: str) -> None:
        self.repo = repo
        self.facts: dict[str, tuple[bool, frozenset[str], str]] = {}
        self.sites: list[dict] = []
        self.reads: list[dict] = []
        self.stack: list["cindex.Cursor"] = []
        self.overrides: dict[str, set[str]] = {}

    def mine(self, c: "cindex.Cursor") -> bool:
        f = c.location.file
        return f is not None and f.name.startswith(self.repo + "/src/game/")

    def top(self, tu: "cindex.TranslationUnit") -> None:
        for c in tu.cursor.get_children():
            self.decl(c)

    def decl(self, c: "cindex.Cursor") -> None:
        if not self.mine(c):
            return
        if c.kind in FUNCTION_KINDS and c.is_definition():
            self.function(c)
        elif c.kind == CK.VAR_DECL:
            self.stack.append(c)
            self.walk(c, {"impure": False, "callees": set()}, False)
            self.stack.pop()
        elif c.kind == CK.CXX_METHOD and c.is_pure_virtual_method():
            self.facts.setdefault(usr(c), (False, frozenset(), c.spelling))
            self.overrides.setdefault(usr(c), set()).update(overridden_usrs(c))
        elif c.kind in (CK.NAMESPACE, CK.CLASS_DECL, CK.STRUCT_DECL, CK.CLASS_TEMPLATE, CK.UNION_DECL, CK.LINKAGE_SPEC):
            for k in c.get_children():
                self.decl(k)
        elif c.kind == CK.FUNCTION_TEMPLATE and c.is_definition():
            self.function(c)

    def function(self, fn: "cindex.Cursor") -> None:
        key = usr(fn)
        constructor = fn.kind in (CK.CONSTRUCTOR, CK.DESTRUCTOR)
        state = {"impure": False, "callees": set()}
        self.stack.append(fn)
        self.walk(fn, state, constructor)
        self.stack.pop()
        if fn.kind == CK.CXX_METHOD and fn.is_virtual_method():
            self.overrides.setdefault(key, set()).update(overridden_usrs(fn))
        if key not in self.facts or state["impure"] and not self.facts[key][0]:
            self.facts[key] = (state["impure"], frozenset(state["callees"]), fn.spelling)

    def call_info(self, c: "cindex.Cursor", state: dict) -> tuple[str, str]:
        """(key, label) of a call node: key is the callee USR, '!' + reason when it can never be read-only."""
        ref = c.referenced
        if c.kind == CK.CXX_NEW_EXPR:
            return "!new", "new"
        if ref is None:
            return "!indirect", c.spelling or "indirect"
        if ref.kind == CK.CONSTRUCTOR and c.type.get_canonical().is_pod():
            return "=trivial", ref.spelling
        if ref.is_virtual_method():
            return usr(ref), ref.spelling
        if ref.spelling in READ_ONLY_EXTERNALS:
            return "=known", ref.spelling
        return usr(ref), ref.spelling

    def walk(self, node: "cindex.Cursor", state: dict, constructor: bool) -> None:
        for c in node.get_children():
            if c.kind in (CK.CALL_EXPR, CK.CXX_NEW_EXPR):
                key, _ = self.call_info(c, state)
                if key.startswith("!"):
                    state["impure"] = True
                elif not key.startswith("="):
                    state["callees"].add(key)
                if c.kind == CK.CALL_EXPR:
                    self.check_site(c)
            elif c.kind == CK.CXX_DELETE_EXPR or c.kind == CK.ASM_STMT:
                state["impure"] = True
            else:
                target = assignment_target(c)
                if target is not None and not is_local_target(target, constructor):
                    state["impure"] = True
            self.walk(c, state, constructor)

    def calls_in(self, node: "cindex.Cursor") -> list[tuple[str, str]]:
        """Calls evaluated when the argument is: lambda bodies and unevaluated operands are not entered."""
        found = []
        pending = [node]
        while pending:
            c = pending.pop()
            if c.kind in (CK.LAMBDA_EXPR, CK.CXX_TYPEID_EXPR):
                continue
            if c.kind in (CK.CALL_EXPR, CK.CXX_NEW_EXPR):
                found.append(self.call_info(c, {}))
            else:
                target = assignment_target(c)
                if target is not None:
                    found.append(("!assign", operator_token(c)))
            pending.extend(c.get_children())
        return found

    def check_site(self, call: "cindex.Cursor") -> None:
        if not self.mine(call):
            return
        args = list(call.get_arguments())
        busy = [(a, self.calls_in(a)) for a in args]
        busy = [(a, calls) for a, calls in busy if calls]
        if len(busy) == 1:
            if call.spelling.startswith("operator") and call.spelling.rstrip("=") != call.spelling and call.spelling not in ("operator==", "operator!=", "operator<=", "operator>="):
                return
            readers = [a for a in args if a is not busy[0][0] and self.reads_memory(a)]
            if readers:
                self.reads.append(self.site(call, [[(k, label) for k, label in busy[0][1]]]))
            return
        if len(busy) < 2:
            return
        self.sites.append(self.site(call, [[(k, label) for k, label in calls] for _, calls in busy]))

    def reads_memory(self, node: "cindex.Cursor") -> bool:
        """An argument that reads a variable another argument's call might change: non-const globals, members, dereferences."""
        for c in node.walk_preorder():
            if c.kind == CK.MEMBER_REF_EXPR and not c.type.is_const_qualified():
                return True
            if c.kind == CK.DECL_REF_EXPR and c.referenced is not None and c.referenced.kind == CK.VAR_DECL and \
                    c.referenced.storage_class != cindex.StorageClass.NONE and not c.type.is_const_qualified():
                return True
            if c.kind == CK.UNARY_OPERATOR and operator_token(c) == "*":
                return True
        return False

    def site(self, call: "cindex.Cursor", arg_calls: list) -> dict:
        enclosing = self.stack[-1] if self.stack else None
        text = " ".join(t.spelling for t in call.get_tokens())
        return ({
            "file": call.location.file.name[len(self.repo) + 1:], "line": call.location.line, "callee": call.spelling,
            "function": enclosing.spelling if enclosing else "?", "klass": scope_name(enclosing),
            "hash": hashlib.sha1(re.sub(r"\s+", " ", text).encode()).hexdigest()[:10],
            "args": arg_calls,
        })


def scan(job: tuple[str, list[str], str, str]):
    filename, arguments, resource, repo = job
    index = cindex.Index.create()
    tu = index.parse(None, args=arguments + ["-resource-dir", resource, "-Wno-everything"])
    visitor = Visitor(repo)
    visitor.top(tu)
    return visitor.facts, visitor.sites, visitor.overrides, visitor.reads


def compile_jobs(build: Path, resource: str, repo: str, only: set[str] | None) -> list[tuple[str, list[str], str, str]]:
    db = cindex.CompilationDatabase.fromDirectory(str(build))
    jobs = []
    for cmd in db.getAllCompileCommands():
        if "src/game/" not in cmd.filename or (only is not None and cmd.filename not in only):
            continue
        keep: list[str] = []
        skip = 0
        for a in list(cmd.arguments)[1:]:
            if skip:
                skip -= 1
            elif a in ("-o", "-MT", "-MF"):
                skip = 1
            elif a not in ("-c", "-MD") and a != cmd.filename:
                keep.append(a)
        jobs.append((cmd.filename, keep + [cmd.filename], resource, repo))
    return jobs


def read_only_set(facts: dict[str, tuple[bool, frozenset[str], str]], overrides: dict[str, set[str]]) -> set[str]:
    """Functions that are read-only: greatest fixpoint over 'known body, no impure operation, only read-only callees'."""
    callees = {k: set(v[1]) for k, v in facts.items()}
    for derived, bases in overrides.items():
        for base in bases:
            callees.setdefault(base, set()).add(derived)
    good = {k for k, (impure, _, _) in facts.items() if not impure and k in callees}
    changed = True
    while changed:
        changed = False
        for k in list(good):
            if any(c not in good for c in callees[k]):
                good.discard(k)
                changed = True
    return good


def load_allowlist() -> dict[str, str]:
    allowed = {}
    if ALLOWLIST.exists():
        for line in ALLOWLIST.read_text().splitlines():
            if line.strip() and not line.startswith("#"):
                key, _, reason = line.partition("  ")
                allowed[key.strip()] = reason.strip()
    return allowed


def site_key(site: dict) -> str:
    return f"{site['file']}:{site['klass']}::{site['function']}:{site['hash']}"


def describe(site: dict, good: set[str]) -> str:
    parts = []
    for calls in site["args"]:
        parts.append(",".join(("" if k in good or k.startswith("=") else "!") + label for k, label in calls))
    return f"{site['file']}:{site['line']}: {site['klass']}::{site['function']}: {site['callee']}( " + " | ".join(parts) + " )"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("build", type=Path)
    ap.add_argument("--repo", type=Path, default=Path.cwd())
    ap.add_argument("--resource-dir", default="/usr/lib/llvm-19/lib/clang/19")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--reads", action="store_true", help="also list argument lists with one side-effecting call whose siblings read non-const memory")
    args = ap.parse_args()
    repo = str(args.repo.resolve())
    jobs = compile_jobs(args.build, args.resource_dir, repo, None)
    with multiprocessing.Pool() as pool:
        results = pool.map(scan, jobs, chunksize=4)
    facts: dict[str, tuple[bool, frozenset[str], str]] = {}
    overrides: dict[str, set[str]] = {}
    for f, _, o, _ in results:
        for k, v in f.items():
            if k not in facts or v[0] and not facts[k][0]:
                facts[k] = v
        for k, v in o.items():
            overrides.setdefault(k, set()).update(v)
    good = read_only_set(facts, overrides)
    allowed = load_allowlist()
    seen: set[tuple[str, int]] = set()
    pending, listed, readonly = [], [], []
    for _, sites, _, _ in results:
        for s in sites:
            if (s["file"], s["line"], s["callee"]) in seen:
                continue
            seen.add((s["file"], s["line"], s["callee"]))
            if all(k in good or k.startswith("=") for calls in s["args"] for k, _ in calls):
                readonly.append(s)
            elif site_key(s) in allowed:
                listed.append(s)
            else:
                pending.append(s)
    pending.sort(key=lambda s: (s["file"], s["line"]))
    if not args.check or pending:
        for s in pending:
            print(describe(s, good))
            print(f"    key: {site_key(s)}")
        if args.all:
            for s in listed:
                print("allowed  " + describe(s, good) + f"   [{allowed[site_key(s)]}]")
            for s in readonly:
                print("readonly " + describe(s, good))
    if args.reads:
        seen_reads: set[tuple[str, int]] = set()
        for _, _, _, reads in results:
            for s in reads:
                if (s["file"], s["line"]) not in seen_reads and not all(k in good or k.startswith("=") for calls in s["args"] for k, _ in calls):
                    seen_reads.add((s["file"], s["line"]))
                    print("reads    " + describe(s, good))
        print(f"arg-order: {len(seen_reads)} lists with one side-effecting call next to a memory read")
    stale = sorted(set(allowed) - {site_key(s) for s in listed})
    print(f"arg-order: {len(pending)} unclassified, {len(listed)} allowlisted, {len(readonly)} read-only, {len(stale)} stale allowlist entries "
          f"({len(seen)} multi-call argument lists)")
    for k in stale if not args.check else []:
        print(f"stale: {k}")
    return 1 if args.check and pending else 0


if __name__ == "__main__":
    raise SystemExit(main())
