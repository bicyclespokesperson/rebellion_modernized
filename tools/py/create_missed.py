# gq script: create functions at the entry points listed in symbols/missed_functions.csv (kinds given in KINDS).
import csv
from ghidra.app.cmd.disassemble import DisassembleCommand
from ghidra.app.cmd.function import CreateFunctionCmd
KINDS = globals().get("KINDS", ("data_ref", "static_init"))
created = failed = existing = 0
fails = []
with open(ROOT / "symbols" / "missed_functions.csv", newline="") as fh:
    rows = [r for r in csv.DictReader(fh) if r["kind"] in KINDS]
for r in rows:
    a = addr(int(r["address"], 16))
    if fm.getFunctionAt(a) is not None:
        existing += 1
        continue
    if listing.getInstructionAt(a) is None:
        DisassembleCommand(a, None, True).applyTo(program, monitor)
    cmd = CreateFunctionCmd(a)
    if cmd.applyTo(program, monitor) and fm.getFunctionAt(a) is not None:
        created += 1
    else:
        failed += 1
        fails.append(r["address"])
print(f"created {created}, already existed {existing}, failed {failed}: {fails[:15]}")
