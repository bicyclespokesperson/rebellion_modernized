#include "gameobj/object_type_db.h"

#include "ai/ai_common.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "data/dat_record.h"
#include "data/seed_list.h"
#include "economy/side.h"
#include "gameobj/object_name_node.h"

namespace
{

constexpr uint32_t kFirstLevelKind = 0x28;
constexpr uint32_t kEndLevelKind = 0x2c;
constexpr uint32_t kUnitFamilyFirst = 0x10;
constexpr uint32_t kUnitFamilyEnd = 0x40;
constexpr uint32_t kFamilyResourceBase = 0x642;
constexpr int kFamilyNameResources[] = {0x6b4, 0x6a9, 0x6ac, 0x6a8, 0x6ab, 0x6b2, 0x6ad, 0x6a7, 0x6b5,
                                        0x6ae, 0x6b0, 0x6a6, 0x6a4, 0x6af, 0x6a5, 0x6aa, 0x6b3};

IdAvlTree* RecordsOf(void* entry)
{
  return &static_cast<SeedListFile*>(entry)->m_list.m_records;
}

DatRecord* AsRecord(AvlNode* node)
{
  return static_cast<DatRecord*>(static_cast<IdNode*>(node));
}

const ResourceStringId* NameResource(const DatRecord* record)
{
  return reinterpret_cast<const ResourceStringId*>(&record->m_small);
}

bool Available(const SdRecord* record, int view)
{
  return (view == 1 && record->m_field40 != 0) || (view == 2 && record->m_field44 != 0);
}

} // namespace

// ---- ObjectTypeDb ------------------------------------------------------------------------------------------

ObjectTypeDb::ObjectTypeDb() : m_families(nullptr)
{
  m_ok = BuildFamilies();
}

ObjectTypeDb::~ObjectTypeDb()
{
  if (m_families)
  {
    delete m_families;
    m_families = nullptr;
  }
}

int ObjectTypeDb::BuildFamilies()
{
  if (m_families)
  {
    delete m_families;
    m_families = nullptr;
  }
  void* memory = ::operator new(sizeof(SeedListFileTree));
  m_families = memory ? new (memory) SeedListFileTree() : nullptr;
  return m_families ? m_families->m_ready : 0;
}

int ObjectTypeDb::LoadFamilies()
{
  int ok = BuildFamilies();
  if (!ok)
    return 0;
  for (int nameResource : kFamilyNameResources)
  {
    ok = AddFamily(kFamilyResourceBase, nameResource);
    if (!ok)
      return 0;
  }
  int unique = 1;
  ok = AreLevelsUnique(1, 0x28, &unique);
  if (!ok)
    return 0;
  if (unique)
    ok = AreLevelsUnique(1, 0x29, &unique);
  if (!ok)
    return 0;
  if (unique)
    ok = AreLevelsUnique(1, 0x2a, &unique);
  if (!ok)
    return 0;
  if (unique)
    ok = AreLevelsUnique(2, 0x28, &unique);
  if (!ok)
    return 0;
  if (unique)
    ok = AreLevelsUnique(2, 0x29, &unique);
  if (!ok)
    return 0;
  if (unique)
    ok = AreLevelsUnique(2, 0x2a, &unique);
  return ok;
}

int ObjectTypeDb::AddFamily(int a, int b)
{
  void* memory = ::operator new(sizeof(SeedListFile));
  void* entry = memory ? new (memory) SeedListFile() : nullptr;
  int ok = entry != nullptr;
  if (ok)
  {
    ok = static_cast<SeedListFile*>(entry)->m_version;
    if (ok)
      ok = static_cast<SeedListFile*>(entry)->Load(a, b);
  }
  if (entry)
  {
    for (const auto& ownedRecord : RecordsOf(entry)->Entries())
    {
      if (!ok)
        break;
      DatRecord* datRecord = AsRecord(ownedRecord.get());
      datRecord->m_name = LoadStringResource(NameResource(datRecord));
    }
  }
  if (ok)
    ok = static_cast<int>(m_families->AddWithId(static_cast<IdNode*>(entry)));
  return ok;
}

int ObjectTypeDb::ForEachFamily(Crc32* crc)
{
  int ok = 1;
  for (const auto& family : m_families->Entries())
  {
    if (!ok)
      break;
    AvlNode* entry = family.get();
    ok = static_cast<SeedListFile*>(entry)->m_list.Checksum(crc);
  }
  return ok;
}

DatRecord* ObjectTypeDb::FindRecord(const SeedKey* key)
{
  DatRecord* record = nullptr;
  if (SeedListFile* entry = m_families->FindByFamily(key->m_key >> 24))
  {
    record = AsRecord(RecordsOf(entry)->FindById(key->m_key & 0xffffff));
  }
  return record;
}

