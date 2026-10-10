#include "character/character_mgr.h"
#include "character/character.h"
#include "character/role.h"
#include "core/vptr_store.h"
#include "data/random.h"
#include "fleet/capship.h"
#include "fleet/detector.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "gameobj/object_queries.h"
#include "mission/mission.h"
#include "mission/mission_encounter.h"
#include "mission/mission_encounter_decl.h"
#include "mission/mission_rules.h"

// EncounterContext (0x587550-0x588fee): the walkers over the units and persons of a mission's location and the rolls
// applied to them; see mission_encounter.h.

using namespace mission_encounter;

namespace {

inline Character* asCharacter(Role* role) { return static_cast<Character*>(role); }

// Calls `body(current)` for every accepted child until `stop` is set. The body's result is ANDed into `ok`.
template <typename Iterator, typename Body>
void walk(Iterator& iterator, const int& stop, Body body) {
    for (iterator.Begin(); iterator.Current() && stop == 0; iterator.Advance()) body(iterator.Current());
}

int skipsDecoyed(int skipDecoyed, GameObjNotifier* unit) {
    return skipDecoyed != 0 && (static_cast<Detector*>(unit)->m_detectorFlags & Detector::kIsDecoyedFlag) != 0;
}

}  // namespace

// ---- construction -------------------------------------------------------------------------------------------------------

EncounterContext::EncounterContext()
    : m_unk04(0),
      m_location(nullptr),
      m_mission(nullptr),
      m_valueA(0),
      m_valueB(0),
      m_flag18(0),
      m_flag1c(0),
      m_flag20(0),
      m_flag24(0),
      m_flag28(0),
      m_teamCount(0),
      m_decoyCount(0),
      m_unitCount(0),
      m_betrayerKey() {}

EncounterContext::~EncounterContext() { REB_KEEP_VPTR_STORE(); }

// ---- walkers over the units -----------------------------------------------------------------------------------------------

int EncounterContext::VisitUnits(UnitVisitor* visitor, NotifyContext* ctx) {
    return ForEachUnit(visitor, 1, 1, 1, 0, ctx);
}

int EncounterContext::VisitUndecoyedUnits(UnitVisitor* visitor, NotifyContext* ctx) {
    return ForEachUnit(visitor, 1, 1, 1, 1, ctx);
}

int EncounterContext::VisitUndecoyedCapShips(UnitVisitor* visitor, NotifyContext* ctx) {
    return ForEachUnit(visitor, 1, 0, 0, 1, ctx);
}

int EncounterContext::VisitUndecoyedSquadrons(UnitVisitor* visitor, NotifyContext* ctx) {
    return ForEachUnit(visitor, 0, 1, 0, 1, ctx);
}

int EncounterContext::ForEachUnit(UnitVisitor* visitor, int capShips, int squadrons, int troops, int skipDecoyed,
                                  NotifyContext* ctx) {
    int ok = 1;
    int stop = 0;
    if (m_flag18 != 0) {
        if (m_flag24 != 0 && squadrons != 0) {
            FightSquadIterator iterator(m_location, kStateUsable);
            walk(iterator, stop, [&](GameObjNotifier* current) {
                if (!skipsDecoyed(skipDecoyed, current))
                    ok = visitor->Visit(static_cast<Detector*>(current), nullptr, &stop, ctx) && ok;
            });
        }
        if (m_flag28 != 0 && troops != 0) {
            TroopRegIterator iterator(m_location, kStateUsable);
            walk(iterator, stop, [&](GameObjNotifier* current) {
                if (!skipsDecoyed(skipDecoyed, current))
                    ok = visitor->Visit(static_cast<Detector*>(current), nullptr, &stop, ctx) && ok;
            });
        }
    }
    if (m_flag1c != 0) {
        FleetIterator fleets(m_location, kStateUsable);
        for (fleets.Begin(); fleets.Current() && stop == 0; fleets.Advance()) {
            Fleet* fleet = static_cast<Fleet*>(fleets.Current());
            CapShipIterator capShipIterator(fleet, kStateUsable);
            for (capShipIterator.Begin(); capShipIterator.Current() && stop == 0; capShipIterator.Advance()) {
                GameObjNotifier* capShip = capShipIterator.Current();
                if (m_flag20 != 0 && capShips != 0 && !skipsDecoyed(skipDecoyed, capShip))
                    ok = visitor->Visit(static_cast<Detector*>(capShip), fleet, &stop, ctx) && ok;
                if (m_flag24 != 0 && squadrons != 0) {
                    FightSquadIterator squadrons2(capShip, kStateUsable);
                    walk(squadrons2, stop, [&](GameObjNotifier* current) {
                        if (!skipsDecoyed(skipDecoyed, current))
                            ok = visitor->Visit(static_cast<Detector*>(current), fleet, &stop, ctx) && ok;
                    });
                }
                if (m_flag28 != 0 && troops != 0) {
                    TroopRegIterator regiments(capShip, kStateUsable);
                    walk(regiments, stop, [&](GameObjNotifier* current) {
                        if (!skipsDecoyed(skipDecoyed, current))
                            ok = visitor->Visit(static_cast<Detector*>(current), fleet, &stop, ctx) && ok;
                    });
                }
            }
        }
    }
    return ok;
}

