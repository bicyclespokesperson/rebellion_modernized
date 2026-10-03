# gq script: write symbols/symbols.csv from Ghidra (user-assigned names only; FUN_/DAT_ names resolve by pattern).
import csv
from ghidra.program.model.symbol import SourceType, SymbolType

rows = []
for f in fm.getFunctions(True):
    name = f.getName(True)  # namespace-qualified, e.g. CFoo::Bar
    if f.isThunk() or name.startswith("Unwind@") or f.getSymbol().getSource() == SourceType.DEFAULT:
        continue
    rows.append((f.getEntryPoint().getOffset(), "func", name.replace("::", "::"), f.getBody().getNumAddresses()))
st = program.getSymbolTable()
for s in st.getAllSymbols(True):
    if s.getSymbolType() != SymbolType.LABEL or s.getSource() == SourceType.DEFAULT:
        continue
    a = s.getAddress()
    if not a.isMemoryAddress() or fm.getFunctionContaining(a) is not None:
        continue
    blk = program.getMemory().getBlock(a)
    if blk is None or blk.getName() not in (".data", ".rdata"):
        continue
    d = listing.getDataAt(a)
    rows.append((a.getOffset(), "data", s.getName(True), d.getLength() if d else 0))
rows.sort()
out = ROOT / "symbols" / "symbols.csv"
with open(out, "w", newline="") as fh:
    w = csv.writer(fh, lineterminator="\n")
    w.writerow(["address", "kind", "name", "size"])
    for a, k, n, sz in rows:
        w.writerow([f"0x{a:08x}", k, n, sz])
print(len(rows), "symbols ->", out)
