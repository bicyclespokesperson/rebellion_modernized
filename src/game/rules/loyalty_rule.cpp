#include "rules/loyalty_rule.h"

#include <new>

namespace
{

constexpr uint32_t kSystemFamiliesFirst = 0x90;
constexpr uint32_t kSystemFamiliesEnd = 0x98;
constexpr int kDefaultThreshold = 100;

} // namespace

LoyaltyRule::LoyaltyRule() : m_familyRange()
{
  Reset();
}

LoyaltyRule::~LoyaltyRule() = default;

LoyaltyRule& LoyaltyRule::operator=(const LoyaltyRule& other)
{
  if (this != &other)
    CopyFrom(other);
  return *this;
}

void LoyaltyRule::Reset()
{
  m_controlSide = 0;
  m_familyRange = IntPair(kSystemFamiliesFirst, kSystemFamiliesEnd);
  m_unitSide = 1;
  m_threshold = kDefaultThreshold;
  m_direction = 1;
  m_amount = 0;
}

void LoyaltyRule::CopyFrom(const LoyaltyRule& other)
{
  m_controlSide = other.m_controlSide;
  m_familyRange = other.m_familyRange;
  m_unitSide = other.m_unitSide;
  m_threshold = other.m_threshold;
  m_direction = other.m_direction;
  m_amount = other.m_amount;
}

IntPair* LoyaltyRule::GetFamilyRange(IntPair* out)
{
  new (out) IntPair(m_familyRange);
  return out;
}

void LoyaltyRule::SetFamilyRange(uint32_t first, uint32_t second)
{
  m_familyRange = IntPair(first, second);
}
