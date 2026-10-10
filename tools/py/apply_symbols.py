# gq script: apply symbols/symbols.csv (the source of truth, tracked in git) to the Ghidra program.
# Run `tools/gq --save` afterwards to persist.
import csv
applied = 0
errors = []
with open(ROOT / "symbols" / "symbols.csv", newline="") as fh:
    for row in csv.DictReader(fh):
        a = int(row["address"], 16)
        try:
            if row["kind"] == "func":
                name_func(a, row["name"])
            else:
                name_data(a, row["name"])
            applied += 1
        except Exception as e:
            errors.append(f'{row["address"]} {row["name"]}: {e}')
print(f"applied {applied} symbols; {len(errors)} errors")
for e in errors[:20]:
    print("  ", e)
