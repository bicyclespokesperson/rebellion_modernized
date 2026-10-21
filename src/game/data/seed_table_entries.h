#pragma once
#include <cstdint>

#include "data/table_entry.h"
#include "gameobj/object_key.h"

// Entry of SYFCCRTB.DAT / SYFCRMTB.DAT: a game object key (family in the top byte, id in the low 24 bits).
class SeedTableEntry : public TableEntry
{
  public:
  SeedTableEntry();
  ~SeedTableEntry() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int ParseText(istream* in) override;
  int Checksum(Crc32* crc) override;
  int FirstValue() const override
  {
    return static_cast<int>(m_key);
  }

  uint32_t m_key; // +0x24
};

// Entry of the CMUN*TB / FACL*TB tables: a number and a set of keys. The text form lists the entry's own key
// followed by four groups of (family, id, count), each adding `count` copies of the key.
class SeedFamilyTableEntry : public TableEntry
{
  public:
  SeedFamilyTableEntry();
  ~SeedFamilyTableEntry() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int ParseText(istream* in) override;
  int Checksum(Crc32* crc) override;
  int FirstValue() const override
  {
    return m_field24;
  }

  int m_field24;         // +0x24
  ObjectRefList m_items; // +0x28
};

// Entry of the integer lookup tables (the *MSTB tables and other gameplay tables): the inherited +0x20 is the
// threshold of the step, +0x24 the value that applies from it.
class IntTableEntry : public TableEntry
{
  public:
  IntTableEntry();
  ~IntTableEntry() override;
  int Write(ostream* out) override;
  int Read(istream* in) override;
  int ParseText(istream* in) override;
  int Checksum(Crc32* crc) override;
  int FirstValue() const override
  {
    return m_value;
  }

  int m_value; // +0x24
};

// Factories registered under the entry class names.
void* CreateSeedTableEntry();
void* CreateSeedFamilyTableEntry();
void* CreateIntTableEntry();
