#include <cstdint>
#include <cstdlib>

// VC5 rand/srand (0x61a310, 0x61a300): seed = seed * 214013 + 2531011, result bits 16..30. The seed is per thread and
// starts at 1. The original keeps it at +0x14 of the thread data block (_tiddata) that __getptd (0x61d430) returns; the
// hybrid uses exactly that block so the hooks are interchangeable with the original code (difftest compares the
// memory). Without it (standalone) it is an ordinary thread_local.
#ifndef REB_STANDALONE  // the shared flags also define REB_HYBRID in the standalone build
extern "C" uint8_t* __cdecl _getptd();

namespace {
uint32_t& HoldRand() { return *reinterpret_cast<uint32_t*>(_getptd() + 0x14); }
}  // namespace
#else
namespace {
thread_local uint32_t t_holdRand = 1;
uint32_t& HoldRand() { return t_holdRand; }
}  // namespace
#endif

extern "C" void __cdecl srand(unsigned int seed) { HoldRand() = seed; }

extern "C" int __cdecl rand(void) {
    uint32_t& seed = HoldRand();
    seed = seed * 214013u + 2531011u;
    return static_cast<int>((seed >> 16) & 0x7fff);
}
