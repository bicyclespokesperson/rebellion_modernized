#include "ai/ai_character_records.h"

#include "ai/ai_common.h"
#include "ai/ai_side_controller.h"
#include "character/character.h"
#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"
#include "gameobj/object_lookup.h"
#include "orders/mission_orders.h"
#include "orders/order_base.h"

namespace
{

constexpr uint32_t kMissionOrderKind = 0x240;
constexpr int kForceWeak = 0;
constexpr int kForceStrong = 99;
constexpr int kSkillMinimumForBonus = 0x4f;

struct MissionCapabilityKey
{
  uint32_t key;
  uint32_t capability;
};

// The mission types a character can be sent on, as the record keys the mission manager lists them with.
constexpr MissionCapabilityKey kMissionCapabilities[] = {
  {0x51000010, 0x1},   {0x61000011, 0x2},    {0x69000012, 0x4},    {0x52000013, 0x8},   {0x62000017, 0x10},
  {0x57000080, 0x20},  {0x63000081, 0x40},   {0x6a000041, 0x80},   {0x56000040, 0x100}, {0x54000015, 0x200},
  {0x53000020, 0x800}, {0x53000022, 0x1000}, {0x53000021, 0x2000}, {0x55000016, 0x400},
};

ObjectKey KeyOf(GameObjNotifier* object)
{
  ObjectKey key;
  return *GetObjectKey(object, &key);
}

int ValueOfMissionKey(uint32_t key)
{
  for (const MissionCapabilityKey& entry : kMissionCapabilities)
    if (entry.key == key)
      return static_cast<int>(entry.capability);
  return 0;
}

} // namespace

ObjectKey* GetAiRecordKey(DatRecord* record, ObjectKey* out)
{
  const uint32_t id = record->m_id;
  const uint32_t family = record->m_third;
  out->m_value = (id & 0xffffff) | (family << 24);
  return out;
}

ObjectKey* GetObjectKey(GameObjNotifier* object, ObjectKey* out)
{
  return object->GetKey(out);
}

ObjectKey* GetObjectRecordKey(GameObjNotifier* object, ObjectKey* out)
{
  if (!object->m_record)
  {
    out->m_value = 0;
    return out;
  }
  ObjectKey recordKey;
  out->m_value = GetAiRecordKey(object->m_record, &recordKey)->m_value;
  return out;
}

int IsStoryKeyOf4(uint32_t key)
{
  return key == 0x32000242 || key == 0x30000240 || key == 0x34000280 || key == 0x35000281;
}

int IsStoryKeyOf6(uint32_t key)
{
  return key == 0x32000242 || key == 0x30000240 || key == 0x31000241 || key == 0x33000243 || key == 0x34000280 ||
         key == 0x35000281;
}

int IsStoryKeyOf2(uint32_t key)
{
  return key == 0x32000242 || key == 0x35000281;
}

// ---- AiCharacterRecord ----------------------------------------------------------------------------------------------

AiCharacterRecord::AiCharacterRecord()
: ObjectKeyNode(),
  m_side(0),
  m_x24(0),
  m_agentId(0),
  m_eligibilityRank(0),
  m_flags(0),
  m_capabilities(0),
  m_location(),
  m_owner(nullptr),
  m_values()
{
}

