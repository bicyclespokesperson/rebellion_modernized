#include "character/character.h"
#include "character/role.h"
#include "core/vptr_store.h"
#include "data/random.h"
#include "economy/defense_facility.h"
#include "fleet/capship.h"
#include "fleet/fleet_iterator.h"
#include "gameobj/object_queries.h"
#include "mission/mission.h"
#include "mission/mission_encounter.h"
#include "mission/mission_encounter_decl.h"
#include "mission/mission_rules.h"

// EncounterRunner (0x5897c0-0x58ad80): one resolution run of a mission against the units and persons at its location.
// See mission_encounter.h.

using namespace mission_encounter;

namespace {

constexpr int kKeyTaken = 1;
constexpr int kKeyMarked = 2;
// The keys of the two story characters that are chosen before anybody else (Luke's and the Emperor's).
constexpr uint32_t kFirstChoiceKey = 0x32000242;
constexpr uint32_t kSecondChoiceKey = 0x35000281;

inline Mission* missionOf(EncounterContext& context) { return static_cast<Mission*>(context.m_mission); }

uint32_t otherSide(uint32_t side) { return side == 3 ? 3 : 2 - (side != 1); }

ObjectKeyValueNode* valueNode(ObjectKeyNode* node) { return static_cast<ObjectKeyValueNode*>(node); }

ObjectKeyNode* nextNode(ObjectKeyNode* node) { return static_cast<ObjectKeyNode*>(node->m_successor); }

}  // namespace

// ---- construction -------------------------------------------------------------------------------------------------------

EncounterRunner::EncounterRunner()
    : m_unk04(1), m_context(), m_secondPass(1), m_unk48(0), m_unk4c(0), m_setA(), m_setB(), m_setC(), m_setD() {}

EncounterRunner::~EncounterRunner() { REB_KEEP_VPTR_STORE(); }

// ---- the run ---------------------------------------------------------------------------------------------------------------

int EncounterRunner::Run(NotifyContext* ctx) {
    if (m_context.m_mission != nullptr && m_context.m_location != nullptr &&
        (m_context.m_mission->m_flags & 0xc0) != 0xc0) {
        m_secondPass = 1;
        const int firstPass = RunPass(ctx) != 0;
        m_secondPass = 0;
        if (RunPass(ctx) && firstPass) return 1;
    }
    return 0;
}

int EncounterRunner::RunPass(NotifyContext* ctx) {
    int ok = Prepare(ctx) != 0;
    if (m_unk48 == 0) {
        ok = RunDetection(ctx) && ok;
        ok = RunEncounters(ctx) && ok;
        ok = RunMembers(ctx) && ok;
        ok = RunPromotion(ctx) && ok;
        ok = RunFinalMembers(ctx) && ok;
        // 0x51ebb0 follows here in the original: a shared stub that returns 1.
        return ok;
    }
    return ok;
}

