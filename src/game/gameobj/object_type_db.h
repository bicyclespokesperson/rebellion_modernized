#pragma once
class ObjectNameList;
#include <cstdint>
#include <vector>

#include "core/entry_identity.h"
#include "core/game_string.h"
#include "data/seed_key.h"
#include "data/seed_list.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/object_key.h"
#include "gameobj/status_code.h"

// The object-type database: the in-memory index of the game's data-table records (DatRecord, owned by the data-table
// module) grouped by family. Each family entry owns the records whose key family lies in its range; a record is found
// from a SeedKey (family + id). The facility/unit "levels" the strategic UI shows (kinds 0x28-0x2b) are also queried
// here.
//
// Layout of the pieces this module touches (all defined by the data-table module):
//   SeedListFileTree   ID registry of loaded seed-list files (0x58ec40), m_ready at +0xc
//   family entry  0x3c bytes: +0x1c ok flag, +0x20 loader object, +0x28/+0x2c family range, +0x30 ID registry of
//   records record        a DatRecord: kind range (m_first/m_second), name resource id (m_small); records used for
//   level
//                 queries are actual SdRecord-derived objects with per-view availability and level fields

class DatRecord;
class Crc32;
class ObjectRefList;

class ObjectTypeDb
{
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
  int ForEachFamily(Crc32* crc);
  DatRecord* FindRecord(const SeedKey* key);
  std::vector<DatRecord*> RecordsOfFamily(uint32_t family);
  // Adds a key node for every record of every family to `out` (cleared first).
  int ListAllRecordKeys(ObjectRefList* out);
  // Builds in `out` one node per record of kind `kind` (0x28-0x2b) that is available to `view` (1 or 2), choosing by
  // the record's level compared with `level` whether `whenBelow`, `whenEqual` or `whenAbove` is non-zero; *maxLevel is
  // the highest level of any available record.
  int CollectRecords(int view, uint32_t kind, int level, int whenBelow, int whenEqual, int whenAbove,
                     ObjectNameList* out, int* maxLevel);
  // The record of exactly `level` among the available ones (null if none).
  int FindRecordAtLevel(int view, uint32_t kind, int level, DatRecord** out);
  // First level above `level` that has a record; *found says whether one exists, *foundLevel/*extraValue describe it.
  int FindNextLevel(int view, int kind, int level, int* found, int* foundLevel, int* extraValue);
  // *unique is 0 if some level has more than one record.
  int AreLevelsUnique(int view, uint32_t kind, int* unique);
  // "<name> (ID=n)" for the record named by `key`, a "can't find name" form if it has none, "<empty SeedKey>" if empty.
  void DescribeKey(const SeedKey* key, std::string* out);

  int m_ok;                     // +0x04
  SeedListFileTree* m_families; // +0x08
};

extern ObjectTypeDb* g_objectTypeDb; // 0x6bb7a4

ObjectTypeDb* GetObjectTypeDb();
void CreateObjectTypeDb();
void DestroyObjectTypeDb();

// Wrappers over the object-type database (0x51cab0-0x51cce0): all do nothing and return 0 while there is no database.
DatRecord* FindDataRecord(const SeedKey* key); // 0x51cab0
// Callers hold the key as an ObjectKey or NullableKey; the lookup reads only the first field of the SeedKey.
inline const SeedKey* AsLookupKey(const void* key)
{
  return static_cast<const SeedKey*>(key);
}
std::vector<DatRecord*> GetRecordsOfType(int family);
int ListAllDataRecordKeys(ObjectRefList* out); // 0x51caf0: clears `out` first
int CollectDataRecords(int view, uint32_t kind, int level, int whenBelow, int whenEqual, int whenAbove,
                       ObjectNameList* out,
                       int* maxLevel);                                                              // 0x51cb20
int FindDataRecordAtLevel(int view, uint32_t kind, int level, DatRecord** out);                     // 0x51cb60
int FindNextDataLevel(int view, int kind, int level, int* found, int* foundLevel, int* extraValue); // 0x51cb90
// The same for the facility kinds 0x28, 0x29 and 0x2a.
int GetTierRecord28(int view, int level, DatRecord** out);                             // 0x51cbd0
int GetTierRecord29(int view, int level, DatRecord** out);                             // 0x51cbf0
int GetTierRecord2a(int view, int level, DatRecord** out);                             // 0x51cc10
int FindNextTier28(int view, int level, int* found, int* foundLevel, int* extraValue); // 0x51cc30
int FindNextTier29(int view, int level, int* found, int* foundLevel, int* extraValue); // 0x51cc60
int FindNextTier2a(int view, int level, int* found, int* foundLevel, int* extraValue); // 0x51cc90
void DescribeDataKey(const SeedKey* key, std::string* out);                            // 0x51ccc0
int ForEachDataFamily(Crc32* crc);                                                     // 0x51cce0

// The kind range of a record (copies record+0x24).
IntPair* GetRecordKindRange(DatRecord* record, IntPair* out);
