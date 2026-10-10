# gq script: symbols/functions.csv — every function in REBEXE.EXE except Unwind@ funclets: address, size, region.
import csv
GAME_END, LIB_END = 0x6153b0, 0x628e10
with open(ROOT / "symbols" / "functions.csv", "w", newline="") as fh:
    w = csv.writer(fh, lineterminator="\n")
    w.writerow(["address", "size", "region"])
    n = 0
    for f in fm.getFunctions(True):
        if f.getName().startswith("Unwind@"):
            continue
        a = f.getEntryPoint().getOffset()
        region = "thunk" if f.isThunk() else "game" if a < GAME_END or a >= LIB_END else "library"
        w.writerow([f"0x{a:08x}", f.getBody().getNumAddresses(), region])
        n += 1
print(n, "functions")
