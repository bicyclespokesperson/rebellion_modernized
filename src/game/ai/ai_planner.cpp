#include "ai/ai_planner.h"

#include "ai/ai_character_records.h"
#include "ai/ai_order_util.h"
#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "gameobj/object_lookup.h"
#include "mission/mission.h"
#include "orders/mission_orders.h"

using ai::AddToTeam;
using ai::IsSectorKey;
using ai::IsSystemKey;
using ai::KeyInFamilies;
using ai::KeyOfNode;
using ai::KeyOfObject;

namespace
{

constexpr int kMissionOrder = 0x240;
constexpr int kMissionCancelOrder = 0x250;

// AiMissionPlanner::m_flags
constexpr uint32_t kHasLeaders = 1;
constexpr uint32_t kHasMembers = 2;
constexpr uint32_t kHasDestination = 4;
constexpr uint32_t kTeamInPlace = 8;
constexpr uint32_t kDestinationReached = 0x10;
constexpr uint32_t kMoving = 0x40;
constexpr uint32_t kStalled = 0x80;
constexpr uint32_t kFinished = 0x10000000;
constexpr uint32_t kAbortNow = 0x20000000;
constexpr uint32_t kAbortRequested = 0x40000000;
constexpr uint32_t kMissionChanged = 0x80000000;

// AiCharacterRecord::m_flags
constexpr uint32_t kCharacterUnavailable = 0x74000f1;
constexpr uint32_t kCharacterUnusable = 0x7400051;
constexpr uint32_t kCharacterBusy = 0x50;
constexpr uint32_t kCharacterReleased = 0x800000;

AiCharacterRecord* FindCharacter(AiMissionPlanner* planner, const ObjectKey& key)
{
  return static_cast<AiCharacterRecord*>(planner->m_characters->Find(&key));
}

AiSystemEval* FindSystem(AiMissionPlanner* planner, const ObjectKey& key)
{
  return planner->m_owner->m_systems.FindEval(key);
}

// The team members (leaders then members) of the planner as a flat list of the keys of the nodes.
template <class Visit>
void ForEachKey(ObjectKeySet& set, Visit visit)
{
  for (const auto& entry : set.Entries())
    visit(KeyOfNode(static_cast<ObjectKeyNode*>(entry.get())));
}

// Removes every key of the set with `remove` (taking the successor first, as the originals do).
template <class Remove>
void RemoveAll(ObjectKeySet& set, Remove remove)
{
  auto position = set.Entries().begin();
  while (position != set.Entries().end())
  {
    auto* node = static_cast<ObjectKeyNode*>((*position).get());
    ++position;
    remove(KeyOfNode(node));
  }
}

// The system the character is at: its location if that is a system, else the system its ship is in (an unset key if none).
ObjectKey SystemOf(AiMissionPlanner* planner, const AiCharacterRecord* character)
{
  ObjectKey location = character->m_location;
  if (!IsSystemKey(location))
  {
    GameObjNotifier* object = GetObjectInFamilies_01_ff(planner->m_view, &location);
    if (object)
    {
      bool isSystem;
      do
      {
        isSystem = IsSystemKey(KeyOfObject(object));
      } while (!isSystem && (object = object->m_parent) != nullptr);
      if (object)
        location = KeyOfObject(object);
    }
  }
  return location;
}

} // namespace

ObjectKey* AiSystemEval::GetSectorKey(ObjectKey* out)
{
  return m_sector->GetKey(out);
}

// ---- AiMissionPlanner -------------------------------------------------------------------------------------------------

AiMissionPlanner::AiMissionPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
: IdNode(),
  m_flags(0),
  m_missionObject(),
  m_target(),
  m_x34(),
  m_destination(),
  m_leaders(),
  m_members(),
  m_finished(),
  m_selector()
{
  m_missionKey.m_value = 0;
  m_owner = owner;
  m_view = view;
  m_state = 0;
  m_characters = characters;
  m_stall = 0;
}

