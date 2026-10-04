#include "mission/mission_encounter.h"
#include "mission/mission_exec_decl.h"

using namespace mission_exec;

// MissionMgr reactions to object events (0x545180-0x546f80). Each handler re-evaluates the missions that concern the
// object named by `key` (UpdatePhaseForTeam on the missions that target it or use it as objective) and then runs the
// story pipeline when the key is one of the named story characters.

namespace {

Role* personOf(const ObjectKey* key) { return static_cast<Role*>(GetMasterInFamilies_30_40(key)); }
Mission* missionOf(const ObjectKey* key) { return static_cast<Mission*>(GetMasterInFamilies_40_80(key)); }

bool isUnitOrSystemFamily(const ObjectKey* key) {
    return InFamilies(key, kFamilySystemBegin, kFamilySystemOrNextEnd) || InFamilies(key, 0x10, 0x40);
}

}  // namespace

int MissionMgr::OnCharacterEvent(ObjectKey* key, int unused, NotifyContext* ctx) {
    int ok = 1;
    if (InFamilies(key, kFamilyPersonBegin, kFamilyPersonEnd)) {
        Character* person = static_cast<Character*>(GetMasterInFamilies_30_40(key));
        ok = person != nullptr;
        if (person) {
            CreateMissionType41ForCharacter(person, ctx);
            GameObjNotifier* rules = GetGameRootMember_bc();
            ok = rules != nullptr;
            if (rules && !(person->m_stateFlags & kStateEnrouteActive)) ok = FUN_00556710(rules, person, ctx) && ok;
        }
    }
    return ok;
}

int MissionMgr::UpdateMissionsForObject(ObjectKey* key, int unused, ObjectKey* location, int skipMissions,
                                        NotifyContext* ctx) {
    int ok = 1;
    if (FUN_005438a0(key)) ok = RunStoryPipeline(ctx) != 0;
    if (skipMissions != 0) return ok;
    if (isUnitOrSystemFamily(key)) {
        MissionIterator missions(this);
        for (missions.Begin(); missions.Current(); missions.Advance()) {
            Mission* mission = static_cast<Mission*>(missions.Current());
            if (mission->TargetKeyIs(key) || mission->ObjectiveKeyIs(key)) ok = mission->UpdatePhaseForTeam(ctx) && ok;
            if (mission->TargetKeyIs(key)) {
                if (!mission->HasCompletionStatus() && mission->IsDataField1cAbove6())
                    ok = mission->CompleteWith(7, ctx) && ok;
            }
        }
    }
    if (InFamilies(key, 0x28, 0x2c)) {
        MissionIteratorFam53 missions(this);
        for (missions.get()->Begin(); missions.get()->Current(); missions.get()->Advance()) {
            Mission* mission = missions.Current();
            if (mission->TargetLocationKeyIs(location)) ok = mission->UpdatePhaseForTeam(ctx) && ok;
        }
    }
    return ok;
}

int MissionMgr::UpdateMissionsForObjective(ObjectKey* key, int unused, int unused2, int skipMissions,
                                           NotifyContext* ctx) {
    int ok = 1;
    if (FUN_005438a0(key)) ok = RunStoryPipeline(ctx) != 0;
    if (skipMissions == 0 && InFamilies(key, 0x10, 0x40)) {
        MissionIterator missions(this);
        for (missions.Begin(); missions.Current(); missions.Advance()) {
            Mission* mission = static_cast<Mission*>(missions.Current());
            if (mission->ObjectiveKeyIs(key)) ok = mission->UpdatePhaseForTeam(ctx) && ok;
        }
    }
    return ok;
}

int MissionMgr::UpdateMissionsForTargetOrObjective(ObjectKey* key, int unused, int unused2, NotifyContext* ctx) {
    int ok = 1;
    if (isUnitOrSystemFamily(key)) {
        MissionIterator missions(this);
        for (missions.Begin(); missions.Current(); missions.Advance()) {
            Mission* mission = static_cast<Mission*>(missions.Current());
            if (mission->TargetKeyIs(key) || mission->ObjectiveKeyIs(key)) ok = mission->UpdatePhaseForTeam(ctx) && ok;
        }
    }
    return ok;
}

