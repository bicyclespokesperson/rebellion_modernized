#include <new>

#include "mission/mission_exec_decl.h"
#include "mission/mission_rules.h"

using namespace mission_exec;

// What happens to the persons of a mission that ended or lost its members (0x548910-0x549850), the story pipeline
// (0x549910-0x549a20) and the factory of the manager (0x549af0).

namespace {

constexpr uint32_t kHomeAllianceKey = 0x98000440;
constexpr uint32_t kHomeEmpireKey = 0x98000480;
constexpr int kInjuryReasonMission = 10;
constexpr uint32_t kSideBitsMask = 0xc0;
constexpr uint32_t kSideBitsAlliance = 0x40;

Role* personOf(const ObjectKey* key) { return static_cast<Role*>(GetMasterInFamilies_30_40(key)); }

ObjectKey keyOf(uint32_t value) {
    ObjectKey key;
    key.m_value = value;
    return key;
}

int contextAsInt(NotifyContext* ctx) { return static_cast<int>(reinterpret_cast<intptr_t>(ctx)); }

void collectAdrift(GameObj* parent, uint32_t stateFilter, ObjectKeyValueSet* out) {
    RoleIterator roles(parent, stateFilter);
    for (roles.Begin(); roles.Current(); roles.Advance()) {
        Role* person = static_cast<Role*>(roles.Current());
        if ((person->m_flags78 >> 6) & 1) {
            ObjectKey key;
            out->AddKeyValue(person->GetKey(&key), 0);
        }
    }
}

}  // namespace

int MissionMgr::ReassignMissionMembers(Mission* mission, int* moved, NotifyContext* ctx) {
    *moved = 0;
    MissionPhaseLevels levels;
    int ok = mission->ComputePhaseLevels(&levels) != 0;
    const uint32_t side = mission->ControlSide();
    const int memberCount = mission->CountMembers();
    bool haveFirstKey = false;
    ObjectKey firstKey;
    ObjectKey currentKey;
    ObjectKeyValueSet movers;
    int anyInMission = 0;
    {
        MissionPersonIterator members(mission);
        for (members.Begin(); members.Current(); members.Advance()) {
            Role* person = static_cast<Role*>(members.Current());
            int resigning = 0;
            int stays = 0;
            int flagC = 0;
            ok = mission->GetMemberTransitFlags(person, &resigning, &stays, &flagC) && ok;
            if (!(resigning != 0 && stays != 0 && !(person->m_flags78 & role_flag::kMissionRemoveRequest))) continue;

            ok = FindPersonMissionPoint(person, mission, flagC, &currentKey) && ok;
            if (!haveFirstKey) {
                haveFirstKey = true;
                firstKey = currentKey;
            }
            if (firstKey.m_value != currentKey.m_value) continue;

            ObjectKey personKey;
            movers.AddKeyValue(person->GetKey(&personKey), 0);
            const bool inTeamOrDecoys = mission->IsInTeam(person->GetKey(&personKey)) != 0 ||
                                        mission->IsInDecoys(person->GetKey(&personKey)) != 0;
            if (!inTeamOrDecoys) continue;
            anyInMission = 1;
            if (firstKey.IsSet() && mission->HasCompletionStatus()) {
                ObjectKeyValueSet set4(mission->m_finishedMembers);
                set4.AddKeyValue(person->GetKey(&personKey), 0);
                mission->SetFinishedMembers(&set4, ctx);
            }
        }
    }
    if (movers.Count() != 0) {
        *moved = 1;
        for (ObjectKeyNode* node = movers.First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
            ObjectKey nodeKey;
            node->GetKey(&nodeKey);
            ok = mission->RemoveMember(&nodeKey, anyInMission, ctx) && ok;
        }
        if (anyInMission == 0) {
            ok = ReleaseFreePersons(&movers, ctx) && ok;
        } else {
            int step;
            if (!firstKey.IsSet()) {
                step = ReleasePersons(&movers, ctx);
            } else {
                step = CreateMissionType42(static_cast<int>(side), &levels, &firstKey, &movers,
                                           (mission->m_flagsA4 >> 2) & 1, ctx);
            }
            ok = step && ok;
        }
    }
    int result = 0;
    if (ok) result = !(mission->CountMembers() == memberCount && *moved != 0);
    return result;
}

