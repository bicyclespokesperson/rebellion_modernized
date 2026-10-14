#include "data/seed_table_entries.h"

#include "core/checked_new.h"
#include "core/utility_classes.h"
#include "data/dat_record.h"
#include "gameobj/object_key.h"

namespace
{

constexpr int kSeedGroupCount = 4;
constexpr uint32_t kKeyIdMask = 0xffffff;

uint32_t makeKey(uint32_t family, uint32_t id)
{
  return (family << 24) | (id & kKeyIdMask);
}

} // namespace

// ---- SeedTableEntry ----------------------------------------------------------------------------------------

SeedTableEntry::SeedTableEntry() : m_key(0)
{
}

SeedTableEntry::~SeedTableEntry() = default;

void* CreateSeedTableEntry()
{
  return NewOrNull<SeedTableEntry>();
}

int SeedTableEntry::Read(istream* in)
{
  TableEntry::Read(in);
  StreamRead32(in, &m_key);
  return 1;
}

int SeedTableEntry::Write(ostream* out)
{
  TableEntry::Write(out);
  StreamWrite32(out, &m_key);
  return 1;
}

int SeedTableEntry::Checksum(Crc32* crc)
{
  const int result = TableEntry::Checksum(crc);
  crc->AddInt(m_key);
  return result;
}

int SeedTableEntry::ParseText(istream* in)
{
  ScopedStrstream token;
  int endOfLine = 0;
  const int ok = ParseHeader(in, &endOfLine);
  if (ok)
  {
    unsigned int family;
    unsigned int id;
    ParseUnsignedField(in, token.get(), &endOfLine, &family);
    ParseUnsignedField(in, token.get(), &endOfLine, &id);
    m_key = makeKey(family, id);
  }
  SkipRestOfLine(in, &endOfLine);
  return ok;
}

// ---- SeedFamilyTableEntry ----------------------------------------------------------------------------------

SeedFamilyTableEntry::SeedFamilyTableEntry() : m_field24(0)
{
}

SeedFamilyTableEntry::~SeedFamilyTableEntry() = default;

void* CreateSeedFamilyTableEntry()
{
  return NewOrNull<SeedFamilyTableEntry>();
}

int SeedFamilyTableEntry::Read(istream* in)
{
  TableEntry::Read(in);
  ReadValue32(in, &m_field24);
  m_items.Read(in);
  return 1;
}

int SeedFamilyTableEntry::Write(ostream* out)
{
  TableEntry::Write(out);
  WriteValue32(out, &m_field24);
  m_items.Write(out);
  return 1;
}

int SeedFamilyTableEntry::Checksum(Crc32* crc)
{
  const int result = TableEntry::Checksum(crc);
  crc->AddInt(m_field24);
  m_items.Checksum(crc);
  return result;
}

int SeedFamilyTableEntry::ParseText(istream* in)
{
  ScopedStrstream token;
  int endOfLine = 0;
  bool ok = ParseHeader(in, &endOfLine);
  if (ok)
  {
    unsigned int family;
    unsigned int id;
    ParseLongField(in, token.get(), &endOfLine, &m_field24);
    ParseUnsignedField(in, token.get(), &endOfLine, &family);
    ParseUnsignedField(in, token.get(), &endOfLine, &id);
    ok = m_items.AddKey(ObjectKey::Make(family, id)) != nullptr;
  }
  for (int group = 0; ok && group < kSeedGroupCount; ++group)
  {
    unsigned int family;
    unsigned int id;
    int count;
    ParseUnsignedField(in, token.get(), &endOfLine, &family);
    ParseUnsignedField(in, token.get(), &endOfLine, &id);
    ParseLongField(in, token.get(), &endOfLine, &count);
    for (int i = 0; ok && i < count; ++i)
      ok = m_items.AddKey(ObjectKey::Make(family, id)) != nullptr;
  }
  SkipRestOfLine(in, &endOfLine);
  return ok;
}

// ---- IntTableEntry -----------------------------------------------------------------------------------------

IntTableEntry::IntTableEntry() : m_value(0)
{
}

IntTableEntry::~IntTableEntry() = default;

void* CreateIntTableEntry()
{
  return NewOrNull<IntTableEntry>();
}

int IntTableEntry::Read(istream* in)
{
  TableEntry::Read(in);
  ReadValue32(in, &m_value);
  return 1;
}

int IntTableEntry::Write(ostream* out)
{
  TableEntry::Write(out);
  WriteValue32(out, &m_value);
  return 1;
}

int IntTableEntry::Checksum(Crc32* crc)
{
  const int result = TableEntry::Checksum(crc);
  crc->AddInt(m_value);
  return result;
}

int IntTableEntry::ParseText(istream* in)
{
  ScopedStrstream token;
  int endOfLine = 0;
  const int ok = ParseHeader(in, &endOfLine);
  if (ok)
    ParseLongField(in, token.get(), &endOfLine, &m_value);
  SkipRestOfLine(in, &endOfLine);
  return ok;
}
