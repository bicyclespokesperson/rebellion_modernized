#include "ai/ai_posture_util.h"
#include "ai/ai_postures.h"

using namespace ai_posture;
using ai::IsSystemKey;

AiPostureKind5::AiPostureKind5(int view, AiSideController* owner, AiSystemRecord* record) : AiPosture(view, owner, record) {}

AiPostureKind5::~AiPostureKind5() = default;

int AiPostureKind5::Virtual03() {
    int result = 0;
    AiSectorEval* sector = FindSector(this);
    AiSystemEval* system = m_owner->FindSystemEval(&m_record->m_systemKey);
    if (system && sector) result = ((system->Stat(0x98) - system->Stat(0x88)) - sector->Stat(0x78)) + 10;
    return result;
}

void AiPostureKind5::GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) {
    DefaultPlannerRequest(createKind, createSubtype, releaseKind, releaseSubtype);
}

void AiPostureKind5::GetAgentRequest(int* agentKind) { AgentRequestA(agentKind); }

void AiPostureKind5::Virtual08(PersistentInt* flagsA, PersistentInt*) {
    flagsA->m_value |= 0x4000000;
    AiSectorEval* sector = FindSector(this);
    if (sector) {
        const int flags = flagsA->m_value;
        flagsA->m_value = flags | 0x8000000;
        if ((sector->m_flags2c.m_value & 2) == 0 || (sector->m_flags2c.m_value & 8) != 0) flagsA->m_value = flags | 0x8020000;
    }
}

void AiPostureKind5::Virtual09(ObjectKey* a, ObjectKey* b) {
    static const SystemQuery queries[] = {
        ByStat(0, 0x400, 0x1000, 0, 0, 0, 0x1d, 2, 0xc00, 4),
        ByStat(0, 0x800, 0x1000, 0, 0, 0, 0x1d, 2, 0x800, 0x7000000),
        ByStat(0, 0x400, 0, 0, 0, 0, 0x1d, 2, 0xc00, 4),
        ByStat(0, 0x800, 0, 0, 0, 0, 0x1d, 2, 0x800, 4),
    };
    SelectCharacterSystem(this, a, b, queries);
}

void AiPostureKind5::Virtual10(ObjectKey* a, ObjectKey* b) { SelectSystemByAgeOrChance(this, a, b); }

void AiPostureKind5::Virtual11(ObjectKey* a, ObjectKey* b) { SelectSystemByAge(this, a, b); }

void AiPostureKind5::Virtual12(ObjectKey* a, ObjectKey* b) {
    static const SystemQuery queries[] = {
        ByStat(0, 0x40000, 0x100, 0, 0, 0, 0x1d, 2, 0x40000),
        ByStat(0, 0x80000, 0, 0, 0, 0, 0x1d, 2, 0x80000),
        ByStat(0, 0x100000, 0, 0, 0, 0, 0x1d, 2, 0x100000),
        ByStat(0, 0x40000, 0, 0, 0, 0, 0x1d, 2, 0x40000),
        ByStat(0, 0x200000, 0, 0, 0, 0, 0x1d, 2, 0x200000),
        ByStat(0, 0x400000, 0, 0, 0, 0, 0x1d, 2, 0x400000),
        ByStat(0, 0x800000, 0x200, 0, 0, 0, 0x1d, 2, 0x800000),
    };
    SelectObjectSystem(this, a, b, queries);
}

int AiPostureKind5::Virtual13() { return ChooseMarkedTarget(this, 0x20000, 0xfff8ffff, 0xfffeffff, 0x60000); }

OrderBase* AiPostureKind5::Virtual14() {
    AiSectorEval* sector = FindSector(this);
    AiSystemEval* system = m_owner->m_systems.FindEval(m_record->m_systemKey);
    if (!sector || !system) return nullptr;
    return OrderTroopsOfSystem(this, system, sector);
}

int AiPostureKind5::Virtual15() { return AssignTakeFleet(this); }

int AiPostureKind5::Virtual17() { return AssignOrderFleet(this, 0x24, 0x21); }

int AiPostureKind5::Virtual18() { return AssignSupportFleet(this); }

void AiPostureKind5::Virtual20() {
    AiSectorEval* sector = FindSector(this);
    if (!sector) return;
    int budget = (sector->Stat(0x190) * 3) / 2;
    if (m_budget[0] < budget) m_budget[0] = budget;
    budget = (sector->Stat(0x194) * 3) / 2;
    if ((sector->m_flags2c.m_value & 8) != 0 && budget < m_owner->m_classes.m_x30) budget = m_owner->m_classes.m_x30;
    if (m_budget[1] < budget) m_budget[1] = budget;
    budget = ((sector->Stat(0x138) + sector->Stat(0x198)) * 12) / 10;
    if (m_budget[3] < budget) m_budget[3] = budget;
    if (m_budget[2] < 1) m_budget[2] = 1;
    if ((m_owner->m_flags.Get() & 4) != 0 && 0 < sector->Stat(0x19c)) m_budget[4] = 1;
    if ((m_owner->m_flags.Get() & 0x1000000) != 0) m_budget[6] = 1;
    m_budget[7] = m_record->m_x94;
    m_budget[8] = m_record->m_x98;
    m_budget[9] = 1;
}

ObjectKey* AiPostureKind5::Virtual21(ObjectKey* out) {
    ObjectKey chosen;
    ObjectKey scratch;
    AiFleetEval* fleet = FindTargetFleet(this);
    if (fleet) {
        if (static_cast<int16_t>(m_record->m_fleets.Count()) == 1 && !IsSystemKey(m_record->m_systemKey) &&
            (fleet->m_flags.m_value & 0xa00) == 0) {
            m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&m_record->m_sectorKey, 0, 0, 0, 0, 0x8000000, 0x700201, 1));
            chosen = *m_selector.PickBest(&scratch);
            m_selector.Reset();
        }
        ChooseFleetDestination(this, fleet, &chosen);
    }
    *out = chosen;
    return out;
}
