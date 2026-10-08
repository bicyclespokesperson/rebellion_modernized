#include "tactical/tactical_math.h"

TacticalVec4::TacticalVec4() : m_x(0), m_y(0), m_z(0), m_w(-1.0f) {}

void TacticalVec4::Set(float x, float y, float z) {
    m_x = x;
    m_y = y;
    m_z = z;
    m_w = -1.0f;
}

TacticalVec4* TacticalVec4::CopyFrom(const TacticalVec4* other) {
    *this = *other;
    return this;
}
