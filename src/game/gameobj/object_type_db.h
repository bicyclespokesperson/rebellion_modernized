#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"

// The object-type database: the in-memory index of the game's data-table records (DatRecord, owned by the data-table
// module) grouped by family. Each family entry owns the records whose key family lies in its range; a record is found
// from a SeedKey (family + id). The facility/unit "levels" the strategic UI shows (kinds 0x28-0x2b) are also queried
// here.
//
// Layout of the pieces this module touches (all defined by the data-table module):
//   FamilyTable   IdAvlTree of family entries (0x58ec40), m_ok at +0xc
//   family entry  0x3c bytes: +0x1c ok flag, +0x20 loader object, +0x28/+0x2c family range, +0x30 IdAvlTree of records
//   record        +0x24 IntPair kind range, +0x30 name resource id, +0x40/+0x44 per-view availability, +0x50 level,
//                 +0x54 extra value

class DatRecord;
class String;
class ObjectRefList;

// The table of family entries (0x58ec40, data-table module): an IdAvlTree keyed by family-entry id.
class FamilyTable : public IdAvlTree {
public:
    virtual ~FamilyTable();

    int m_ok;  // +0x0c
};

class ObjectTypeDb {
public:
    ObjectTypeDb();
    virtual ~ObjectTypeDb();

    // Replaces the family table with a new empty one; returns its ok flag.
    int BuildFamilies();
    // Builds the table, loads the 17 standard families and checks that each of the six (view, kind) level sets is
    // consistent. Returns 0 on any failure.
    int LoadFamilies();
    // Creates a family entry for the resource pair (a, b), names its records and adds it. Returns the entry's id.
    int AddFamily(int a, int b);
    // Calls the loader object of every family entry with `arg`; stops at the first failure.
    int ForEachFamily(int arg);
    DatRecord* FindRecord(const SeedKey* key);
    DatRecord* FirstRecordOfFamily(uint32_t family);
    // Adds a key node for every record of every family to `out` (cleared first).
    int ListAllRecordKeys(ObjectRefList* out);
    // Builds in `out` one node per record of kind `kind` (0x28-0x2b) that is available to `view` (1 or 2), choosing by
    // the record's level compared with `level` whether `whenBelow`, `whenEqual` or `whenAbove` is non-zero; *maxLevel is
    // the highest level of any available record.
    int CollectRecords(int view, uint32_t kind, int level, int whenBelow, int whenEqual, int whenAbove, IdAvlTree* out,
                       int* maxLevel);
    // The record of exactly `level` among the available ones (null if none).
    int FindRecordAtLevel(int view, uint32_t kind, int level, DatRecord** out);
    // First level above `level` that has a record; *found says whether one exists, *foundLevel/*extraValue describe it.
    int FindNextLevel(int view, int kind, int level, int* found, int* foundLevel, int* extraValue);
    // *unique is 0 if some level has more than one record.
    int AreLevelsUnique(int view, uint32_t kind, int* unique);
    // "<name> (ID=n)" for the record named by `key`, a "can't find name" form if it has none, "<empty SeedKey>" if empty.
    void DescribeKey(const SeedKey* key, String* out);

    int m_ok;                // +0x04
    FamilyTable* m_families;  // +0x08
};

extern ObjectTypeDb* g_objectTypeDb;  // 0x6bb7a4

ObjectTypeDb* GetObjectTypeDb();
void CreateObjectTypeDb();
void DestroyObjectTypeDb();

// The kind range of a record (copies record+0x24).
IntPair* __thiscall GetRecordKindRange(DatRecord* record, IntPair* out);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(ObjectTypeDb, 0x0c);
REB_ASSERT_OFFSET(ObjectTypeDb, m_families, 0x08);
#pragma clang diagnostic pop