AiMissionPlanner::~AiMissionPlanner()
{
  m_characters = nullptr;
  m_owner = nullptr;
}

void AiMissionPlanner::Read(istream* in)
{
  ReadId(in);
  ReadValue32(in, &m_state);
  ReadValuePersistentInt(in, &m_flags);
  ReadValue32(in, &m_view);
  m_missionObject.Read(in);
  m_missionKey.Read(in);
  m_target.Read(in);
  m_x34.Read(in);
  m_destination.Read(in);
  m_selector.Read(in);
  m_leaders.Load(in);
  m_members.Load(in);
  m_finished.Load(in);
  ReadValue32(in, &m_stall);
}

void AiMissionPlanner::Write(ostream* out)
{
  WriteId(out);
  WriteValue32(out, &m_state);
  WriteValuePersistentInt(out, &m_flags);
  WriteValue32(out, &m_view);
  m_missionObject.Write(out);
  m_missionKey.Write(out);
  m_target.Write(out);
  m_x34.Write(out);
  m_destination.Write(out);
  m_selector.Write(out);
  m_leaders.Save(out);
  m_members.Save(out);
  m_finished.Save(out);
  WriteValue32(out, &m_stall);
}

void AiMissionPlanner::OnPlannerEvent(int event)
{
  if (event >= 3 && event <= 10)
    ResetMissionMark();
}

int AiMissionPlanner::ShouldDeferAbort()
{
  return 0;
}

void AiMissionPlanner::OnFinished()
{
}

int AiMissionPlanner::CanUse(const ObjectKey*)
{
  return 1;
}

void AiMissionPlanner::SetMissionObject(const ObjectKey* key)
{
  if (!m_finished.Find(key))
  {
    m_missionObject = *key;
    m_flags.m_value |= kMissionChanged;
  }
}

int AiMissionPlanner::AdvanceStall()
{
  ++m_stall;
  const int limit = GetStallLimit();
  const int stalled = limit < m_stall;
  if (stalled)
  {
    m_flags.m_value |= kStalled;
    m_stall = 0;
  }
  return stalled;
}

void AiMissionPlanner::ReleaseUnfit()
{
  m_flags.m_value &= 0x1fffffff;
  auto leaderPosition = m_leaders.Entries().begin();
  while (leaderPosition != m_leaders.Entries().end())
  {
    auto* node = static_cast<ObjectKeyNode*>((*leaderPosition).get());
    ++leaderPosition;
    bool keep = false;
    AiCharacterRecord* character = FindCharacter(this, KeyOfNode(node));
    if (character)
    {
      character->m_flags.m_value &= 0xffffbfff;
      character->MarkChanged();
      if ((m_flags.m_value & kStalled) == 0 && (character->m_flags.m_value & kCharacterBusy) == 0)
        keep = true;
    }
    const ObjectKey key = KeyOfNode(node);
    if (!keep)
      RemoveLeader(&key);
  }
  if ((m_flags.m_value & kStalled) != 0)
    m_destination.Reset();
  m_flags.m_value &= ~kStalled;
  auto memberPosition = m_members.Entries().begin();
  while (memberPosition != m_members.Entries().end())
  {
    auto* node = static_cast<ObjectKeyNode*>((*memberPosition).get());
    ++memberPosition;
    bool keep = false;
    AiCharacterRecord* character = FindCharacter(this, KeyOfNode(node));
    if (character)
    {
      character->m_flags.m_value &= 0xffffbfff;
      character->MarkChanged();
      if ((character->m_flags.m_value & kCharacterBusy) == 0)
        keep = true;
    }
    const ObjectKey key = KeyOfNode(node);
    if (!keep)
      RemoveMember(&key);
  }
}

