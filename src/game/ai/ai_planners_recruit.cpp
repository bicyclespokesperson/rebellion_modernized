#include "ai/ai_planner_util.h"
#include "ai/ai_planners.h"
#include "core/vptr_store.h"
#include "gameobj/object_lookup.h"

using namespace ai_planner;
using ai::IsSystemKey;
using ai::KeyOfNode;
using ai::KeyOfObject;

AiRecruitPlanner::AiRecruitPlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiFreePlanner(view, owner, characters) {
    m_missionKey = ObjectKey::Make(0x55, 0x16);
}

AiRecruitPlanner::~AiRecruitPlanner() { REB_KEEP_VPTR_STORE(); }

int AiRecruitPlanner::Virtual06() { return SumLeaderValue(this, 6); }

void AiRecruitPlanner::Virtual12(int event) {
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
        if (system && (system->m_flags30.m_value & 1) != 0 && (system->m_flags30.m_value & 0x200) == 0) m_destination = m_x34;
        m_stall = 0;
        ResetMissionMark();
    }
}

void AiRecruitPlanner::Prepare() { ChooseLeader(this, 0x400, 6, 0x50); }

int AiRecruitPlanner::ChooseTarget() {
    m_target.Reset();
    m_flags.m_value &= ~0x10u;
    if ((m_flags.m_value & 1) == 0) {
        m_x34.Reset();
    } else {
        if (IsSystemKey(m_x34)) {
            AiSystemEval* system = m_owner->m_systems.FindEval(m_x34);
            if (system && (system->m_flags2c.m_value & 2) != 0) m_flags.m_value |= 0x10;
        }
        if ((m_flags.m_value & 0x10) == 0) {
            ObjectKeyNode* leader = m_leaders.First();
            if (leader) {
                const ObjectKey leaderKey = KeyOfNode(leader);
                AiCharacterRecord* character = static_cast<AiCharacterRecord*>(m_owner->m_characters.Find(&leaderKey));
                if (character) {
                    ObjectKey cursor = character->m_location;
                    bool done = false;
                    while (!IsSystemKey(cursor) && !done) {
                        GameObjNotifier* object = GetObjectInFamilies_01_ff(m_view, &cursor);
                        if (object) {
                            if (object->m_parent == nullptr) {
                                cursor.Reset();
                                done = true;
                            } else {
                                cursor = KeyOfObject(object->m_parent);
                            }
                        }
                    }
                    if (IsSystemKey(cursor)) {
                        AiSystemEval* system = m_owner->m_systems.FindEval(cursor);
                        if (system && (system->m_flags2c.m_value & 2) != 0) {
                            m_x34 = cursor;
                            m_flags.m_value |= 0x10;
                        }
                    }
                }
            }
            if ((m_flags.m_value & 0x10) == 0) RankTargetSystem(this, 8, 2, 0x3c);
        }
    }
    if ((m_flags.m_value & 0x10) == 0) return 0;
    m_target = m_x34;
    return 1;
}