std::vector<DatRecord*> ObjectTypeDb::RecordsOfFamily(uint32_t family)
{
  std::vector<DatRecord*> records;
  SeedListFile* entry = m_families ? m_families->FindByFamily(family) : nullptr;
  if (!entry)
    return records;
  const auto& storedRecords = RecordsOf(entry)->Entries();
  records.reserve(storedRecords.size());
  for (const auto& storedRecord : storedRecords)
    records.push_back(AsRecord(storedRecord.get()));
  return records;
}

int ObjectTypeDb::ListAllRecordKeys(ObjectRefList* out)
{
  out->Clear();
  int ok = m_families != nullptr;
  if (!ok)
    return 0;
  for (const auto& family : m_families->Entries())
  {
    if (!ok)
      break;
    AvlNode* entry = family.get();
    for (const auto& storedRecord : RecordsOf(entry)->Entries())
    {
      if (!ok)
        break;
      AvlNode* record = storedRecord.get();
      ObjectKey key;
      key = ObjectKey::Make(AsRecord(record)->m_third, AsRecord(record)->m_id);
      ok = out->AddKey(key) != nullptr;
    }
  }
  return ok;
}

int ObjectTypeDb::CollectRecords(int view,
                                 uint32_t kind,
                                 int level,
                                 int whenBelow,
                                 int whenEqual,
                                 int whenAbove,
                                 IdAvlTree* out,
                                 int* maxLevel)
{
  *maxLevel = 0;
  bool valid = false;
  if (view >= 1 && view <= 2)
    valid = kind >= kFirstLevelKind && kind < kEndLevelKind;
  if (!valid)
    return 0;

  int ok = m_families != nullptr;
  if (!ok)
    return 0;
  for (const auto& family : m_families->Entries())
  {
    for (const auto& ownedRecord : RecordsOf(family.get())->Entries())
    {
      AvlNode* node = ownedRecord.get();
      DatRecord* record = AsRecord(node);
      IntPair range;
      GetRecordKindRange(record, &range);
      const bool kindMatches = kind >= range.m_first && kind < range.m_second;
      if (!kindMatches)
        continue;
      SdRecord* levelRecord = dynamic_cast<SdRecord*>(record);
      if (!levelRecord || !levelRecord->GetField48() || !Available(levelRecord, view))
        continue;

      const int recordLevel = static_cast<int>(levelRecord->m_field50);
      if (*maxLevel < recordLevel)
        *maxLevel = recordLevel;
      int wanted = whenBelow;
      if (level <= recordLevel)
        wanted = recordLevel == level ? whenEqual : whenAbove;
      if (!wanted)
        continue;

      ObjectNameNode* nameNode = NewOrNull<ObjectNameNode>();
      ok = nameNode != nullptr;
      if (nameNode)
      {
        ok = nameNode->Load(record);
        if (ok)
          ok = out->AddWithId(nameNode) != 0;
        if (!ok)
          delete nameNode;
      }
    }
  }
  return ok;
}

int ObjectTypeDb::FindRecordAtLevel(int view, uint32_t kind, int level, DatRecord** out)
{
  *out = nullptr;
  ObjectNameList names;
  int maxLevel = 0;
  int ok = CollectRecords(view, kind, level, 0, 1, 0, &names, &maxLevel);
  if (ok)
  {
    if (AvlNode* first = names.First())
    {
      ObjectKey key;
      GetObjectNameNodeKey(static_cast<ObjectNameNode*>(first), &key);
      const uint32_t family = key.Family();
      ok = 0;
      if (family >= kUnitFamilyFirst && family < kUnitFamilyEnd)
      {
        SeedKey seed;
        seed.m_key = key.m_value;
        *out = FindRecord(&seed);
        ok = *out != nullptr;
      }
    }
  }
  return ok;
}

int ObjectTypeDb::FindNextLevel(int view, int kind, int level, int* found, int* foundLevel, int* extraValue)
{
  *found = 0;
  *foundLevel = 0;
  *extraValue = 0;
  ObjectNameList names;
  int maxLevel = 0;
  DatRecord* record = nullptr;
  int ok = CollectRecords(view, static_cast<uint32_t>(kind), 0, 0, 0, 0, &names, &maxLevel);
  for (int candidate = level + 1; ok && candidate <= maxLevel && *found == 0; ++candidate)
  {
    ok = FindRecordAtLevel(view, static_cast<uint32_t>(kind), candidate, &record);
    if (ok)
    {
      *found = record != nullptr;
      if (*found)
      {
        *foundLevel = candidate;
        SdRecord* levelRecord = dynamic_cast<SdRecord*>(record);
        if (!levelRecord)
        {
          ok = 0;
          break;
        }
        *extraValue = static_cast<int>(levelRecord->m_field54);
      }
    }
  }
  return ok;
}

