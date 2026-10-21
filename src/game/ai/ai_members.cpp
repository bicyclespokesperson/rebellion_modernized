#include "ai/ai_members.h"
#include "mission/mission.h"

#include <algorithm>
#include <new>

#include "ai/ai_agent_bases.h"
#include "ai/ai_agents.h"
#include "ai/ai_character_records.h"
#include "ai/ai_common.h"
#include "ai/ai_planners.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "core/checked_new.h"
#include "core/stream_io.h"
#include "data/dat_record.h"
#include "data/random.h"
#include "economy/side.h"
#include "galaxy/sector.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_name_node.h"
#include "gameobj/object_type_db.h"

using ai::InFamilies;

namespace
{

constexpr uint32_t kDirty = 0x80000000;

// Shared by the sets that save their id-keyed nodes with the node's own Write (AiSystemRecordSet, AiTaskRecordSet).
template <class Set>
void SaveIdNodes(Set* set, ostream* out)
{
  WriteValue32(out, &set->m_view);
  if (!WriteCount32(out, set->Count())) {
    return;
  }
  for (const auto& entry : set->Entries()) {
    auto* node = entry.get();
    static_cast<AiTaskRecord*>(node)->Write(out);
  }
}

template <class Set>
ObjectKeyNode* FirstNode(Set& set)
{
  return set.First();
}

} // namespace

// ---- AiClassTable
// -----------------------------------------------------------------------------------------------------

AiClassTable::AiClassTable()
    : m_maxShipTier(0)
    , m_maxTroopTier(0)
    , m_maxFacilityTier(0)
    , m_majorCharacters(0)
    , m_capitalTier(0)
    , m_troopTier(0)
    , m_facilityTier(0)
    , m_bestDefense(0)
    , m_x28(0)
    , m_x2c(0)
    , m_x30(0)
    , m_x34(0)
    , m_x38(0)
    , m_x3c(0)
{
}

AiClassTable::~AiClassTable()
{
  Clear();
}

void AiClassTable::Clear()
{
  decltype(m_records) detached;
  m_records.swap(detached);
}

int AiClassTable::Insert(AiClassRecordBase* record)
{
  if (!record) {
    return 0;
  }
  std::unique_ptr<AiClassRecordBase> pending(record);
  try {
    m_records.emplace(record->m_key.m_value, std::move(pending));
  } catch (...) {
    pending.release();
    throw;
  }
  return 1;
}

void AiClassTable::LoadValues(istream* in)
{
  ReadValue32(in, &m_capitalTier);
  ReadValue32(in, &m_troopTier);
  ReadValue32(in, &m_facilityTier);
}

void AiClassTable::SaveValues(ostream* out)
{
  WriteValue32(out, &m_capitalTier);
  WriteValue32(out, &m_troopTier);
  WriteValue32(out, &m_facilityTier);
}

AiClassRecordBase* AiClassTable::Find(const ObjectKey* key)
{
  const uint32_t id = key->m_value & 0xffffff;
  const uint32_t family = key->m_value >> 24;
  const uint32_t value = (family << 24) | id;
  const auto node = m_records.find(value);
  return node == m_records.end() ? nullptr : node->second.get();
}

void AiClassTable::Build(int side)
{
  auto addClasses
      = [&](int recordType, uint32_t firstFamily, uint32_t lastFamily, int* maxTier, bool matchSide, auto make) {
          for (DatRecord* record : GetRecordsOfType(recordType)) {
            if (!InFamilies(record->m_third, firstFamily, lastFamily)) {
              continue;
            }
            AiClassRecordBase* entry = make(record);
            if (!entry) {
              continue;
            }
            Insert(entry);
            if (maxTier && (!matchSide || entry->m_view == side) && *maxTier < entry->m_valueC) {
              *maxTier = entry->m_valueC;
            }
          }
        };
  addClasses(0x14, 0x14, 0x1c, &m_maxShipTier, true, [](DatRecord* r) -> AiClassRecordBase* {
    return NewOrNull<AiCapitalShipClass>(r);
  });
  addClasses(0x1c, 0x1c, 0x20, &m_maxShipTier, true, [](DatRecord* r) -> AiClassRecordBase* {
    return NewOrNull<AiFighterClass>(r);
  });
  addClasses(0x10, 0x10, 0x14, &m_maxTroopTier, true, [](DatRecord* r) -> AiClassRecordBase* {
    return NewOrNull<AiTroopClass>(r);
  });
  addClasses(0x22, 0x22, 0x28, &m_maxFacilityTier, false, [](DatRecord* r) -> AiClassRecordBase* {
    return NewOrNull<AiDefenseClass>(r);
  });
  addClasses(0x28, 0x28, 0x2c, &m_maxFacilityTier, false, [](DatRecord* r) -> AiClassRecordBase* {
    return NewOrNull<AiManufacturingClass>(r);
  });
  addClasses(0x3c, 0x3c, 0x40, nullptr, false, [](DatRecord* r) -> AiClassRecordBase* {
    return NewOrNull<AiSpecialForceClass>(r);
  });
  for (DatRecord* record : GetRecordsOfType(0x38)) {
    if (!InFamilies(record->m_third, 0x38, 0x3c)) {
      continue;
    }
    const uint32_t sideBit = static_cast<SdRecord*>(record)->m_field44;
    if (sideBit == 0) {
      if (side == 1) {
        ++m_majorCharacters;
      }
    } else if (side == 2) {
      ++m_majorCharacters;
    }
  }
  SetCapitalShipTier(m_capitalTier, side);
  SetTroopTier(m_troopTier, side);
  SetFacilityTier(m_facilityTier, side);
}

