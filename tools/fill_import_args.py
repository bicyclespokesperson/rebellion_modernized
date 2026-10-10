#!/usr/bin/env python3
"""Fill symbols/imports.csv arg_bytes/convention from x86 import-library decorations (run in container)."""
import csv
import re
import subprocess
from pathlib import Path

LIBS = Path("/opt/xwin/sdk/lib/um/x86")
ORDINALS = {  # imports by ordinal: (name, convention, arg bytes)
    ("COMCTL32.DLL", "Ordinal_17"): ("InitCommonControls", "stdcall", 0),
    ("DPLAYX.DLL", "Ordinal_1"): ("DirectPlayCreate", "stdcall", 12),
    ("DPLAYX.DLL", "Ordinal_2"): ("DirectPlayEnumerateA", "stdcall", 8),
    ("DPLAYX.DLL", "Ordinal_4"): ("DirectPlayLobbyCreateA", "stdcall", 20),
    ("D3DRM.DLL", "Ordinal_1"): ("Direct3DRMCreate", "stdcall", 4),
}
decorated: dict[str, tuple[str, int]] = {}
for lib in LIBS.glob("*.[Ll][Ii][Bb]"):
    out = subprocess.run(["llvm-nm-19", "--defined-only", str(lib)], capture_output=True, text=True).stdout
    for m in re.finditer(r"__imp__([A-Za-z0-9_]+?)(?:@(\d+))?$", out, re.M):
        name, n = m.group(1), m.group(2)
        decorated.setdefault(name, ("stdcall", int(n)) if n is not None else ("cdecl", 0))
smack = {}
for line in Path("src/hybrid/smackw32.def").read_text().splitlines():
    m = re.match(r"\s+_(\w+)@(\d+) = \S+ @(\d+)", line)
    if m:
        smack[f"Ordinal_{m.group(3)}"] = (m.group(1), "stdcall", int(m.group(2)))
path = Path("symbols/imports.csv")
rows = list(csv.DictReader(open(path)))
missing = []
for r in rows:
    key = (r["dll"].upper(), r["name"])
    if key in ORDINALS:
        r["name"], r["convention"], r["arg_bytes"] = ORDINALS[key]
    elif r["dll"].upper() == "SMACKW32.DLL" and r["name"] in smack:
        r["name"], r["convention"], r["arg_bytes"] = smack[r["name"]]
    elif m := re.fullmatch(r"_(\w+)@(\d+)", r["name"]):
        r["name"], r["convention"], r["arg_bytes"] = m.group(1), "stdcall", int(m.group(2))
    elif r["name"] == "Direct3DRMCreate":
        r["convention"], r["arg_bytes"] = "stdcall", 4
    elif r["name"] in decorated:
        r["convention"], r["arg_bytes"] = decorated[r["name"]]
    else:
        missing.append(f'{r["dll"]}!{r["name"]}')
with open(path, "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=["iat", "dll", "name", "convention", "arg_bytes"], lineterminator="\n")
    w.writeheader()
    w.writerows(rows)
print("unresolved:", missing)