int EncounterRunner::Prepare(NotifyContext* ctx) {
    Mission* mission = missionOf(m_context);
    int ok = mission->UpdatePhaseForTeam(ctx) != 0;
    if (mission->HasCompletionStatus() != 0) m_unk48 = 1;
    if (mission->GetRecordDetectionPhases() == 0) m_unk48 = 1;
    if (mission->GetPhaseLevel() == 0) m_unk48 = 1;
    if (m_unk48 == 0) {
        const uint32_t missionSide = mission->ControlSide();
        const uint32_t enemySide = otherSide(missionSide);
        FleetIterator enemyFleets(m_context.m_location, 1, enemySide);
        const int hasEnemyFleets = enemyFleets.HasCurrent();
        const int locationIsEnemy = m_context.m_location->ControlSide() == enemySide;
        int hasDefenses;
        {
            DefenseFacility22Iterator defenses(m_context.m_location, 1, missionSide);
            hasDefenses = defenses.HasCurrent();
        }
        auto joinOwnSideGround = [&] {
            m_context.m_valueA = 1;
            if (locationIsEnemy) {
                m_context.m_flag18 = 1;
                m_context.m_flag28 = 1;
            }
        };
        auto joinEnemyUnits = [&] {
            m_context.m_valueB = 1;
            if (mission->TargetLocationDiffersFromOrigin() != 0) m_context.m_valueA = 1;
            if (hasEnemyFleets != 0 && hasDefenses == 0) {
                m_context.m_flag1c = 1;
                m_context.m_flag20 = 1;
                m_context.m_flag24 = 1;
            }
        };
        switch (mission->GetPhaseLevel()) {
            case 1:
                if (m_secondPass != 0)
                    joinEnemyUnits();
                else
                    m_unk48 = 1;
                break;
            case 0:
                m_unk48 = 1;
                break;
            case 2:
                if (m_secondPass == 0)
                    m_unk48 = 1;
                else
                    joinOwnSideGround();
                break;
            case 3:
                if (m_secondPass == 0)
                    joinOwnSideGround();
                else
                    joinEnemyUnits();
                break;
            case 4: {
                if (m_secondPass == 0) {
                    m_unk48 = 1;
                    break;
                }
                bool sameOwner = false;
                GameObj* origin = nullptr;
                GameObjNotifier* parent = nullptr;
                ok = mission->GetObjectiveInOwnerView(&origin) && ok;
                if (origin != nullptr) {
                    ok = origin->GetParent(&parent) && ok;
                    if (origin != nullptr && parent != nullptr) {
                        ObjectKey originKey;
                        ObjectKey ownerKey;
                        origin->GetKey(&originKey);
                        ObjectKey* owner = GetOwnerKeyValue(origin, &ownerKey);
                        if (owner->m_value == originKey.m_value) {
                            sameOwner = true;
                        } else {
                            ObjectKey parentKey;
                            ObjectKey ownerKey2;
                            parent->GetKey(&parentKey);
                            ObjectKey* owner2 = GetOwnerKeyValue(origin, &ownerKey2);
                            sameOwner = owner2->m_value == parentKey.m_value;
                        }
                    }
                }
                m_context.m_valueB = 1;
                m_context.m_valueA = 1;
                if (sameOwner) {
                    if (locationIsEnemy) {
                        m_context.m_flag18 = 1;
                        m_context.m_flag28 = 1;
                    }
                } else if (hasEnemyFleets != 0) {
                    m_context.m_flag1c = 1;
                    m_context.m_flag20 = 1;
                    m_context.m_flag24 = 1;
                }
                break;
            }
            default:
                ok = 0;
                break;
        }
    }
    ClearDecoyedVisitor clearDecoyed(&m_context);
    ok = m_context.VisitUnits(&clearDecoyed, ctx) && ok;
    ClearDecoyingVisitor clearDecoying(&m_context);
    ok = m_context.VisitDecoys(&clearDecoying, ctx) && ok;
    ok = m_context.CountAll(ctx) && ok;
    return ok ? 1 : 0;
}

// ---- the phases --------------------------------------------------------------------------------------------------------------

int EncounterRunner::RunDetection(NotifyContext* ctx) {
    int ok = PrepareSelections(ctx) != 0;
    ObjectKey chosen;
    do {
        chosen.Reset();
        ok = ChooseDetection(&chosen) && ok;
        if (chosen.OrNull()) ok = RunForceDetectDispatch(&chosen, ctx) && ok;
    } while (chosen.OrNull());
    return ok;
}

int EncounterRunner::RunEncounters(NotifyContext* ctx) {
    int ok = 1;
    if (missionOf(m_context)->IsFinalPhase() != 0 && m_unk4c == 0) {
        BetrayalRollVisitor betrayal(&m_context);
        ok = m_context.VisitAllMembers(&betrayal, ctx) != 0;
        if (m_context.m_betrayerKey.IsSet()) {
            m_unk4c = 1;
            IdentifyTraitorVisitor identify(&m_context);
            ok = m_context.VisitAllMajorMinor(&identify, ctx) && ok;
        }
    }
    return ok;
}

int EncounterRunner::RunMembers(NotifyContext* ctx) {
    EncounterFightVisitor fights(&m_context);
    int ok = m_context.VisitDecoyMajorMinor(&fights, ctx) != 0;
    if (m_unk4c == 0) {
        UnitAgainstDecoyVisitor decoyRoll(&m_context);
        if (m_context.m_flag18 != 0) ok = m_context.VisitUndecoyedUnits(&decoyRoll, ctx) && ok;
        if (m_context.m_flag1c != 0) {
            ok = m_context.VisitUndecoyedCapShips(&decoyRoll, ctx) && ok;
            ok = m_context.VisitUndecoyedSquadrons(&decoyRoll, ctx) && ok;
        }
    }
    return ok;
}

int EncounterRunner::RunPromotion(NotifyContext* ctx) {
    int ok = 1;
    if (m_unk4c == 0) {
        UnitFoilVisitor foil(&m_context);
        ok = m_context.VisitUndecoyedUnits(&foil, ctx) != 0;
        if (ok) m_unk4c = foil.m_foiled;
    }
    return ok;
}

