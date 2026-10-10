#!/usr/bin/env -S uv run
# /// script
# dependencies = ["pefile"]
# ///
import sys
from pathlib import Path
import pefile

for path in sorted(Path(sys.argv[1]).glob("*")):
    if path.suffix.lower() not in (".dll", ".exe"):
        continue
    pe = pefile.PE(str(path), fast_load=False)
    secs = ", ".join(f"{s.Name.rstrip(b'\0').decode()}:{s.Misc_VirtualSize:#x}" for s in pe.sections)
    imports = []
    if hasattr(pe, "DIRECTORY_ENTRY_IMPORT"):
        for e in pe.DIRECTORY_ENTRY_IMPORT:
            imports.append(f"{e.dll.decode()}({len(e.imports)})")
    nexp = len(pe.DIRECTORY_ENTRY_EXPORT.symbols) if hasattr(pe, "DIRECTORY_ENTRY_EXPORT") else 0
    rich = pe.parse_rich_header()
    linker = f"{pe.OPTIONAL_HEADER.MajorLinkerVersion}.{pe.OPTIONAL_HEADER.MinorLinkerVersion}"
    print(f"== {path.name}  base={pe.OPTIONAL_HEADER.ImageBase:#x} linker={linker} ts={pe.FILE_HEADER.TimeDateStamp:#x} exports={nexp} rich={'yes' if rich else 'no'}")
    print(f"   sections: {secs}")
    print(f"   imports: {' '.join(imports)}")
