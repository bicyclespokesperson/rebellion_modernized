#pragma once
#include <cstdint>

#include "core/layout.h"

// The four-float record the tactical engine uses for positions and directions (x, y, z and a fourth component that
// defaults to -1). Constructors 0x59faf0 / 0x59fb10, copy 0x5a33b0.
struct TacticalVec4 {
    float m_x;  // +0x00
    float m_y;  // +0x04
    float m_z;  // +0x08
    float m_w;  // +0x0c

    TacticalVec4();                          // 0x59faf0
    TacticalVec4(float x, float y, float z) : m_x(x), m_y(y), m_z(z), m_w(-1.0f) {}
    void Set(float x, float y, float z);      // 0x59fb10 (the original constructor does not return this)
    TacticalVec4* CopyFrom(const TacticalVec4* other);  // 0x5a33b0
};

REB_ASSERT_SIZE(TacticalVec4, 0x10);
