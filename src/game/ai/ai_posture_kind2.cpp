#include "ai/ai_posture_util.h"
#include "ai/ai_postures.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"

using namespace ai_posture;
using ai::IsSystemKey;

namespace {

void RememberSystem(ObjectKeySet* set, const ObjectKey& key) {
    ObjectKeyNode* node = NewOrNull<ObjectKeyNode>(key);
    if (node) set->Add(node);
}

}  // namespace

AiPostureKind2::AiPostureKind2(int view, AiSideController* owner, AiSystemRecord* record)
    : AiPosture(view, owner, record), m_chosen() {}

AiPostureKind2::~AiPostureKind2() { REB_KEEP_VPTR_STORE(); }

void AiPostureKind2::Read(istream* in) {
    AiPosture::Read(in);
    m_chosen.Load(in);
}

void AiPostureKind2::Write(ostream* out) {
    AiPosture::Write(out);
    m_chosen.Save(out);
}

void AiPostureKind2::GetPlannerRequest(int* createKind, int* createSubtype, int* releaseKind, int* releaseSubtype) {
    DefaultPlannerRequest(createKind, createSubtype, releaseKind, releaseSubtype);
}

void AiPostureKind2::GetAgentRequest(int* agentKind) { AgentRequestC(agentKind); }

void AiPostureKind2::Virtual08(PersistentInt* flagsA, PersistentInt*) {
    bool found = false;
    flagsA->m_value |= 0x4000000;
    AiSectorEval* sector = FindSector(this);
    if (!sector) return;
    const int flags = flagsA->m_value;
    flagsA->m_value = flags | 0x8000000;
    if ((sector->m_flags2c.m_value & 0x40000) != 0) {
        flagsA->m_value = flags | 0x8020000;
        return;
    }
    ObjectKey scratch;
    m_selector.Add(m_owner->RankSystemsOfSectorByAge(&m_record->m_sectorKey, 0, 0, 0x20, 0, 0, 0x700201, 1));
    ObjectKey key = *m_selector.PickBest(&scratch);
    while (IsSystemKey(key) && !found) {
        if (!m_chosen.Find(&key)) found = true;
        key = *m_selector.PickBest(&scratch);
    }
    m_selector.Reset();
    if (found) {
        flagsA->m_value |= 0x20000;
        sector->m_flags2c.m_value |= 0x20000000;
    }
}

void AiPostureKind2::Virtual09(ObjectKey* a, ObjectKey* b) {
    static const SystemQuery queries[] = {
        ByStat(0, 0x400, 0, 0, 0, 0, 0x1d, 2, 0xc00, 4),
        ByStat(0, 0x800, 0, 0, 0, 0, 0x1d, 2, 0x800, 4),
    };
    SelectCharacterSystem(this, a, b, queries);
}

void AiPostureKind2::Virtual10(ObjectKey* a, ObjectKey* b) { SelectSystemByAgeOnce(this, a, b); }

void AiPostureKind2::Virtual11(ObjectKey* a, ObjectKey* b) { SelectSystemByAge(this, a, b); }

void AiPostureKind2::Virtual12(ObjectKey* a, ObjectKey* b) {
    static const SystemQuery queries[] = {
        ByStat(0, 0x400000, 0, 0, 0, 0, 0x1d, 2, 0x400000),
        ByStat(0, 0x800000, 0x200, 0, 0, 0, 0x1d, 2, 0x800000),
        ByStat(0, 0x20000, 0, 0, 0, 0, 0x1d, 2, 0x20000),
    };
    SelectObjectSystem(this, a, b, queries);
}

int AiPostureKind2::Virtual15() {
    int assigned = 0;
    AiFleetEval* fleet = FindTargetFleet(this);
    if (fleet) {
        auto accepts = [fleet](AiSystemEval* system) { return AiPosture::CanTakeSystem(fleet, system) != 0; };
        if (0 < fleet->Stat(0x58)) {
            m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0, 0, 0x200, 0, 0, 0x100000, 0x27, 1));
            assigned = AssignFleet(this, fleet, 0x100000, 0x10000000, accepts);
        }
        if (assigned == 0 && 0 < fleet->Stat(0x5c)) {
            m_selector.Add(m_owner->RankSystemsOfSector(&m_record->m_sectorKey, 0, 0x400000, 4, 0, 0, 0x100000, 0x1b, 1));
            assigned = AssignFleet(this, fleet, 0x100000, 0x10000000, accepts);
        }
    }
    if (assigned != 0) RememberSystem(&m_chosen, fleet->m_x2c);
    return assigned;
}

void AiPostureKind2::Virtual20() {
    AiSectorEval* sector = FindSector(this);
    if (!sector) return;
    AiClassTable& classes = m_owner->m_classes;
    int budget = (sector->Stat(0x190) * 3) / 2;
    if (budget < classes.m_x2c) budget = classes.m_x2c;
    if (m_budget[0] < budget) m_budget[0] = budget;
    budget = (sector->Stat(0x13c) * 3) / 2 + (sector->Stat(0x194) * 3) / 2;
    if (budget < classes.m_x30) budget = classes.m_x30;
    if (m_budget[1] < budget) m_budget[1] = budget;
    budget = ((sector->Stat(0x138) + sector->Stat(0x198)) * 3) / 2;
    if (budget < classes.m_x34) budget = classes.m_x34;
    if (m_budget[3] < budget) m_budget[3] = budget;
    m_budget[2] = 0;
    m_budget[4] = 0;
    if ((m_owner->m_flags.Get() & 0x1000000) != 0) m_budget[6] = 1;
    m_budget[7] = 0;
    m_budget[8] = m_record->m_x98;
    m_budget[9] = 1;
}

ObjectKey* AiPostureKind2::Virtual21(ObjectKey* out) {
    ObjectKey chosen;
    ObjectKey scratch;
    bool found = false;
    AiFleetEval* fleet = FindTargetFleet(this);
    if (fleet) {
        if (static_cast<int16_t>(m_record->m_fleets.Count()) == 1) {
            fleet = FindTargetFleet(this);
            if (fleet && (fleet->m_flags.m_value & 0xa00) == 0 && m_owner->m_classes.m_x2c <= fleet->Stat(0x58) &&
                m_owner->m_classes.m_x30 <= fleet->Stat(0x5c)) {
                m_selector.Add(m_owner->RankSystemsOfSectorByAge(&m_record->m_sectorKey, 0, 0, 0x20, 0, 0, 0x700201, 1));
                chosen = *m_selector.PickBest(&scratch);
                while (IsSystemKey(chosen) && !found) {
                    if (!m_chosen.Find(&chosen))
                        found = true;
                    else
                        chosen = *m_selector.PickBest(&scratch);
                }
                m_selector.Reset();
                if (found) RememberSystem(&m_chosen, chosen);
            }
        }
        ChooseFleetDestination(this, fleet, &chosen);
    }
    *out = chosen;
    return out;
}
