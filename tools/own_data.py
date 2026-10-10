"""Which of REBEXE.EXE's data the REB_OWN_DATA hybrid moves into the DLL (shared by gen_link_symbols and gen_data_image).

REBEXE.EXE's .data/.bss interleave game data with the statically linked CRT/LIBCI data (boundaries measured from the
references of the library code, see docs/phase2_plan.md, "M5"):

    0x6a6000-0x6a7c10  static-initializer tables (_initterm walks them in the EXE until M6)  -> stays in the EXE
    0x6a7c10-0x6ac970  game .data (strings, option tables, initialised globals)              -> DLL
    0x6ac970-0x6b059c  LIBCI RTTI + LIBCMT .data (locale tables, _iob, sbh, math tables)      -> stays in the EXE
    0x6b059c-0x6be708  game .bss                                                               -> DLL
    0x6be708-0x6bfe50  LIBCI/LIBCMT .bss                                                       -> stays in the EXE

Both game segments live in the DLL as one block of the original size (the CRT gap between them is zero-filled), so a
single delta relocates an original address into the block.
"""
from __future__ import annotations

BLOCK_BASE = 0x6A7C10
BLOCK_END = 0x6BE708
OWN_SEGMENTS = ((0x6A7C10, 0x6AC970), (0x6B059C, 0x6BE708))
RDATA_RANGE = (0x658000, 0x6A6000)
TEXT_RANGE = (0x401000, 0x657400)
IMAGE_END = 0x6C0000


def in_segment(address: int) -> bool:
    return any(lo <= address < hi for lo, hi in OWN_SEGMENTS)


def is_copied_rdata(address: int, name: str) -> bool:
    """Named .rdata globals whose bytes are copied into the image. Vtables stay where they are: the hybrid's vtable
    patching points objects at the originals."""
    lo, hi = RDATA_RANGE
    return lo <= address < hi and "vftable" not in name and "vbtable" not in name


def owns(address: int, name: str) -> bool:
    """True when the data symbol is defined by the data image rather than bound to its original address."""
    return in_segment(address) or is_copied_rdata(address, name)
