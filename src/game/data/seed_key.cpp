#include "data/seed_key.h"

#include "core/utility_classes.h"

SeedKey::SeedKey()
    : m_key(0)
    , m_second(0)
    , m_third(0)
    , m_fourth(0)
    , m_fifth(0)
{
}

SeedKey::SeedKey(const SeedKey& other) = default;

SeedKey& SeedKey::operator=(const SeedKey& other) = default;

int SeedKey::operator==(const SeedKey& other) const
{
  return m_key == other.m_key && m_second == other.m_second && m_third == other.m_third && m_fourth == other.m_fourth
         && m_fifth == other.m_fifth;
}

int SeedKey::operator<(const SeedKey& other) const
{
  if (m_key != other.m_key) {
    return m_key < other.m_key;
  }
  if (m_second != other.m_second) {
    return m_second < other.m_second;
  }
  if (m_third != other.m_third) {
    return m_third < other.m_third;
  }
  if (m_fourth != other.m_fourth) {
    return m_fourth < other.m_fourth;
  }
  return m_fifth < other.m_fifth;
}

int SeedKey::Read(istream* in)
{
  StreamRead32(in, &m_key);
  StreamRead32(in, &m_second);
  StreamRead32(in, &m_third);
  StreamRead32(in, &m_fourth);
  StreamRead32(in, &m_fifth);
  return 1;
}

int SeedKey::Write(ostream* out) const
{
  StreamWrite32(out, &m_key);
  StreamWrite32(out, &m_second);
  StreamWrite32(out, &m_third);
  StreamWrite32(out, &m_fourth);
  StreamWrite32(out, &m_fifth);
  return 1;
}

int SeedKey::AddToChecksum(Crc32* crc) const
{
  crc->AddInt(m_key);
  crc->AddInt(m_second);
  crc->AddInt(m_third);
  crc->AddInt(m_fourth);
  crc->AddInt(m_fifth);
  return 1;
}

int SeedKey::ReadValue(istream* in)
{
  return Read(in);
}

int SeedKey::WriteValue(ostream* out)
{
  return Write(out);
}
