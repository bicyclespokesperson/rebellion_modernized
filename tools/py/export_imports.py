# gq script: symbols/imports.csv — every import of REBEXE.EXE with its IAT slot and stdcall stack-arg bytes.
import csv
rows = []
st = program.getSymbolTable()
for ext in st.getExternalSymbols():
    lib = ext.getParentNamespace().getName()
    f = fm.getFunctionAt(ext.getAddress()) if ext.getAddress() else None
    ef = ext.getObject()
    func_obj = None
    try:
        func_obj = ef.getFunction() if hasattr(ef, "getFunction") else None
    except Exception:
        pass
    params = None
    cc = ""
    if func_obj is not None:
        sig = func_obj.getSignature()
        params = sum(max(4, (p.getLength() + 3) // 4 * 4) for p in func_obj.getParameters())
        cc = func_obj.getCallingConventionName()
        if func_obj.hasVarArgs():
            cc += "+varargs"
    for ref in program.getReferenceManager().getReferencesTo(ext.getAddress()):
        if ref.getReferenceType().isData() or True:
            iat = ref.getFromAddress()
            blk = program.getMemory().getBlock(iat)
            if blk is not None and blk.getName() == ".rdata" and listing.getInstructionAt(iat) is None:
                rows.append((iat.getOffset(), lib, ext.getName(), cc, "" if params is None else params))
rows = sorted(set(rows))
with open(ROOT / "symbols" / "imports.csv", "w", newline="") as fh:
    w = csv.writer(fh, lineterminator="\n")
    w.writerow(["iat", "dll", "name", "convention", "arg_bytes"])
    for r in rows:
        w.writerow([f"0x{r[0]:08x}", *r[1:]])
print(len(rows), "imports;", sum(1 for r in rows if r[4] == ""), "without signature")
