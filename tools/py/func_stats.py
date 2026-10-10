from collections import Counter
fm = program.getFunctionManager()
sizes = []
thunks = 0
for f in fm.getFunctions(True):
    if f.isThunk():
        thunks += 1
        continue
    sizes.append(f.getBody().getNumAddresses())
sizes.sort()
n = len(sizes)
print("functions(non-thunk):", n, "thunks:", thunks, "total bytes:", sum(sizes))
for q in (0.25, 0.5, 0.75, 0.9, 0.99):
    print(f"p{int(q*100)}:", sizes[int(q*n)])
buckets = Counter()
for s in sizes:
    b = "<=16" if s <= 16 else "<=64" if s <= 64 else "<=256" if s <= 256 else "<=1024" if s <= 1024 else ">1024"
    buckets[b] += 1
print(dict(buckets))
listing = program.getListing()
text = program.getMemory().getBlock(".text")
covered = 0
for f in fm.getFunctions(True):
    covered += f.getBody().intersect(__import__("ghidra.program.model.address", fromlist=["AddressSet"]).AddressSet(text.getStart(), text.getEnd())).getNumAddresses()
print(".text size", text.getSize(), "covered by functions", covered)