void AiClassTable::SetCapitalShipTier(int tier, int side)
{
  ObjectKey unused;
  unused.m_value = 0;
  m_capitalTier = tier;
  m_x28 = Select(&unused, side, tier, 0x200000, 0x400, 1);
  m_x2c = Select(&unused, side, m_capitalTier, 1, 0x200, 1);
  m_x30 = Select(&unused, side, m_capitalTier, 2, 0x400, 1);
  m_x34 = Select(&unused, side, m_capitalTier, 0x100000, 0x80, 1);
  m_x38 = Select(&unused, side, m_capitalTier, 0x100000, 0x100, 1);
  m_x3c = Select(&unused, side, m_capitalTier, 0x800, 0x800, 1);
}

void AiClassTable::SetTroopTier(int tier, int side)
{
  ObjectKey unused;
  unused.m_value = 0;
  m_troopTier = tier;
  m_bestDefense = Select(&unused, side, tier, 0x2000, 0x2000, 1);
}

void AiClassTable::SetFacilityTier(int tier, int)
{
  m_facilityTier = tier;
}

int AiClassTable::Select(ObjectKey* out, int view, int maxTier, uint32_t flagMask, uint32_t valueMask, int descending)
{
  AiRankedSelector selector;
  ObjectKey key;
  int result = 0;
  out->m_value = 0;
  auto* list = NewOrNull<AiRankedList>(descending);
  if (list) {
    for (const auto& entry : Entries()) {
      AiClassRecordBase* classRecord = entry.second.get();
      if (classRecord->m_view == view && (flagMask & classRecord->m_flags.m_value) != 0
          && classRecord->m_valueC <= maxTier) {
        key = classRecord->m_key;
        PersistentInt mask(static_cast<int>(valueMask));
        auto* item = NewOrNull<AiRankedItem>(key, classRecord->GetValue(&mask));
        list->AddItem(item);
      }
    }
    list->Number();
    selector.Add(list);
    ObjectKey picked;
    key = *selector.PickBest(&picked);
    *out = key;
  }
  if ((out->m_value & 0xff000000) != 0) {
    AiClassRecordBase* entry = Find(out);
    PersistentInt mask(static_cast<int>(valueMask));
    result = entry->GetValue(&mask);
  }
  return result;
}

namespace
{

// Drops the names of `list` whose class record is missing or lacks one of the requested flags.
void FilterByClassFlags(AiClassTable* table, ObjectNameList* list, uint32_t requiredFlags)
{
  auto position = list->Entries().begin();
  while (position != list->Entries().end()) {
    auto* node = (*position).get();
    ++position;
    ObjectKey key;
    auto* name = static_cast<ObjectNameNode*>(node);
    GetObjectNameNodeKey(name, &key);
    AiClassRecordBase* entry = table->Find(&key);
    if (!entry || (entry->m_flags.m_value & requiredFlags) != requiredFlags) {
      const uint32_t id = static_cast<IdNode*>(node)->m_id;
      list->DeleteById(id);
    } else {
    }
  }
}

} // namespace

int AiClassTable::ChooseFromSide(ObjectKey* out, GameObjNotifier* side, int recordType, uint32_t requiredFlags,
                                 int mode, int descending)
{
  ObjectNameList list;
  int result = 0;
  out->m_value = 0;
  if (side && static_cast<Side*>(side)->CollectRdRecords(recordType, &list)) {
    FilterByClassFlags(this, &list, requiredFlags);
  }

  if (mode == 0) {
    const int count = static_cast<int>(list.Count());
    const int choice = GameplayRandomNext(count);
    int index = 0;
    auto position = list.Entries().begin();
    if (position != list.Entries().end()) {
      do {
        if (choice <= index) {
          break;
        }
        ++position;
        ++index;
      } while (position != list.Entries().end());
      if (position != list.Entries().end()) {
        ObjectKey key;
        GetObjectNameNodeKey(static_cast<ObjectNameNode*>((*position).get()), &key);
        *out = key;
      }
    }
  } else {
    AiRankedSelector selector;
    ObjectKey key;
    auto* ranked = NewOrNull<AiRankedList>(descending);
    if (ranked) {
      for (const auto& entry : list.Entries()) {
        auto* node = entry.get();
        ObjectKey nameKey;
        GetObjectNameNodeKey(static_cast<ObjectNameNode*>(node), &nameKey);
        AiClassRecordBase* classRecord = Find(&nameKey);
        if (classRecord) {
          key = classRecord->m_key;
          PersistentInt mask(mode);
          auto* item = NewOrNull<AiRankedItem>(key, classRecord->GetValue(&mask));
          if (item) {
            ranked->AddItem(item);
          }
        }
      }
      ranked->Number();
      selector.Add(ranked);
      ObjectKey picked;
      key = *selector.PickBest(&picked);
      *out = key;
    }
  }
  if ((out->m_value & 0xff000000) != 0) {
    AiClassRecordBase* entry = Find(out);
    result = entry->m_valueB;
  }
  return result;
}