// ---- walkers over the persons ---------------------------------------------------------------------------------------------

int EncounterContext::VisitDecoys(MemberVisitor* visitor, NotifyContext* ctx) {
    return ForEachMember(visitor, 1, 1, 0, 1, 0, ctx);
}

int EncounterContext::VisitTeam(MemberVisitor* visitor, NotifyContext* ctx) {
    return ForEachMember(visitor, 1, 1, 1, 0, 1, ctx);
}

int EncounterContext::VisitTeamSpecialForces(MemberVisitor* visitor, NotifyContext* ctx) {
    return ForEachMember(visitor, 0, 1, 1, 0, 1, ctx);
}

int EncounterContext::VisitAllMembers(MemberVisitor* visitor, NotifyContext* ctx) {
    return ForEachMember(visitor, 1, 1, 1, 1, 1, ctx);
}

// Runs one member walk: GetMemberValue decides whether the visitor sees the person.
#define REB_WALK_MEMBERS(ITERATOR_TYPE)                                                              \
    {                                                                                                \
        ITERATOR_TYPE iterator(m_mission);                                                           \
        for (iterator.Begin(); iterator.Current() && stop == 0; iterator.Advance()) {                \
            Role* current = static_cast<Role*>(iterator.Current());                                  \
            int wanted = 0;                                                                          \
            ok = GetMemberValue(current, flag, &wanted) && ok;                                       \
            if (wanted != 0) ok = visitor->Visit(static_cast<Role*>(iterator.Current()), &stop, ctx) && ok; \
        }                                                                                            \
    }

int EncounterContext::ForEachMember(MemberVisitor* visitor, int characters, int specialForces, int team, int decoys,
                                    int flag, NotifyContext* ctx) {
    int ok = 1;
    int stop = 0;
    Mission* mission = static_cast<Mission*>(m_mission);
    if (specialForces != 0 && team != 0) {
        MissionSpecialForcesTeamIterator iterator(mission);
        for (iterator.Begin(); iterator.Current() && stop == 0; iterator.Advance()) {
            int wanted = 0;
            ok = GetMemberValue(static_cast<Role*>(iterator.Current()), flag, &wanted) && ok;
            if (wanted != 0) ok = visitor->Visit(static_cast<Role*>(iterator.Current()), &stop, ctx) && ok;
        }
    }
    if (ok != 0) {
        if (characters != 0 && team != 0) {
            MissionCharacterTeamIterator iterator(mission);
            for (iterator.Begin(); iterator.Current() && stop == 0; iterator.Advance()) {
                int wanted = 0;
                ok = GetMemberValue(static_cast<Role*>(iterator.Current()), flag, &wanted) && ok;
                if (wanted != 0) ok = visitor->Visit(static_cast<Role*>(iterator.Current()), &stop, ctx) && ok;
            }
        }
    }
    if (ok != 0 && specialForces != 0 && decoys != 0) {
        MissionSpecialForcesDecoyIterator iterator(mission);
        for (iterator.Begin(); iterator.Current() && stop == 0; iterator.Advance()) {
            int wanted = 0;
            ok = GetMemberValue(static_cast<Role*>(iterator.Current()), flag, &wanted) && ok;
            if (wanted != 0) ok = visitor->Visit(static_cast<Role*>(iterator.Current()), &stop, ctx) && ok;
        }
    }
    if (ok != 0 && characters != 0 && decoys != 0) {
        MissionCharacterDecoyIterator iterator(mission);
        for (iterator.Begin(); iterator.Current() && stop == 0; iterator.Advance()) {
            int wanted = 0;
            ok = GetMemberValue(static_cast<Role*>(iterator.Current()), flag, &wanted) && ok;
            if (wanted != 0) ok = visitor->Visit(static_cast<Role*>(iterator.Current()), &stop, ctx) && ok;
        }
    }
    return ok;
}

