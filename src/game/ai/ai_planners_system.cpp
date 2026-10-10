#include "ai/ai_planner_util.h"
#include "ai/ai_planners.h"
#include "core/vptr_store.h"

using namespace ai_planner;

namespace {

constexpr uint32_t kForbidUnusableAndDead = 0x4f400051;
constexpr uint32_t kForbidUnusableAndKilled = 0x47400051;

}  // namespace

// ---- 0x69 Sabotage -----------------------------------------------------------------------------------------------------

AiSabotagePlanner::AiSabotagePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiSystemPlanner(view, owner, characters) {
    m_missionKey = ObjectKey::Make(0x69, 0x12);
}

AiSabotagePlanner::~AiSabotagePlanner() { REB_KEEP_VPTR_STORE(); }

int AiSabotagePlanner::Virtual06() {
    int sum = 0;
    if ((m_flags.m_value & 0x20) == 0) {
        for (ObjectKeyNode* node = m_leaders.First(); node; node = ai::NextNode(node)) {
            const ObjectKey key = ai::KeyOfNode(node);
            AiCharacterRecord* character = static_cast<AiCharacterRecord*>(m_characters->Find(&key));
            if (character) sum += (character->m_values[1] + character->m_values[5]) / 2;
        }
    }
    return sum;
}

void AiSabotagePlanner::PickLeaders() {
    ChooseSystemLeaders(this, {0x800002, 4, kForbidUnusableAndDead, 5, 0x33, 1, 2});
}

void AiSabotagePlanner::PickMembers() {
    ChooseMembers(this, MembersWantedAtTarget(this), 4);
}

// ---- 0x54 (system planner) ---------------------------------------------------------------------------------------------------

AiMission54Planner::AiMission54Planner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiSystemPlanner(view, owner, characters) {
    m_missionKey = ObjectKey::Make(0x54, 0x15);
}

AiMission54Planner::~AiMission54Planner() { REB_KEEP_VPTR_STORE(); }

int AiMission54Planner::Virtual06() { return SumLeaderValue(this, 1); }

void AiMission54Planner::Virtual12(int event) {
    if (event == 0) {
        m_flags.m_value |= 0x80000000;
        return;
    }
    if (event == 2) {
        ResetMissionMark();
        return;
    }
    if (event == 3) {
        AiSystemEval* system = m_owner->FindSystemEval(&m_x34);
        if (system) system->NoteTouched();
        m_stall = 0;
        ResetMissionMark();
    }
}

void AiMission54Planner::PickLeaders() {
    ChooseSystemLeaders(this, {0x800002, 0x200, kForbidUnusable, 1, 0, -1, 1});
}

// ---- 0x52 Espionage (system planner) ------------------------------------------------------------------------------------------

AiEspionagePlanner::AiEspionagePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiSystemPlanner(view, owner, characters) {
    m_missionKey = ObjectKey::Make(0x52, 0x13);
}

AiEspionagePlanner::~AiEspionagePlanner() { REB_KEEP_VPTR_STORE(); }

int AiEspionagePlanner::Virtual06() { return SumLeaderValue(this, 1); }

void AiEspionagePlanner::Virtual12(int event) {
    if (event == 0) {
        m_flags.m_value |= 0x80000000;
        return;
    }
    if (event == 2) {
        AdvanceStall();
        ResetMissionMark();
        return;
    }
    if (event == 3) {
        AiSystemEval* system = m_owner->FindSystemEval(&m_x34);
        if (system) system->NoteTouched();
        m_stall = 0;
        ResetMissionMark();
    }
}

void AiEspionagePlanner::PickLeaders() {
    ChooseSystemLeaders(this, {0x10800002, 8, kForbidUnusable, 1, 0x33, -1, 2});
}

void AiEspionagePlanner::PickMembers() {
    ChooseMembers(this, MembersWantedAtTarget(this), 8);
}

// ---- 0x62 Abduction / 0x63 Assassination (system planner) -----------------------------------------------------------------------

AiAbductionPlanner::AiAbductionPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiSystemPlanner(view, owner, characters) {
    if (view == 2) {
        m_missionKey = ObjectKey::Make(0x63, 0x81);
        m_capability = 0x40;
    } else {
        m_missionKey = ObjectKey::Make(0x62, 0x17);
        m_capability = 0x10;
    }
}

AiAbductionPlanner::~AiAbductionPlanner() { REB_KEEP_VPTR_STORE(); }

void AiAbductionPlanner::Read(istream* in) {
    AiSystemPlanner::Read(in);
    ReadValue32(in, reinterpret_cast<int*>(&m_capability));
}

void AiAbductionPlanner::Write(ostream* out) {
    AiSystemPlanner::Write(out);
    WriteValue32(out, reinterpret_cast<int*>(&m_capability));
}

int AiAbductionPlanner::Virtual06() { return SumLeaderValue(this, 5); }

void AiAbductionPlanner::PickLeaders() {
    ChooseSystemLeaders(this, {0x10800002, m_capability, kForbidUnusableAndKilled, 5, 0x33, -1, 2});
}

void AiAbductionPlanner::PickMembers() {
    ChooseMembers(this, MembersWantedAtTarget(this), m_capability);
}