AiRankedList* AiClassTable::RankClasses(GameObjNotifier* side, int recordType, uint32_t requiredFlags,
                                        uint32_t valueMask, int descending)
{
  ObjectNameList list;
  ObjectKey key;
  if (side && static_cast<Side*>(side)->CollectRdRecords(recordType, &list)) {
    FilterByClassFlags(this, &list, requiredFlags);
  }
  auto* ranked = NewOrNull<AiRankedList>(descending);
  if (ranked) {
    for (const auto& entry : list.Entries()) {
      auto* node = entry.get();
      ObjectKey nameKey;
      GetObjectNameNodeKey(static_cast<ObjectNameNode*>(node), &nameKey);
      AiClassRecordBase* classRecord = Find(&nameKey);
      if (classRecord) {
        key = classRecord->m_key;
        PersistentInt mask(static_cast<int>(valueMask));
        auto* item = NewOrNull<AiRankedItem>(key, classRecord->GetValue(&mask));
        if (item) {
          ranked->AddItem(item);
        }
      }
    }
    ranked->Number();
  }
  return ranked;
}

// ---- AiTaskRecordSet
// ----------------------------------------------------------------------------------------------------

AiTaskRecordSet::AiTaskRecordSet()
    : m_view(0)
    , m_owner(nullptr)
{
}

AiTaskRecordSet::~AiTaskRecordSet()
{
  Clear();
  m_owner = nullptr;
}

void AiTaskRecordSet::Clear()
{
  m_records.Clear();
}

uint32_t AiTaskRecordSet::AddWithId(AiTaskRecord* record)
{
  return m_records.AddWithId(record);
}

AiTaskRecord* AiTaskRecordSet::FindById(uint32_t id) const
{
  return m_records.FindById(id);
}

void AiTaskRecordSet::DeleteById(uint32_t id)
{
  m_records.DeleteById(id);
}

void AiTaskRecordSet::Load(istream* in)
{
  ReadValue32(in, &m_view);
  Clear();
  uint32_t count;
  ReadValue32(in, &count);
  for (uint32_t i = 0; i < count; ++i) {
    auto* record = NewOrNull<AiTaskRecord>();
    if (record) {
      record->m_owner = m_owner;
      record->Read(in);
      AddWithId(record);
    }
  }
}

void AiTaskRecordSet::Save(ostream* out)
{
  SaveIdNodes(this, out);
}

uint32_t AiTaskRecordSet::CreateRecord()
{
  uint32_t id = 0;
  auto* record = NewOrNull<AiTaskRecord>(m_view, m_owner);
  if (record) {
    id = AddWithId(record);
  }
  return id;
}

uint32_t AiTaskRecordSet::DeleteFirstUnassigned()
{
  uint32_t id = 0;
  auto* record = static_cast<AiTaskRecord*>(First());
  if (record) {
    if (!record->IsFinished()) {
      id = record->m_id;
      record->Release();
      DeleteById(id);
    }
  }
  return id;
}

// ---- AiAgentSet
// -------------------------------------------------------------------------------------------------------

AiAgentSet::AiAgentSet()
    : m_view(0)
{
}

AiAgentSet::~AiAgentSet()
{
  Clear();
  m_characters = nullptr;
  m_owner = nullptr;
}

auto AiAgentSet::PositionOf(const AiAgentBase* agent) const -> decltype(m_agents.Entries().begin())
{
  return m_agents.PositionOf(agent);
}

void AiAgentSet::Clear()
{
  m_agents.Clear();
}

uint32_t AiAgentSet::AddWithId(AiAgentBase* agent)
{
  return m_agents.AddWithId(agent);
}

AiAgentBase* AiAgentSet::FindById(uint32_t id) const
{
  return m_agents.FindById(id);
}

AiAgentBase* AiAgentSet::DetachById(uint32_t id)
{
  return m_agents.DetachById(id);
}

void AiAgentSet::DeleteById(uint32_t id)
{
  m_agents.DeleteById(id);
}

void AiAgentSet::Load(istream* in)
{
  ReadValue32(in, &m_view);
  Clear();
  uint32_t count;
  ReadValue32(in, &count);
  for (uint32_t i = 0; i < count; ++i) {
    int kind;
    ReadValue32(in, &kind);
    AiAgentBase* agent = CreateAgent(kind);
    if (agent) {
      agent->m_characters = m_characters;
      agent->m_owner = m_owner;
      agent->Read(in);
      AddWithId(agent);
    }
  }
}

void AiAgentSet::Save(ostream* out)
{
  WriteValue32(out, &m_view);
  if (!WriteCount32(out, Count())) {
    return;
  }
  for (const auto& entry : Entries()) {
    auto* agent = static_cast<AiAgentBase*>(entry.get());
    int kind = agent->GetKind();
    WriteValue32(out, &kind);
    agent->Write(out);
  }
}

uint32_t AiAgentSet::AddAgent(int kind)
{
  AiAgentBase* agent = CreateAgent(kind);
  if (agent) {
    const uint32_t id = AddWithId(agent);
    agent->m_characters = m_characters;
    agent->m_owner = m_owner;
    return id;
  }
  return kind;
}

void AiAgentSet::ReleaseAgent(uint32_t id)
{
  if (id == 0) {
    return;
  }
  auto* agent = static_cast<AiAgentBase*>(FindById(id));
  if (!agent) {
    return;
  }
  if (agent->GetAgentClass() == 1) {
    auto* targeted = static_cast<AiAgentTargeted*>(agent);
    auto* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(targeted->m_systemRecordId));
    if (record) {
      record->RemoveAgent(targeted);
    }
  }
  agent->m_flags.m_value |= kDirty;
}

