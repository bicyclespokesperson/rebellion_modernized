#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "data/seed_key.h"
#include "data/table.h"

class Crc32;
class DatRecord;

// The class named "SeedList" in the table class registry: the loader of one *SD.DAT file (ships, characters,
// systems, facilities, ...). The file has a version, the number of records and the range [low, high) of record
// families it holds; each record is created from the family range through the SD record factory registry
// (RegisterRecordFactories) and then Read() from the file. The records are DatRecords keyed by their ids.
struct RecordFamilyRange {
    uint32_t m_low;
    uint32_t m_high;
};

class SeedList : public TableBase {
public:
    SeedList();
    virtual ~SeedList();
    virtual int Read(istream* in) override;
    virtual int Write(ostream* out) override;
    virtual int ParseText(istream* in) override;
    virtual int Checksum(Crc32* crc);

    // Adds a record to the list (nothing for null).
    void AddRecord(IdNode* record);

    RecordFamilyRange m_range;  // +0x08
    IdAvlTree m_records;        // +0x10
};

// A SeedList loaded from a file, keyed by the id the SD record registry gives it.
class SeedListFile : public IdNode {
public:
    SeedListFile();
    virtual ~SeedListFile();

    // Reads the list from the file `<resource string firstId><resource string secondId>`; see TableBase::Open.
    int Load(uint16_t firstId, uint16_t secondId);

    int m_version;     // +0x1c
    SeedList m_list;   // +0x20
};

// All loaded SeedListFiles.
class SeedListFileTree : public IdAvlTree {
public:
    SeedListFileTree();
    virtual ~SeedListFileTree();

    // The file whose family range contains `family` (the top byte of a record key), or null.
    SeedListFile* FindByFamily(uint32_t family);

    int m_ready;  // +0x0c, 1
};

// Display data cached for one record: its key and its localized name.
class RecordNameNode : public IdNode {
public:
    RecordNameNode();
    virtual ~RecordNameNode();

    // Takes the key and the name (resource string id at +0x30) of `record`. Returns whether the name is valid.
    int SetFromRecord(DatRecord* record);

    int m_ready;      // +0x1c, the validity of m_name after construction
    uint32_t m_key;   // +0x20
    String m_name;    // +0x24
};

// A seed key with a quantity, saved with the seed key's five values followed by the count.
class SeedKeyCountNode : public AvlNode {
public:
    SeedKeyCountNode();
    explicit SeedKeyCountNode(const SeedKey& key);
    SeedKeyCountNode(const SeedKeyCountNode& other);
    virtual ~SeedKeyCountNode();
    virtual SeedKeyCountNode* Clone();
    virtual void Load(istream* in);
    virtual void Read(istream* in);
    virtual void Save(ostream* out);
    virtual void Write(ostream* out);
    virtual int Checksum(Crc32* crc);

    SeedKey m_key;   // +0x18
    int m_count;     // +0x2c
};

// Registers the "SeedList" class with the table class registry (a static initializer of the original).
void InitSeedListClassRegistration();
void* CreateSeedList();

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(SeedList, 0x1c);
REB_ASSERT_OFFSET(SeedList, m_range, 0x08);
REB_ASSERT_OFFSET(SeedList, m_records, 0x10);
REB_ASSERT_SIZE(SeedListFile, 0x3c);
REB_ASSERT_OFFSET(SeedListFile, m_list, 0x20);
REB_ASSERT_SIZE(SeedListFileTree, 0x10);
REB_ASSERT_SIZE(RecordNameNode, 0x30);
REB_ASSERT_OFFSET(RecordNameNode, m_name, 0x24);
REB_ASSERT_SIZE(SeedKeyCountNode, 0x30);
REB_ASSERT_OFFSET(SeedKeyCountNode, m_key, 0x18);
REB_ASSERT_OFFSET(SeedKeyCountNode, m_count, 0x2c);
#pragma clang diagnostic pop
