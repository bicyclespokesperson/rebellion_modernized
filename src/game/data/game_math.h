#pragma once

#include <cstdint>

// Integer helpers used by every rules module (0x53e0d0-0x53e1d0). The percentage limits are const ints in the
// original's .rdata (0 and 100) that the compiler left as memory loads.
extern const int g_percentMin; // 0x661a84
extern const int g_percentMax; // 0x661a88

int ClampPercent(int value);
int Clamp(int value, int low, int high);
int ClampNegativeToZero(int value);
int MaxInt(int a, int b);
int MinInt(int a, int b);
// Integer division truncates toward zero. Division by zero throws std::domain_error; an unrepresentable result
// throws std::overflow_error.
int IntDivide(int numerator, int denominator);
// Mathematical ceiling of numerator / denominator for any signed operands. Uses the same exception policy as
// IntDivide.
int DivideRoundUp(int numerator, int denominator);
// (value * numerator) / denominator with truncation toward zero and checked int result. Uses the same exception
// policy as IntDivide.
int ScaleRatio(int value, int numerator, int denominator);
// (value * percent) / 100
int ScalePercent(int value, int percent);
// (part * 100) / whole
int RatioToPercent(int part, int whole);
int IntegerSqrt(int value);
uint64_t IntegerSqrt(uint64_t value);
