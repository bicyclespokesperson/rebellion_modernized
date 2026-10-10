# gq script: merge Ghidra's user-assigned names into symbols/symbols.csv (the source of truth, in git).
# Rows keep their `signature` column; FUN_/DAT_ default names are never exported (they resolve by pattern).
import csv
from ghidra.program.model.symbol import SourceType, SymbolType

FIELDS = ["address", "kind", "name", "signature", "size"]
path = ROOT / "symbols" / "symbols.csv"
existing = {}
if path.exists():
    with open(path, newline="") as fh:
        for row in csv.DictReader(fh):
            existing[(int(row["address"], 16), row["kind"])] = row

current = {}
for f in fm.getFunctions(True):
    if f.isThunk() or f.getName().startswith("Unwind@") or f.getSymbol().getSource() == SourceType.DEFAULT:
        continue
    current[(f.getEntryPoint().getOffset(), "func")] = (f.getName(True), f.getBody().getNumAddresses())
for s in program.getSymbolTable().getAllSymbols(True):
    if s.getSymbolType() != SymbolType.LABEL or s.getSource() == SourceType.DEFAULT:
        continue
    a = s.getAddress()
    blk = program.getMemory().getBlock(a) if a.isMemoryAddress() else None
    if blk is None or blk.getName() not in (".data", ".rdata") or fm.getFunctionContaining(a) is not None:
        continue
    d = listing.getDataAt(a)
    current[(a.getOffset(), "data")] = (s.getName(True), d.getLength() if d else 0)

with open(path, "w", newline="") as fh:
    w = csv.DictWriter(fh, fieldnames=FIELDS, lineterminator="\n")
    w.writeheader()
    for key in sorted(set(current) | set(existing)):
        row = {k: existing.get(key, {}).get(k, "") for k in FIELDS}
        row["address"], row["kind"] = f"0x{key[0]:08x}", key[1]
        if key in current:  # rows only in the CSV (not yet applied to Ghidra) are kept as-is
            row["name"], row["size"] = current[key]
        w.writerow(row)
print(len(set(current) | set(existing)), "symbols ->", path)