AiCharacterRecord::AiCharacterRecord(GameObjNotifier* character, int side, AiSideController* owner)
: ObjectKeyNode(KeyOf(character)),
  m_side(side),
  m_x24(0),
  m_agentId(0),
  m_eligibilityRank(0),
  m_flags(0),
  m_capabilities(0),
  m_location(),
  m_owner(owner),
  m_values()
{
  m_controlSide = character->ControlSide();
  const uint32_t expectedSide = ai::ViewOfSide(m_side);
  m_flags.m_value |= ai_character_flag::kAlwaysEligible;

  ObjectKey key;
  GetRecordKey(&key);
  if (ai::KeyInFamilies(key, 0x3c, 0x40))
  {
    m_flags.m_value |= ai_character_flag::kSpecialForce;
    if (m_controlSide == expectedSide)
    {
      m_flags.m_value |= ai_character_flag::kSpecialForce | ai_character_flag::kOwnSide;
      ObjectKey recordKey;
      GetObjectRecordKey(character, &recordKey);
      SetSpecialForceCapability(&recordKey);
    }
    else
    {
      m_flags.m_value |= ai_character_flag::kOtherSide;
    }
    FindMissionCapabilities();
    return;
  }

  Character* person = static_cast<Character*>(character);
  m_flags.m_value |= ai_character_flag::kCharacter;
  const bool captured = (person->m_flagsAc & 1) != 0;
  if (m_controlSide == expectedSide)
    m_flags.m_value |= captured ? ai_character_flag::kOtherSide : ai_character_flag::kOwnSide;
  else
    m_flags.m_value |= captured ? ai_character_flag::kOwnSide : ai_character_flag::kOtherSide;
  if (captured)
    m_flags.m_value |= ai_character_flag::kCaptured;

  if ((owner->m_flags.Get() & 1) == 0)
  {
    GetRecordKey(&key);
    if (IsStoryKeyOf4(key.m_value))
    {
      m_flags.m_value |= ai_character_flag::kStoryKey;
      GetRecordKey(&key);
      if (key.m_value == 0x34000280)
        m_flags.m_value |= ai_character_flag::kDarthVader;
    }
  }
  GetRecordKey(&key);
  if (IsStoryKeyOf6(key.m_value))
    m_flags.m_value |= ai_character_flag::kStoryCharacter;

  if ((m_flags.m_value & ai_character_flag::kStoryCharacter) == 0)
  {
    if (person->RecordCanBeAdmiral())
      m_capabilities.m_value |= 0x1000000;
    if (person->RecordCanBeCommander())
      m_capabilities.m_value |= 0x2000000;
    if (person->RecordCanBeGeneral())
      m_capabilities.m_value |= 0x4000000;
  }
  FindMissionCapabilities();
}

AiCharacterRecord::~AiCharacterRecord()
{
  m_owner = nullptr;
}

ObjectKey* AiCharacterRecord::GetRecordKey(ObjectKey* out)
{
  out->m_value = m_key.m_value;
  return out;
}

void AiCharacterRecord::ReadRecord(istream* in, AiSideController* owner)
{
  ObjectKeyNode::Load(in);
  ReadValue32(in, &m_side);
  ReadValue32(in, &m_controlSide);
  ReadValue32(in, &m_x24);
  ReadValue32(in, &m_agentId);
  ReadValue32(in, &m_eligibilityRank);
  m_flags.Read(in);
  m_capabilities.Read(in);
  m_location.Read(in);
  for (int& value : m_values)
    ReadValue32(in, &value);
  m_owner = owner;
  m_flags.m_value |= ai_character_flag::kDirty;
}

void AiCharacterRecord::Save(ostream* out)
{
  ObjectKeyNode::Save(out);
  WriteValue32(out, &m_side);
  WriteValue32(out, &m_controlSide);
  WriteValue32(out, &m_x24);
  WriteValue32(out, &m_agentId);
  WriteValue32(out, &m_eligibilityRank);
  m_flags.Write(out);
  m_capabilities.Write(out);
  m_location.Write(out);
  for (int& value : m_values)
    WriteValue32(out, &value);
}

void AiCharacterRecord::MarkChanged()
{
  m_flags.m_value |= ai_character_flag::kDirty;
  const uint32_t family = m_location.Family();
  if (family != 0 && family <= 0xfe)
    m_owner->NoteObject(&m_location);
  m_owner->MarkDirty();
}

void AiCharacterRecord::SetLocation(const ObjectKey* location, const ObjectKey*)
{
  m_flags.m_value |= ai_character_flag::kDirty;
  m_location = *location;
}

