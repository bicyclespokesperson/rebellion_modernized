#pragma once

// Integer helpers used by every rules module (0x53e0d0-0x53e1d0). The percentage limits are const ints in the
// original's .rdata (0 and 100) that the compiler left as memory loads.
extern const int g_percentMin;  // 0x661a84
extern const int g_percentMax;  // 0x661a88

int ClampPercent(int value);
int Clamp(int value, int low, int high);
// value & ((value < 0) - 1): negative values become 0.
int ClampNegativeToZero(int value);
int MaxInt(int a, int b);
int MinInt(int a, int b);
int IntDivide(int numerator, int denominator);
// (numerator + denominator - 1) / denominator, i.e. ceiling for positive operands.
int DivideRoundUp(int numerator, int denominator);
// (value * numerator) / denominator
int ScaleRatio(int value, int numerator, int denominator);
// (value * percent) / 100
int ScalePercent(int value, int percent);
// (part * 100) / whole
int RatioToPercent(int part, int whole);
int IntegerSqrt(int value);