int MissionMgr::RunStoryPipelineForStoryKey(ObjectKey* key, int unused, int unused2, NotifyContext* ctx) {
    int result = 1;
    if (FUN_005438a0(key)) result = RunStoryPipeline(ctx);
    return result;
}

int MissionMgr::UpdatePersonMission(ObjectKey* key, int unused, NotifyContext* ctx) {
    int result = 1;
    if (InFamilies(key, kFamilyPersonBegin, kFamilyPersonEnd) && unused == 0) {
        Role* person = personOf(key);
        ObjectKey missionKey;
        int found = 0;
        if (person) {
            ObjectKey scratch;
            missionKey = *person->GetMissionKey(&scratch);
            found = missionKey.OrNull() != nullptr;
        }
        Mission* mission = nullptr;
        if (missionKey.OrNull()) {
            mission = missionOf(&missionKey);
            found = mission != nullptr;
        }
        result = found;
        if (mission) result = mission->UpdateTeamAndTarget(ctx);
    }
    return result;
}

int MissionMgr::OnSystemEvent(ObjectKey* key, int unused, NotifyContext* ctx) {
    int ok = 1;
    if (InFamilies(key, kFamilySystemBegin, kFamilySystemEnd) && unused == 0) ok = ReleaseAdriftPersonsOfSystem(key, ctx) != 0;
    return ok;
}

int MissionMgr::OnSystemEventNoop(int a, int b, int c) { return 1; }

int MissionMgr::UpdateMissionsAtSystemA(ObjectKey* key, int enabled, NotifyContext* ctx) {
    int ok = 1;
    if (InFamilies(key, kFamilySystemBegin, kFamilySystemEnd) && enabled != 0) {
        MissionIteratorFam58 missions(this);
        for (missions.get()->Begin(); missions.get()->Current(); missions.get()->Advance()) {
            Mission* mission = missions.Current();
            if (mission->TargetLocationKeyIs(key)) ok = FUN_00570470(mission, ctx) && ok;
        }
    }
    return ok;
}

int MissionMgr::UpdateMissionsAtSystemB(ObjectKey* key, int enabled, NotifyContext* ctx) {
    int ok = 1;
    if (InFamilies(key, kFamilySystemBegin, kFamilySystemEnd) && enabled != 0) {
        MissionIteratorFam58 missions(this);
        for (missions.get()->Begin(); missions.get()->Current(); missions.get()->Advance()) {
            Mission* mission = missions.Current();
            if (mission->TargetLocationKeyIs(key)) ok = FUN_005704b0(mission, ctx) && ok;
        }
    }
    return ok;
}

int MissionMgr::UpdateMissionsAtLocation53(ObjectKey* key, int unused, NotifyContext* ctx) {
    int ok = 1;
    if (InFamilies(key, kFamilySystemBegin, kFamilySystemEnd)) {
        MissionIteratorFam53 missions(this);
        for (missions.get()->Begin(); missions.get()->Current(); missions.get()->Advance()) {
            Mission* mission = missions.Current();
            if (mission->TargetLocationKeyIs(key)) ok = mission->UpdatePhaseForTeam(ctx) && ok;
        }
    }
    return ok;
}

int MissionMgr::UpdateMissionsAtLocation50to60(ObjectKey* key, int unused, NotifyContext* ctx) {
    int ok = 1;
    MissionIteratorFam50to60 missions(this);
    for (missions.get()->Begin(); missions.get()->Current(); missions.get()->Advance()) {
        Mission* mission = missions.Current();
        if (mission->TargetLocationKeyIs(key)) ok = mission->UpdatePhaseForTeam(ctx) && ok;
    }
    return ok;
}

