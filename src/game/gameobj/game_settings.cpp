#include "gameobj/game_settings.h"

#include "core/utility_classes.h"

namespace
{

constexpr int kDefaultField0c = 7;

} // namespace

GameSettings::GameSettings() : m_field04(0), m_field08(0), m_field0c(kDefaultField0c), m_field10(0)
{
}

GameSettings::~GameSettings() = default;

GameSettings& GameSettings::operator=(const GameSettings& other)
{
  if (this != &other)
  {
    m_field04 = other.m_field04;
    m_field08 = other.m_field08;
    m_field0c = other.m_field0c;
    m_field10 = other.m_field10;
  }
  return *this;
}

int GameSettings::operator==(const GameSettings& other) const
{
  return m_field04 == other.m_field04 && m_field08 == other.m_field08 && m_field0c == other.m_field0c &&
         m_field10 == other.m_field10;
}

int GameSettings::Read(istream* in)
{
  int value;
  ReadValue32(in, &m_field04);
  ReadValue32(in, &value);
  m_field08 = value;
  ReadValue32(in, &value);
  m_field0c = value;
  ReadValue32(in, &m_field10);
  return 1;
}

int GameSettings::Checksum(Crc32* crc)
{
  crc->AddInt(m_field04);
  crc->AddInt(m_field08);
  crc->AddInt(m_field0c);
  crc->AddInt(m_field10);
  return 1;
}

int GameSettings::Write(ostream* out)
{
  WriteValue32(out, &m_field04);
  int value = m_field08;
  WriteValue32(out, &value);
  value = m_field0c;
  WriteValue32(out, &value);
  WriteValue32(out, &m_field10);
  return 1;
}