void AiMissionPlanner::Finish()
{
  if ((m_flags.m_value & kFinished) != 0 && (m_flags.m_value & 0xe0000000) == 0)
  {
    RemoveAll(m_leaders,
              [this](const ObjectKey& key)
              {
                RemoveLeader(&key);
              });
    RemoveAll(m_members,
              [this](const ObjectKey& key)
              {
                RemoveMember(&key);
              });
    m_owner->RetirePlanner(m_id);
  }
  OnFinished();
}

void AiMissionPlanner::RefreshTeams()
{
  m_flags.m_value &= 0xfffffeff;
  auto leaderPosition = m_leaders.Entries().begin();
  while (leaderPosition != m_leaders.Entries().end())
  {
    auto* node = static_cast<ObjectKeyNode*>((*leaderPosition).get());
    ++leaderPosition;
    const ObjectKey key = KeyOfNode(node);
    if (IsUsable(&key) == 0)
      RemoveLeader(&key);
    else
      m_flags.m_value |= 0x100;
  }
  auto memberPosition = m_members.Entries().begin();
  while (memberPosition != m_members.Entries().end())
  {
    auto* node = static_cast<ObjectKeyNode*>((*memberPosition).get());
    ++memberPosition;
    const ObjectKey key = KeyOfNode(node);
    if (IsUsable(&key) == 0)
      RemoveMember(&key);
    else
      m_flags.m_value |= 0x100;
  }
}

OrderBase* AiMissionPlanner::AbortMission()
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet team;
  auto* mission = static_cast<Mission*>(GetObjectInFamilies_40_80(m_view, &m_missionObject));
  if (mission && mission->IsPhase8() != 0)
  {
    if ((m_flags.m_value & kAbortRequested) != 0 && ShouldDeferAbort() == 0)
    {
      m_flags.m_value &= 0xbfffffff;
      m_flags.m_value |= kAbortNow;
    }
    if ((m_flags.m_value & kAbortNow) != 0)
    {
      order = static_cast<OrderBase*>(CreateMessageByKind(kMissionCancelOrder));
      if (order)
      {
        order->m_side = m_view;
        auto* node = NewOrNull<ObjectKeyValueNode>(m_missionObject, 0);
        if (node)
          team.Add(node);
        order->SetTeam(&team);
      }
    }
    m_flags.m_value &= 0x9fffffff;
  }
  return order;
}

OrderBase* AiMissionPlanner::StartMission()
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet leaders;
  ObjectKeyValueSet members;
  if (KeyInFamilies(m_target, 1, 0xff))
  {
    order = static_cast<OrderBase*>(CreateMessageByKind(kMissionOrder));
    if (order)
    {
      auto* mission = static_cast<MissionOrder*>(order);
      order->m_side = m_view;
      order->SetTarget(&m_target);
      mission->m_missionKey.m_value = m_missionKey.m_value;
      mission->m_unk44 = m_id;
      for (auto position = m_leaders.Entries().begin(); position != m_leaders.Entries().end(); ++position)
      {
        auto* node = static_cast<ObjectKeyNode*>((*position).get());
        AiCharacterRecord* character = FindCharacter(this, KeyOfNode(node));
        if (character && (character->m_flags.m_value & kCharacterUnavailable) == 0)
        {
          character->m_flags.m_value |= 0x4000;
          AddToTeam(&leaders, KeyOfNode(node));
        }
      }
      order->SetTeam(&leaders);
      for (auto position = m_members.Entries().begin(); position != m_members.Entries().end(); ++position)
      {
        auto* node = static_cast<ObjectKeyNode*>((*position).get());
        AiCharacterRecord* character = FindCharacter(this, KeyOfNode(node));
        if (character && (character->m_flags.m_value & kCharacterUnavailable) == 0)
        {
          character->m_flags.m_value |= 0x4000;
          AddToTeam(&members, KeyOfNode(node));
        }
      }
      mission->m_characters = members;
    }
  }
  return order;
}