int MissionMgr::OnPersonStateChanged(ObjectKey* key, int released, NotifyContext* ctx) {
    int ok = 1;
    if (InFamilies(key, kFamilyPersonBegin, kFamilyPersonEnd)) {
        if (released != 0) ok = ReleasePersonsOfKey(key, ctx) != 0;
        if (FUN_005438a0(key)) ok = RunStoryPipeline(ctx) && ok;
    }
    return ok;
}

int MissionMgr::OnPersonRemoved(ObjectKey* key, int unused, NotifyContext* ctx) {
    int ok = 1;
    if (InFamilies(key, kFamilyPersonBegin, kFamilyPersonEnd)) {
        Role* person = personOf(key);
        ok = person != nullptr;
        if (person) {
            GameObjNotifier* rules = GetGameRootMember_bc();
            ok = rules != nullptr;
            if (rules && !(person->m_flags78 & role_flag::kOnMission)) {
                ok = FUN_00556710(rules, person, ctx) && ok;
                if (person->m_stateFlags & kStateExisting) {
                    ObjectKey recordKey;
                    FUN_00402d80(person, &recordKey);
                    if (recordKey.m_value == 0x3c000009) ok = person->SetDestroyedReason(0x14, ctx) && ok;
                }
            }
        }
        if (FUN_005438a0(key)) ok = RunStoryPipeline(ctx) && ok;
    }
    return ok;
}

int MissionMgr::ProcessResignRequest(ObjectKey* key, int enabled, NotifyContext* ctx) {
    int ok = 1;
    if (enabled == 0) return ok;
    Role* person = personOf(key);
    ok = person != nullptr;
    if (!person) return ok;
    ObjectKey missionKey;
    if (person->GetMissionKey(&missionKey)->IsSet() && (person->m_flags78 & role_flag::kMissionResignRequest)) {
        Mission* mission = missionOf(person->GetMissionKey(&missionKey));
        ok = ok && mission != nullptr;
        if (mission) {
            int resigning = 0;
            int unusedB = 0;
            int unusedC = 0;
            bool someoneStays = false;
            MissionPersonTeamIterator team(mission);
            team.Begin();
            while (team.Current()) {
                ok = FUN_00521220(mission, static_cast<Role*>(team.Current()), &resigning, &unusedB, &unusedC) && ok;
                if (resigning == 0) someoneStays = true;
                team.Advance();
            }
            if (!someoneStays) {
                MissionPersonCapturedIterator captured(mission);
                captured.Begin();
                while (captured.Current()) {
                    Role* member = static_cast<Role*>(captured.Current());
                    if (member->m_flags78 & role_flag::kCanResignFromMission) ok = member->SetMissionResignRequest(1, ctx) && ok;
                    captured.Advance();
                }
            }
            ok = DetachMissionMembers(mission, ctx) && ok;
        }
    }
    return ok;
}

int MissionMgr::ProcessRemoveRequest(ObjectKey* key, int enabled, NotifyContext* ctx) {
    int ok = 1;
    if (enabled == 0) return ok;
    Role* person = personOf(key);
    ok = person != nullptr;
    if (!person) return ok;
    ObjectKey missionKey;
    if (person->GetMissionKey(&missionKey)->IsSet() && (person->m_stateFlags & kStateDestroyed) &&
        (person->m_flags78 & role_flag::kMissionRemoveRequest)) {
        Mission* mission = missionOf(person->GetMissionKey(&missionKey));
        ok = mission != nullptr;
        if (mission) {
            ObjectKey personKey;
            mission->RemoveMember(person->GetKey(&personKey), 0, ctx);
            ok = mission->UpdatePhaseForTeam(ctx);
        }
    }
    return ok;
}

int MissionMgr::RunStoryPipelineForStoryKey2(ObjectKey* key, int unused, NotifyContext* ctx) {
    int ok = 1;
    if (FUN_005438a0(key)) ok = RunStoryPipeline(ctx) != 0;
    return ok;
}

