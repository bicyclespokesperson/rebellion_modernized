#pragma once
#include <cstdint>

#include "ai/ai_character_records.h"
#include "ai/ai_order_util.h"
#include "ai/ai_planner.h"
#include "ai/ai_records.h"
#include "ai/ai_selector.h"
#include "ai/ai_side_controller.h"
#include "core/checked_new.h"
#include "data/random.h"
#include "gameobj/object_lookup.h"

// Building blocks the concrete planners (ai_planners.h) share. Most of them are the same state machine steps with different
// constants, so each step is written once and the planner classes call it with their numbers.

namespace ai_planner
{

// The planners' common part of a character query: the characters (flags 2) that are free for a planner, see AiMissionPlanner.
constexpr uint32_t kForbidUnusable = 0x7400051;

// The sum of the value `valueIndex` of the leaders' records (0 once the planner is stalled).
int SumLeaderValue(AiMissionPlanner* planner, int valueIndex);

// A leader for a planner that has none: the most eligible reachable character with the capability `capabilities` whose value
// `valueIndex` is at least `minValue`; sets flag 1 when the planner has a leader afterwards.
void ChooseLeader(AiFreePlanner* planner,
                  uint32_t capabilities,
                  uint32_t valueIndex,
                  int minValue,
                  uint32_t forbidFlags = kForbidUnusable,
                  uint32_t forbidCaps = 0,
                  int wanted = 1,
                  int secondValueIndex = -1);

// The leaders of a system planner: up to `wanted` characters with the capabilities `capabilities`, taken from the best of one or
// two rankings by value (`firstIndex`, `secondIndex` or -1) and from those nearest to the destination; sets flag 1 once there are
// enough.
struct SystemLeaders
{
  uint32_t requireFlags;
  uint32_t capabilities;
  uint32_t forbidFlags;
  int firstIndex;
  int firstMin;
  int secondIndex;
  int wanted;
};
void ChooseSystemLeaders(AiMissionPlanner* planner, const SystemLeaders& spec);

// How many members a system planner wants for the system m_x34: half its statistic 0xbc, rounded up.
int MembersWantedAtTarget(AiMissionPlanner* planner);

// The members of a planner: fills the member set up to `wanted` characters that have the capabilities, nearest to the
// destination first; sets flag 2 once there are enough (dropping a surplus member).
void ChooseMembers(AiMissionPlanner* planner, int wanted, uint32_t capabilities);

// The target of a planner: the system m_x34 if it has one of the flags `systemMask` (sets flag 0x10), else the best system by
// distance from the destination (or by the sector statistic) with that flag. Returns non-zero once a target is set.
int ChooseTarget(AiFreePlanner* planner, uint32_t sectorMask, uint32_t systemMask, uint32_t sectorStat);

// The ranking part of ChooseTarget: picks m_x34 near the destination (or by the sector statistic) and flags the planner when it
// is a system.
void RankTargetSystem(AiFreePlanner* planner, uint32_t sectorMask, uint32_t systemMask, uint32_t sectorStat);

// Whether the system m_x34 has one of the flags `systemMask`; takes it as the destination when it is ours and free of enemies.
int NoteTargetSystem(AiFreePlanner* planner, uint32_t systemMask);

// The planners that hunt for one character (Rescue 0x61 and the free 0x62 planner) differ only in these masks.
struct CharacterHunt
{
  uint32_t sectorMask; // flags28/2c of the sectors that may hold targets (RankSectors requireB)
  uint32_t sectorStat; // the sector statistic the sectors are ranked by
  uint32_t systemStat; // the system statistic the systems of a sector are ranked by
  uint32_t characterSystemMask; // systems (flags2c) whose own characters are candidates; also the systems ranked first
  uint32_t shipSystemMask; // systems (flags2c) whose fleets' characters are candidates; ranked second, descending
  uint32_t fleetMask; // fleet flags a fleet needs for its ships' characters to be candidates
  uint32_t characterFlagsA; // flags a candidate character record needs (both)
  uint32_t characterFlagsB;
  bool (*keepsTarget)(uint32_t characterFlags); // whether the target chosen earlier is still wanted
};

bool IsCharacterTargetKey(const ObjectKey& key);

// The system the character record is at, as a key (the unset key if the walk up the parents ends elsewhere).
ObjectKey SystemOfCharacter(AiFreePlanner* planner, const AiCharacterRecord* record);

// Collects the usable candidate characters of the system m_x34 (AiCharacterRecord flags A and B set) and picks a random one as
// the target.
void ChooseCandidateTarget(AiFreePlanner* planner, const CharacterHunt& hunt);

// The ChooseTarget of the hunting planners; Planner::PickCandidate is their ChooseCandidateTarget.
template <class Planner>
int ChooseCharacterTarget(Planner* planner, const CharacterHunt& hunt)
{
  using ai::IsSectorKey;
  using ai::IsSystemKey;
  ObjectKey scratch;
  ObjectKey sector;
  ObjectKeySet candidates;
  AiRankedSelector systemRanking;
  AiSideController* owner = planner->m_owner;
  planner->m_flags.m_value &= ~0x10u;
  const int membersAtDestination = planner->CountMembersAtDestination();
  if (IsSystemKey(planner->m_x34) && IsCharacterTargetKey(planner->m_target))
  {
    auto* record = static_cast<AiCharacterRecord*>(planner->m_characters->Find(&planner->m_target));
    planner->m_target.Reset();
    if (record)
    {
      const ObjectKey location = SystemOfCharacter(planner, record);
      if (planner->m_x34.m_value == location.m_value && hunt.keepsTarget(record->m_flags.m_value))
        planner->m_target = *record->GetRecordKey(&scratch);
    }
  }
  if (!IsCharacterTargetKey(planner->m_target))
  {
    planner->m_x34.Reset();
    planner->m_target.Reset();
    planner->m_selector.Add(owner->RankSectors(0, hunt.sectorMask, 0, 0, 0, 0, hunt.sectorStat, 1));
    sector = *planner->m_selector.PickBest(&scratch);
    while (IsSectorKey(sector))
    {
      systemRanking.Add(
        owner->RankSystemsOfSector(&sector, 0, hunt.characterSystemMask, 0, 0, 0, 0, hunt.systemStat, 1));
      planner->m_x34 = *systemRanking.PickBest(&scratch);
      while (IsSystemKey(planner->m_x34))
      {
        auto* node = NewOrNull<ObjectKeyNode>(planner->m_x34);
        if (node)
          candidates.Add(node);
        planner->m_x34 = *systemRanking.PickBest(&scratch);
      }
      systemRanking.Reset();
      systemRanking.Add(owner->RankSystemsOfSector(&sector, 0, hunt.shipSystemMask, 0, 0, 0, 0, hunt.systemStat, 2));
      planner->m_x34 = *systemRanking.PickBest(&scratch);
      while (IsSystemKey(planner->m_x34))
      {
        auto* node = NewOrNull<ObjectKeyNode>(planner->m_x34);
        if (node)
          candidates.Add(node);
        planner->m_x34 = *systemRanking.PickBest(&scratch);
      }
      systemRanking.Reset();
      sector = *planner->m_selector.PickBest(&scratch);
    }
    planner->m_selector.Reset();
    if (static_cast<uint16_t>(candidates.Count()) != 0)
    {
      bool chosen = false;
      for (auto position = candidates.Entries().begin(); position != candidates.Entries().end() && !chosen; ++position)
      {
        auto* node = static_cast<ObjectKeyNode*>((*position).get());
        AiSystemEval* system = owner->m_systems.FindEval(ai::KeyOfNode(node));
        chosen = false;
        if (system && (membersAtDestination == planner->GetMemberLimitOrSelectMembers() || system->Stat(0xbc) / 2 <= membersAtDestination))
        {
          planner->m_x34 = ai::KeyOfNode(node);
          chosen = true;
        }
      }
      if (IsSystemKey(planner->m_x34))
        planner->PickCandidate();
    }
  }
  int result = 0;
  if (IsCharacterTargetKey(planner->m_target))
  {
    result = 1;
    planner->m_flags.m_value |= 0x10;
  }
  return result;
}

} // namespace ai_planner
