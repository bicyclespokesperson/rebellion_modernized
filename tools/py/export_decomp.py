# Runs inside the gq server: exports raw decompilation of all non-unwind functions to work/decomp/<addr>.c
import threading
from concurrent.futures import ThreadPoolExecutor
from ghidra.app.decompiler import DecompInterface, DecompileOptions
from ghidra.util.task import ConsoleTaskMonitor

OUT = ROOT / "work" / "decomp"
OUT.mkdir(parents=True, exist_ok=True)
funcs = [f for f in fm.getFunctions(True) if not f.getName().startswith("Unwind@") and not f.isThunk()]
STATUS = {"done": 0, "failed": 0, "total": len(funcs)}
_local = threading.local()

def _iface():
    if not hasattr(_local, "d"):
        d = DecompInterface(); d.setOptions(DecompileOptions()); d.openProgram(program); _local.d = d
    return _local.d

def _one(f):
    res = _iface().decompileFunction(f, 180, ConsoleTaskMonitor())
    name = f"{f.getEntryPoint().getOffset():08x}.c"
    if res.decompileCompleted():
        (OUT / name).write_text(res.getDecompiledFunction().getC())
        STATUS["done"] += 1
    else:
        (OUT / name).write_text(f"// decompile failed: {res.getErrorMessage()}\n")
        STATUS["failed"] += 1

def _run():
    with ThreadPoolExecutor(max_workers=8) as ex:
        list(ex.map(_one, funcs))
    STATUS["finished"] = True

EXPORT_STATUS = STATUS
threading.Thread(target=_run, daemon=True).start()
print("export started for", len(funcs), "functions")
