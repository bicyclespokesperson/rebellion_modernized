#include "gameobj/side_bits.h"

#include "core/utility_classes.h"
#include "data/gnprtb.h"

namespace
{

constexpr uint32_t kDefaultValue = 0xf;
constexpr uint32_t kUnusedBits = 0xffffffc0;

const char* sideName(uint32_t side)
{
  switch (side) {
    case 1:
      return "Alliance";
    case 2:
      return "Empire  ";
    case 3:
      return "Neutral ";
    default:
      return "UNKNOWN!";
  }
}

} // namespace

SideBits::SideBits()
{
  Reset();
}

SideBits::SideBits(const SideBits& other)
{
  Reset();
  Assign(other);
}

SideBits::SideBits(uint32_t value)
{
  Reset();
  const bool controllerSet = (value & 3) != 0;
  const bool loyaltySet = ((value >> 2) & 3) != 0;
  if (controllerSet && loyaltySet && (value & kUnusedBits) == 0) {
    m_value = value;
  }
}

SideBits& SideBits::operator=(const SideBits& other)
{
  if (this != &other) {
    Assign(other);
  }
  return *this;
}

bool SideBits::operator==(const SideBits& other) const
{
  return m_value == other.m_value;
}

void SideBits::Reset()
{
  m_value = kDefaultValue;
}

void SideBits::Assign(const SideBits& other)
{
  m_value = other.m_value;
}

void SideBits::SetController(uint32_t side)
{
  m_value = ((side ^ m_value) & 3) ^ m_value;
}

void SideBits::SetLoyalty(uint32_t side)
{
  m_value = (m_value & 0xfffffff3) | ((side & 3) << 2);
}

void SideBits::SetGarrison(uint32_t side)
{
  m_value = (m_value & 0xffffffcf) | ((side & 3) << 4);
}

int SideBits::Read(istream* in)
{
  StreamRead32(in, this);
  return 1;
}

int SideBits::Write(ostream* out)
{
  StreamWrite32(out, this);
  return 1;
}

int SideBits::Checksum(Crc32* crc)
{
  crc->AddInt(static_cast<int>(m_value));
  return 1;
}

int SideBits::AppendDescription(std::string* out)
{
  std::string suffix(")");
  std::string garrison(sideName(Garrison()));
  std::string garrisonLabel(",G=");
  std::string loyalty(sideName(Loyalty()));
  std::string loyaltyLabel(" (L=");
  std::string controller(sideName(Controller()));
  *out += controller;
  *out += loyaltyLabel;
  *out += loyalty;
  *out += garrisonLabel;
  *out += garrison;
  *out += suffix;
  return 1;
}

int BindGnprtb12()
{
  const int first = BindGnprtbParameter(0x1200, &g_gnprtb1200);
  const int second = BindGnprtbParameter(0x1201, &g_gnprtb1201);
  return second != 0 && first != 0;
}
