# Helpers available in every gq snippet (exec'd into the server environment at startup).
from ghidra.program.model.symbol import SourceType
from ghidra.program.model.listing import Function


def _namespace_for(qualified):
    parts = qualified.split("::")
    st = program.getSymbolTable()
    ns = program.getGlobalNamespace()
    for p in parts[:-1]:
        existing = st.getNamespace(p, ns)
        ns = existing if existing is not None else st.createClass(ns, p, SourceType.USER_DEFINED)
    return ns, parts[-1]


def name_func(address, qualified, create=True):
    """Name the function at `address` with a C++ qualified name like 'CFoo::Bar' (creates class namespaces)."""
    a = addr(address)
    f = fm.getFunctionAt(a)
    if f is None and create:
        f = fapi.createFunction(a, None)
    if f is None:
        raise ValueError(f"no function at {a}")
    ns, short = _namespace_for(qualified)
    f.setParentNamespace(ns)
    f.setName(short, SourceType.USER_DEFINED)
    return f


def name_data(address, qualified):
    """Name a global data address (label) with an optionally qualified name."""
    a = addr(address)
    ns, short = _namespace_for(qualified)
    st = program.getSymbolTable()
    sym = st.getPrimarySymbol(a)
    if sym is not None and sym.getSource() != SourceType.DEFAULT:
        sym.setNameAndNamespace(short, ns, SourceType.USER_DEFINED)
    else:
        st.createLabel(a, short, ns, SourceType.USER_DEFINED).setPrimary()
    return st.getPrimarySymbol(a)


def set_comment(address, text):
    f = func(address)
    f.setComment(text)