AiAgentBase* AiAgentSet::CreateAgent(int kind)
{
  switch (kind) {
    case 1:
      return NewOrNull<AiAgentKind1>(m_view, m_owner, m_characters);
    case 3:
      return NewOrNull<AiAgentKind3>(m_view, m_owner, m_characters);
    case 4:
      return NewOrNull<AiAgentKind4>(m_view, m_owner, m_characters);
    case 5:
      return NewOrNull<AiAgentKind5>(m_view, m_owner, m_characters);
    case 6:
      return NewOrNull<AiAgentKind6>(m_view, m_owner, m_characters);
  }
  return nullptr;
}

// ---- AiSystemRecordSet
// ------------------------------------------------------------------------------------------------

AiSystemRecordSet::AiSystemRecordSet()
    : m_view(0)
    , m_owner(nullptr)
{
}

AiSystemRecordSet::~AiSystemRecordSet()
{
  Clear();
  m_owner = nullptr;
}

auto AiSystemRecordSet::PositionOf(const AiSystemRecord* record) const -> decltype(m_records.Entries().begin())
{
  return m_records.PositionOf(record);
}

void AiSystemRecordSet::Clear()
{
  m_records.Clear();
}

uint32_t AiSystemRecordSet::AddWithId(AiSystemRecord* record)
{
  return m_records.AddWithId(record);
}

AiSystemRecord* AiSystemRecordSet::FindById(uint32_t id) const
{
  return m_records.FindById(id);
}

void AiSystemRecordSet::DeleteById(uint32_t id)
{
  m_records.DeleteById(id);
}

void AiSystemRecordSet::Load(istream* in)
{
  ReadValue32(in, &m_view);
  Clear();
  uint32_t count;
  ReadValue32(in, &count);
  for (uint32_t i = 0; i < count; ++i) {
    auto* record = NewOrNull<AiSystemRecord>();
    if (record) {
      record->m_owner = m_owner;
      record->Read(in);
      AddWithId(record);
    }
  }
}

void AiSystemRecordSet::Save(ostream* out)
{
  WriteValue32(out, &m_view);
  if (!WriteCount32(out, Count())) {
    return;
  }
  for (const auto& entry : Entries()) {
    static_cast<AiSystemRecord*>(entry.get())->Write(out);
  }
}

AiSystemRecord* AiSystemRecordSet::FindByTarget(const ObjectKey* key)
{
  auto position = Entries().begin();
  AiSystemRecord* record = position == Entries().end() ? nullptr : static_cast<AiSystemRecord*>((*position).get());
  bool found = false;
  while (record && !found) {
    if (record->m_systemKey.m_value == key->m_value) {
      found = true;
    } else {
      ++position;
      record = position == Entries().end() ? nullptr : static_cast<AiSystemRecord*>((*position).get());
    }
  }
  return record;
}

uint32_t AiSystemRecordSet::CreateRecord()
{
  uint32_t id = 0;
  auto* record = NewOrNull<AiSystemRecord>(m_view, m_owner);
  if (record) {
    id = AddWithId(record);
  }
  return id;
}

uint32_t AiSystemRecordSet::DeleteFirstIdle()
{
  bool found = false;
  uint32_t id = 0;
  for (const auto& entry : Entries()) {
    auto* record = static_cast<AiSystemRecord*>(entry.get());
    if (found) {
      break;
    }
    const bool hasSector = InFamilies(record->m_sectorKey.m_value >> 24, 0x80, 0x90);
    if (!hasSector) {
      id = record->m_id;
    } else {
      AiSectorEval* sector = m_owner->m_sectors.FindEval(record->m_sectorKey);
      if (sector && sector->Stat(0x18c) > 0) {
        id = record->m_id;
        found = true;
      }
    }
    found = found || !hasSector;
  }
  if (found) {
    auto* record = static_cast<AiSystemRecord*>(FindById(id));
    if (record) {
      record->Reset();
      DeleteById(id);
      return id;
    }
  }
  return id;
}

// ---- AiPlannerSet
// -----------------------------------------------------------------------------------------------------

AiPlannerSet::AiPlannerSet()
    : m_view(0)
{
}

AiPlannerSet::~AiPlannerSet()
{
  Clear();
  m_characters = nullptr;
  m_owner = nullptr;
}

auto AiPlannerSet::PositionOf(const AiMissionPlanner* planner) const -> decltype(m_planners.Entries().begin())
{
  return m_planners.PositionOf(planner);
}

void AiPlannerSet::Clear()
{
  m_planners.Clear();
}

uint32_t AiPlannerSet::AddWithId(AiMissionPlanner* planner)
{
  return m_planners.AddWithId(planner);
}

AiMissionPlanner* AiPlannerSet::FindById(uint32_t id) const
{
  return m_planners.FindById(id);
}

AiMissionPlanner* AiPlannerSet::DetachById(uint32_t id)
{
  return m_planners.DetachById(id);
}

void AiPlannerSet::DeleteById(uint32_t id)
{
  m_planners.DeleteById(id);
}

void AiPlannerSet::Load(istream* in)
{
  ReadValue32(in, &m_view);
  Clear();
  uint32_t count;
  ReadValue32(in, &count);
  for (uint32_t i = 0; i < count; ++i) {
    int kind;
    int subtype;
    ReadValue32(in, &kind);
    ReadValue32(in, &subtype);
    AiMissionPlanner* planner = CreatePlanner(kind, subtype);
    if (planner) {
      planner->m_characters = m_characters;
      planner->m_owner = m_owner;
      planner->Read(in);
      AddWithId(planner);
    }
  }
}