int MissionMgr::AdvanceMissionPhase(ObjectKey* missionKey, int enabled, NotifyContext* ctx) {
    Mission* mission = missionOf(missionKey);
    int result = mission != nullptr;
    if (mission && enabled != 0) result = mission->AdvancePhase(ctx);
    return result;
}

int MissionMgr::RunMissionPhase(ObjectKey* missionKey, int fromPhase, int toPhase, NotifyContext* ctx) {
    Mission* mission = missionOf(missionKey);
    int ok = mission != nullptr;
    if (mission) {
        int step = ReleaseObjectiveSystem(mission, ctx) && ok;
        step = DetachMissionMembers(mission, ctx) && step;
        if (toPhase == 2) step = ApplyMissionOutcomeModifiers(mission, ctx) && step;
        step = UpdateSystemMissionScores(mission, fromPhase, toPhase, ctx) && step;
        ok = OnMissionPhaseStory(mission, fromPhase, toPhase, ctx) && step;
        if (mission) ok = mission->ReadyUnlessPhase4Or8(ctx);
    }
    return ok;
}

int MissionMgr::UpdateStoryFlags(NotifyContext* ctx) { return FUN_00543af0(this, 1, ctx) != 0; }

// ---- OnPersonArrived -------------------------------------------------------------------------------------------

namespace {

constexpr uint32_t kFamilyMissionOfObjective = 0x62;

bool isMemberOfMissionList(Mission* mission, Character* person) {
    ObjectKey key;
    if (person->m_flagsAc & mission_exec::kCharacterCaptured) return mission->IsInCaptured(person->GetKey(&key)) != 0;
    if (mission->IsInTeam(person->GetKey(&key))) return true;
    return mission->IsInDecoys(person->GetKey(&key)) != 0;
}

// The object a person was held by, if it still exists and has an owner.
GameObj* livingHolder(const ObjectKey* holderKey, int* ok) {
    GameObj* holder = static_cast<GameObj*>(GetMasterInFamilies_01_ff(holderKey));
    *ok = holder != nullptr && *ok;
    if (holder && (holder->m_stateFlags & kStateDestroyed)) holder = nullptr;
    if (holder) {
        ObjectKey owner;
        holder->GetOwnerKey(&owner);
        if (!owner.IsSet()) holder = nullptr;
    }
    return holder;
}

}  // namespace

