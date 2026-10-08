#pragma once
#include <cstdint>

#include "ai/ai_character_records.h"
#include "ai/ai_order_util.h"
#include "ai/ai_planner.h"
#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"

// Building blocks the concrete planners (ai_planners.h) share. Most of them are the same state machine steps with different
// constants, so each step is written once and the planner classes call it with their numbers.

namespace ai_planner {

// The planners' common part of a character query: the characters (flags 2) that are free for a planner, see AiMissionPlanner.
constexpr uint32_t kForbidUnusable = 0x7400051;

// The sum of the value `valueIndex` of the leaders' records (0 once the planner is stalled).
int SumLeaderValue(AiMissionPlanner* planner, int valueIndex);

// A leader for a planner that has none: the most eligible reachable character with the capability `capabilities` whose value
// `valueIndex` is at least `minValue`; sets flag 1 when the planner has a leader afterwards.
void ChooseLeader(AiFreePlanner* planner, uint32_t capabilities, uint32_t valueIndex, int minValue,
                  uint32_t forbidFlags = kForbidUnusable, uint32_t forbidCaps = 0, int wanted = 1, int secondValueIndex = -1);

// The leaders of a system planner: up to `wanted` characters with the capabilities `capabilities`, taken from the best of one or
// two rankings by value (`firstIndex`, `secondIndex` or -1) and from those nearest to the destination; sets flag 1 once there are
// enough.
struct SystemLeaders {
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

}  // namespace ai_planner
