#include "gameobj/tracked_stat.h"

#include "core/utility_classes.h"

namespace
{
constexpr uint32_t kMaxLevel = 0x7fff;
constexpr uint32_t kMaxProgress = 0x3fff;
} // namespace

TrackedStat::TrackedStat()
{
  Reset();
}

TrackedStat::TrackedStat(const TrackedStat& other)
{
  Reset();
  CopyFrom(other);
}

TrackedStat& TrackedStat::operator=(const TrackedStat& other)
{
  if (this != &other) {
    CopyFrom(other);
  }
  return *this;
}

void TrackedStat::Reset()
{
  m_changeCount = 0;
  m_packed = 0;
}

void TrackedStat::CopyFrom(const TrackedStat& other)
{
  m_changeCount = other.m_changeCount;
  m_packed = other.m_packed;
}

void TrackedStat::SetFlag(uint32_t on)
{
  if ((m_packed & 1) != on) {
    m_packed = ((on ^ m_packed) & 1) ^ m_packed;
    ++m_changeCount;
  }
}

void TrackedStat::SetLevel(uint32_t level)
{
  if (static_cast<int>(level) > static_cast<int>(kMaxLevel)) {
    level = kMaxLevel;
  }
  if ((m_packed >> 16) != level) {
    m_packed = (m_packed & 0xffff) | (level << 16);
    ++m_changeCount;
    if (static_cast<int>(level) < 0) {
      SetFlag(0);
    }
  }
}

void TrackedStat::SetProgress(uint32_t progress)
{
  if (static_cast<int>(progress) > static_cast<int>(kMaxProgress)) {
    progress = kMaxProgress;
  }
  if (((m_packed >> 1) & 0x7fff) != progress) {
    m_packed = (m_packed & 0xffff0001) | ((progress & 0x7fff) << 1);
    ++m_changeCount;
  }
}

int TrackedStat::Read(istream* in)
{
  return ReadFields(in);
}

int TrackedStat::Write(ostream* out)
{
  return WriteFields(out);
}

int TrackedStat::ReadFields(istream* in)
{
  StreamRead32(in, &m_changeCount);
  StreamRead32(in, &m_packed);
  return 1;
}

int TrackedStat::WriteFields(ostream* out)
{
  StreamWrite32(out, &m_changeCount);
  StreamWrite32(out, &m_packed);
  return 1;
}

int TrackedStat::Checksum(Crc32* crc)
{
  crc->AddInt(m_changeCount);
  crc->AddInt(static_cast<int>(m_packed));
  return 1;
}