int MissionMgr::OnPersonArrived(ObjectKey* key, int unused, int expectedState, NotifyContext* ctx) {
    Character* person = static_cast<Character*>(GetMasterInFamilies_30_3c(key));
    GameObjNotifier* rules = nullptr;
    int ok = 0;
    if (person) {
        rules = GetGameRootMember_bc();
        ok = rules != nullptr;
    }
    if (FUN_005438a0(key)) ok = RunStoryPipeline(ctx) && ok;
    if (!person || !rules) return ok;

    const bool arrived = !(person->m_stateFlags & kStateDestroyed) && person->m_state == expectedState &&
                         (person->m_stateFlags & kStateAutorouting);
    if (arrived) {
        ObjectKey holderKey;
        person->GetHolderKey(&holderKey);
        const int stopped = person->SetAutorouting(0, ctx) && ok;
        ObjectKey noHolder;
        ok = person->SetHolderKey(&noHolder) && stopped;

        ObjectKey personMissionKey;
        ObjectKey scratch;
        personMissionKey = *person->GetMissionKey(&scratch);
        Mission* personMission = nullptr;
        if (personMissionKey.OrNull()) {
            personMission = missionOf(&personMissionKey);
            ok = personMission != nullptr;
        }
        bool needsTransfer = true;
        if (personMission) needsTransfer = !isMemberOfMissionList(personMission, person);

        if (needsTransfer) {
            GameObj* holder = nullptr;
            if (holderKey.OrNull()) holder = livingHolder(&holderKey, &ok);

            Mission* holderMission = nullptr;
            ObjectKey holderMissionKey;
            ObjectKey holderCompletionParent;
            if (holder) {
                Role* holderPerson = static_cast<Role*>(holder);
                if (InFamilies(holder->GetFamily(), kFamilyPersonBegin, kFamilyPersonEnd) && holderPerson->Virtual117() == 0) {
                    ObjectKey holderMissionKeyCopy;
                    if (holderPerson->GetMissionKey(&holderMissionKeyCopy)->IsSet()) {
                        holderMission = missionOf(holderPerson->GetMissionKey(&holderMissionKeyCopy));
                        ok = holderMission != nullptr && ok;
                        if (holderMission) {
                            holderMissionKey = *holderPerson->GetMissionKey(&holderMissionKeyCopy);
                            holderCompletionParent = *holderPerson->GetParentAtMissionCompletionKey(&holderMissionKeyCopy);
                        }
                    }
                }
            }

            ObjectKey parentKey;
            ok = person->GetValidParentKey(&parentKey) && ok;
            ObjectKey holderParentKey;
            if (holder) {
                const uint32_t family = holder->GetFamily();
                const bool hasValidParent = InFamilies(family, kFamilyPersonBegin, kFamilyPersonEnd) ||
                                            InFamilies(family, 0x1c, 0x20) || InFamilies(family, 0x10, 0x14);
                if (hasValidParent) {
                    ok = holder->GetValidParentKey(&holderParentKey) && ok;
                } else if (InFamilies(family, 0x14, 0x1c)) {
                    ObjectKey holderSelfKey;
                    holderParentKey = *holder->GetKey(&holderSelfKey);
                } else {
                    ok = 0;
                }
            }

            bool moveBefore = false;
            bool moveAfter = false;
            if (holderParentKey.OrNull() && parentKey.m_value != holderParentKey.m_value) {
                if (!personMissionKey.OrNull()) {
                    if (holderMissionKey.OrNull()) moveAfter = true;
                } else {
                    moveBefore = true;
                }
            }
            ObjectKey personKey;
            if (moveBefore) ok = FUN_00556390(rules, person->GetKey(&personKey), &holderParentKey, ctx) && ok;

            const int holderHasMission = holderMissionKey.IsSet();
            if (personMissionKey.OrNull())
                ok = personMission->RemoveMember(person->GetKey(&personKey), holderHasMission, ctx) && ok;

            if (holderMissionKey.OrNull()) {
                const int added = holderMission->AddMember(person->GetKey(&personKey), 0, ctx) && ok;
                ok = person->SetParentAtMissionCompletionKey(&holderCompletionParent, ctx) && added;
            }
            if (moveAfter) ok = FUN_00556390(rules, person->GetKey(&personKey), &holderParentKey, ctx) && ok;

            GameObjNotifier* parent = person->m_parent;
            if (holderMissionKey.OrNull() && (person->m_flagsAc & mission_exec::kCharacterCaptured) && parent &&
                ((person->m_flags ^ parent->m_flags) & 0xc0) == 0) {
                bool objectiveIsPerson = false;
                if (holderMission && holderMission->GetFamily() == kFamilyMissionOfObjective)
                    objectiveIsPerson = holderMission->m_objectiveKey.m_value == person->GetKey(&personKey)->m_value;
                if (!objectiveIsPerson) ok = holderMission->RemoveMember(person->GetKey(&personKey), 0, ctx) && ok;
            }
        }
    }

    ok = person->Virtual44(ctx) && ok;
    CharacterTargetMissionIterator missions(this);
    for (missions.Begin(); missions.Current(); missions.Advance()) {
        Mission* mission = static_cast<Mission*>(missions.Current());
        if (mission->ObjectiveKeyIs(key)) ok = mission->UpdatePhaseForTeam(ctx) && ok;
    }
    return ok;
}
