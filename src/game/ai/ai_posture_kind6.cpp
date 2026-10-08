#include "ai/ai_posture_util.h"
#include "ai/ai_postures.h"
#include "core/vptr_store.h"

using namespace ai_posture;
using ai::IsSystemKey;

AiPostureKind6::AiPostureKind6(int view, AiSideController* owner, AiSystemRecord* record) : AiPosture(view, owner, record) {}

AiPostureKind6::~AiPostureKind6() { REB_KEEP_VPTR_STORE(); }

int AiPostureKind6::Virtual03() {
    int result = 0;
    AiSectorEval* sector = FindSector(this);
    AiSystemEval* system = m_owner->FindSystemEval(&m_record->m_systemKey);
    if (system && sector) result = (sector->Stat(0xa0) + system->Stat(0x98)) - system->Stat(0x88);
    return result;
}

void AiPostureKind6::GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) {
    DefaultPlannerRequest(createKind, createSubtype, releaseKind, releaseSubtype);
}

void AiPostureKind6::GetAgentRequest(int* agentKind) { AgentRequestB(agentKind); }

void AiPostureKind6::Virtual08(PersistentInt* flagsA, PersistentInt*) {
    flagsA->m_value |= 0x4000000;
    AiSectorEval* sector = FindSector(this);
    if (sector) {
        const int flags = flagsA->m_value;
        flagsA->m_value = flags | 0x8000000;
        const uint32_t sectorFlags = sector->m_flags2c.m_value;
        if ((sectorFlags & 8) != 0 && (sectorFlags & 0x400) != 0 && ((sectorFlags & 0x400000) != 0 || (flags & 0x200000) != 0))
            flagsA->m_value = flags | 0x8020000;
    }
}

void AiPostureKind6::Virtual09(ObjectKey* a, ObjectKey* b) {
    static const SystemQuery queries[] = {
        ByStat(0, 0x400, 0x1000, 0, 0, 0, 0x1d, 2, 0xc00, 4),
        ByStat(0, 0x800, 0x1000, 0, 0, 0, 0x1d, 2, 0x800, 0x7000000),
        ByStat(0, 0x400, 0, 0, 0, 0, 0x1d, 2, 0xc00, 4),
        ByStat(0, 0x800, 0, 0, 0, 0, 0x1d, 2, 0x800, 4),
    };
    SelectCharacterSystem(this, a, b, queries);
}

void AiPostureKind6::Virtual10(ObjectKey* a, ObjectKey* b) { SelectSystemByAgeOrChance(this, a, b); }

void AiPostureKind6::Virtual11(ObjectKey* a, ObjectKey* b) { SelectSystemByAge(this, a, b); }

void AiPostureKind6::Virtual12(ObjectKey* a, ObjectKey* b) {
    static const SystemQuery queries[] = {
        ByStat(0, 0x40000, 0x100, 0, 0, 0, 0x1d, 2, 0x40000),
        ByStat(0, 0x2000000, 0x80000000, 0, 0, 0, 0x1d, 2, 0x2000000),
        ByStat(0, 0x80000, 0, 0, 0, 0, 0x1d, 2, 0x80000),
        ByStat(0, 0x100000, 0, 0, 0, 0, 0x1d, 2, 0x100000),
        ByStat(0, 0x40000, 0, 0, 0, 0, 0x1d, 2, 0x40000),
        ByStat(0, 0x200000, 0, 0, 0, 0, 0x1d, 2, 0x200000),
        ByStat(0, 0x400000, 0, 0, 0, 0, 0x1d, 2, 0x400000),
        ByStat(0, 0x800000, 0x200, 0, 0, 0, 0x1d, 2, 0x800000),
    };
    SelectObjectSystem(this, a, b, queries);
}

int AiPostureKind6::Virtual13() { return ChooseMarkedTarget(this, 0x10000, 0xfff8ffff, 0xfffdffff, 0x50000); }