int MissionMgr::ReleasePersonsOfKey(ObjectKey* key, NotifyContext* ctx) {
    Role* person = personOf(key);
    int ok = person != nullptr;
    if (person && ((person->m_stateFlags & kStateAutorouting) || (person->m_flags78 & kPersonFlagAdrift))) {
        ObjectKey owner;
        const int got = person->GetOwnerKey(&owner) && ok;
        ok = 0;
        if (got) ok = owner.IsSet();
        if (owner.OrNull()) ok = ReleaseAdriftPersonsOfSystem(&owner, ctx) && ok;
    }
    return ok;
}

int MissionMgr::ReleaseAdriftPersonsOfSystem(ObjectKey* systemKey, NotifyContext* ctx) {
    GameObj* system = static_cast<GameObj*>(GetMasterInFamilies_01_ff(systemKey));
    ObjectKeyValueSet adrift;
    if (system) {
        collectAdrift(system, 3, &adrift);
        FleetIterator fleets(system, 4);
        for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
            CapShipIterator ships(fleets.Current(), 4);
            for (ships.Begin(); ships.Current(); ships.Advance()) collectAdrift(static_cast<GameObj*>(ships.Current()), 3, &adrift);
        }
    }
    int ok = system != nullptr;
    for (ObjectKeyNode* node = adrift.First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
        ObjectKey nodeKey;
        node->GetKey(&nodeKey);
        Role* person = personOf(&nodeKey);
        if (person) ok = person->Reparent(systemKey, ctx) && ok;
    }
    if (system) {
        const bool battlePending = InFamilies(system->GetFamily(), kFamilySystemBegin, kFamilySystemEnd) &&
                                   (static_cast<System*>(system)->m_flags & system_flag::kBattlePending);
        if (!battlePending) {
            int moved = 0;
            do {
                ok = ReleaseSystemPersons(system, &moved, ctx) && ok;
            } while (moved != 0);
        }
    }
    return ok;
}

int MissionMgr::ReleaseSystemPersons(GameObj* system, int* moved, NotifyContext* ctx) {
    *moved = 0;
    int unusedA = 0;
    uint32_t side = 0;
    uint32_t flag = 0;
    ObjectKey key;
    ObjectKeyValueSet persons;
    int ok = CollectStrandedPersons(system, &unusedA, &side, &flag, &key, &persons, ctx) != 0;
    {
        FleetIterator fleets(system, 4);
        for (fleets.Begin(); fleets.Current(); fleets.Advance()) {
            CapShipIterator ships(fleets.Current(), 4);
            for (ships.Begin(); ships.Current(); ships.Advance())
                ok = CollectStrandedPersons(static_cast<GameObj*>(ships.Current()), &unusedA, &side, &flag, &key, &persons, ctx) && ok;
        }
    }
    *moved = persons.Count() != 0;
    if (*moved) {
        if (!key.OrNull()) {
            ok = ReleasePersons(&persons, ctx) && ok;
        } else {
            int hasFreePerson = 0;
            for (ObjectKeyNode* node = persons.First(); node && !hasFreePerson;
                 node = static_cast<ObjectKeyNode*>(node->m_successor)) {
                ObjectKey nodeKey;
                node->GetKey(&nodeKey);
                if (InFamilies(&nodeKey, kFamilyPersonBegin, kFamilyPersonEnd)) {
                    Role* person = personOf(&nodeKey);
                    if (person && person->Virtual117() == 0) hasFreePerson = 1;
                }
            }
            int step;
            if (!hasFreePerson) {
                step = ReleaseFreePersons(&persons, ctx);
            } else if (flag == 0) {
                step = CreateMissionType43(static_cast<int>(side), &key, &persons, 0, ctx);
            } else {
                step = CreateMissionType44(static_cast<int>(side), &key, &persons, ctx);
            }
            ok = step && ok;
        }
    }
    return ok;
}

