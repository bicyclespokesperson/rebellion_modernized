#include <cmath>
#include <cstdint>
#include <cstring>

// VC5 floor (0x619610): the x87 rounds toward minus infinity (control word 0x173f, kept in DAT_006acac0) around
// FRNDINT, then the caller's control word comes back. Infinities and NaNs return unchanged; the original also reports
// NaNs through _matherr/errno, which nothing in the game looks at.
namespace {

constexpr uint16_t kRoundDownControlWord = 0x173f;

bool IsInfOrNan(double x) {
    uint64_t bits;
    std::memcpy(&bits, &x, sizeof bits);
    return (bits >> 52 & 0x7ff) == 0x7ff;
}

}  // namespace

extern "C" double __cdecl floor(double x) {
    if (IsInfOrNan(x)) return x;
    uint16_t savedControlWord;
    uint16_t roundDown = kRoundDownControlWord;
    double result;
    __asm__ volatile(
        "fnstcw %1\n\t"
        "fldcw %2\n\t"
        "frndint\n\t"
        "fldcw %1"
        : "=t"(result), "=m"(savedControlWord)
        : "m"(roundDown), "0"(x));
    return result;
}
