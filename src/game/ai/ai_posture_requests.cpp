#include "ai/ai_posture_util.h"

using namespace ai_posture;

AiSystemEval* AiSideController::FindSystemEval(const ObjectKey* key) { return m_systems.FindEval(*key); }

void AiPosture::DefaultPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) {
    *createKind = 0;
    *createSubtype = 0;
    *releaseKind = 0;
    *releaseSubtype = 0;
    AiSectorEval* sector = FindSector(this);
    if (!sector) return;
    const uint32_t sectorFlags = sector->m_flags2c.m_value;
    const int* planners = m_record->m_plannerCount;
    if (*createKind == 0 && planners[2] < 2 && (sectorFlags & 0x18) != 0) {
        *createKind = 0x54;
    } else if (*releaseKind == 0 && (planners[2] > 2 || ((sectorFlags & 0x18) == 0 && planners[2] > 0))) {
        *releaseKind = 0x54;
    }
    if (*createKind == 0 && planners[1] == 0) {
        *createKind = 0x52;
    } else if (*releaseKind == 0 && planners[1] > 1) {
        *releaseKind = 0x52;
    }
    if (*createKind == 0 && planners[3] == 0 && (sectorFlags & 8) != 0) {
        *createKind = 0x69;
    } else if (*releaseKind == 0 && (planners[3] > 1 || ((sectorFlags & 8) == 0 && planners[3] > 0))) {
        *releaseKind = 0x69;
    }
    if (*createKind == 0 && planners[0] == 0 && (sectorFlags & 0x40008) != 0) {
        *createSubtype = 4;
        *createKind = (m_view != 1) + 0x62;
        return;
    }
    if (*releaseKind == 0 && (planners[0] > 1 || ((sectorFlags & 0x40008) == 0 && planners[0] > 0))) {
        *releaseSubtype = 4;
        *releaseKind = (m_view != 1) + 0x62;
    }
}

void AiPosture::AgentRequestA(int* agentKind) {
    *agentKind = 0;
    AiSectorEval* sector = FindSector(this);
    if (!sector || *agentKind != 0) return;
    if (m_record->m_agentCount[0] < 1 && static_cast<uint16_t>(m_record->m_fleets.Count()) != 0) *agentKind = 3;
    if (*agentKind == 0 && m_record->m_agentCount[2] < 1 && 0 < m_record->m_x98 && (sector->m_flags2c.m_value & 0x40008) != 0)
        *agentKind = 5;
}

void AiPosture::AgentRequestB(int* agentKind) {
    *agentKind = 0;
    AiSectorEval* sector = FindSector(this);
    if (!sector) return;
    if (m_record->m_agentCount[1] < 1 && 0 < m_record->m_x94 && (sector->m_flags2c.m_value & 8) != 0) *agentKind = 4;
    if (*agentKind != 0) return;
    if (m_record->m_agentCount[0] < 1 && static_cast<uint16_t>(m_record->m_fleets.Count()) != 0) *agentKind = 3;
    if (*agentKind == 0 && m_record->m_agentCount[2] < 1 && 0 < m_record->m_x98 && (sector->m_flags2c.m_value & 0x40008) != 0)
        *agentKind = 5;
}

void AiPosture::AgentRequestC(int* agentKind) {
    *agentKind = 0;
    AiSectorEval* sector = FindSector(this);
    if (!sector) return;
    if (m_record->m_agentCount[0] < 1 && static_cast<uint16_t>(m_record->m_fleets.Count()) != 0) *agentKind = 3;
    if (*agentKind == 0 && m_record->m_agentCount[2] < 1 && 0 < m_record->m_x98 && (sector->m_flags2c.m_value & 0x40008) != 0)
        *agentKind = 5;
}

int AiPosture::HasSeveralAgentsOfClass1() { return FindSector(this) && 1 < m_record->m_agentCount[1] ? 1 : 0; }
