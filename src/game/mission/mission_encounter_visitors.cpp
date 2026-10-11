#include <new>

#include "character/character.h"
#include "character/role.h"
#include "fleet/detector.h"
#include "fleet/fleet.h"
#include "gameobj/object_queries.h"
#include "mission/mission_encounter.h"
#include "mission/mission_encounter_decl.h"

// The visitor classes of the encounter resolution (0x587100-0x587550 and 0x589000-0x5897b0); see mission_encounter.h.

using namespace mission_encounter;

namespace {

constexpr FamilyRange kCharacterTargetMissions(0x60, 0x68);

}  // namespace

// ---- CharacterTargetMissionIterator -----------------------------------------------------------------------------------------

CharacterTargetMissionIterator::CharacterTargetMissionIterator(GameObjNotifier* parent)
    : ChildIterator(parent, &kCharacterTargetMissions, kStateUsable) {}

CharacterTargetMissionIterator::~CharacterTargetMissionIterator() = default;

// ---- bases ------------------------------------------------------------------------------------------------------------------

MemberVisitorA::MemberVisitorA(EncounterContext* context) : m_context(context) {}
MemberVisitorA::~MemberVisitorA() = default;

MemberVisitor::MemberVisitor(EncounterContext* context) : m_context(context) {}
MemberVisitor::~MemberVisitor() = default;

UnitVisitor::UnitVisitor(EncounterContext* context) : m_context(context) {}
UnitVisitor::~UnitVisitor() = default;

// ---- counters and pickers ---------------------------------------------------------------------------------------------------

MemberCounter::MemberCounter(EncounterContext* context) : MemberVisitor(context), m_count(0) {}
MemberCounter::~MemberCounter() = default;

int MemberCounter::Visit(Role* member, int* stop, NotifyContext* ctx) {
    ++m_count;
    *stop = 0;
    return 1;
}

UnitCounter::UnitCounter(EncounterContext* context) : UnitVisitor(context), m_count(0) {}
UnitCounter::~UnitCounter() = default;

int UnitCounter::Visit(Detector* unit, Fleet* container, int* stop, NotifyContext* ctx) {
    ++m_count;
    *stop = 0;
    return 1;
}

MemberPicker::MemberPicker(EncounterContext* context)
    : MemberVisitor(context), m_index(0), m_target(0), m_result(nullptr) {}
MemberPicker::~MemberPicker() = default;

int MemberPicker::Visit(Role* member, int* stop, NotifyContext* ctx) {
    *stop = 0;
    const int present = member != nullptr;
    if (present) {
        *stop = m_index == m_target;
        if (*stop) {
            m_result = member;
            return present;
        }
        ++m_index;
    }
    return present;
}

UnitPicker::UnitPicker(EncounterContext* context)
    : UnitVisitor(context), m_index(0), m_target(0), m_result(nullptr), m_resultFleet(nullptr) {}
UnitPicker::~UnitPicker() = default;

int UnitPicker::Visit(Detector* unit, Fleet* container, int* stop, NotifyContext* ctx) {
    *stop = 0;
    const int present = unit != nullptr;
    if (present) {
        *stop = m_index == m_target;
        if (*stop) {
            m_result = unit;
            m_resultFleet = container;
            return present;
        }
        ++m_index;
    }
    return present;
}

MemberEspionageSum::MemberEspionageSum(EncounterContext* context) : MemberVisitor(context), m_sum(0) {}
MemberEspionageSum::~MemberEspionageSum() = default;

int MemberEspionageSum::Visit(Role* member, int* stop, NotifyContext* ctx) {
    *stop = 0;
    const int present = member != nullptr;
    if (present) m_sum += member->GetEspionage();
    return present;
}

// ---- action visitors --------------------------------------------------------------------------------------------------------

ClearDecoyingVisitor::ClearDecoyingVisitor(EncounterContext* context) : MemberVisitor(context) {}
ClearDecoyingVisitor::~ClearDecoyingVisitor() = default;

int ClearDecoyingVisitor::Visit(Role* member, int* stop, NotifyContext* ctx) {
    *stop = 0;
    member->SetIsDecoying(0, ctx);
    return 1;
}

BetrayalRollVisitor::BetrayalRollVisitor(EncounterContext* context) : MemberVisitor(context) {}
BetrayalRollVisitor::~BetrayalRollVisitor() = default;

int BetrayalRollVisitor::Visit(Role* member, int* stop, NotifyContext* ctx) {
    *stop = 0;
    int betrays = 0;
    const int ok = m_context->RollNotBetray(member, &betrays, ctx);
    *stop = betrays;
    return ok != 0;
}

ClearDecoyedVisitor::ClearDecoyedVisitor(EncounterContext* context) : UnitVisitor(context) {}
ClearDecoyedVisitor::~ClearDecoyedVisitor() = default;