void AiPlannerSet::Save(ostream* out)
{
  WriteValue32(out, &m_view);
  if (!WriteCount32(out, Count())) {
    return;
  }
  for (const auto& entry : Entries()) {
    auto* planner = static_cast<AiMissionPlanner*>(entry.get());
    uint32_t kind = planner->m_missionKey.m_value >> 24;
    WriteValue32(out, &kind);
    int subtype = planner->GetSubtype();
    WriteValue32(out, &subtype);
    planner->Write(out);
  }
}

uint32_t AiPlannerSet::AddPlanner(int kind, int subtype)
{
  AiMissionPlanner* planner = CreatePlanner(kind, subtype);
  if (planner) {
    const uint32_t id = AddWithId(planner);
    planner->m_characters = m_characters;
    planner->m_owner = m_owner;
    m_owner->MarkDirty();
    return id;
  }
  return subtype;
}

void AiPlannerSet::ReleasePlanner(uint32_t id)
{
  if (id == 0) {
    return;
  }
  auto* planner = static_cast<AiMissionPlanner*>(FindById(id));
  if (!planner) {
    return;
  }
  planner->m_flags.m_value |= 0x10000000;
  if (planner->GetPlannerRole() == 1) {
    auto* record = static_cast<AiSystemRecord*>(
        m_owner->m_systemRecords.FindById(static_cast<AiSystemPlanner*>(planner)->m_systemRecordId));
    if (record) {
      record->RemovePlanner(planner);
    }
  } else if (planner->GetPlannerRole() == 2) {
    m_owner->UncountPlanner(planner);
  }
}

AiMissionPlanner* AiPlannerSet::FindByTarget(const int* key)
{
  auto position = Entries().begin();
  AiMissionPlanner* planner = position == Entries().end() ? nullptr : static_cast<AiMissionPlanner*>((*position).get());
  bool found = false;
  while (planner && !found) {
    if (static_cast<int>(planner->m_missionObject.m_value) == *key) {
      found = true;
    } else {
      ++position;
      planner = position == Entries().end() ? nullptr : static_cast<AiMissionPlanner*>((*position).get());
    }
  }
  return planner;
}

void AiPlannerSet::NoteMissionObject(const ObjectKey* key)
{
  if (FindByTarget(reinterpret_cast<const int*>(key))) {
    return;
  }
  if (!InFamilies(key->Family(), 0x50, 0x80)) {
    return;
  }
  GameObjNotifier* object = GetObjectInFamilies_40_80(m_view, key);
  if (!object) {
    return;
  }
  const auto* mission = static_cast<const Mission*>(object);
  if ((mission->m_flagsA4 & 4) != 0) {
    return;
  }
  const auto plannerId = static_cast<uint32_t>(mission->m_userId);
  if (plannerId == 0) {
    return;
  }
  auto* planner = static_cast<AiMissionPlanner*>(FindById(plannerId));
  if (planner) {
    planner->SetMissionObject(key);
  }
}

void AiPlannerSet::NoteMission(const ObjectKey* key)
{
  AiMissionPlanner* planner = FindByTarget(reinterpret_cast<const int*>(key));
  if (planner) {
    planner->ReleaseUnfit();
  }
}

void AiPlannerSet::OnMissionEvent(const int* key, int argument)
{
  AiMissionPlanner* planner = FindByTarget(key);
  if (planner) {
    planner->OnMissionEvent(argument);
  }
}

void AiPlannerSet::OnPlannerEvent(const int* key, int argument)
{
  AiMissionPlanner* planner = FindByTarget(key);
  if (planner) {
    planner->OnPlannerEvent(argument);
  }
}

AiMissionPlanner* AiPlannerSet::CreatePlanner(int kind, int subtype)
{
  switch (kind) {
    case 0x51:
      return NewOrNull<AiDiplomacyPlanner>(m_view, m_owner, m_characters);
    case 0x52:
      if (subtype == 6) {
        return NewOrNull<AiEspionageState6Planner>(m_view, m_owner, m_characters);
      }
      return NewOrNull<AiEspionagePlanner>(m_view, m_owner, m_characters);
    case 0x53:
      if (subtype == 1) {
        return NewOrNull<AiResearchPlanner1>(m_view, m_owner, m_characters);
      }
      if (subtype == 2) {
        return NewOrNull<AiResearchPlanner2>(m_view, m_owner, m_characters);
      }
      if (subtype == 3) {
        return NewOrNull<AiResearchPlanner3>(m_view, m_owner, m_characters);
      }
      return nullptr;
    case 0x54:
      if (subtype == 5) {
        return NewOrNull<AiMission54State5Planner>(m_view, m_owner, m_characters);
      }
      return NewOrNull<AiMission54Planner>(m_view, m_owner, m_characters);
    case 0x55:
      return NewOrNull<AiRecruitPlanner>(m_view, m_owner, m_characters);
    case 0x56:
      return NewOrNull<AiIncitePlanner>(m_view, m_owner, m_characters);
    case 0x57:
      return NewOrNull<AiSubduePlanner>(m_view, m_owner, m_characters);
    case 0x61:
      return NewOrNull<AiRescuePlanner>(m_view, m_owner, m_characters);
    case 0x62:
      if (subtype != 4) {
        return NewOrNull<AiMission62Planner>(m_view, m_owner, m_characters);
      }
      return NewOrNull<AiAbductionPlanner>(m_view, m_owner, m_characters);
    case 0x63:
      return NewOrNull<AiAbductionPlanner>(m_view, m_owner, m_characters);
    case 0x69:
      return NewOrNull<AiSabotagePlanner>(m_view, m_owner, m_characters);
    case 0x6a:
      return NewOrNull<AiDsSabotagePlanner>(m_view, m_owner, m_characters);
  }
  return nullptr;
}