int MissionMgr::ReleasePersons(ObjectKeyValueSet* persons, NotifyContext* ctx) {
    int ok = 1;
    for (ObjectKeyNode* node = persons->First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
        ObjectKey nodeKey;
        node->GetKey(&nodeKey);
        Character* person = static_cast<Character*>(GetMasterInFamilies_30_40(&nodeKey));
        ok = person != nullptr && ok;
        if (!person) continue;

        ObjectKeyValueSet single;
        ObjectKey personKey;
        single.AddKeyValue(person->GetKey(&personKey), 0);
        if (!(person->m_stateFlags & kStateDestroyed)) {
            if (person->Virtual117() == 0) {
                ObjectKey noHolder;
                ok = person->Virtual131(&noHolder, ctx) && ok;
            } else {
                ok = person->Virtual134(ctx) && ok;
            }
        }
        ObjectKey location;
        if (!(person->m_stateFlags & kStateDestroyed)) {
            ok = FindPersonDestination(person, 0, &location) && ok;
            if (!location.OrNull()) {
                if (!(person->m_stateFlags & kStateDestroyed) && person->Virtual117() == 0) {
                    ObjectKey noHolder;
                    ok = person->Virtual131(&noHolder, ctx) && ok;
                }
                location = keyOf((person->m_flags & kSideBitsMask) == kSideBitsAlliance ? kHomeAllianceKey : kHomeEmpireKey);
            }
        }
        ok = person->SetAutorouting(0, ctx) && ok;
        if (!(person->m_stateFlags & kStateDestroyed)) {
            const int implied = person->Virtual117();
            ok = CreateMissionType43(static_cast<int>(person->ControlSide()), &location, &single, implied, ctx) && ok;
        }
    }
    return ok;
}

int MissionMgr::ReleaseFreePersons(ObjectKeyValueSet* persons, NotifyContext* ctx) {
    int ok = 1;
    for (ObjectKeyNode* node = persons->First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
        ObjectKey nodeKey;
        node->GetKey(&nodeKey);
        Character* person = static_cast<Character*>(GetMasterInFamilies_30_40(&nodeKey));
        if (person && person->Virtual117() != 0 && !(person->m_stateFlags & kStateDestroyed))
            ok = person->Virtual134(ctx) && ok;
    }
    return ok;
}

int MissionMgr::RunStoryPipeline(NotifyContext* ctx) {
    int ok = UpdateHanCapturedAtPalace(ctx) != 0;
    ok = UpdateFinalBattleReadyStep(ctx) && ok;
    ok = UpdateLukeDagobahStep(ctx) && ok;
    ok = UpdateLukePalaceStep(ctx) && ok;
    ok = UpdateLeiaPalaceStep(ctx) && ok;
    ok = UpdateChewbaccaPalaceStep(ctx) && ok;
    ok = UpdateDarthPickupStep(ctx) && ok;
    ok = UpdateDarthToLukeStep(ctx) && ok;
    ok = UpdateDarthToEmperorStep(ctx) && ok;
    ok = UpdateFinalBattleStep(ctx) && ok;
    return ok;
}

int MissionMgr::InjureStoryCharacter(NotifyContext* ctx) {
    ObjectKey parentKey;
    GameObjNotifier* story = nullptr;
    GameObjNotifier* parent = nullptr;
    int ok = FindLuke(&story, &parent, &parentKey);
    int amount = 0;
    ok = RollMissionRuleBonus1809(&amount) && ok;
    if (story) ok = static_cast<Character*>(story)->Virtual188(amount, kInjuryReasonMission, ctx) && ok;
    return ok;
}