int AiMissionPlanner::AddLeader(const ObjectKey* character)
{
  int added = 0;
  if (KeyInFamilies(*character, 0x30, 0x40) && !m_leaders.Find(character))
  {
    auto* node = NewOrNull<ObjectKeyNode>(*character);
    if (node)
    {
      auto* record = static_cast<AiCharacterRecord*>(m_characters->Find(character));
      if (!record)
      {
        delete node;
      }
      else
      {
        record->m_flags.m_value &= 0xff7fffff;
        record->m_x24 = m_id;
        record->m_agentId = 0;
        record->m_eligibilityRank = GetTier();
        record->MarkChanged();
        m_leaders.Add(node);
        m_stall = 0;
        added = 1;
      }
    }
  }
  return added;
}

int AiMissionPlanner::AddMember(const ObjectKey* character)
{
  int added = 0;
  if (KeyInFamilies(*character, 0x30, 0x40) && !m_members.Find(character))
  {
    auto* node = NewOrNull<ObjectKeyNode>(*character);
    if (node)
    {
      auto* record = static_cast<AiCharacterRecord*>(m_characters->Find(character));
      if (!record)
      {
        delete node;
      }
      else
      {
        record->m_flags.m_value &= 0xff7fffff;
        record->m_x24 = m_id;
        record->m_agentId = 0;
        record->m_eligibilityRank = GetTier();
        record->MarkChanged();
        m_members.Add(node);
        added = 1;
      }
    }
  }
  return added;
}

int AiMissionPlanner::RemoveLeader(const ObjectKey* character)
{
  if (!m_leaders.Find(character))
    return 0;
  auto* record = static_cast<AiCharacterRecord*>(m_characters->Find(character));
  if (record && record->m_x24 == static_cast<int>(m_id))
  {
    record->m_flags.m_value |= kCharacterReleased;
    record->m_x24 = 0;
    record->m_agentId = 0;
    record->m_eligibilityRank = 0;
    record->MarkChanged();
  }
  m_leaders.RemoveKey(character);
  return 1;
}

int AiMissionPlanner::RemoveMember(const ObjectKey* character)
{
  if (!m_members.Find(character))
    return 0;
  auto* record = static_cast<AiCharacterRecord*>(m_characters->Find(character));
  if (record && record->m_x24 == static_cast<int>(m_id))
  {
    record->m_flags.m_value |= kCharacterReleased;
    record->m_x24 = 0;
    record->m_agentId = 0;
    record->m_eligibilityRank = 0;
    record->MarkChanged();
  }
  m_members.RemoveKey(character);
  return 1;
}

void AiMissionPlanner::ResetMissionMark()
{
  m_flags.m_value &= 0x7fffffff;
  if (!m_finished.Find(&m_missionObject))
  {
    auto* node = NewOrNull<ObjectKeyNode>(m_missionObject);
    if (node)
      m_finished.Add(node);
  }
}

void AiMissionPlanner::RemoveFirstMember()
{
  const ObjectKey key = KeyOfNode(m_members.First());
  RemoveMember(&key);
}

int AiMissionPlanner::IsUsable(const ObjectKey* character)
{
  int usable = 0;
  auto* record = static_cast<AiCharacterRecord*>(m_characters->Find(character));
  if (record && (record->m_flags.m_value & kCharacterUnusable) == 0 && record->m_x24 == static_cast<int>(m_id))
  {
    if (IsReachable(character) != 0 && CanUse(character) != 0)
      usable = 1;
  }
  return usable;
}

int AiMissionPlanner::IsReachable(const ObjectKey* character)
{
  int reachable = 0;
  ObjectKey location;
  auto* record = static_cast<AiCharacterRecord*>(m_owner->m_characters.Find(character));
  if (record)
  {
    reachable = 1;
    location = record->m_location;
    if (IsSystemKey(location) && (record->m_flags.m_value & 0x20) == 0)
    {
      AiSystemEval* system = m_owner->m_systems.FindEval(location);
      if (system && (system->m_flags28.m_value & 2) != 0)
        reachable = 0;
    }
  }
  return reachable;
}