int EncounterContext::VisitAllMajorMinor(MemberVisitorA* visitor, NotifyContext* ctx) {
    return ForEachMajorMinor(visitor, 1, 1, 1, 1, 1, ctx);
}

int EncounterContext::VisitTeamMajorMinor(MemberVisitorA* visitor, NotifyContext* ctx) {
    return ForEachMajorMinor(visitor, 1, 1, 1, 0, 1, ctx);
}

int EncounterContext::VisitDecoyMajorMinor(MemberVisitorA* visitor, NotifyContext* ctx) {
    return ForEachMajorMinor(visitor, 1, 1, 0, 1, 1, ctx);
}

int EncounterContext::VisitMajors(MemberVisitorA* visitor, NotifyContext* ctx) {
    return ForEachMajorMinor(visitor, 1, 0, 1, 1, 1, ctx);
}

int EncounterContext::VisitMinors(MemberVisitorA* visitor, NotifyContext* ctx) {
    return ForEachMajorMinor(visitor, 0, 1, 1, 1, 1, ctx);
}

int EncounterContext::ForEachMajorMinor(MemberVisitorA* visitor, int majors, int minors, int team, int decoys, int flag,
                                        NotifyContext* ctx) {
    int ok = 1;
    int stop = 0;
    Mission* mission = static_cast<Mission*>(m_mission);
    if (majors != 0 && team != 0) {
        MissionMajorTeamIterator iterator(mission);
        for (iterator.Begin(); iterator.Current() && stop == 0; iterator.Advance()) {
            int wanted = 0;
            ok = GetMemberValue(static_cast<Role*>(iterator.Current()), flag, &wanted) && ok;
            if (wanted != 0) ok = visitor->Visit(static_cast<Role*>(iterator.Current()), &stop, ctx) && ok;
        }
    }
    if (ok != 0) {
        if (minors != 0 && team != 0) {
            MissionMinorTeamIterator iterator(mission);
            for (iterator.Begin(); iterator.Current() && stop == 0; iterator.Advance()) {
                int wanted = 0;
                ok = GetMemberValue(static_cast<Role*>(iterator.Current()), flag, &wanted) && ok;
                if (wanted != 0) ok = visitor->Visit(static_cast<Role*>(iterator.Current()), &stop, ctx) && ok;
            }
        }
    }
    if (ok != 0 && majors != 0 && decoys != 0) {
        MissionMajorDecoyIterator iterator(mission);
        for (iterator.Begin(); iterator.Current() && stop == 0; iterator.Advance()) {
            int wanted = 0;
            ok = GetMemberValue(static_cast<Role*>(iterator.Current()), flag, &wanted) && ok;
            if (wanted != 0) ok = visitor->Visit(static_cast<Role*>(iterator.Current()), &stop, ctx) && ok;
        }
    }
    if (ok != 0 && minors != 0 && decoys != 0) {
        MissionMinorDecoyIterator iterator(mission);
        for (iterator.Begin(); iterator.Current() && stop == 0; iterator.Advance()) {
            int wanted = 0;
            ok = GetMemberValue(static_cast<Role*>(iterator.Current()), flag, &wanted) && ok;
            if (wanted != 0) ok = visitor->Visit(static_cast<Role*>(iterator.Current()), &stop, ctx) && ok;
        }
    }
    return ok;
}

int EncounterContext::GetMemberValue(Role* member, int flag, int* out) {
    *out = 0;
    int ok = 1;
    if ((member->m_flags78 & (role_flag::kMissionRemoveRequest | role_flag::kMissionResignRequest)) == 0 &&
        (flag == 0 || (member->m_flags78 & role_flag::kIsDecoying) == 0)) {
        GameObjNotifier* parent = member->m_parent;
        ok = parent != nullptr;
        if (parent) {
            ObjectKey ownerKey;
            ObjectKey parentKey;
            const ObjectKey* owner = GetOwnerKeyValue(parent, &ownerKey);
            parent->GetKey(&parentKey);
            *out = parentKey.m_value == owner->m_value ? m_valueA : m_valueB;
        }
    }
    return ok;
}

