// Stand-ins for the game's statically linked VC5 CRT / ICF-folded helpers that the reconstructed code still references and
// the container's UCRT does not provide under that name. TODO(M6): the CRT fidelity pieces (_CIpow, ...) come from the
// CRT reconstruction (docs/phase2_plan.md, M4); the shims here are the bring-up versions.
#include <ctype.h>
#include <math.h>

int icf_return_zero() { return 0; }  // 0x6158b0, also strstreambuf::sync and _matherr

int __thiscall IcfReturnZeroSlot(void*) __asm__("reb_orig_006158b0");  // strstreambuf::sync's vftable slot
int __thiscall IcfReturnZeroSlot(void*) { return 0; }

void FUN_00619730() {}  // 0x619730 icf_empty_function: the linker folded every empty no-argument function into it

extern "C" {
int __cdecl _isprint(int c) { return isprint(c); }
int __cdecl _isspace(int c) { return isspace(c); }
int __cdecl _ispunct(int c) { return ispunct(c); }

// x87 calling convention of the VC5 CRT: exponent in st(0), base in st(1), result in st(0).
__declspec(naked) void CIpow() {  // referenced as "_CIpow" from inline asm: the linker symbol has a single underscore
    __asm {
        sub esp, 16
        fstp qword ptr [esp + 8]
        fstp qword ptr [esp]
        call pow
        add esp, 16
        ret
    }
}
}