// ---- AiFreePlanner ----------------------------------------------------------------------------------------------------

AiFreePlanner::AiFreePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
: AiMissionPlanner(view, owner, characters)
{
}

AiFreePlanner::~AiFreePlanner() = default;

void AiFreePlanner::Read(istream* in)
{
  AiMissionPlanner::Read(in);
}

void AiFreePlanner::Write(ostream* out)
{
  AiMissionPlanner::Write(out);
}

int AiFreePlanner::IsActive()
{
  return (m_flags.m_value & kMissionChanged) == 0;
}

OrderBase* AiFreePlanner::Step(int* done)
{
  OrderBase* order = nullptr;
  *done = 0;
  switch (m_state)
  {
    case 1:
      if ((m_flags.m_value & 0x60000000) == 0)
      {
        if ((m_flags.m_value & kFinished) == 0)
        {
          m_state = 2;
        }
        else
        {
          Finish();
          *done = 1;
        }
      }
      else
      {
        order = AbortMission();
        m_state = 0;
        *done = 1;
      }
      break;
    case 2:
      RefreshTeams();
      m_state = 3;
      break;
    case 3:
      Prepare();
      m_state = 4;
      break;
    case 4:
      PrepareMembers();
      m_state = 5;
      break;
    case 5:
      ChooseTarget();
      m_state = 6;
      break;
    case 6:
      if (ChooseDestination() != 0)
      {
        m_state = 7;
        break;
      }
      *done = 1;
      break;
    case 7:
      order = MoveTeams();
      if ((m_flags.m_value & kTeamInPlace) != 0 && (m_flags.m_value & kDestinationReached) != 0)
      {
        m_state = 8;
        break;
      }
      *done = 1;
      break;
    case 8:
      order = StartMission();
      *done = 1;
      break;
    default:
      m_state = 1;
      break;
  }
  if (*done != 0)
    m_state = 0;
  return order;
}

int AiFreePlanner::CountMembersAtDestination()
{
  int count = 0;
  ForEachKey(m_members,
             [&](const ObjectKey& key)
             {
               AiCharacterRecord* character = FindCharacter(this, key);
               if (character && character->m_location.m_value == m_destination.m_value &&
                   (character->m_flags.m_value & kCharacterUnavailable) == 0)
                 ++count;
             });
  return count;
}

int AiFreePlanner::ChooseDestination()
{
  ObjectKey scratch;
  m_flags.m_value &= ~kHasDestination;
  if ((m_flags.m_value & kHasLeaders) == 0)
  {
    m_destination.Reset();
    return (m_flags.m_value & kHasDestination) != 0;
  }
  if (IsSystemKey(m_destination))
  {
    AiSystemEval* system = FindSystem(this, m_destination);
    if (system)
    {
      if ((system->m_flags30.m_value & 1) == 0 || (system->m_flags28.m_value & 2) != 0)
        m_destination.Reset();
      else
        m_flags.m_value |= kHasDestination;
    }
  }
  if ((m_flags.m_value & kHasDestination) != 0)
    return 1;
  if (IsSystemKey(m_x34))
  {
    AiSystemEval* system = FindSystem(this, m_x34);
    if (system)
    {
      if ((system->m_flags30.m_value & 1) == 0 || (system->m_flags28.m_value & 2) != 0)
      {
        ObjectKey sector;
        system->GetSectorKey(&sector);
        m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&sector, 0, 0, 1, 2, 0, 0, 1));
        m_destination = *m_selector.PickBest(&scratch);
        m_selector.Reset();
        if (!IsSystemKey(m_destination))
        {
          system->GetSectorKey(&sector);
          m_destination = *m_owner->FindNearestSystemWithFleets(&scratch, &sector, 0, 0, 1, 0, 0, 0x200);
        }
      }
      else
      {
        m_destination = m_x34;
      }
    }
  }
  if (IsSystemKey(m_destination))
    m_flags.m_value |= kHasDestination;
  if ((m_flags.m_value & kHasDestination) != 0)
    return 1;
  for (auto position = m_leaders.Entries().begin(); position != m_leaders.Entries().end(); ++position)
  {
    auto* node = static_cast<ObjectKeyNode*>((*position).get());
    if ((m_flags.m_value & kHasDestination) != 0)
      return 1;
    const ObjectKey leaderKey = KeyOfNode(node);
    auto* character = static_cast<AiCharacterRecord*>(m_owner->m_characters.Find(&leaderKey));
    if (!character)
      continue;
    ObjectKey location = SystemOf(this, character);
    if (!IsSystemKey(location))
      continue;
    AiSystemEval* system = FindSystem(this, location);
    if (system && (system->m_flags30.m_value & 1) != 0 && (system->m_flags28.m_value & 2) == 0)
    {
      m_flags.m_value |= kHasDestination;
      system->GetKey(&m_destination);
    }
  }
  return (m_flags.m_value & kHasDestination) != 0;
}