// ---- counts and picks -----------------------------------------------------------------------------------------------------

int EncounterContext::CountUnits(int* out, NotifyContext* ctx) {
    UnitCounter counter(this);
    const int ok = VisitUndecoyedUnits(&counter, ctx);
    *out = counter.m_count;
    return ok != 0;
}

int EncounterContext::CountTeam(int* out, NotifyContext* ctx) {
    MemberCounter counter(this);
    const int ok = VisitTeam(&counter, ctx);
    *out = counter.m_count;
    return ok != 0;
}

int EncounterContext::CountDecoys(int* out, NotifyContext* ctx) {
    MemberCounter counter(this);
    const int ok = VisitDecoys(&counter, ctx);
    *out = counter.m_count;
    return ok != 0;
}

int EncounterContext::CountAll(NotifyContext* ctx) {
    const int unitsOk = CountUnits(&m_unitCount, ctx);
    const int teamOk = CountTeam(&m_teamCount, ctx);
    const int allOk = teamOk && unitsOk;
    const int decoysOk = CountDecoys(&m_decoyCount, ctx);
    return decoysOk && allOk;
}

int EncounterContext::PickRandomUnit(Detector** unit, Fleet** container, NotifyContext* ctx) {
    *unit = nullptr;
    *container = nullptr;
    int ok = 1;
    if (m_unitCount != 0) {
        UnitPicker picker(this);
        picker.m_target = GameRandom(m_unitCount - 1);
        ok = VisitUndecoyedUnits(&picker, ctx) != 0;
        *unit = picker.m_result;
        *container = picker.m_resultFleet;
    }
    return ok;
}

int EncounterContext::PickRandomDecoy(Role** decoy, NotifyContext* ctx) {
    *decoy = nullptr;
    int ok = 1;
    if (m_decoyCount != 0) {
        MemberPicker picker(this);
        picker.m_target = GameRandom(m_decoyCount - 1);
        ok = VisitDecoys(&picker, ctx) != 0;
        *decoy = picker.m_result;
    }
    return ok;
}

int EncounterContext::AverageTeamEspionage(int* out, NotifyContext* ctx) {
    *out = 0;
    int ok = m_teamCount != 0;
    if (m_teamCount != 0) {
        MemberEspionageSum sum(this);
        ok = VisitTeam(&sum, ctx) && ok;
        *out = sum.m_sum / m_teamCount;
    }
    return ok;
}

// ---- rolls ----------------------------------------------------------------------------------------------------------------

int EncounterContext::IsForeignDetector(Role* member, int* out) {
    Character* character = asCharacter(member);
    if (IsAtLeastGnprtb0E02(character->m_force) && (character->m_flagsAc & 1) == 0 &&
        ((member->m_flags ^ m_mission->m_flags) & 0xc0) != 0) {
        *out = 1;
        return 1;
    }
    *out = 0;
    return 1;
}

int EncounterContext::IsBetrayalCandidate(Role* member, int* out) {
    Character* character = asCharacter(member);
    if (IsAtLeastGnprtb0E01(character->m_force) &&
        (member->m_flags78 & (role_flag::kMissionRemoveRequest | role_flag::kMissionResignRequest)) == 0 &&
        (character->m_flagsAc & 1) == 0) {
        *out = 1;
        return 1;
    }
    *out = 0;
    return 1;
}

int EncounterContext::RollFoil(Detector* unit, Fleet* container, int skill, int count, int* foiled, NotifyContext* ctx) {
    *foiled = 0;
    GameObjNotifier* found = nullptr;
    int ok;
    if (container != nullptr)
        ok = container->FindShipByNumber(unit->Virtual111(), &found);
    else
        ok = static_cast<System*>(m_location)->FindCharacterByCommandKind(unit->Virtual111(), &found);
    int foundEspionage = 0;
    if (found) foundEspionage = static_cast<Role*>(found)->GetEspionage();
    int chance = 0;
    const uint32_t side = unit->ControlSide();
    RollFoilTable(skill, foundEspionage, side, unit->GetRecordField5c(), count, container != nullptr, foiled, &chance);
    return ok != 0;
}