int AiCharacterRecord::Refresh()
{
  if ((m_flags.m_value & ai_character_flag::kDirty) == 0)
    return 0;
  m_flags.m_value &= 0x7080440e;
  for (int& value : m_values)
    value = 0;

  GameObjNotifier* object = GetObjectInFamilies_30_40(m_side, &m_key);
  if (!object)
    return 1;
  Role* role = static_cast<Role*>(object);
  if (role->m_stateFlags & 8)
    return 1;

  m_controlSide = role->ControlSide();
  if (role->m_flags78 & 0x80)
    m_flags.m_value |= ai_character_flag::kOnMission;
  if (role->m_flags78 & 0x200)
    m_flags.m_value |= ai_character_flag::kOnMandatoryMission;
  if (role->m_stateFlags & 0x10)
    m_flags.m_value |= ai_character_flag::kEnroute;
  if ((role->m_stateFlags & 4) == 0)
    m_flags.m_value |= ai_character_flag::kNotCompleted;

  if ((m_flags.m_value & ai_character_flag::kCharacter) == 0)
  {
    m_values[9] = role->GetRecordField4c();
  }
  else
  {
    Character* character = static_cast<Character*>(role);
    if (character->m_flagsAc & 1)
      m_flags.m_value |= ai_character_flag::kCaptured;
    if (character->m_injury != 0)
      m_flags.m_value |= ai_character_flag::kInjured;
    const uint32_t ownerFlags = m_owner->m_flags.Get();
    if ((m_capabilities.m_value & 0x800) && (ownerFlags & 0x2000000) == 0)
      m_flags.m_value |= ai_character_flag::kUnsuited;
    if ((m_capabilities.m_value & 0x1000) && (ownerFlags & 0x4000000) == 0)
      m_flags.m_value |= ai_character_flag::kUnsuited;
    if ((m_capabilities.m_value & 0x2000) && (ownerFlags & 0x8000000) == 0)
      m_flags.m_value |= ai_character_flag::kUnsuited;
    if (character->m_commandKind == 1)
      m_flags.m_value |= 0x4000000;
    else if (character->m_commandKind == 2)
      m_flags.m_value |= 0x1000000;
    else if (character->m_commandKind == 3)
      m_flags.m_value |= 0x2000000;
    m_values[8] = character->m_force;
    if (m_values[8] > kForceWeak && m_values[8] < kForceStrong + 1)
    {
      if (!IsStoryKeyOf2(m_key.m_value))
        m_flags.m_value |= ai_character_flag::kWeakForce;
    }
    if (m_values[8] > kForceStrong)
    {
      if (IsStoryKeyOf2(m_key.m_value))
        m_flags.m_value |= ai_character_flag::kStrongForce;
    }
  }

  m_values[0] = role->GetDiplomacy();
  m_values[1] = role->GetEspionage();
  m_values[2] = role->GetShipyardRD();
  m_values[3] = role->GetTrainingFacilRD();
  m_values[4] = role->GetConstructionYardRD();
  m_values[5] = role->GetCombat();
  m_values[6] = role->GetLeadership();
  m_values[7] = role->GetLoyalty();
  if ((m_capabilities.m_value & 1) && m_values[0] > kSkillMinimumForBonus)
    m_flags.m_value |= ai_character_flag::kUnsuited;
  if (m_location.Family() == 0 && role->m_parent)
  {
    ObjectKey parentKey;
    m_location = *GetObjectKey(role->m_parent, &parentKey);
  }
  return 1;
}

void AiCharacterRecord::SetSpecialForceCapability(const ObjectKey* recordKey)
{
  const uint32_t key = recordKey->m_value;
  if (key == 0x3c000001 || key == 0x3c000005)
    m_capabilities.m_value |= 0x10000000;
  else if (key == 0x3c000002 || key == 0x3c000006)
    m_capabilities.m_value |= 0x20000000;
  else if (key == 0x3c000003 || key == 0x3c000007)
    m_capabilities.m_value |= 0x40000000;
  else if (key == 0x3c000004 || key == 0x3c000008)
    m_capabilities.m_value |= 0x80000000;
}

void AiCharacterRecord::FindMissionCapabilities()
{
  ObjectRefList candidates;
  ObjectKeyValueSet team;
  MissionOrder240* order = static_cast<MissionOrder240*>(CreateMessageByKind(kMissionOrderKind));
  if (!order)
    return;
  order->m_side = m_side;
  ObjectKey key;
  ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(*GetRecordKey(&key), 0);
  if (node)
  {
    team.Add(node);
    order->SetTeam(&team);
    if (order->CollectCandidatesA(reinterpret_cast<ObjectKeyValueSet*>(&candidates)))
    {
      for (const auto& candidate : candidates.Entries())
      {
        auto* entry = static_cast<ObjectRefNode*>(candidate.get());
        ObjectKey missionKey;
        m_capabilities.m_value |= ValueOfMissionKey(entry->GetKey(&missionKey)->m_value);
      }
    }
  }
  delete order;
}

// ---- AiCharacterRecordSet -------------------------------------------------------------------------------------------

AiCharacterRecordSet::AiCharacterRecordSet()
: ObjectKeySet(),
  m_view(0),
  m_built(0),
  m_owner(nullptr),
  m_cursor(nullptr),
  m_usableCount(0)
{
}

AiCharacterRecordSet::~AiCharacterRecordSet()
{
  m_cursor = nullptr;
  m_owner = nullptr;
}