// ---- AiFleetEvalSet
// ---------------------------------------------------------------------------------------------------

AiFleetEvalSet::AiFleetEvalSet()
    : ObjectKeySet()
    , m_view(0)
    , m_owner(nullptr)
    , m_ships(nullptr)
{
}

AiFleetEvalSet::~AiFleetEvalSet()
{
  Clear();
  m_ships = nullptr;
  m_owner = nullptr;
}

AiFleetEval* AiFleetEvalSet::FindEval(ObjectKey key)
{
  return static_cast<AiFleetEval*>(Find(&key));
}

void AiFleetEvalSet::Load(istream* in)
{
  ObjectKey scratch;
  ReadValue32(in, &m_view);
  Clear();
  uint32_t count;
  ReadValue32(in, &count);
  for (uint32_t i = 0; i < count; ++i) {
    auto* eval = NewOrNull<AiFleetEval>();
    if (eval) {
      eval->ReadRecord(in, m_ships, m_owner);
      AddEval(eval);
    }
  }
}

void AiFleetEvalSet::Save(ostream* out)
{
  ObjectKey scratch;
  WriteValue32(out, &m_view);
  if (!WriteCount32(out, Count())) {
    return;
  }
  for (const auto& entry : Entries()) {
    auto* node = static_cast<ObjectKeyNode*>(entry.get());
    node->Save(out);
  }
}

void AiFleetEvalSet::AddEval(AiFleetEval* eval)
{
  ObjectKey key;
  eval->GetKey(&key);
  if (!FindEval(key)) {
    Add(eval);
  }
}

void AiFleetEvalSet::NoteFleet(const ObjectKey* key)
{
  if (!InFamilies(key->Family(), 8, 0x10)) {
    return;
  }
  ObjectKey copy = *key;
  AiFleetEval* eval = FindEval(copy);
  if (eval) {
    eval->MarkDirty();
  }
}

void AiFleetEvalSet::NoteFleetCombat(const ObjectKey* key)
{
  if (!InFamilies(key->Family(), 8, 0x10)) {
    return;
  }
  ObjectKey copy = *key;
  AiFleetEval* eval = FindEval(copy);
  if (eval) {
    eval->NoteCombat();
  }
}

void AiFleetEvalSet::RemoveShip(ObjectKey* fleetKey, const ObjectKey* shipKey)
{
  if (!InFamilies(fleetKey->Family(), 8, 0x10)) {
    return;
  }
  ObjectKey copy = *fleetKey;
  AiFleetEval* eval = FindEval(copy);
  if (!eval) {
    return;
  }
  eval->RemoveShip(const_cast<ObjectKey*>(shipKey));
  if (eval->m_shipKeys.Count() == 0) {
    ObjectKey evalKey;
    eval->GetKey(&evalKey);
    DeleteIfEmpty(&evalKey);
  }
}

void AiFleetEvalSet::AddShip(const ObjectKey* fleetKey, const ObjectKey* shipKey)
{
  if (!InFamilies(fleetKey->Family(), 8, 0x10)) {
    return;
  }
  ObjectKey copy = *fleetKey;
  AiFleetEval* eval = FindEval(copy);
  if (!eval) {
    GameObjNotifier* fleet = GetObjectInFamilies_08_10(m_view, fleetKey);
    if (!fleet) {
      return;
    }
    eval = NewOrNull<AiFleetEval>(&m_view, fleet, m_ships, m_owner);
    if (!eval) {
      return;
    }
    eval->MarkDirty();
    AddEval(eval);
  }
  eval->AddShip(shipKey);
}

void AiFleetEvalSet::DeleteIfEmpty(ObjectKey* fleetKey)
{
  m_owner->MarkFleetSystemChanged(fleetKey);
  RemoveKey(fleetKey);
}

// ---- AiShipRecordSet
// --------------------------------------------------------------------------------------------------

AiShipRecordSet::AiShipRecordSet()
    : ObjectKeySet()
    , m_view(0)
    , m_done(0)
    , m_cursor(nullptr)
    , m_fleets(nullptr)
    , m_classes(nullptr)
    , m_owner(nullptr)
{
}

AiShipRecordSet::~AiShipRecordSet()
{
  Clear();
  m_cursor = nullptr;
  m_fleets = nullptr;
  m_classes = nullptr;
  m_owner = nullptr;
}

void AiShipRecordSet::Load(istream* in)
{
  ObjectKey scratch;
  ReadValue32(in, &m_view);
  Clear();
  uint32_t count;
  ReadValue32(in, &count);
  for (uint32_t i = 0; i < count; ++i) {
    auto* record = NewOrNull<AiShipRecord>();
    if (record) {
      record->ReadRecord(in, m_fleets, m_classes, m_owner);
      AddRecord(record);
    }
  }
  ReadValue32(in, &m_done);
}

void AiShipRecordSet::Save(ostream* out)
{
  ObjectKey scratch;
  WriteValue32(out, &m_view);
  if (!WriteCount32(out, Count())) {
    return;
  }
  for (const auto& entry : Entries()) {
    auto* node = static_cast<ObjectKeyNode*>(entry.get());
    node->Save(out);
  }
  WriteValue32(out, &m_done);
}