int EncounterContext::TryDecoy(Role* member, Detector* unit, Fleet* container, NotifyContext* ctx) {
    GameObjNotifier* found = nullptr;
    int ok;
    if (container != nullptr)
        ok = container->FindShipByNumber(unit->Virtual111(), &found);
    else
        ok = static_cast<System*>(m_location)->FindCharacterByCommandKind(unit->Virtual111(), &found);
    ok = ok != 0;
    int foundEspionage = 0;
    if (found) foundEspionage = static_cast<Role*>(found)->GetEspionage();
    int decoyed = 0;
    int chance = 0;
    const uint32_t side = member->ControlSide();
    RollDecoyTable(side, member->GetEspionage(), foundEspionage, unit->GetRecordField5c(), container != nullptr, &decoyed,
                   &chance);
    if (decoyed != 0) {
        ok = member->SetIsDecoying(1, ctx) && ok;
        const int unitOk = unit->SetIsDecoyed(1, ctx);
        --m_unitCount;
        return unitOk && ok;
    }
    return ResolveFailedDecoy(unit, container, member, ctx) && ok;
}

int EncounterContext::ResolveFailedDecoy(Detector* unit, Fleet* container, Role* member, NotifyContext* ctx) {
    GameObjNotifier* found = nullptr;
    int ok;
    if (container != nullptr)
        ok = container->FindShipByNumber(unit->Virtual111(), &found);
    else
        ok = static_cast<System*>(m_location)->FindCharacterByCommandKind(unit->Virtual111(), &found);
    ok = ok != 0;
    ObjectKey opponent;
    int combat = 0;
    ObjectKey key;
    if (found) {
        combat = static_cast<Role*>(found)->GetCombat();
        found->GetKey(&key);
    } else {
        unit->GetKey(&key);
    }
    opponent = key;
    ok = member->RollCombatCheck(&opponent, combat, ctx) && ok;
    if ((member->m_flags78 & role_flag::kMissionRemoveRequest) == 0 &&
        (member->m_flags78 & role_flag::kCanResignFromMission) != 0) {
        ObjectKey missionKey;
        if (member->GetMissionKey(&missionKey)->IsSet()) ok = member->SetMissionResignRequest(1, ctx) && ok;
    }
    if ((member->m_flags78 & (role_flag::kMissionRemoveRequest | role_flag::kMissionResignRequest)) != 0) {
        if ((member->m_flags78 & role_flag::kDecoy) != 0)
            --m_decoyCount;
        else
            --m_teamCount;
    }
    return ok;
}

int EncounterContext::ResolveEncounterFight(Role* member, Role* other, NotifyContext* ctx) {
    CharacterMgr* manager = GetCharacterManager();
    int ok = manager != nullptr;
    if (manager) ok = manager->ResolveEncounterFight(static_cast<Character*>(member), static_cast<Character*>(other), ctx) && ok;
    if ((member->m_flags78 & (role_flag::kMissionRemoveRequest | role_flag::kMissionResignRequest)) != 0) {
        if ((member->m_flags78 & role_flag::kDecoy) != 0)
            --m_decoyCount;
        else
            --m_teamCount;
    }
    return ok;
}

int EncounterContext::RollNotBetray(Role* member, int* betrays, NotifyContext* ctx) {
    *betrays = 0;
    const uint32_t side = member->ControlSide();
    int chance = 0;
    const int loyalty = member->GetLoyalty();
    const int ok = RollComplementChance(side, loyalty, betrays, &chance);
    if (*betrays != 0) member->GetKey(&m_betrayerKey);
    return ok != 0;
}

int EncounterContext::RollIdentifyTraitor(Role* member, int* identified, NotifyContext* ctx) {
    int ok = 1;
    *identified = 0;
    ObjectKey memberKey;
    member->GetKey(&memberKey);
    if (m_betrayerKey.m_value != memberKey.m_value) {
        int chance = 0;
        ok = RollDirectChance(member->ControlSide(), asCharacter(member)->m_force, identified, &chance) != 0;
    }
    if (*identified != 0) {
        Character* character = asCharacter(member);
        ObjectKey unset;
        ok = character->SetTraitorDiscoveredKey(&unset, ctx) && ok;
        ok = character->SetTraitorDiscoveredKey(&m_betrayerKey, ctx) && ok;
    }
    return ok;
}