int ObjectTypeDb::AreLevelsUnique(int view, uint32_t kind, int* unique)
{
  *unique = 1;
  ObjectNameList names;
  int maxLevel = 0;
  int ok = CollectRecords(view, kind, 0, 0, 0, 0, &names, &maxLevel);
  for (int level = 1; ok && level <= maxLevel; ++level)
  {
    ObjectNameList atLevel;
    ok = CollectRecords(view, kind, level, 0, 1, 0, &atLevel, &maxLevel);
    if (ok)
    {
      const int count = atLevel.Count();
      if (count != 0 && count != 1)
        *unique = 0;
    }
  }
  return ok;
}

void ObjectTypeDb::DescribeKey(const SeedKey* key, std::string* out)
{
  out->clear();
  if (key->m_key == 0)
  {
    *out = "<empty SeedKey>";
    return;
  }
  const uint32_t id = key->m_key & 0xffffff;
  if (DatRecord* record = FindRecord(key))
  {
    *out += LoadStringResource(NameResource(record));
    *out += std::string(" (ID=");
    out->append(std::to_string(id));
    *out += std::string(")");
    return;
  }
  *out += std::string("<can't find name, type = ");
  out->append(std::to_string(key->m_key >> 24));
  *out += std::string("> ");
  *out += std::string(" (ID=");
  out->append(std::to_string(id));
  *out += std::string(")");
}

// ---- singleton ---------------------------------------------------------------------------------------------

ObjectTypeDb* GetObjectTypeDb()
{
  return g_objectTypeDb;
}

void CreateObjectTypeDb()
{
  DestroyObjectTypeDb();
  g_objectTypeDb = NewOrNull<ObjectTypeDb>();
  if (g_objectTypeDb && g_objectTypeDb->m_ok && g_objectTypeDb->LoadFamilies())
    return;
  DestroyObjectTypeDb();
}

void DestroyObjectTypeDb()
{
  if (g_objectTypeDb)
  {
    delete g_objectTypeDb;
    g_objectTypeDb = nullptr;
  }
}

IntPair* GetRecordKindRange(DatRecord* record, IntPair* out)
{
  *out = IntPair(record->m_first, record->m_second);
  return out;
}

// ---- wrappers (0x51cab0-0x51cce0) -----------------------------------------------------------------------------------

DatRecord* FindDataRecord(const SeedKey* key)
{
  ObjectTypeDb* db = GetObjectTypeDb();
  return db ? db->FindRecord(key) : nullptr;
}

std::vector<DatRecord*> GetRecordsOfType(int family)
{
  ObjectTypeDb* db = GetObjectTypeDb();
  return db ? db->RecordsOfFamily(static_cast<uint32_t>(family)) : std::vector<DatRecord*>{};
}

int ListAllDataRecordKeys(ObjectRefList* out)
{
  out->Clear();
  ObjectTypeDb* db = GetObjectTypeDb();
  return db ? db->ListAllRecordKeys(out) : 0;
}

int CollectDataRecords(int view,
                       uint32_t kind,
                       int level,
                       int whenBelow,
                       int whenEqual,
                       int whenAbove,
                       IdAvlTree* out,
                       int* maxLevel)
{
  ObjectTypeDb* db = GetObjectTypeDb();
  return db ? db->CollectRecords(view, kind, level, whenBelow, whenEqual, whenAbove, out, maxLevel) : 0;
}

int FindDataRecordAtLevel(int view, uint32_t kind, int level, DatRecord** out)
{
  ObjectTypeDb* db = GetObjectTypeDb();
  return db ? db->FindRecordAtLevel(view, kind, level, out) : 0;
}

int FindNextDataLevel(int view, int kind, int level, int* found, int* foundLevel, int* extraValue)
{
  ObjectTypeDb* db = GetObjectTypeDb();
  return db ? db->FindNextLevel(view, kind, level, found, foundLevel, extraValue) : 0;
}

int GetTierRecord28(int view, int level, DatRecord** out)
{
  return FindDataRecordAtLevel(view, 0x28, level, out);
}

int GetTierRecord29(int view, int level, DatRecord** out)
{
  return FindDataRecordAtLevel(view, 0x29, level, out);
}

int GetTierRecord2a(int view, int level, DatRecord** out)
{
  return FindDataRecordAtLevel(view, 0x2a, level, out);
}

int FindNextTier28(int view, int level, int* found, int* foundLevel, int* extraValue)
{
  return FindNextDataLevel(view, 0x28, level, found, foundLevel, extraValue);
}

int FindNextTier29(int view, int level, int* found, int* foundLevel, int* extraValue)
{
  return FindNextDataLevel(view, 0x29, level, found, foundLevel, extraValue);
}

int FindNextTier2a(int view, int level, int* found, int* foundLevel, int* extraValue)
{
  return FindNextDataLevel(view, 0x2a, level, found, foundLevel, extraValue);
}

void DescribeDataKey(const SeedKey* key, std::string* out)
{
  ObjectTypeDb* db = GetObjectTypeDb();
  if (db)
    db->DescribeKey(key, out);
}

int ForEachDataFamily(Crc32* crc)
{
  ObjectTypeDb* db = GetObjectTypeDb();
  return db ? db->ForEachFamily(crc) : 0;
}
