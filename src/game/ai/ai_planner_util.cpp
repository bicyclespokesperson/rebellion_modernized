#include "ai/ai_planner_util.h"

using ai::IsSystemKey;
using ai::KeyInFamilies;
using ai::KeyOfNode;
using ai::NextNode;

namespace ai_planner {

namespace {

constexpr uint32_t kHasLeaders = 1;
constexpr uint32_t kTargetChosen = 0x10;
constexpr uint32_t kStalled = 0x20;

bool IsCharacterKey(const ObjectKey& key) { return KeyInFamilies(key, 0x30, 0x40) != 0; }

}  // namespace

int SumLeaderValue(AiMissionPlanner* planner, int valueIndex) {
    int sum = 0;
    if ((planner->m_flags.m_value & kStalled) == 0) {
        for (ObjectKeyNode* node = planner->m_leaders.First(); node; node = NextNode(node)) {
            const ObjectKey key = KeyOfNode(node);
            AiCharacterRecord* character = static_cast<AiCharacterRecord*>(planner->m_characters->Find(&key));
            if (character) sum += character->m_values[valueIndex];
        }
    }
    return sum;
}

void ChooseLeader(AiFreePlanner* planner, uint32_t capabilities, uint32_t valueIndex, int minValue, uint32_t forbidFlags,
                  uint32_t forbidCaps, int wanted, int secondValueIndex) {
    ObjectKey picked;
    ObjectKey scratch;
    planner->m_flags.m_value &= ~kHasLeaders;
    if (static_cast<uint16_t>(planner->m_leaders.Count()) < wanted) {
        planner->m_selector.Add(planner->m_characters->RankEligible(0, 2, capabilities, forbidFlags, forbidCaps, valueIndex, minValue, 10000, 1));
        if (secondValueIndex >= 0)
            planner->m_selector.Add(planner->m_characters->RankEligible(0, 2, capabilities, forbidFlags, forbidCaps, secondValueIndex, minValue, 10000, 1));
        picked = *planner->m_selector.PickBest(&scratch);
        while (IsCharacterKey(picked) && static_cast<uint16_t>(planner->m_leaders.Count()) < wanted) {
            if (planner->IsReachable(&picked) != 0) planner->AddLeader(&picked);
            picked = *planner->m_selector.PickBest(&scratch);
        }
        planner->m_selector.Reset();
    }
    if (static_cast<uint16_t>(planner->m_leaders.Count()) >= wanted) planner->m_flags.m_value |= kHasLeaders;
}

void ChooseSystemLeaders(AiMissionPlanner* planner, const SystemLeaders& spec) {
    ObjectKey picked;
    ObjectKey scratch;
    planner->m_flags.m_value &= ~kHasLeaders;
    if (static_cast<uint16_t>(planner->m_leaders.Count()) < spec.wanted) {
        planner->m_selector.Add(planner->m_characters->RankByValue(spec.requireFlags, spec.capabilities, spec.forbidFlags, 0,
                                                                   spec.firstIndex, spec.firstMin, 10000, 1));
        if (spec.secondIndex >= 0)
            planner->m_selector.Add(planner->m_characters->RankByValue(spec.requireFlags, spec.capabilities, spec.forbidFlags, 0,
                                                                       spec.secondIndex, spec.firstMin, 10000, 1));
        if (IsSystemKey(planner->m_destination))
            planner->m_selector.Add(planner->m_characters->RankByDistance(&planner->m_destination, spec.requireFlags,
                                                                          spec.capabilities, spec.forbidFlags, 0, 2));
        picked = *planner->m_selector.PickBest(&scratch);
        while (IsCharacterKey(picked) && static_cast<uint16_t>(planner->m_leaders.Count()) < spec.wanted) {
            if (planner->IsReachable(&picked) != 0) planner->AddLeader(&picked);
            picked = *planner->m_selector.PickBest(&scratch);
        }
        planner->m_selector.Reset();
    }
    if (static_cast<uint16_t>(planner->m_leaders.Count()) >= spec.wanted) planner->m_flags.m_value |= kHasLeaders;
}

int MembersWantedAtTarget(AiMissionPlanner* planner) {
    AiSystemEval* system = planner->m_owner->m_systems.FindEval(planner->m_x34);
    return system ? (system->Stat(0xbc) + 2) / 2 : 0;
}

void ChooseMembers(AiMissionPlanner* planner, int wanted, uint32_t capabilities) {
    ObjectKey picked;
    ObjectKey scratch;
    planner->m_flags.m_value &= ~2u;
    if (static_cast<uint16_t>(planner->m_members.Count()) < wanted) {
        planner->m_selector.Add(planner->m_characters->RankByValue(0x10800002, capabilities, kForbidUnusable, 0, 1, 0x33, 10000, 1));
        if (IsSystemKey(planner->m_destination))
            planner->m_selector.Add(planner->m_characters->RankByDistance(&planner->m_destination, 0x10800002, capabilities, kForbidUnusable, 0, 2));
        picked = *planner->m_selector.PickBest(&scratch);
        while (IsCharacterKey(picked) && static_cast<uint16_t>(planner->m_members.Count()) < wanted) {
            if (planner->IsReachable(&picked) != 0) planner->AddMember(&picked);
            picked = *planner->m_selector.PickBest(&scratch);
        }
        planner->m_selector.Reset();
    }
    if (static_cast<uint16_t>(planner->m_members.Count()) >= wanted) {
        planner->m_flags.m_value |= 2;
        if (static_cast<uint16_t>(planner->m_members.Count()) > wanted) planner->RemoveFirstMember();
    }
}

void RankTargetSystem(AiFreePlanner* planner, uint32_t sectorMask, uint32_t systemMask, uint32_t sectorStat) {
    ObjectKey scratch;
    AiSideController* owner = planner->m_owner;
    AiRankedSelector& selector = planner->m_selector;
    if (IsSystemKey(planner->m_destination)) {
        selector.Add(owner->RankSectorsByDistance(&planner->m_destination, 0, sectorMask, 0, 0, 0, 0, 2));
        planner->m_x34 = *selector.PickBest(&scratch);
        selector.Reset();
        selector.Add(owner->RankSystemsOfSectorByDistance(&planner->m_x34, &planner->m_destination, 0, systemMask, 0, 0, 0, 0, 2));
        planner->m_x34 = *selector.PickBest(&scratch);
    } else {
        selector.Add(owner->RankSectors(0, sectorMask, 0, 0, 0, 0, sectorStat, 1));
        planner->m_x34 = *selector.PickBest(&scratch);
        selector.Reset();
        selector.Add(owner->RankSystemsOfSector(&planner->m_x34, 0, systemMask, 0, 0, 0, 0, 0, 1));
        planner->m_x34 = *selector.PickBest(&scratch);
    }
    selector.Reset();
    if (IsSystemKey(planner->m_x34)) {
        planner->m_target = planner->m_x34;
        planner->m_flags.m_value |= kTargetChosen;
    }
}

int ChooseTarget(AiFreePlanner* planner, uint32_t sectorMask, uint32_t systemMask, uint32_t sectorStat) {
    ObjectKey scratch;
    AiSideController* owner = planner->m_owner;
    AiRankedSelector& selector = planner->m_selector;
    planner->m_target.Reset();
    planner->m_flags.m_value &= ~kTargetChosen;
    if ((planner->m_flags.m_value & kHasLeaders) == 0) {
        planner->m_x34.Reset();
    } else {
        if (IsSystemKey(planner->m_x34)) {
            AiSystemEval* system = owner->m_systems.FindEval(planner->m_x34);
            if (system && (system->m_flags2c.m_value & systemMask) != 0) {
                planner->m_target = planner->m_x34;
                planner->m_flags.m_value |= kTargetChosen;
            }
        }
        if ((planner->m_flags.m_value & kTargetChosen) != 0) return 1;
        RankTargetSystem(planner, sectorMask, systemMask, sectorStat);
    }
    return (planner->m_flags.m_value & kTargetChosen) != 0;
}

int NoteTargetSystem(AiFreePlanner* planner, uint32_t systemMask) {
    int result = 0;
    AiSystemEval* system = planner->m_owner->FindSystemEval(&planner->m_x34);
    if (system) {
        result = (system->m_flags2c.m_value & systemMask) != 0;
        if ((system->m_flags30.m_value & 1) != 0 && (system->m_flags30.m_value & 0x200) == 0) planner->m_destination = planner->m_x34;
    }
    return result;
}

}  // namespace ai_planner
