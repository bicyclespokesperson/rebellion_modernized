// Stand-ins for the game's statically linked VC5 CRT / ICF-folded helpers that the reconstructed code still references and
// the container's UCRT does not provide under that name. The CRT fidelity pieces (_CIpow, rand,
// qsort, floor, ...) are reconstructed in src/game/crt (docs/library_code.md).
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
}

#ifdef REB_ZERO_NEW
#include <stddef.h>
void* RebAlloc(size_t size);  // src/port/heap_probe.cpp: zeroed memory, optionally with the REB_HEAP_PROBE address perturbation
void RebFree(void* memory);
void* operator new(size_t size) { return RebAlloc(size); }
void* operator new[](size_t size) { return RebAlloc(size); }
void operator delete(void* memory) { RebFree(memory); }
void operator delete[](void* memory) { RebFree(memory); }
#endif