int AiShipRecordSet::AddRecord(AiShipRecord* record)
{
  int result = 0;
  ObjectKey key;
  record->GetKey(&key);
  if (!Find(&key)) {
    result = static_cast<int>(Add(record));
  }
  return result;
}

int AiShipRecordSet::BuildStep()
{
  int finished = 0;
  if (!m_cursor) {
    Clear();
    m_cursor = GetFirstObjectInFamilies_14_1c(m_view);
    return finished;
  }
  ObjectKey parentKey;
  if (m_cursor->m_parent) {
    ObjectKey tmp;
    parentKey = *GetObjectKey(m_cursor->m_parent, &tmp);
  }
  ObjectKey cursorKey;
  GetObjectKey(m_cursor, &cursorKey);
  if (!Find(&cursorKey)) {
    auto* record = NewOrNull<AiShipRecord>(m_cursor, m_view, m_fleets, m_classes, m_owner);
    if (record) {
      if (!AddRecord(record)) {
        delete record;
      } else {
        record->ClearFleet();
        ObjectKey shipKey;
        record->GetKey(&shipKey);
        m_fleets->AddShip(&parentKey, &shipKey);
      }
    }
  }
  const FamilyRange range(0x14, 0x1c);
  m_cursor = static_cast<GameObj*>(m_cursor)->FindNextSibling(&range, 1);
  if (!m_cursor) {
    finished = 1;
    m_done = 1;
  }
  return finished;
}

void AiShipRecordSet::SetFleet(ObjectKey* shipKey, const ObjectKey* fleetKey)
{
  if (!InFamilies(shipKey->Family(), 0x14, 0x1c)) {
    return;
  }
  auto* record = static_cast<AiShipRecord*>(Find(shipKey));
  if (record) {
    record->m_fleetKey = *fleetKey;
  }
}

void AiShipRecordSet::AddOrRefresh(const ObjectKey* key)
{
  ObjectKey parentKey;
  auto* existing = static_cast<AiShipRecord*>(Find(key));
  if (existing) {
    existing->ClearFleet();
    return;
  }
  if (!m_done) {
    return;
  }
  GameObjNotifier* ship = GetObjectInFamilies_14_1c(m_view, key);
  if (!ship || (static_cast<GameObj*>(ship)->m_stateFlags & 8)) {
    return;
  }
  auto* record = NewOrNull<AiShipRecord>(ship, m_view, m_fleets, m_classes, m_owner);
  if (!record) {
    return;
  }
  if (ship->m_parent) {
    ObjectKey tmp;
    parentKey = *GetObjectKey(ship->m_parent, &tmp);
  }
  if (!AddRecord(record)) {
    delete record;
  } else {
    record->ClearFleet();
    ObjectKey shipKey;
    record->GetKey(&shipKey);
    m_fleets->AddShip(&parentKey, &shipKey);
  }
}

void AiShipRecordSet::MoveShip(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to)
{
  auto* record = static_cast<AiShipRecord*>(Find(key));
  if (record) {
    record->MoveToFleet(to, from);
  }
}

void AiShipRecordSet::RemoveShip(const ObjectKey* key, const ObjectKey* fleet)
{
  auto* record = static_cast<AiShipRecord*>(Find(key));
  if (record && record->ChangeFleet(fleet)) {
    RemoveKey(key);
  }
}

// ---- AiSectorEvalSet
// --------------------------------------------------------------------------------------------------

AiSectorEvalSet::AiSectorEvalSet()
    : ObjectKeySet()
    , m_view(0)
    , m_cursor(nullptr)
    , m_cursorObject(nullptr)
{
}

AiSectorEvalSet::~AiSectorEvalSet()
{
  Clear();
  m_cursorObject = nullptr;
  m_cursor = nullptr;
}

AiSectorEval* AiSectorEvalSet::FindEval(ObjectKey key)
{
  return static_cast<AiSectorEval*>(Find(&key));
}

void AiSectorEvalSet::LoadAll(istream* in, AiSystemEvalSet* systems, AiSideController* owner)
{
  ObjectKey key;
  ReadValue32(in, &m_view);
  Clear();
  uint32_t count;
  ReadValue32(in, &count);
  if (count != 0) {
    do {
      ReadOne(in, systems, owner);
      --count;
    } while (count != 0);
  }
  uint32_t flag;
  ReadValue32(in, &flag);
  if (flag == 0) {
    m_cursorObject = nullptr;
  } else {
    key.Read(in);
    m_cursorObject = GetObjectInFamilies_80_90(m_view, &key);
  }
  ReadValue32(in, &flag);
  if (flag == 0) {
    m_cursor = nullptr;
  } else {
    key.Read(in);
    m_cursor = FindEval(key);
  }
}

void AiSectorEvalSet::Save(ostream* out)
{
  ObjectKey key;
  WriteValue32(out, &m_view);
  if (!WriteCount32(out, Count())) {
    return;
  }
  for (const auto& entry : Entries()) {
    auto* node = static_cast<ObjectKeyNode*>(entry.get());
    node->Save(out);
  }
  if (!m_cursorObject) {
    uint32_t none = 0;
    WriteValue32(out, &none);
  } else {
    GetObjectKey(m_cursorObject, &key);
    uint32_t family = key.m_value >> 24;
    WriteValue32(out, &family);
    key.Write(out);
  }
  if (!m_cursor) {
    uint32_t none = 0;
    WriteValue32(out, &none);
  } else {
    m_cursor->GetKey(&key);
    uint32_t family = key.m_value >> 24;
    WriteValue32(out, &family);
    key.Write(out);
  }
}