OrderBase* AiFreePlanner::MoveTeams()
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet team;
  m_flags.m_value &= ~kTeamInPlace;
  ForEachKey(m_leaders,
             [&](const ObjectKey& key)
             {
               AiCharacterRecord* character = FindCharacter(this, key);
               if (character && (character->m_flags.m_value & kCharacterUnavailable) == 0)
               {
                 if (character->m_location.m_value == m_destination.m_value)
                   m_flags.m_value |= kTeamInPlace;
                 else
                   AddToTeam(&team, key);
               }
             });
  ForEachKey(m_members,
             [&](const ObjectKey& key)
             {
               AiCharacterRecord* character = FindCharacter(this, key);
               if (character && (character->m_flags.m_value & kCharacterUnavailable) == 0 &&
                   character->m_location.m_value != m_destination.m_value)
                 AddToTeam(&team, key);
             });
  if (team.First() && (order = static_cast<OrderBase*>(CreateMessageByKind(ai::kMoveOrder))))
  {
    order->m_side = m_view;
    order->SetTeam(&team);
    order->SetTarget(&m_destination);
  }
  return order;
}

// ---- AiSystemPlanner --------------------------------------------------------------------------------------------------

AiSystemPlanner::AiSystemPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
: AiMissionPlanner(view, owner, characters),
  m_sectorKey()
{
}

AiSystemPlanner::~AiSystemPlanner() = default;

void AiSystemPlanner::Read(istream* in)
{
  AiMissionPlanner::Read(in);
  ReadValue32(in, &m_systemRecordId);
  ReadValue32(in, &m_tier);
  m_sectorKey.Read(in);
}

void AiSystemPlanner::Write(ostream* out)
{
  AiMissionPlanner::Write(out);
  WriteValue32(out, &m_systemRecordId);
  WriteValue32(out, &m_tier);
  m_sectorKey.Write(out);
}

void AiSystemPlanner::SetTier(int tier)
{
  if (m_tier == tier)
    return;
  m_tier = tier;
  auto retier = [&](const ObjectKey& key)
  {
    auto* character = static_cast<AiCharacterRecord*>(m_owner->m_characters.Find(&key));
    if (character && character->m_x24 == static_cast<int>(m_id))
      character->m_eligibilityRank = tier;
  };
  ForEachKey(m_leaders, retier);
  ForEachKey(m_members, retier);
}

int AiSystemPlanner::IsActive()
{
  return (m_flags.m_value & kMissionChanged) == 0;
}