int ClearDecoyedVisitor::Visit(Detector* unit, Fleet* container, int* stop, NotifyContext* ctx) {
    *stop = 0;
    unit->SetIsDecoyed(0, ctx);
    return 1;
}

UnitAgainstMemberVisitor::UnitAgainstMemberVisitor(EncounterContext* context) : MemberVisitor(context) {}
UnitAgainstMemberVisitor::~UnitAgainstMemberVisitor() = default;

int UnitAgainstMemberVisitor::Visit(Role* member, int* stop, NotifyContext* ctx) {
    *stop = 0;
    Detector* unit = nullptr;
    Fleet* container = nullptr;
    m_context->PickRandomUnit(&unit, &container, ctx);
    *stop = unit == nullptr;
    if (unit) m_context->ResolveFailedDecoy(unit, container, member, ctx);
    return 1;
}

EncounterFightVisitor::EncounterFightVisitor(EncounterContext* context) : MemberVisitorA(context) {}
EncounterFightVisitor::~EncounterFightVisitor() = default;

int EncounterFightVisitor::Visit(Role* member, int* stop, NotifyContext* ctx) {
    *stop = 0;
    int ok = 1;
    Character* character = static_cast<Character*>(member);
    if (character->m_encounterKey.IsSet()) {
        Role* other = static_cast<Role*>(GetMasterInFamilies_30_3c(&character->m_encounterKey));
        ok = other != nullptr;
        if (other) ok = m_context->ResolveEncounterFight(member, other, ctx) && ok;
    }
    return ok;
}

EncounterDispatchVisitor::EncounterDispatchVisitor(EncounterContext* context) : MemberVisitorA(context) {}
EncounterDispatchVisitor::~EncounterDispatchVisitor() = default;

int EncounterDispatchVisitor::Visit(Role* member, int* stop, NotifyContext* ctx) {
    *stop = 0;
    Character* character = static_cast<Character*>(member);
    int ok;
    if (!character->m_encounterKey.OrNull()) {
        UnitAgainstMemberVisitor visitor(m_context);
        ok = visitor.Visit(member, stop, ctx) != 0;
        *stop = 0;
    } else {
        EncounterFightVisitor visitor(m_context);
        ok = visitor.Visit(member, stop, ctx) != 0;
    }
    return ok;
}

IdentifyTraitorVisitor::IdentifyTraitorVisitor(EncounterContext* context) : MemberVisitorA(context) {}
IdentifyTraitorVisitor::~IdentifyTraitorVisitor() = default;

int IdentifyTraitorVisitor::Visit(Role* member, int* stop, NotifyContext* ctx) {
    *stop = 0;
    int identified = 0;
    const int ok = m_context->RollIdentifyTraitor(member, &identified, ctx);
    *stop = identified;
    return ok != 0;
}

BetrayalCandidateCollector::BetrayalCandidateCollector(EncounterContext* context, ObjectKeyValueSet* keys)
    : MemberVisitorA(context), m_keys(keys) {}
BetrayalCandidateCollector::~BetrayalCandidateCollector() = default;

int BetrayalCandidateCollector::Visit(Role* member, int* stop, NotifyContext* ctx) {
    *stop = 0;
    int candidate = 0;
    int ok = m_context->IsBetrayalCandidate(member, &candidate) != 0;
    if (candidate) {
        ObjectKey key;
        member->GetKey(&key);
        ok = m_keys->AddKeyValue(&key, 0) && ok;
    }
    return ok;
}

UnitAgainstDecoyVisitor::UnitAgainstDecoyVisitor(EncounterContext* context) : UnitVisitor(context) {}
UnitAgainstDecoyVisitor::~UnitAgainstDecoyVisitor() = default;

int UnitAgainstDecoyVisitor::Visit(Detector* unit, Fleet* container, int* stop, NotifyContext* ctx) {
    *stop = 0;
    Role* decoy = nullptr;
    m_context->PickRandomDecoy(&decoy, ctx);
    *stop = decoy == nullptr;
    if (decoy) m_context->TryDecoy(decoy, unit, container, ctx);
    return 1;
}

UnitFoilVisitor::UnitFoilVisitor(EncounterContext* context)
    : UnitVisitor(context), m_foiled(0), m_prepared(0), m_averageSkill(0), m_teamCount(0) {}
UnitFoilVisitor::~UnitFoilVisitor() = default;

int UnitFoilVisitor::Visit(Detector* unit, Fleet* container, int* stop, NotifyContext* ctx) {
    *stop = 0;
    if (m_context->m_teamCount != 0) {
        if (m_prepared == 0) {
            m_prepared = 1;
            m_context->AverageTeamEspionage(&m_averageSkill, ctx);
            MemberCounter counter(m_context);
            m_context->VisitTeamSpecialForces(&counter, ctx);
            m_teamCount = counter.m_count;
        }
        if (*stop == 0) m_context->RollFoil(unit, container, m_averageSkill, m_teamCount, &m_foiled, ctx);
    }
    *stop = m_foiled;
    return 1;
}