void AiCharacterRecordSet::Load(istream* in)
{
  ObjectKey scratch;
  ReadValue32(in, &m_view);
  ReadValue32(in, &m_usableCount);
  Clear();
  uint32_t count = 0;
  ReadValue32(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return;
  for (uint32_t index = 0; index < count; ++index)
  {
    AiCharacterRecord* record = NewOrNull<AiCharacterRecord>();
    if (!record)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    record->ReadRecord(in, m_owner);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    {
      delete record;
      return;
    }
    ObjectKey key;
    if (!ObjectKeySet::Find(record->GetRecordKey(&key)))
    {
      if (!ObjectKeySet::Add(record))
      {
        delete record;
        iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
        return;
      }
      record->MarkChanged();
    }
    else
    {
      delete record;
    }
  }
  ReadValue32(in, &m_built);
}

void AiCharacterRecordSet::Save(ostream* out)
{
  ObjectKey scratch;
  WriteValue32(out, &m_view);
  WriteValue32(out, &m_usableCount);
  if (!WriteCount32(out, Count()))
    return;
  for (const auto& entry : Entries())
  {
    auto* node = static_cast<ObjectKeyNode*>(entry.get());
    node->Save(out);
  }
  WriteValue32(out, &m_built);
}

int AiCharacterRecordSet::BuildStep()
{
  int finished = 0;
  int added = 0;
  if (!m_cursor)
  {
    Clear();
    m_cursor = GetFirstObjectInFamilies_30_40(m_view);
    return finished;
  }
  ObjectKey key;
  if (!ObjectKeySet::Find(GetObjectKey(m_cursor, &key)))
  {
    AiCharacterRecord* record = NewOrNull<AiCharacterRecord>(m_cursor, m_view, m_owner);
    if (record)
    {
      ObjectKey recordKey;
      if (!ObjectKeySet::Find(record->GetRecordKey(&recordKey)))
      {
        added = ObjectKeySet::Add(record);
        record->MarkChanged();
      }
      if (added == 0)
      {
        delete record;
      }
      else
      {
        record->MarkChanged();
        const uint32_t flags = record->m_flags.m_value;
        if ((flags & ai_character_flag::kOwnSide) && (flags & ai_character_flag::kCharacter) &&
            (flags & ai_character_flag::kStoryCharacter) == 0)
          --m_usableCount;
      }
    }
  }
  const FamilyRange range(0x30, 0x40);
  m_cursor = static_cast<GameObj*>(m_cursor)->FindNextSibling(&range, 1);
  if (!m_cursor)
  {
    finished = 1;
    m_built = 1;
  }
  return finished;
}

namespace
{

struct RecordFilter
{
  uint32_t requireFlags, requireCaps, forbidFlags, forbidCaps;

  bool Matches(const AiCharacterRecord* record) const
  {
    const uint32_t flags = record->m_flags.m_value;
    const uint32_t caps = record->m_capabilities.m_value;
    return (requireFlags & flags) == requireFlags && (forbidFlags & flags) == 0 &&
           (requireCaps & caps) == requireCaps && (forbidCaps & caps) == 0;
  }
};

} // namespace

AiRankedList* AiCharacterRecordSet::RankByValue(uint32_t requireFlags,
                                                uint32_t requireCaps,
                                                uint32_t forbidFlags,
                                                uint32_t forbidCaps,
                                                uint32_t valueIndex,
                                                int minValue,
                                                int maxValue,
                                                int descending)
{
  AiRankedList* list = NewOrNull<AiRankedList>(descending);
  if (!list)
    return list;
  const RecordFilter filter{requireFlags, requireCaps, forbidFlags, forbidCaps};
  for (const auto& entry : Entries())
  {
    auto* record = static_cast<AiCharacterRecord*>(entry.get());
    if (!filter.Matches(record))
      continue;
    const int value = record->m_values[valueIndex & 0xffff];
    if (value < minValue || value > maxValue)
      continue;
    ObjectKey key;
    list->AddItem(NewOrNull<AiRankedItem>(*record->GetRecordKey(&key), value));
  }
  list->Number();
  return list;
}

AiRankedList* AiCharacterRecordSet::RankEligible(uint32_t rank,
                                                 uint32_t requireFlags,
                                                 uint32_t requireCaps,
                                                 uint32_t forbidFlags,
                                                 uint32_t forbidCaps,
                                                 uint32_t valueIndex,
                                                 int minValue,
                                                 int maxValue,
                                                 int descending)
{
  AiRankedList* list = NewOrNull<AiRankedList>(descending);
  if (!list)
    return list;
  const RecordFilter filter{requireFlags, requireCaps, forbidFlags, forbidCaps};
  for (const auto& entry : Entries())
  {
    auto* record = static_cast<AiCharacterRecord*>(entry.get());
    if (!filter.Matches(record))
      continue;
    if (!(rank < static_cast<uint32_t>(record->m_eligibilityRank) ||
          (record->m_flags.m_value & ai_character_flag::kAlwaysEligible)))
      continue;
    const int value = record->m_values[valueIndex & 0xffff];
    if (value < minValue || value > maxValue)
      continue;
    ObjectKey key;
    list->AddItem(NewOrNull<AiRankedItem>(*record->GetRecordKey(&key), value));
  }
  list->Number();
  return list;
}

AiRankedList* AiCharacterRecordSet::RankByDistance(const ObjectKey* target,
                                                   uint32_t requireFlags,
                                                   uint32_t requireCaps,
                                                   uint32_t forbidFlags,
                                                   uint32_t forbidCaps,
                                                   int descending)
{
  AiRankedList* list = nullptr;
  ObjectKey targetSystem;
  FindContainingSystem(&targetSystem, target);
  if (ai::KeyInFamilies(targetSystem, 0x90, 0x98))
  {
    AiSystemEval* system = m_owner->m_systems.FindEval(targetSystem);
    if (system)
    {
      const int32_t targetPosition = system->m_position;
      list = NewOrNull<AiRankedList>(descending);
      if (list)
      {
        const RecordFilter filter{requireFlags, requireCaps, forbidFlags, forbidCaps};
        for (const auto& entry : Entries())
        {
          auto* record = static_cast<AiCharacterRecord*>(entry.get());
          if (!filter.Matches(record))
            continue;
          ObjectKey recordKey;
          ObjectKey recordSystem;
          FindContainingSystem(&recordSystem, record->GetRecordKey(&recordKey));
          if (!ai::KeyInFamilies(recordSystem, 0x90, 0x98))
            continue;
          AiSystemEval* other = m_owner->m_systems.FindEval(recordSystem);
          if (!other)
            continue;
          const int distance = m_owner->DistanceSquared(
            static_cast<int16_t>(targetPosition), static_cast<int16_t>(targetPosition >> 16),
            static_cast<int16_t>(other->m_position), static_cast<int16_t>(other->m_position >> 16));
          list->AddItem(NewOrNull<AiRankedItem>(*record->GetRecordKey(&recordKey), distance));
        }
        list->Number();
      }
    }
  }
  return list;
}

void AiCharacterRecordSet::AddOrRefresh(const ObjectKey* key)
{
  AiCharacterRecord* existing = FindRecord(key->m_value);
  if (existing)
  {
    existing->MarkChanged();
    return;
  }
  if (!m_built)
    return;
  GameObjNotifier* object = GetObjectInFamilies_30_40(m_view, key);
  if (!object || (static_cast<GameObj*>(object)->m_stateFlags & 8))
    return;
  AiCharacterRecord* record = NewOrNull<AiCharacterRecord>(object, m_view, m_owner);
  if (!record)
    return;
  int added = 0;
  ObjectKey recordKey;
  if (!ObjectKeySet::Find(record->GetRecordKey(&recordKey)))
  {
    added = ObjectKeySet::Add(record);
    record->MarkChanged();
  }
  if (added == 0)
  {
    delete record;
  }
  else
  {
    record->MarkChanged();
    const uint32_t flags = record->m_flags.m_value;
    if ((flags & ai_character_flag::kOwnSide) && (flags & ai_character_flag::kCharacter) &&
        (flags & ai_character_flag::kStoryCharacter) == 0)
      --m_usableCount;
  }
}

void AiCharacterRecordSet::SetRecordLocation(const ObjectKey* key, const ObjectKey* from, const ObjectKey* to)
{
  AiCharacterRecord* record = static_cast<AiCharacterRecord*>(ObjectKeySet::Find(key));
  if (record)
    record->SetLocation(to, from);
}

void AiCharacterRecordSet::RemoveCharacter(const ObjectKey* key)
{
  if (ObjectKeySet::Find(key))
    RemoveRecord(key->m_value);
}

ObjectKey* AiCharacterRecordSet::FindContainingSystem(ObjectKey* out, const ObjectKey* key)
{
  ObjectKey current = *key;
  for (;;)
  {
    if (ai::KeyInFamilies(current, 0x90, 0x98))
      break;
    GameObjNotifier* object = GetObjectInFamilies_01_ff(m_view, &current);
    if (!object || !object->m_parent)
      break;
    ObjectKey parentKey;
    current = *GetObjectKey(object->m_parent, &parentKey);
  }
  *out = current;
  return out;
}

AiCharacterRecord* AiCharacterRecordSet::FindRecord(uint32_t key)
{
  ObjectKey probe;
  probe.m_value = key;
  return static_cast<AiCharacterRecord*>(ObjectKeySet::Find(&probe));
}

void AiCharacterRecordSet::RemoveRecord(uint32_t key)
{
  ObjectKey probe;
  probe.m_value = key;
  ObjectKeySet::RemoveKey(&probe);
}
