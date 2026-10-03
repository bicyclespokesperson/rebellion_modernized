# gq script: create/refresh Ghidra structures from work/types.json (written by tools/sync_types.py).
import json
import re
from ghidra.program.model.data import (ArrayDataType, BooleanDataType, ByteDataType, CategoryPath, CharDataType,
                                       DataTypeConflictHandler, DoubleDataType, FloatDataType, IntegerDataType,
                                       LongLongDataType, PointerDataType, ShortDataType, StructureDataType,
                                       UnsignedCharDataType, UnsignedIntegerDataType, UnsignedLongLongDataType,
                                       UnsignedShortDataType, Undefined4DataType, VoidDataType)

records = json.loads((ROOT / "work" / "types.json").read_text())
dtm = program.getDataTypeManager()
CAT = CategoryPath("/")
PRIMITIVES = {
    "char": CharDataType.dataType, "signed char": CharDataType.dataType, "unsigned char": UnsignedCharDataType.dataType,
    "short": ShortDataType.dataType, "unsigned short": UnsignedShortDataType.dataType,
    "int": IntegerDataType.dataType, "unsigned int": UnsignedIntegerDataType.dataType,
    "long": IntegerDataType.dataType, "unsigned long": UnsignedIntegerDataType.dataType,
    "long long": LongLongDataType.dataType, "unsigned long long": UnsignedLongLongDataType.dataType,
    "bool": BooleanDataType.dataType, "float": FloatDataType.dataType, "double": DoubleDataType.dataType,
    "void": VoidDataType.dataType, "wchar_t": UnsignedShortDataType.dataType,
}

structs = {}
for name, rec in records.items():
    existing = dtm.getDataType(CAT, name)
    if existing is None:
        existing = dtm.addDataType(StructureDataType(CAT, name, rec["size"]), DataTypeConflictHandler.REPLACE_HANDLER)
    structs[name] = existing


def resolve(t):
    t = re.sub(r"\b(const|volatile)\b", "", t).strip()
    m = re.match(r"^(.*)\[(\d+)\]$", t)
    if m:
        inner = resolve(m.group(1).strip())
        return ArrayDataType(inner, int(m.group(2)), inner.getLength()) if inner and inner.getLength() > 0 else None
    if t.endswith("*") or t.endswith("&"):
        inner = resolve(t[:-1].strip())
        return PointerDataType(inner if inner is not None else VoidDataType.dataType, 4, dtm)
    t = re.sub(r"^(class|struct|union|enum) ", "", t)
    if t in PRIMITIVES:
        return PRIMITIVES[t]
    if t in structs:
        return structs[t]
    return None


updated = 0
skipped = []
for name, rec in records.items():
    s = StructureDataType(CAT, name, rec["size"], dtm)
    for f in rec["fields"]:
        if f["kind"] == "unknown":
            continue
        dt = resolve(f["type"])
        if dt is None:
            if f["type"].startswith("enum "):
                dt = Undefined4DataType.dataType
            else:
                skipped.append(f"{name}.{f['name']}: {f['type']}")
                continue
        s.replaceAtOffset(f["offset"], dt, dt.getLength(), f["name"], None)
    structs[name].replaceWith(s)
    updated += 1
print(f"structures updated: {updated}; fields skipped (unknown type): {len(skipped)} {skipped[:10]}")
