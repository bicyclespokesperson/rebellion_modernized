#include "ai/ai_posture_util.h"
#include "ai/ai_postures.h"

using namespace ai_posture;

AiPostureKind3::AiPostureKind3(int view, AiSideController* owner, AiSystemRecord* record) : AiPosture(view, owner, record) {}

AiPostureKind3::~AiPostureKind3() = default;

void AiPostureKind3::GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) {
    DefaultPlannerRequest(createKind, createSubtype, releaseKind, releaseSubtype);
}

void AiPostureKind3::GetAgentRequest(int* agentKind) { AgentRequestA(agentKind); }

void AiPostureKind3::Virtual08(PersistentInt* flagsA, PersistentInt*) {
    flagsA->m_value |= 0x4000000;
    AiSectorEval* sector = FindSector(this);
    if (sector) {
        const int flags = flagsA->m_value;
        flagsA->m_value = flags | 0x8000000;
        if ((sector->m_flags2c.m_value & 4) != 0) flagsA->m_value = flags | 0x8020000;
    }
}

void AiPostureKind3::Virtual09(ObjectKey* a, ObjectKey* b) {
    static const SystemQuery queries[] = {
        Randomly(0, 0x400, 0x1000, 0, 0, 0, 1, 0xc00, 4),
        ByStat(0, 0x800, 0x1000, 0, 0, 0, 0x1d, 2, 0x800, 0x7000000),
        ByStat(0, 0x400, 0, 0, 0, 0, 0x1d, 2, 0xc00, 4),
        ByStat(0, 0x800, 0, 0, 0, 0, 0x1d, 2, 0x800, 4),
    };
    SelectCharacterSystem(this, a, b, queries);
}

void AiPostureKind3::Virtual10(ObjectKey* a, ObjectKey* b) { SelectSystemByAgeOrChance(this, a, b); }

void AiPostureKind3::Virtual11(ObjectKey* a, ObjectKey* b) { SelectSystemByAge(this, a, b); }

void AiPostureKind3::Virtual12(ObjectKey* a, ObjectKey* b) {
    static const SystemQuery queries[] = {
        ByStat(0, 0x40000, 0x100, 0, 0, 0, 0x1d, 2, 0x40000),
        ByStat(0, 0x80000, 0, 0, 0, 0, 0x1d, 2, 0x80000),
        ByStat(0, 0x100000, 0, 0, 0, 0, 0x1d, 2, 0x100000),
        ByStat(0, 0x40000, 0, 0, 0, 0, 0x1d, 2, 0x40000),
        ByStat(0, 0x200000, 0, 0, 0, 0, 0x1d, 2, 0x200000),
        ByStat(0, 0x20000, 0, 0, 0, 0, 0x1d, 2, 0x20000),
        ByStat(0, 0x400000, 0, 0, 0, 0, 0x1d, 2, 0x400000),
        ByStat(0, 0x800000, 0x200, 0, 0, 0, 0x1d, 2, 0x800000),
    };
    SelectObjectSystem(this, a, b, queries);
}

int AiPostureKind3::Virtual15() { return AssignTakeFleet(this); }

int AiPostureKind3::Virtual17() { return AssignOrderFleet(this, 4, 0x10); }

void AiPostureKind3::Virtual20() {
    AiSectorEval* sector = FindSector(this);
    if (!sector) return;
    AiClassTable& classes = m_owner->m_classes;
    int budget = (sector->Stat(0x190) * 12) / 10;
    if (budget < classes.m_x2c) budget = classes.m_x2c;
    if (m_budget[0] < budget) m_budget[0] = budget;
    budget = (sector->Stat(0x194) * 12) / 10;
    if (budget < classes.m_x30) budget = classes.m_x30;
    if (m_budget[1] < budget) m_budget[1] = budget;
    budget = ((sector->Stat(0x198) + sector->Stat(0x138)) * 12) / 10;
    if (m_budget[3] < budget) m_budget[3] = budget;
    m_budget[2] = 0;
    m_budget[4] = 0;
    if ((m_owner->m_flags.Get() & 0x1000000) != 0) m_budget[6] = 1;
    m_budget[7] = 0;
    m_budget[8] = m_record->m_x98;
    m_budget[9] = 1;
}

ObjectKey* AiPostureKind3::Virtual21(ObjectKey* out) {
    ObjectKey chosen;
    AiFleetEval* fleet = FindTargetFleet(this);
    if (fleet) ChooseFleetDestination(this, fleet, &chosen);
    *out = chosen;
    return out;
}
