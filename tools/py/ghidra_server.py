"""Long-lived Ghidra session serving Python snippets over a unix socket (see tools/gq).

Snippets run with helpers bound: program, fapi (FlatProgramAPI), addr(x), func(x), decomp(x), listing, fm.
Each request runs inside a transaction; 'save' persists the program.
"""
import contextlib
import io
import json
import os
import socket
import traceback

from ghidra.app.decompiler import DecompInterface, DecompileOptions
from ghidra.program.flatapi import FlatProgramAPI
from ghidra.util.task import ConsoleTaskMonitor

SOCK = str(ROOT / "work" / "ghidra.sock")
fapi = FlatProgramAPI(program)
listing = program.getListing()
fm = program.getFunctionManager()
monitor = ConsoleTaskMonitor()

_decomp = DecompInterface()
_opts = DecompileOptions()
_decomp.setOptions(_opts)
_decomp.toggleCCode(True)
_decomp.toggleSyntaxTree(True)
_decomp.openProgram(program)


def addr(x):
    if isinstance(x, int):
        return fapi.toAddr(x)
    if isinstance(x, str):
        return fapi.toAddr(int(x, 16)) if not x.startswith("FUN_") else fapi.toAddr(int(x[4:], 16))
    return x


def func(x):
    a = addr(x)
    return fm.getFunctionContaining(a) if a is not None else None


def decomp(x, timeout=120):
    f = func(x)
    if f is None:
        return f"// no function at {x}"
    res = _decomp.decompileFunction(f, timeout, monitor)
    if not res.decompileCompleted():
        return f"// decompile failed: {res.getErrorMessage()}"
    return res.getDecompiledFunction().getC()


ENV = dict(program=program, fapi=fapi, listing=listing, fm=fm, addr=addr, func=func, decomp=decomp,
           monitor=monitor, ROOT=ROOT)
exec((ROOT / "tools" / "py" / "gq_helpers.py").read_text(), ENV)


def handle(req):
    if req.get("cmd") == "save":
        program.save("server save", monitor)
        return "saved"
    out = io.StringIO()
    tx = program.startTransaction("gq")
    ok = False
    try:
        with contextlib.redirect_stdout(out):
            exec(req["code"], ENV)
        ok = True
    except Exception:
        out.write(traceback.format_exc())
    finally:
        program.endTransaction(tx, ok)
    return out.getvalue()


def serve():
    with contextlib.suppress(FileNotFoundError):
        os.unlink(SOCK)
    srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    srv.bind(SOCK)
    srv.listen(4)
    print("ghidra server ready on", SOCK, flush=True)
    while True:
        conn, _ = srv.accept()
        with conn:
            data = b""
            while not data.endswith(b"\n\0"):
                chunk = conn.recv(65536)
                if not chunk:
                    break
                data += chunk
            req = json.loads(data[:-2].decode())
            if req.get("cmd") == "quit":
                program.save("server quit", monitor)
                conn.sendall(b"bye")
                return
            conn.sendall(handle(req).encode())


serve()