int EncounterRunner::RunFinalMembers(NotifyContext* ctx) {
    int ok = 1;
    if (m_unk4c != 0) {
        Mission* mission = missionOf(m_context);
        if (mission->GetRecordCanResign() != 0)
            ok = mission->CompleteWith(3 + (mission->IsDataField1cAbove4() != 0), ctx) != 0;
        EncounterDispatchVisitor dispatch(&m_context);
        ok = m_context.VisitTeamMajorMinor(&dispatch, ctx) && ok;
        UnitAgainstMemberVisitor unitsMeetMembers(&m_context);
        ok = m_context.VisitTeamSpecialForces(&unitsMeetMembers, ctx) && ok;
    }
    return ok;
}

int EncounterRunner::RunForceDetectDispatch(ObjectKey* chosen, NotifyContext* ctx) {
    int ok = ClearSelection();
    Character* person = static_cast<Character*>(GetMasterInFamilies_30_3c(chosen));
    if (person != nullptr) {
        ObjectKey picked;
        do {
            picked.Reset();
            ok = ChooseMemberDetection(&picked) && ok;
            if (picked.OrNull()) ok = ForceDetect(&picked, chosen, ctx) && ok;
        } while (picked.OrNull() && !person->m_encounterKey.IsSet());
    }
    return ok;
}

int EncounterRunner::ClearSelection() {
    const int first = ClearTakenFlags(&m_setA);
    const int second = ClearTakenFlags(&m_setB);
    return second && first;
}

int EncounterRunner::ForceDetect(ObjectKey* picked, ObjectKey* chosen, NotifyContext* ctx) {
    Character* chosenPerson = static_cast<Character*>(GetMasterInFamilies_30_3c(chosen));
    Character* pickedPerson = static_cast<Character*>(GetMasterInFamilies_30_3c(picked));
    int ok = chosenPerson != nullptr && pickedPerson != nullptr;
    int detected = 0;
    if (chosenPerson != nullptr && pickedPerson != nullptr) {
        int chance = 0;
        ok = ComputeDetectionChance(pickedPerson->ControlSide(), pickedPerson->m_force, chosenPerson->m_force, &detected,
                                    &chance) &&
             ok;
    }
    if (detected != 0 && chosenPerson != nullptr && pickedPerson != nullptr) {
        if ((pickedPerson->m_flags78 & role_flag::kDecoy) == 0) m_unk4c = 1;
        GameObjNotifier* manager = GetGameRootMember_c0();
        ok = (manager != nullptr) && ok;
        if (manager) ok = FUN_0055ff60(manager, pickedPerson, chosenPerson, ctx) && ok;
        if (MarkSelected(picked, chosen) && ok) return 1;
        return 0;
    }
    return ok;
}

int EncounterRunner::MarkSelected(ObjectKey* picked, ObjectKey* chosen) {
    int ok = MarkKeyTaken(picked, &m_setA);
    const int second = MarkKeyTaken(picked, &m_setB);
    ok = second && ok;
    ok = MarkKeyTaken(chosen, &m_setD) && ok;
    ok = MarkKeyTaken(chosen, &m_setC) && ok;
    return ok;
}

// ---- selection state ---------------------------------------------------------------------------------------------------------

int EncounterRunner::PrepareSelections(NotifyContext* ctx) {
    const int members = PrepareMemberSelections(ctx);
    const int units = PrepareUnitSelections(ctx);
    return units && members;
}

int EncounterRunner::PrepareMemberSelections(NotifyContext* ctx) {
    m_setA.Clear();
    m_setB.Clear();
    BetrayalCandidateCollector majors(&m_context, &m_setA);
    int ok = m_context.VisitMajors(&majors, ctx) != 0;
    BetrayalCandidateCollector minors(&m_context, &m_setB);
    return m_context.VisitMinors(&minors, ctx) && ok;
}

int EncounterRunner::PrepareUnitSelections(NotifyContext* ctx) {
    m_setD.Clear();
    m_setC.Clear();
    int ok = 1;
    if (m_context.m_flag18 != 0) ok = VisitLocationPersons(m_context.m_location) != 0;
    if (m_context.m_flag1c != 0) {
        FleetIterator fleets(m_context.m_location, 1);
        for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
            CapShipIterator capShips(fleets.Current(), 1);
            for (capShips.Begin(); capShips.Current(); capShips.Advance())
                ok = VisitLocationPersons(capShips.Current()) && ok;
        }
    }
    return ok;
}

