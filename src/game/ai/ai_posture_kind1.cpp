#include "ai/ai_posture_util.h"
#include "ai/ai_postures.h"

using namespace ai_posture;

AiPostureKind1::AiPostureKind1(int view, AiSideController* owner, AiSystemRecord* record) : AiPosture(view, owner, record) {}

AiPostureKind1::~AiPostureKind1() {  }

void AiPostureKind1::GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) {
    DefaultPlannerRequest(createKind, createSubtype, releaseKind, releaseSubtype);
}

void AiPostureKind1::GetAgentRequest(int* agentKind) { AgentRequestC(agentKind); }

void AiPostureKind1::Virtual08(PersistentInt* flagsA, PersistentInt*) {
    flagsA->m_value |= 0x4000000;
    AiSectorEval* sector = FindSector(this);
    if (sector) {
        const int flags = flagsA->m_value;
        flagsA->m_value = flags | 0x8000000;
        if ((sector->m_flags2c.m_value & 0x80000) != 0) flagsA->m_value = flags | 0x8020000;
    }
}

void AiPostureKind1::Virtual09(ObjectKey* a, ObjectKey* b) {
    static const SystemQuery queries[] = {
        ByStat(0, 0x400, 0, 0, 0, 0, 0x1d, 2, 0xc00, 4),
        ByStat(0, 0x800, 0, 0, 0, 0, 0x1d, 2, 0x800, 4),
    };
    SelectCharacterSystem(this, a, b, queries);
}

void AiPostureKind1::Virtual10(ObjectKey* a, ObjectKey* b) { SelectSystemByAgeOnce(this, a, b); }

void AiPostureKind1::Virtual11(ObjectKey* a, ObjectKey* b) { SelectSystemByAge(this, a, b); }

void AiPostureKind1::Virtual12(ObjectKey* a, ObjectKey* b) {
    static const SystemQuery queries[] = {
        ByStat(0, 0x2000000, 0x80000000, 0, 0, 0, 0x1d, 2, 0x2000000),
        ByStat(0, 0x20000, 0, 0, 0, 0, 0x1d, 2, 0x20000),
        ByStat(0, 0x10000, 0, 0, 0, 0, 0x1d, 2, 0x10000),
        ByStat(0, 0x200000, 0, 0, 0, 0, 0x1d, 2, 0x200000),
        ByStat(0, 0x800000, 0x200, 0, 0, 0, 0x1d, 2, 0x800000),
        ByStat(0, 0x400000, 0, 0, 0, 0, 0x1d, 2, 0x400000),
    };
    SelectObjectSystem(this, a, b, queries);
}

int AiPostureKind1::Virtual15() {
    int assigned = 0;
    AiFleetEval* fleet = FindTargetFleet(this);
    if (!fleet) return assigned;
    auto accepts = [fleet](AiSystemEval* system) { return AiPosture::CanTakeSystem(fleet, system) != 0; };
    if (0 < fleet->Stat(0x58)) {
        m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0, 0, 0x200, 0, 0, 0x100000, 0x27, 1));
        assigned = AssignFleet(this, fleet, 0x100000, 0x10000000, accepts);
    }
    if (assigned == 0 && 0 < fleet->Stat(0x5c)) {
        m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0, 0x400000, 4, 0, 0, 0x100000, 0x1b, 1));
        assigned = AssignFleet(this, fleet, 0x100000, 0x10000000, accepts);
    }
    return assigned;
}

int AiPostureKind1::Virtual17() {
    AiFleetEval* fleet = FindTargetFleet(this);
    if (!fleet || fleet->Stat(0x68) < 1) return 0;
    const ObjectKey* sector = &m_record->m_sectorKey;
    m_selector.Add(m_owner->RankSystemsOfSector(sector, 0, 0, 4, 0, 0, 0x700200, 8, 2));
    m_selector.Add(m_owner->RankSystemsOfSector(sector, 0, 0, 4, 0, 0, 0x700200, 0x21, 1));
    m_selector.Add(m_owner->RankSystemsOfSector(sector, 0, 0, 4, 0, 0, 0x700200, 0, 1));
    return AssignFleet(this, fleet, 0x400000, 0x40000000, [this, fleet](AiSystemEval* system) {
        int orderKind;
        return CanOrderFleet(fleet, system, &orderKind) != 0;
    });
}

void AiPostureKind1::Virtual20() {
    AiSectorEval* sector = FindSector(this);
    if (!sector) return;
    AiClassTable& classes = m_owner->m_classes;
    int budget = (sector->Stat(0x190) * 12) / 10;
    if (m_budget[0] < budget) m_budget[0] = budget;
    budget = (sector->Stat(0x194) * 12) / 10;
    if (budget < classes.m_x30) budget = classes.m_x30;
    if (m_budget[1] < budget) m_budget[1] = budget;
    budget = ((sector->Stat(0x138) + sector->Stat(0x198)) * 12) / 10;
    if (m_budget[3] < budget) m_budget[3] = budget;
    m_budget[2] = 0;
    budget = (sector->Stat(0x144) * 11) / 10;
    if (budget < classes.m_x3c) budget = classes.m_x3c;
    if (m_budget[5] < budget) m_budget[5] = budget;
    m_budget[4] = 0;
    if ((m_owner->m_flags.Get() & 0x1000000) != 0) m_budget[6] = 1;
    m_budget[7] = 0;
    m_budget[8] = m_record->m_x98;
    m_budget[9] = 1;
}

ObjectKey* AiPostureKind1::Virtual21(ObjectKey* out) {
    ObjectKey chosen;
    ObjectKey scratch;
    AiFleetEval* fleet = FindTargetFleet(this);
    if (fleet) {
        if (static_cast<int16_t>(m_record->m_fleets.Count()) == 1) {
            fleet = FindTargetFleet(this);
            if (fleet && (fleet->m_flags.m_value & 0xa00) == 0) {
                m_selector.Add(m_owner->RankSystemsOfSectorByAge(&m_record->m_sectorKey, 0, 0, 0, 0, 0, 0x700201, 1));
                chosen = *m_selector.PickBest(&scratch);
                m_selector.Reset();
            }
        }
        ChooseFleetDestination(this, fleet, &chosen);
    }
    *out = chosen;
    return out;
}