OrderBase* AiSystemPlanner::Step(int* done)
{
  OrderBase* order = nullptr;
  *done = 0;
  switch (m_state)
  {
    case 1:
      if ((m_flags.m_value & 0x60000000) == 0)
      {
        if ((m_flags.m_value & kFinished) == 0)
        {
          m_state = 2;
        }
        else
        {
          Finish();
          m_state = 0;
          *done = 1;
        }
      }
      else
      {
        order = AbortMission();
        m_state = 0;
        *done = 1;
      }
      break;
    case 2:
      m_state = FetchOrderTargets() != 0 ? 3 : 9;
      break;
    case 3:
      m_state = ChooseSystem() != 0 ? 4 : 9;
      break;
    case 4:
      RefreshTeams();
      m_state = 5;
      break;
    case 5:
      GetKindIndexOrSelectLeaders();
      m_state = 6;
      break;
    case 6:
      GetMemberLimitOrSelectMembers();
      m_state = 7;
      break;
    case 7:
      order = MoveTeams();
      if ((m_flags.m_value & kTeamInPlace) != 0)
      {
        m_state = 8;
        break;
      }
      m_state = 0;
      *done = 1;
      break;
    case 8:
      order = StartMission();
      m_state = 0;
      *done = 1;
      break;
    case 9:
      Disband();
      m_state = 0;
      *done = 1;
      break;
    default:
      m_state = 1;
      break;
  }
  if (*done != 0)
    m_state = 0;
  return order;
}

int AiSystemPlanner::FetchOrderTargets()
{
  m_x34.Reset();
  m_target.Reset();
  m_flags.m_value &= ~kDestinationReached;
  if (m_systemRecordId != 0)
  {
    auto* record = static_cast<AiSystemRecord*>(m_owner->m_systemRecords.FindById(m_systemRecordId));
    if (record)
      record->GetOrderTargets(m_missionKey.m_value & 0xffffff, &m_x34, &m_target);
  }
  const bool found = KeyInFamilies(m_target, 1, 0xff) && IsSystemKey(m_x34);
  if (found)
    m_flags.m_value |= kDestinationReached;
  return found;
}

int AiSystemPlanner::ChooseSystem()
{
  ObjectKey scratch;
  m_flags.m_value &= ~kHasDestination;
  if (!IsSectorKey(m_sectorKey))
    return (m_flags.m_value & kHasDestination) != 0;
  if (IsSystemKey(m_destination))
  {
    AiSystemEval* system = FindSystem(this, m_destination);
    if (system)
    {
      ObjectKey sector;
      system->GetSectorKey(&sector);
      const bool unusable = (system->m_flags30.m_value & 1) == 0 || (system->m_flags28.m_value & 2) != 0;
      if (sector.m_value == m_sectorKey.m_value)
      {
        if (unusable)
        {
          m_destination.Reset();
        }
        else
        {
          m_flags.m_value &= ~kMoving;
          m_flags.m_value |= kHasDestination;
        }
      }
      else if ((m_flags.m_value & kMoving) != 0)
      {
        if (unusable)
          m_destination.Reset();
        else
          m_flags.m_value |= kHasDestination;
      }
    }
  }
  if ((m_flags.m_value & kHasDestination) != 0)
    return 1;
  for (auto position = m_leaders.Entries().begin(); position != m_leaders.Entries().end(); ++position)
  {
    auto* node = static_cast<ObjectKeyNode*>((*position).get());
    if ((m_flags.m_value & kHasDestination) != 0)
      return 1;
    const ObjectKey leaderKey = KeyOfNode(node);
    auto* character = static_cast<AiCharacterRecord*>(m_owner->m_characters.Find(&leaderKey));
    if (!character)
      continue;
    ObjectKey location = SystemOf(this, character);
    if (!IsSystemKey(location))
      continue;
    AiSystemEval* system = FindSystem(this, location);
    if (!system)
      continue;
    ObjectKey sector;
    system->GetSectorKey(&sector);
    if (sector.m_value == m_sectorKey.m_value && (system->m_flags30.m_value & 1) != 0 &&
        (system->m_flags28.m_value & 2) == 0)
    {
      m_flags.m_value &= ~kMoving;
      m_flags.m_value |= kHasDestination;
      system->GetKey(&m_destination);
    }
  }
  if ((m_flags.m_value & kHasDestination) != 0)
    return 1;
  m_flags.m_value &= ~kMoving;
  if (IsSectorKey(m_sectorKey))
  {
    m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&m_sectorKey, 0, 0, 1, 2, 0, 0, 1));
    m_destination = *m_selector.PickBest(&scratch);
    m_selector.Reset();
  }
  if (IsSystemKey(m_destination))
    m_flags.m_value |= kHasDestination;
  if ((m_flags.m_value & kHasDestination) != 0)
    return 1;
  m_flags.m_value &= ~kMoving;
  if (IsSectorKey(m_sectorKey))
  {
    m_destination = *m_owner->FindNearestSystemWithFleets(&scratch, &m_sectorKey, 0, 0, 1, 0, 0, 0x200);
    if (IsSystemKey(m_destination))
      m_flags.m_value |= kHasDestination | kMoving;
  }
  return (m_flags.m_value & kHasDestination) != 0;
}

