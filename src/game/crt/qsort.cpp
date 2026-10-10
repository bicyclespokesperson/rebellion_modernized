#include "core/reb_abi.h"
#include <cstddef>
#include <cstdlib>

// VC5 qsort (0x61a0c0): quicksort with an explicit stack, the middle element moved to the front as the pivot, and
// partitions of at most 8 elements finished by a selection sort (__shortsort 0x61a270). The order of elements that
// compare equal is part of the game's behaviour (it sorts systems by distance), so this is the algorithm, not "a sort".

using Compare = int(REB_CDECL*)(const void*, const void*);

namespace {
constexpr unsigned kCutoff = 8;
constexpr int kStackSize = 30;
}  // namespace

extern "C" void REB_CDECL _swap(char* a, char* b, unsigned width) {
    if (a == b) return;
    while (width--) {
        const char t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

extern "C" void REB_CDECL _shortsort(char* lo, char* hi, unsigned width, Compare comp) {
    while (hi > lo) {
        char* max = lo;
        for (char* p = lo + width; p <= hi; p += width)
            if (comp(p, max) > 0) max = p;
        _swap(max, hi, width);
        hi -= width;
    }
}

extern "C" void REB_CDECL qsort(void* base, size_t num, size_t width, Compare comp) {
    if (num < 2 || width == 0) return;
    char* lostk[kStackSize];
    char* histk[kStackSize];
    int stkptr = 0;
    char* lo = static_cast<char*>(base);
    char* hi = lo + width * (num - 1);

    for (;;) {
        const unsigned size = static_cast<unsigned>((hi - lo) / width) + 1;
        if (size <= kCutoff) {
            _shortsort(lo, hi, static_cast<unsigned>(width), comp);
        } else {
            char* mid = lo + (size / 2) * width;
            _swap(mid, lo, static_cast<unsigned>(width));
            char* loguy = lo;
            char* higuy = hi + width;
            for (;;) {
                do loguy += width; while (loguy <= hi && comp(loguy, lo) <= 0);
                do higuy -= width; while (higuy > lo && comp(higuy, lo) >= 0);
                if (higuy < loguy) break;
                _swap(loguy, higuy, static_cast<unsigned>(width));
            }
            _swap(lo, higuy, static_cast<unsigned>(width));

            if (higuy - 1 - lo >= hi - loguy) {
                if (lo + width < higuy) {
                    lostk[stkptr] = lo;
                    histk[stkptr] = higuy - width;
                    ++stkptr;
                }
                if (loguy < hi) {
                    lo = loguy;
                    continue;
                }
            } else {
                if (loguy < hi) {
                    lostk[stkptr] = loguy;
                    histk[stkptr] = hi;
                    ++stkptr;
                }
                if (lo + width < higuy) {
                    hi = higuy - width;
                    continue;
                }
            }
        }
        if (--stkptr < 0) return;
        lo = lostk[stkptr];
        hi = histk[stkptr];
    }
}
