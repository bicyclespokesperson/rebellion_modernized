#include "data/gnprtb.h"
#include "ai/ai_common.h"
#include "mission/mission_encounter.h"
#include "mission/mission_exec_decl.h"
#include "mission/mission_rules.h"

using namespace mission_exec;

// Bookkeeping the mission manager does when a mission changes phase (0x547f60-0x548840).

namespace {

constexpr uint32_t kRecordKeyStoryMission = 0x71000043;
constexpr int kPhaseOutcome = 8;
constexpr int kPhaseResolved = 0xb;
constexpr int kPhaseWindowFirst = 8;
constexpr int kPhaseWindowLast = 10;
constexpr int kLevelThreshold = 8;
constexpr uint32_t kFamilyBlockadeMissionA = 0x56;
constexpr uint32_t kFamilyBlockadeMissionB = 0x57;

bool inPhaseWindow(int phase) { return phase >= kPhaseWindowFirst && phase <= kPhaseWindowLast; }

}  // namespace

int MissionMgr::ReleaseObjectiveSystem(Mission* mission, NotifyContext* ctx) {
    int ok = 1;
    if (mission->HasCompletionStatus()) return ok;

    GameObj* objective = nullptr;
    const int found = mission->IsDataField1cAbove4() ? mission->GetTargetLocationSystem(&objective)
                                                     : mission->GetOriginSystem(&objective);
    ok = found != 0;
    if (!objective) return ok;
    if (!InFamilies(objective->GetFamily(), kFamilySystemBegin, kFamilySystemEnd)) return ok;

    EncounterRunner check;
    check.m_context.m_location = objective;
    check.m_context.m_mission = mission;
    ok = check.Run(ctx) && ok;
    if (mission->HasCompletionStatus()) {
        int otherSide = -1;
        const uint32_t side = mission->ControlSide();
        if (side == 1) {
            otherSide = 2;
        } else if (side == 2) {
            otherSide = 1;
        }
        ok = otherSide != -1 && ok;
        if (otherSide != -1) {
            NotifyContext viewContext(*ctx);
            viewContext.m_flag04 = 1;
            viewContext.m_flag08 = 3;
            mission->ApplyAllToView(otherSide, &viewContext);
        }
    }
    return ok;
}

int MissionMgr::DetachMissionMembers(Mission* mission, NotifyContext* ctx) {
    int ok = 1;
    {
        MissionPersonIterator members(mission);
        members.Begin();
        while (members.Current()) {
            Role* person = static_cast<Role*>(members.Current());
            members.Advance();
            int resigning = 0;
            int stays = 0;
            int unusedC = 0;
            ok = mission->GetMemberTransitFlags(person, &resigning, &stays, &unusedC) && ok;
            if (resigning == 0 || stays != 0 || (person->m_flags78 & role_flag::kMissionRemoveRequest)) continue;

            ObjectKey personKey;
            const bool inTeamOrDecoys = mission->IsInTeam(person->GetKey(&personKey)) != 0 ||
                                        mission->IsInDecoys(person->GetKey(&personKey)) != 0;
            ok = mission->RemoveMember(person->GetKey(&personKey), 0, ctx) && ok;
            if (inTeamOrDecoys && mission->HasCompletionStatus()) {
                ObjectKeyValueSet set4(mission->m_finishedMembers);
                set4.AddKeyValue(person->GetKey(&personKey), 0);
                mission->SetFinishedMembers(&set4, ctx);
            }
        }
    }
    int moved = 0;
    do {
        ok = ReassignMissionMembers(mission, &moved, ctx) && ok;
    } while (moved != 0);
    return ok;
}

int MissionMgr::ApplyMissionOutcomeModifiers(Mission* mission, NotifyContext* ctx) {
    Character* story = static_cast<Character*>(GetMaster_33000243());
    int ok = story != nullptr;
    int linked = 0;
    if (story) {
        ObjectKey missionKey;
        if (story->m_missionKey.m_value == mission->GetKey(&missionKey)->m_value &&
            !(story->m_flagsAc & kCharacterCaptured))
            linked = 1;
    }
    int hasForces;
    {
        MissionSpecialForcesIterator forces(mission);
        hasForces = forces.HasAny();
    }
    int modifier = g_gnprtbDefault;
    int result = GetMissionHyperdriveModifier(linked, hasForces, &modifier) && ok;
    MissionCharacterIterator persons(mission);
    for (persons.Begin(); persons.Current(); persons.Advance())
        result = static_cast<Character*>(persons.Current())->SetMissionHyperdriveModifier(modifier, ctx);
    return result;
}

int MissionMgr::UpdateSystemMissionScores(Mission* mission, int fromPhase, int toPhase, NotifyContext* ctx) {
    int ok = 1;
    const uint32_t family = mission->GetFamily();
    if (family != kFamilyBlockadeMissionA && family != kFamilyBlockadeMissionB) return ok;
    if (inPhaseWindow(fromPhase) == inPhaseWindow(toPhase)) return ok;

    GameObj* leader = nullptr;
    mission->GetTargetLocationSystem(&leader);
    ok = leader != nullptr;
    if (!leader || !InFamilies(leader->GetFamily(), kFamilySystemBegin, kFamilySystemEnd)) return ok;

    int firstScore = 0;
    int secondScore = 0;
    int bonus = 0;
    bool found = false;
    MissionInciteIterator firstGroup(this);
    for (firstGroup.Begin(); firstGroup.Current(); firstGroup.Advance()) {
        Mission* other = static_cast<Mission*>(firstGroup.Current());
        ObjectKey leaderKey;
        if (other->IsPhase8() && leader->GetKey(&leaderKey)->m_value == other->m_targetLocationKey.m_value) {
            found = true;
            firstScore += DivideByMissionDivisor(other->AverageMemberSkill());
        }
    }
    MissionSubdueIterator secondGroup(this);
    for (secondGroup.Begin(); secondGroup.Current(); secondGroup.Advance()) {
        Mission* other = static_cast<Mission*>(secondGroup.Current());
        ObjectKey leaderKey;
        if (other->IsPhase8() && leader->GetKey(&leaderKey)->m_value == other->m_targetLocationKey.m_value)
            secondScore += NegatedDivideByMissionDivisor(other->AverageMemberSkill());
    }
    if (found) bonus = GetMissionRuleValue1801();
    System* system = static_cast<System*>(leader);
    ok = system->SetData74(firstScore) && ok;
    const int second = system->SetData78(secondScore) && ok;
    ok = system->SetData7c(bonus) && second;
    return ok;
}

int MissionMgr::OnMissionPhaseStory(Mission* mission, int fromPhase, int toPhase, NotifyContext* ctx) {
    int ok = 1;
    ObjectKey recordKey;
    GetObjectRecordKey(mission, &recordKey);
    if (recordKey.m_value == kRecordKeyStoryMission) {
        if (toPhase == kPhaseResolved) {
            if (mission->GetDataField1c() >= kLevelThreshold) ok = SetLukeDagobahRequired(0, ctx) != 0;
        } else if (toPhase == kPhaseOutcome) {
            ok = RunStoryPipeline(ctx) != 0;
        }
    }
    return ok;
}