OrderBase* AiSystemPlanner::MoveTeams()
{
  OrderBase* order = nullptr;
  ObjectKeyValueSet team;
  m_flags.m_value &= ~kTeamInPlace;
  // Everyone is in place when there are leaders and no leader or member is unavailable or still away.
  bool allInPlace = static_cast<uint16_t>(m_leaders.Count()) != 0;
  auto gather = [&](ObjectKeySet& set)
  {
    for (auto position = set.Entries().begin(); position != set.Entries().end(); ++position)
    {
      auto* node = static_cast<ObjectKeyNode*>((*position).get());
      AiCharacterRecord* character = FindCharacter(this, KeyOfNode(node));
      if (!character)
        continue;
      if ((character->m_flags.m_value & kCharacterUnavailable) != 0)
      {
        allInPlace = false;
      }
      else if (character->m_location.m_value != m_destination.m_value)
      {
        allInPlace = false;
        AddToTeam(&team, KeyOfNode(node));
      }
    }
  };
  gather(m_leaders);
  gather(m_members);
  if (team.First() && (order = static_cast<OrderBase*>(CreateMessageByKind(ai::kMoveOrder))))
  {
    order->m_side = m_view;
    order->SetTeam(&team);
    order->SetTarget(&m_destination);
  }
  if (allInPlace && (m_flags.m_value & kHasLeaders) != 0 && (m_flags.m_value & kHasMembers) != 0)
    m_flags.m_value |= kTeamInPlace;
  return order;
}

void AiMissionPlanner::HandleEvent(int event)
{
  if (event == 0)
  {
    m_flags.m_value |= kMissionChanged;
    return;
  }
  if (event == 2)
  {
    ResetMissionMark();
    if (AdvanceStall() != 0)
    {
      m_flags.m_value |= kAbortNow;
      return;
    }
    m_flags.m_value |= kAbortRequested;
    return;
  }
  if (event == 3)
  {
    ResetMissionMark();
    m_stall = 0;
    m_flags.m_value |= kAbortRequested;
  }
}

void AiMissionPlanner::MarkMembersAssigned()
{
  m_flags.m_value |= kHasMembers;
}

void AiMissionPlanner::HandleQuietEvent(int event)
{
  if (event == 0)
  {
    m_flags.m_value |= kMissionChanged;
    return;
  }
  if (event == 2)
  {
    AdvanceStall();
    ResetMissionMark();
    return;
  }
  if (event == 3)
  {
    m_stall = 0;
    ResetMissionMark();
  }
}

void AiMissionPlanner::AssignNoMembers()
{
  m_flags.m_value |= kHasMembers;
  if (static_cast<uint16_t>(m_members.Count()) != 0)
    RemoveFirstMember();
}

void AiSystemPlanner::Disband()
{
  if ((m_flags.m_value & 0xe0000000) != 0)
    return;
  RemoveAll(m_leaders,
            [this](const ObjectKey& key)
            {
              RemoveLeader(&key);
            });
  RemoveAll(m_members,
            [this](const ObjectKey& key)
            {
              RemoveMember(&key);
            });
}
