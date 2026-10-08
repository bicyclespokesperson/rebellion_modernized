#include "ai/ai_planner_util.h"
#include "ai/ai_planners.h"
#include "core/vptr_store.h"

using namespace ai_planner;
using ai::IsSystemKey;

AiIncitePlanner::AiIncitePlanner(int view, AiSideController* owner, AiCharacterRecordSet* characters)
    : AiFreePlanner(view, owner, characters) {
    m_missionKey = ObjectKey::Make(0x56, 0x40);
}

AiIncitePlanner::~AiIncitePlanner() { REB_KEEP_VPTR_STORE(); }

int AiIncitePlanner::Virtual06() { return SumLeaderValue(this, 6); }

int AiIncitePlanner::Virtual13() { return NoteTargetSystem(this, 0x100); }

void AiIncitePlanner::Prepare() { ChooseLeader(this, 0x100, 6, 0x31, 0x4f400051); }

void AiIncitePlanner::Virtual19() {
    int wanted = 0;
    AiSystemEval* system = m_owner->m_systems.FindEval(m_x34);
    if (system) wanted = (system->Stat(0xbc) + 2) / 2;
    ChooseMembers(this, wanted, 0x100);
}

int AiIncitePlanner::ChooseTarget() {
    ObjectKey sector;
    ObjectKey scratch;
    m_target.Reset();
    m_flags.m_value &= ~0x10u;
    if ((m_flags.m_value & 1) == 0) {
        m_x34.Reset();
    } else {
        if (IsSystemKey(m_x34)) {
            AiSystemEval* system = m_owner->m_systems.FindEval(m_x34);
            if (system && (system->m_flags2c.m_value & 0x100) != 0 && (system->m_flags30.m_value & 0x200) == 0) {
                m_target = m_x34;
                m_flags.m_value |= 0x10;
            }
        }
        if ((m_flags.m_value & 0x10) != 0) return 1;
        m_x34.Reset();
        if (IsSystemKey(m_destination)) {
            m_selector.Add(m_owner->RankSectorsByDistance(&m_destination, 0, 0x200, 8, 0, 0, 0, 2));
            sector = *m_selector.PickBest(&scratch);
            m_selector.Reset();
            m_selector.Add(m_owner->RankSystemsOfSectorByDistance(&sector, &m_destination, 0, 0x100, 4, 0, 0, 0, 2));
            m_x34 = *m_selector.PickBest(&scratch);
            m_selector.Reset();
        }
        if (!IsSystemKey(m_x34)) {
            m_selector.Add(m_owner->RankSectors(0, 0x200, 8, 0, 0, 0, 4, 1));
            m_selector.Add(m_owner->RankSectorsRandomly(0, 0x200, 8, 0, 0, 0, 1));
            sector = *m_selector.PickBest(&scratch);
            m_selector.Reset();
            m_selector.Add(m_owner->RankSystemsOfSector(&sector, 0, 0x100, 4, 0, 0, 0, 0, 1));
            m_x34 = *m_selector.PickBest(&scratch);
            m_selector.Reset();
        }
        if (IsSystemKey(m_x34)) {
            m_target = m_x34;
            m_flags.m_value |= 0x10;
        }
    }
    return (m_flags.m_value & 0x10) != 0;
}
