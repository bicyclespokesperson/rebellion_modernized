#pragma once
#include <cstdint>

#include "data/table_entry.h"

// Set of seed item keys (a tree of 0x28-byte key nodes), owned by the game object module (ctor 0x4f3f70, dtor
// 0x4f3fb0, Read 0x4f40b0, Write 0x4f41c0, Checksum 0x4f4220, add-by-key 0x4f42a0). Opaque here.
struct SeedItemSet {
    uint32_t m_words[4];
};

// Entry of SYFCCRTB.DAT / SYFCRMTB.DAT: a game object key (family in the top byte, id in the low 24 bits).
class SeedTableEntry : public TableEntry {
public:
    SeedTableEntry();
    virtual ~SeedTableEntry();
    virtual int Write(ostream* out) override;
    virtual int Read(istream* in) override;
    virtual int ParseText(istream* in) override;
    virtual int Checksum(Crc32* crc) override;

    uint32_t m_key;  // +0x24
};

// Entry of the CMUN*TB / FACL*TB tables: a number and a set of keys. The text form lists the entry's own key
// followed by four groups of (family, id, count), each adding `count` copies of the key.
class SeedFamilyTableEntry : public TableEntry {
public:
    SeedFamilyTableEntry();
    virtual ~SeedFamilyTableEntry();
    virtual int Write(ostream* out) override;
    virtual int Read(istream* in) override;
    virtual int ParseText(istream* in) override;
    virtual int Checksum(Crc32* crc) override;

    int m_field24;       // +0x24
    SeedItemSet m_items;  // +0x28
};

// Entry of the integer lookup tables (the *MSTB tables and other gameplay tables): the inherited +0x20 is the
// threshold of the step, +0x24 the value that applies from it.
class IntTableEntry : public TableEntry {
public:
    IntTableEntry();
    virtual ~IntTableEntry();
    virtual int Write(ostream* out) override;
    virtual int Read(istream* in) override;
    virtual int ParseText(istream* in) override;
    virtual int Checksum(Crc32* crc) override;

    int m_value;  // +0x24
};

// Factories registered under the entry class names.
void* CreateSeedTableEntry();
void* CreateSeedFamilyTableEntry();
void* CreateIntTableEntry();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SeedTableEntry, 0x28);
REB_ASSERT_OFFSET(SeedTableEntry, m_key, 0x24);
REB_ASSERT_SIZE(SeedFamilyTableEntry, 0x38);
REB_ASSERT_OFFSET(SeedFamilyTableEntry, m_items, 0x28);
REB_ASSERT_SIZE(IntTableEntry, 0x28);
REB_ASSERT_OFFSET(IntTableEntry, m_value, 0x24);
#pragma clang diagnostic pop