int AiSectorEvalSet::BuildStep(AiSystemEvalSet* systems, AiSideController* owner)
{
  int finished = 0;
  if (!m_cursorObject) {
    Clear();
    m_cursorObject = GetFirstObjectInFamilies_80_90(m_view);
    return finished;
  }
  m_cursor = NewOrNull<AiSectorEval>(m_view);
  if (m_cursor) {
    m_cursor->Attach(static_cast<Sector*>(m_cursorObject), systems);
    m_cursor->m_owner = owner;
    Add(m_cursor);
  }
  const FamilyRange range(0x80, 0x90);
  m_cursorObject = static_cast<GameObj*>(m_cursorObject)->FindNextSibling(&range, 1);
  if (!m_cursorObject) {
    finished = 1;
    m_cursor = nullptr;
  }
  return finished;
}

void AiSectorEvalSet::NoteSector(const ObjectKey* key)
{
  ObjectKey copy = *key;
  AiSectorEval* eval = FindEval(copy);
  if (eval) {
    eval->MarkDirty();
  }
}

void AiSectorEvalSet::ReadOne(istream* in, AiSystemEvalSet* systems, AiSideController* owner)
{
  auto* eval = NewOrNull<AiSectorEval>();
  if (eval) {
    eval->ReadRecord(in, systems);
    eval->m_owner = owner;
    Add(eval);
  }
}

// ---- AiSystemEvalSet
// --------------------------------------------------------------------------------------------------

AiSystemEvalSet::AiSystemEvalSet()
    : ObjectKeySet()
    , m_view(0)
    , m_cursor(nullptr)
    , m_cursorObject(nullptr)
    , m_classes(nullptr)
{
}

AiSystemEvalSet::~AiSystemEvalSet()
{
  Clear();
  m_cursorObject = nullptr;
  m_cursor = nullptr;
  m_classes = nullptr;
}

void AiSystemEvalSet::Load(istream* in)
{
  ObjectKey key;
  ReadValue32(in, &m_view);
  Clear();
  uint32_t count;
  ReadValue32(in, &count);
  for (; count != 0; --count) {
    ReadOne(in);
  }
  uint32_t flag;
  ReadValue32(in, &flag);
  if (flag == 0) {
    m_cursorObject = nullptr;
  } else {
    key.Read(in);
    m_cursorObject = GetObjectInFamilies_90_98(m_view, &key);
  }
  ReadValue32(in, &flag);
  if (flag == 0) {
    m_cursor = nullptr;
  } else {
    key.Read(in);
    m_cursor = FindEval(key);
  }
}

void AiSystemEvalSet::Save(ostream* out)
{
  ObjectKey key;
  WriteValue32(out, &m_view);
  if (!WriteCount32(out, Count())) {
    return;
  }
  for (const auto& entry : Entries()) {
    auto* node = static_cast<ObjectKeyNode*>(entry.get());
    node->Save(out);
  }
  if (!m_cursorObject) {
    uint32_t none = 0;
    WriteValue32(out, &none);
  } else {
    GetObjectKey(m_cursorObject, &key);
    uint32_t family = key.m_value >> 24;
    WriteValue32(out, &family);
    key.Write(out);
  }
  if (!m_cursor) {
    uint32_t none = 0;
    WriteValue32(out, &none);
  } else {
    m_cursor->GetKey(&key);
    uint32_t family = key.m_value >> 24;
    WriteValue32(out, &family);
    key.Write(out);
  }
}

int AiSystemEvalSet::BuildStep()
{
  if (!m_cursorObject) {
    Clear();
    m_cursorObject = GetFirstObjectInFamilies_90_98(m_view);
    return 0;
  }
  ObjectKey key;
  GetObjectKey(m_cursorObject, &key);
  m_cursor = FindEval(key);
  if (!m_cursor) {
    m_cursor = NewOrNull<AiSystemEval>(m_view, static_cast<System*>(m_cursorObject));
    if (m_cursor) {
      m_cursor->MarkDirty();
      m_cursor->m_classes = m_classes;
      Add(m_cursor);
    }
  }
  const FamilyRange range(0x90, 0x98);
  m_cursorObject = static_cast<GameObj*>(m_cursorObject)->FindNextSibling(&range, 1);
  if (!m_cursorObject) {
    m_cursor = nullptr;
    return 1;
  }
  return 0;
}

void AiSystemEvalSet::NoteSystem(const ObjectKey* key)
{
  ObjectKey copy = *key;
  AiSystemEval* eval = FindEval(copy);
  if (eval) {
    eval->MarkChanged();
  }
}

void AiSystemEvalSet::NoteSystemCombat(const ObjectKey* key)
{
  ObjectKey copy = *key;
  AiSystemEval* eval = FindEval(copy);
  if (eval) {
    eval->NoteCombat();
  }
}

void AiSystemEvalSet::MarkAllChanged()
{
  for (const auto& entry : Entries()) {
    auto* node = static_cast<ObjectKeyNode*>(entry.get());
    static_cast<AiSystemEval*>(node)->MarkChanged();
  }
}

void AiSystemEvalSet::ReadOne(istream* in)
{
  auto* eval = NewOrNull<AiSystemEval>();
  if (eval) {
    eval->Load(in);
    eval->m_classes = m_classes;
    Add(eval);
  }
}

ObjectKey* GetObjectNameNodeKey(ObjectNameNode* node, ObjectKey* out)
{
  out->m_value = node->m_key.m_value;
  return out;
}