int EncounterRunner::VisitLocationPersons(GameObjNotifier* parent) {
    int ok = 1;
    {
        MajorCharacterIterator persons(parent, 1);
        for (persons.Begin(); persons.Current(); persons.Advance()) {
            Role* person = static_cast<Role*>(persons.Current());
            int foreign = 0;
            ok = m_context.IsForeignDetector(person, &foreign) && ok;
            if (foreign != 0 && person->ControlSide() != missionOf(m_context)->ControlSide()) {
                ObjectKey key;
                person->GetKey(&key);
                ok = m_setD.AddKeyValue(&key, 0) && ok;
            }
        }
    }
    {
        MinorCharacterIterator persons(parent, 1);
        for (persons.Begin(); persons.Current(); persons.Advance()) {
            Role* person = static_cast<Role*>(persons.Current());
            int foreign = 0;
            ok = m_context.IsForeignDetector(person, &foreign) && ok;
            if (foreign != 0 && person->ControlSide() != missionOf(m_context)->ControlSide()) {
                ObjectKey key;
                person->GetKey(&key);
                ok = m_setC.AddKeyValue(&key, 0) && ok;
            }
        }
    }
    return ok;
}

int EncounterRunner::ChooseDetection(ObjectKey* chosen) {
    int ok = 1;
    chosen->Reset();
    if (!chosen->OrNull()) {
        ObjectKey key;
        key.m_value = kFirstChoiceKey;
        ok = TakeKey(&key, &m_setD, chosen) != 0;
    }
    if (!chosen->OrNull()) {
        ObjectKey key;
        key.m_value = kSecondChoiceKey;
        ok = TakeKey(&key, &m_setD, chosen) && ok;
    }
    if (!chosen->OrNull()) ok = TakeRandomFreeKey(&m_setD, chosen) && ok;
    if (!chosen->OrNull()) ok = TakeRandomFreeKey(&m_setC, chosen) && ok;
    return ok;
}

int EncounterRunner::ChooseMemberDetection(ObjectKey* chosen) {
    int ok = 1;
    chosen->Reset();
    if (!chosen->OrNull()) {
        ObjectKey key;
        key.m_value = kFirstChoiceKey;
        ok = TakeKey(&key, &m_setA, chosen) != 0;
    }
    if (!chosen->OrNull()) {
        ObjectKey key;
        key.m_value = kSecondChoiceKey;
        ok = TakeKey(&key, &m_setA, chosen) && ok;
    }
    if (!chosen->OrNull()) ok = TakeRandomFreeKey(&m_setA, chosen) && ok;
    if (!chosen->OrNull()) ok = TakeRandomFreeKey(&m_setB, chosen) && ok;
    return ok;
}

int EncounterRunner::MarkKeyTaken(ObjectKey* key, ObjectKeySet* set) {
    ObjectKeyNode* node = set->Find(key);
    if (node) valueNode(node)->m_value |= kKeyMarked;
    return 1;
}

int EncounterRunner::TakeKey(ObjectKey* key, ObjectKeySet* set, ObjectKey* chosen) {
    chosen->Reset();
    ObjectKeyNode* node = set->Find(key);
    if (node && valueNode(node)->m_value == 0) {
        valueNode(node)->m_value = kKeyTaken;
        chosen->m_value = key->m_value;
    }
    return 1;
}

int EncounterRunner::TakeRandomFreeKey(ObjectKeySet* set, ObjectKey* chosen) {
    chosen->Reset();
    int free = 0;
    for (ObjectKeyNode* node = set->First(); node; node = nextNode(node))
        if (valueNode(node)->m_value == 0) ++free;
    if (free != 0) {
        const int pick = GameRandom(free - 1);
        int index = 0;
        for (ObjectKeyNode* node = set->First(); node && !chosen->OrNull(); node = nextNode(node)) {
            if (valueNode(node)->m_value == 0) {
                if (index == pick) {
                    valueNode(node)->m_value = kKeyTaken;
                    ObjectKey key;
                    node->GetKey(&key);
                    chosen->m_value = key.m_value;
                }
                ++index;
            }
        }
    }
    return 1;
}

int EncounterRunner::ClearTakenFlags(ObjectKeySet* set) {
    for (ObjectKeyNode* node = set->First(); node; node = nextNode(node)) valueNode(node)->m_value &= ~kKeyTaken;
    return 1;
}
