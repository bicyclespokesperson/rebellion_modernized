#!/usr/bin/env -S uv run --script
# /// script
# dependencies = ["pefile"]
# ///
"""List the DLLs a PE imports (with the number of functions); -v also lists the names."""
import argparse
from pathlib import Path

import pefile


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("exe", type=Path)
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()
    pe = pefile.PE(str(args.exe), fast_load=False)
    for entry in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        print(f"{entry.dll.decode():20} {len(entry.imports):4}")
        if args.verbose:
            print("   " + " ".join((i.name or b"#%d" % i.ordinal).decode() for i in entry.imports))


if __name__ == "__main__":
    main()
