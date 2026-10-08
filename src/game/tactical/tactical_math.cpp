#include "tactical/tactical_math.h"

namespace {

constexpr int kUnknownLength = static_cast<int>(0xbf800000u);

int bitsOf(float value) {
    int bits;
    __builtin_memcpy(&bits, &value, sizeof bits);
    return bits;
}

}  // namespace

TacticalVector::TacticalVector() : m_x(0), m_y(0), m_z(0), m_cachedLength(-1.0f) {}

void TacticalVector::Assign(float x, float y, float z) {
    m_x = x;
    m_y = y;
    m_z = z;
    m_cachedLength = -1.0f;
}

TacticalVector* TacticalVector::CopyFrom(const TacticalVector* other) {
    *this = *other;
    return this;
}

double TacticalVector::Length() {
    if (bitsOf(m_cachedLength) == kUnknownLength) {
        const double sum = (static_cast<double>(m_y) * m_y + static_cast<double>(m_z) * m_z) +
                           static_cast<double>(m_x) * m_x;
        m_cachedLength = static_cast<float>(x87Sqrt(sum));
    }
    return m_cachedLength;
}

void TacticalVector::Set(float x, float y, float z) {
    m_x = x;
    m_y = y;
    m_z = z;
    m_cachedLength = -1.0f;
}

int TacticalVector::MatchesXYZ(const TacticalVector* other) {
    return m_x == other->m_x && m_y == other->m_y && m_z == other->m_z;
}

void TacticalVector::Scale(float factor) {
    m_x = static_cast<float>(static_cast<double>(m_x) * factor);
    m_cachedLength = -1.0f;
    m_y = static_cast<float>(static_cast<double>(factor) * m_y);
    m_z = static_cast<float>(static_cast<double>(m_z) * factor);
}
