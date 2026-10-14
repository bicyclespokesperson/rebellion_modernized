#include "ai/ai_planner_util.h"

#include "character/character.h"
#include "fleet/fleet_iterator.h"

using ai::IsSystemKey;
using ai::KeyInFamilies;
using ai::KeyOfNode;

namespace ai_planner
{

namespace
{

constexpr uint32_t kHasLeaders = 1;
constexpr uint32_t kTargetChosen = 0x10;
constexpr uint32_t kStalled = 0x20;

bool IsCharacterKey(const ObjectKey& key)
{
  return KeyInFamilies(key, 0x30, 0x40) != 0;
}

} // namespace

int SumLeaderValue(AiMissionPlanner* planner, int valueIndex)
{
  int sum = 0;
  if ((planner->m_flags.m_value & kStalled) == 0)
  {
    for (auto position = planner->m_leaders.Entries().begin(); position != planner->m_leaders.Entries().end();
         ++position)
    {
      auto* node = static_cast<ObjectKeyNode*>((*position).get());
      const ObjectKey key = KeyOfNode(node);
      auto* character = static_cast<AiCharacterRecord*>(planner->m_characters->Find(&key));
      if (character)
        sum += character->m_values[valueIndex];
    }
  }
  return sum;
}

void ChooseLeader(AiFreePlanner* planner,
                  uint32_t capabilities,
                  uint32_t valueIndex,
                  int minValue,
                  uint32_t forbidFlags,
                  uint32_t forbidCaps,
                  int wanted,
                  int secondValueIndex)
{
  ObjectKey picked;
  ObjectKey scratch;
  planner->m_flags.m_value &= ~kHasLeaders;
  if (static_cast<uint16_t>(planner->m_leaders.Count()) < wanted)
  {
    planner->m_selector.Add(
      planner->m_characters->RankEligible(0, 2, capabilities, forbidFlags, forbidCaps, valueIndex, minValue, 10000, 1));
    if (secondValueIndex >= 0)
      planner->m_selector.Add(planner->m_characters->RankEligible(0, 2, capabilities, forbidFlags, forbidCaps,
                                                                  secondValueIndex, minValue, 10000, 1));
    picked = *planner->m_selector.PickBest(&scratch);
    while (IsCharacterKey(picked) && static_cast<uint16_t>(planner->m_leaders.Count()) < wanted)
    {
      if (planner->IsReachable(&picked) != 0)
        planner->AddLeader(&picked);
      picked = *planner->m_selector.PickBest(&scratch);
    }
    planner->m_selector.Reset();
  }
  if (static_cast<uint16_t>(planner->m_leaders.Count()) >= wanted)
    planner->m_flags.m_value |= kHasLeaders;
}

void ChooseSystemLeaders(AiMissionPlanner* planner, const SystemLeaders& spec)
{
  ObjectKey picked;
  ObjectKey scratch;
  planner->m_flags.m_value &= ~kHasLeaders;
  if (static_cast<uint16_t>(planner->m_leaders.Count()) < spec.wanted)
  {
    planner->m_selector.Add(planner->m_characters->RankByValue(spec.requireFlags, spec.capabilities, spec.forbidFlags,
                                                               0, spec.firstIndex, spec.firstMin, 10000, 1));
    if (spec.secondIndex >= 0)
      planner->m_selector.Add(planner->m_characters->RankByValue(spec.requireFlags, spec.capabilities, spec.forbidFlags,
                                                                 0, spec.secondIndex, spec.firstMin, 10000, 1));
    if (IsSystemKey(planner->m_destination))
      planner->m_selector.Add(planner->m_characters->RankByDistance(&planner->m_destination, spec.requireFlags,
                                                                    spec.capabilities, spec.forbidFlags, 0, 2));
    picked = *planner->m_selector.PickBest(&scratch);
    while (IsCharacterKey(picked) && static_cast<uint16_t>(planner->m_leaders.Count()) < spec.wanted)
    {
      if (planner->IsReachable(&picked) != 0)
        planner->AddLeader(&picked);
      picked = *planner->m_selector.PickBest(&scratch);
    }
    planner->m_selector.Reset();
  }
  if (static_cast<uint16_t>(planner->m_leaders.Count()) >= spec.wanted)
    planner->m_flags.m_value |= kHasLeaders;
}

int MembersWantedAtTarget(AiMissionPlanner* planner)
{
  AiSystemEval* system = planner->m_owner->m_systems.FindEval(planner->m_x34);
  return system ? (system->Stat(0xbc) + 2) / 2 : 0;
}

void ChooseMembers(AiMissionPlanner* planner, int wanted, uint32_t capabilities)
{
  ObjectKey picked;
  ObjectKey scratch;
  planner->m_flags.m_value &= ~2u;
  if (static_cast<uint16_t>(planner->m_members.Count()) < wanted)
  {
    planner->m_selector.Add(
      planner->m_characters->RankByValue(0x10800002, capabilities, kForbidUnusable, 0, 1, 0x33, 10000, 1));
    if (IsSystemKey(planner->m_destination))
      planner->m_selector.Add(planner->m_characters->RankByDistance(&planner->m_destination, 0x10800002, capabilities,
                                                                    kForbidUnusable, 0, 2));
    picked = *planner->m_selector.PickBest(&scratch);
    while (IsCharacterKey(picked) && static_cast<uint16_t>(planner->m_members.Count()) < wanted)
    {
      if (planner->IsReachable(&picked) != 0)
        planner->AddMember(&picked);
      picked = *planner->m_selector.PickBest(&scratch);
    }
    planner->m_selector.Reset();
  }
  if (static_cast<uint16_t>(planner->m_members.Count()) >= wanted)
  {
    planner->m_flags.m_value |= 2;
    if (static_cast<uint16_t>(planner->m_members.Count()) > wanted)
      planner->RemoveFirstMember();
  }
}

void RankTargetSystem(AiFreePlanner* planner, uint32_t sectorMask, uint32_t systemMask, uint32_t sectorStat)
{
  ObjectKey scratch;
  AiSideController* owner = planner->m_owner;
  AiRankedSelector& selector = planner->m_selector;
  if (IsSystemKey(planner->m_destination))
  {
    selector.Add(owner->RankSectorsByDistance(&planner->m_destination, 0, sectorMask, 0, 0, 0, 0, 2));
    planner->m_x34 = *selector.PickBest(&scratch);
    selector.Reset();
    selector.Add(
      owner->RankSystemsOfSectorByDistance(&planner->m_x34, &planner->m_destination, 0, systemMask, 0, 0, 0, 0, 2));
    planner->m_x34 = *selector.PickBest(&scratch);
  }
  else
  {
    selector.Add(owner->RankSectors(0, sectorMask, 0, 0, 0, 0, sectorStat, 1));
    planner->m_x34 = *selector.PickBest(&scratch);
    selector.Reset();
    selector.Add(owner->RankSystemsOfSector(&planner->m_x34, 0, systemMask, 0, 0, 0, 0, 0, 1));
    planner->m_x34 = *selector.PickBest(&scratch);
  }
  selector.Reset();
  if (IsSystemKey(planner->m_x34))
  {
    planner->m_target = planner->m_x34;
    planner->m_flags.m_value |= kTargetChosen;
  }
}

int ChooseTarget(AiFreePlanner* planner, uint32_t sectorMask, uint32_t systemMask, uint32_t sectorStat)
{
  ObjectKey scratch;
  AiSideController* owner = planner->m_owner;
  AiRankedSelector& selector = planner->m_selector;
  planner->m_target.Reset();
  planner->m_flags.m_value &= ~kTargetChosen;
  if ((planner->m_flags.m_value & kHasLeaders) == 0)
  {
    planner->m_x34.Reset();
  }
  else
  {
    if (IsSystemKey(planner->m_x34))
    {
      AiSystemEval* system = owner->m_systems.FindEval(planner->m_x34);
      if (system && (system->m_flags2c.m_value & systemMask) != 0)
      {
        planner->m_target = planner->m_x34;
        planner->m_flags.m_value |= kTargetChosen;
      }
    }
    if ((planner->m_flags.m_value & kTargetChosen) != 0)
      return 1;
    RankTargetSystem(planner, sectorMask, systemMask, sectorStat);
  }
  return (planner->m_flags.m_value & kTargetChosen) != 0;
}

int NoteTargetSystem(AiFreePlanner* planner, uint32_t systemMask)
{
  int result = 0;
  AiSystemEval* system = planner->m_owner->FindSystemEval(&planner->m_x34);
  if (system)
  {
    result = (system->m_flags2c.m_value & systemMask) != 0;
    if ((system->m_flags30.m_value & 1) != 0 && (system->m_flags30.m_value & 0x200) == 0)
      planner->m_destination = planner->m_x34;
  }
  return result;
}

bool IsCharacterTargetKey(const ObjectKey& key)
{
  return KeyInFamilies(key, 0x30, 0x40) != 0;
}

ObjectKey SystemOfCharacter(AiFreePlanner* planner, const AiCharacterRecord* record)
{
  ObjectKey cursor = record->m_location;
  bool done = false;
  while (!IsSystemKey(cursor) && !done)
  {
    GameObjNotifier* object = GetObjectInFamilies_01_ff(planner->m_view, &cursor);
    if (object)
    {
      if (object->m_parent == nullptr)
      {
        cursor.Reset();
        done = true;
      }
      else
      {
        cursor = ai::KeyOfObject(object->m_parent);
      }
    }
  }
  return cursor;
}

namespace
{

void AddUsableCharacters(AiFreePlanner* planner,
                         GameObjNotifier* container,
                         const CharacterHunt& hunt,
                         ObjectKeySet& candidates)
{
  CharacterIterator characters(container, 1);
  for (characters.Begin(); characters.m_current; characters.Advance())
  {
    const ObjectKey key = ai::KeyOfObject(characters.m_current);
    auto* record = static_cast<AiCharacterRecord*>(planner->m_characters->Find(&key));
    if (record && (record->m_flags.m_value & hunt.characterFlagsA) != 0 &&
        (record->m_flags.m_value & hunt.characterFlagsB) != 0)
    {
      ObjectKey scratch;
      auto* node = NewOrNull<ObjectKeyNode>(*record->GetRecordKey(&scratch));
      if (node)
        candidates.Add(node);
    }
  }
}

} // namespace

void ChooseCandidateTarget(AiFreePlanner* planner, const CharacterHunt& hunt)
{
  ObjectKeySet candidates;
  if (!IsSystemKey(planner->m_x34))
    return;
  GameObjNotifier* systemObject = GetObjectInFamilies_90_98(planner->m_view, &planner->m_x34);
  AiSystemEval* system = planner->m_owner->m_systems.FindEval(planner->m_x34);
  if (systemObject && system)
  {
    if ((system->m_flags2c.m_value & hunt.characterSystemMask) != 0)
      AddUsableCharacters(planner, systemObject, hunt, candidates);
    if ((system->m_flags2c.m_value & hunt.shipSystemMask) != 0)
    {
      FleetIterator fleets(systemObject, 1);
      for (fleets.Begin(); fleets.m_current; fleets.Advance())
      {
        AiFleetEval* fleet = planner->m_owner->m_fleets.FindEval(ai::KeyOfObject(fleets.m_current));
        if (fleet && (fleet->m_flags.m_value & 1) == 0 && (fleet->m_flags.m_value & hunt.fleetMask) != 0)
        {
          for (auto position = fleet->m_shipKeys.Entries().begin(); position != fleet->m_shipKeys.Entries().end();
               ++position)
          {
            auto* node = static_cast<ObjectKeyNode*>((*position).get());
            const ObjectKey shipKey = ai::KeyOfNode(node);
            GameObjNotifier* ship = GetObjectInFamilies_14_1c(planner->m_view, &shipKey);
            if (ship)
              AddUsableCharacters(planner, ship, hunt, candidates);
          }
        }
      }
    }
  }
  const auto count = static_cast<uint16_t>(candidates.Count());
  if (count == 0)
    return;
  int steps = GameplayRandomNext(count);
  auto position = candidates.Entries().begin();
  if (position != candidates.Entries().end())
  {
    do
    {
      if (steps < 1)
        break;
      ++position;
      --steps;
    } while (position != candidates.Entries().end());
    if (position != candidates.Entries().end())
      planner->m_target = ai::KeyOfNode(static_cast<ObjectKeyNode*>((*position).get()));
  }
}

} // namespace ai_planner