OrderBase* AiPostureKind6::Virtual14() {
    AiSystemEval* system = m_owner->m_systems.FindEval(m_record->m_systemKey);
    if (!system) return nullptr;
    return OrderTroopsOfSystem(this, system, nullptr);
}

int AiPostureKind6::Virtual15() { return AssignTakeFleet(this); }

int AiPostureKind6::Virtual16() {
    AiFleetEval* fleet = FindTargetFleet(this);
    if (!fleet || fleet->Stat(0x80) <= 0) return 0;
    AiSectorEval* sector = FindSector(this);
    if (!sector) return 0;
    int assigned = 0;
    auto accepts = [this, fleet](AiSystemEval* system) { return CanUseFleet(fleet, system) != 0; };
    if (0 < sector->Stat(0x19c)) {
        m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0, 0, 4, 0, 0, 0x300200, 0x32, 1));
        assigned = AssignFleet(this, fleet, 0x200000, 0x20000000, accepts);
    }
    if (assigned == 0) {
        m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0, 0, 4, 0, 0, 0x300200, 0x21, 1));
        m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0, 0, 0x24, 0, 0, 0x300200, 0x14, 2));
        assigned = AssignFleet(this, fleet, 0x200000, 0x20000000, accepts);
    }
    return assigned;
}

int AiPostureKind6::Virtual17() { return AssignOrderFleet(this, 0x24, 0x21); }

void AiPostureKind6::Virtual20() {
    AiSectorEval* sector = FindSector(this);
    if (!sector) return;
    AiClassTable& classes = m_owner->m_classes;
    int budget = (sector->Stat(0x190) * 3) / 2;
    if (budget < classes.m_x2c) budget = classes.m_x2c;
    if (m_budget[0] < budget) m_budget[0] = budget;
    budget = (sector->Stat(0x194) * 3) / 2 + (sector->Stat(0x13c) * 3) / 2;
    if (budget < classes.m_x30) budget = classes.m_x30;
    if (m_budget[1] < budget) m_budget[1] = budget;
    budget = ((sector->Stat(0x198) + sector->Stat(0x138)) * 3) / 2;
    if (budget < classes.m_x34) budget = classes.m_x34;
    if (m_budget[3] < budget) m_budget[3] = budget;
    budget = sector->Stat(0x134);
    if (sector->Stat(0x134) < classes.m_x38) budget = classes.m_x38;
    if (m_budget[2] < budget) m_budget[2] = budget;
    if ((m_owner->m_flags.Get() & 4) != 0 && 0 < sector->Stat(0x19c)) m_budget[4] = 1;
    if ((m_owner->m_flags.Get() & 0x1000000) != 0) m_budget[6] = 1;
    m_budget[7] = m_record->m_x94;
    m_budget[8] = m_record->m_x98;
    m_budget[9] = 1;
}

ObjectKey* AiPostureKind6::Virtual21(ObjectKey* out) {
    ObjectKey chosen;
    ObjectKey scratch;
    AiFleetEval* fleet = FindTargetFleet(this);
    if (fleet) {
        if (static_cast<int16_t>(m_record->m_fleets.Count()) == 1 && !IsSystemKey(m_record->m_systemKey)) {
            AiFleetEval* current = FindTargetFleet(this);
            if (current && (current->m_flags.m_value & 0xa00) == 0 && m_owner->m_classes.m_x2c <= current->Stat(0x58) &&
                m_owner->m_classes.m_x30 <= current->Stat(0x5c)) {
                m_selector.Add(m_owner->RankSystemsOfSectorRandomly(&m_record->m_sectorKey, 0, 0, 0, 0, 0, 0x700201, 1));
                chosen = *m_selector.PickBest(&scratch);
                m_selector.Reset();
            }
            fleet = current;
        }
        ChooseFleetDestination(this, fleet, &chosen);
    }
    *out = chosen;
    return out;
}
