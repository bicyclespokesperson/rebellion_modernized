#pragma once
#include <cstdint>

#include "core/layout.h"

// The four-float record the tactical engine uses for positions and directions: x, y, z and a fourth component that
// caches the vector's length (-1 while unknown). Default constructor 0x59faf0, copy 0x5a33b0.
struct TacticalVector {
    float m_x;             // +0x00
    float m_y;             // +0x04
    float m_z;             // +0x08
    float m_cachedLength;  // +0x0c

    TacticalVector();  // 0x59faf0
    TacticalVector(float x, float y, float z) : m_x(x), m_y(y), m_z(z), m_cachedLength(-1.0f) {}

    void Assign(float x, float y, float z);                 // 0x59fb10 (the original constructor does not return this)
    TacticalVector* CopyFrom(const TacticalVector* other);  // 0x5a33b0
    double Length();                                        // 0x5b50b0 (caches the length in m_cachedLength)
    void Set(float x, float y, float z);                    // 0x5b5090
    int MatchesXYZ(const TacticalVector* other);            // 0x5b39e0
    void Scale(float factor);                               // 0x5ba080
};

REB_ASSERT_SIZE(TacticalVector, 0x10);

// The x87 instructions the engine uses for its trigonometry and square roots (the original has no library call; the C
// library would round differently).
inline double x87Cos(double angle) {
    __asm__("fcos" : "+t"(angle));
    return angle;
}

inline double x87Sin(double angle) {
    __asm__("fsin" : "+t"(angle));
    return angle;
}

inline double x87Sqrt(double value) {
    __asm__("fsqrt" : "+t"(value));
    return value;
}
