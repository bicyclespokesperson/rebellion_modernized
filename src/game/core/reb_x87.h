#pragma once
#include <cmath>
#include <cstdint>

// The x87 floating point operations whose exact results the game's behaviour depends on (volume curves, the tactical
// simulation's trigonometry), behind one small API. On i386 they are the original instruction sequences (inline x87
// assembly, or a call to the game's own CRT `_CIpow`); every other target gets the C library's implementation, whose results
// may differ in the last bits (the decision for the 64-bit ports: float drift is acceptable, see docs/phase3_plan.md).
//
// Code outside this header and crt/ must not contain x87 assembly.

#if defined(__i386__) || defined(_M_IX86)
#define REB_X87_NATIVE 1
#else
#define REB_X87_NATIVE 0
#endif

// The CRT's `_CIpow` (base ** exponent) at the x87 precision of the original.
inline double reb_pow(double base, double exponent) {
#if REB_X87_NATIVE
    double result;
    __asm__ volatile("call _CIpow" : "=t"(result) : "0"(exponent), "u"(base) : "st(1)", "eax", "ecx", "edx", "cc", "memory");
    return result;
#else
    return std::pow(base, exponent);
#endif
}

// acos(): the CRT's `_CIacos` is reached through the ordinary library call.
inline double reb_acos(double value) { return std::acos(value); }

// fsin / fcos / fsqrt as single x87 instructions (the original has no library call; the C library would round differently).
inline double reb_cos(double angle) {
#if REB_X87_NATIVE
    __asm__("fcos" : "+t"(angle));
    return angle;
#else
    return std::cos(angle);
#endif
}

inline double reb_sin(double angle) {
#if REB_X87_NATIVE
    __asm__("fsin" : "+t"(angle));
    return angle;
#else
    return std::sin(angle);
#endif
}

inline double reb_sqrt(double value) {
#if REB_X87_NATIVE
    __asm__("fsqrt" : "+t"(value));
    return value;
#else
    return std::sqrt(value);
#endif
}

// log10(n * scale) * factor with the product and the logarithm kept in x87 extended precision, as the volume slider
// of the options window computed it (FYL2X).
inline double reb_scaled_log10(int n, double scale, double factor) {
#if REB_X87_NATIVE
    double result;
    __asm__ volatile("fldlg2\n\tfildl %1\n\tfmull %2\n\tfyl2x\n\tfmull %3\n\tfstpl %0"
                     : "=m"(result)
                     : "m"(n), "m"(scale), "m"(factor)
                     : "st", "st(1)", "cc");
    return result;
#else
    return std::log10(static_cast<double>(n) * scale) * factor;
#endif
}

// pow(base, n * scaleA * scaleB) * factor: the exponent products and the final factor in extended precision around the
// CRT's `_CIpow` (the inverse of the volume slider curve).
inline double reb_scaled_pow(double base, int n, double scaleA, double scaleB, double factor) {
#if REB_X87_NATIVE
    double result;
    __asm__ volatile("fldl %1\n\tfildl %2\n\tfmull %3\n\tfmull %4\n\tcall _CIpow\n\tfmull %5\n\tfstpl %0"
                     : "=m"(result)
                     : "m"(base), "m"(n), "m"(scaleA), "m"(scaleB), "m"(factor)
                     : "st", "st(1)", "eax", "ecx", "edx", "cc", "memory");
    return result;
#else
    return std::pow(base, static_cast<double>(n) * scaleA * scaleB) * factor;
#endif
}

// double -> int32 truncation with the x86 convention for what C leaves undefined: NaN and out-of-range values give
// INT32_MIN (the "integer indefinite" value cvttsd2si and FISTP produce), on every target.
inline int32_t reb_ftol(double value) {
#if REB_X87_NATIVE
    return static_cast<int32_t>(value);
#else
    if (!(value > -2147483649.0 && value < 2147483648.0)) return INT32_MIN;
    return static_cast<int32_t>(value);
#endif
}

// double -> uint32 as the original and x86-64 do it: truncate to 64 bits (the CRT's _ftol, cvttsd2si with a 64-bit destination) and keep
// the low word, so a negative value wraps and NaN / out-of-range values give 0. arm64 would saturate a negative value to 0 instead.
inline uint32_t reb_ftou(double value) {
#if REB_X87_NATIVE
    return static_cast<uint32_t>(value);
#else
    if (!(value > -9223372036854775809.0 && value < 9223372036854775808.0)) return 0;
    return static_cast<uint32_t>(static_cast<int64_t>(value));
#endif
}
