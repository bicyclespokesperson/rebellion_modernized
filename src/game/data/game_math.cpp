#include "data/game_math.h"

#include <cstdint>

namespace {

int wrappingSquare(int value) { return static_cast<int>(static_cast<uint32_t>(value) * static_cast<uint32_t>(value)); }

int wrappingProduct(int a, int b) { return static_cast<int>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b)); }

}  // namespace

int ClampPercent(int value) { return Clamp(value, g_percentMin, g_percentMax); }

int Clamp(int value, int low, int high) { return MinInt(high, MaxInt(low, value)); }

int ClampNegativeToZero(int value) { return value & ((value < 0) - 1); }

int MaxInt(int a, int b) { return a > b ? a : b; }

int MinInt(int a, int b) { return a <= b ? a : b; }

int IntDivide(int numerator, int denominator) { return numerator / denominator; }

int DivideRoundUp(int numerator, int denominator) { return (denominator - 1 + numerator) / denominator; }

int ScaleRatio(int value, int numerator, int denominator) {
    return IntDivide(wrappingProduct(numerator, value), denominator);
}

int ScalePercent(int value, int percent) { return ScaleRatio(value, percent, g_percentMax); }

int RatioToPercent(int part, int whole) { return ScaleRatio(g_percentMax, part, whole); }

// Bisection between hi/2 and hi, where hi is the first power of two whose square reaches `value`.
// BUG(original): IntegerSqrt(4) returns 1 (the search interval never reaches the exact root 2), and the
// squares wrap on overflow.
int IntegerSqrt(int value) {
    if (value <= 3) return value > 0 ? 1 : 0;
    int high = 2;
    if (value > 4) {
        do {
            high += high;
        } while (wrappingSquare(high) < value);
    }
    int low = high / 2;
    int middle = high;
    int previous;
    do {
        previous = middle;
        middle = (low + high) / 2;
        const int square = wrappingSquare(middle);
        if (square > value)
            high = middle;
        else if (square < value)
            low = middle;
    } while (middle != previous);
    return middle;
}
