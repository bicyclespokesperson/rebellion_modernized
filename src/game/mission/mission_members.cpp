#include "character/role.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"
#include "mission/mission.h"
#include "mission/mission_external.h"
#include "mission/mission_hub.h"

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusMissionCategory = 0x40;
constexpr int kStatusNoPersonOnMission = 0x91;
constexpr int kCompletionNoMembers = 5;
constexpr int kPhaseFinished = 0xb;
constexpr uint32_t kRemoveOrResignRequests = role_flag::kMissionRemoveRequest | role_flag::kMissionResignRequest;
constexpr uint32_t kControlSideBits = 0xc0;
constexpr uint32_t kEnrouteActive = 0x20;
constexpr uint32_t kHyperdriveActive = 0x800;

bool isPersonKey(const ObjectKey* key) { return key->Family() >= 0x30 && key->Family() < 0x40; }

ObjectKey seedKeyOf(GameObjNotifier* object) {
    ObjectKey key;
    key.m_value = 0;
    if (object->m_record) {
        key.m_value = (object->m_record->m_third << 24) | (object->m_record->m_id & 0xffffff);
    }
    return key;
}

bool sameLocationAndTravel(Role* person, GameObj* first) {
    ObjectKey personLocation;
    ObjectKey firstLocation;
    bool failed = person->GetOwnerKey(&personLocation) == 0;
    uint32_t matches = 0;
    if (!failed) {
        failed = first->GetOwnerKey(&firstLocation) == 0;
        matches = 0;
        if (!failed) {
            matches = firstLocation.m_value == personLocation.m_value;
            failed = matches == 0;
        }
    }
    if (!failed) matches = (person->m_stateFlags ^ ~first->m_stateFlags) >> 5 & 1;
    bool compatible = false;
    if (matches != 0 && ((person->m_stateFlags ^ ~first->m_stateFlags) >> 11 & 1) != 0) {
        compatible = first->m_eta == person->m_eta;
    }
    return compatible;
}

}  // namespace

int Mission::AddMember(ObjectKey* member, int decoy, NotifyContext* ctx) {
    const int master = IsMasterView();
    bool isPerson = false;
    if (master) isPerson = isPersonKey(member);
    bool candidate = false;
    if (isPerson) candidate = !IsMember(member);
    int result = 0;
    if (!candidate) return result;

    Role* person = static_cast<Role*>(GetMasterInFamilies_30_40(member));
    if (!person || ((m_flags ^ person->m_flags) & kControlSideBits) != 0) return 0;
    const bool allowed = person->Virtual117() == 0 || decoy == 0;
    if (!allowed) return 0;
    ObjectKey currentMission;
    if (person->GetMissionKey(&currentMission)->IsSet()) return 0;

    bool compatible = true;
    MissionPersonIterator members(this);
    members.Begin();
    if (members.Current()) compatible = sameLocationAndTravel(person, members.Current());
    if (!compatible) return 0;

    NotifyContext bothViews(*ctx);
    bothViews.SetViewFlag(1, 1);
    bothViews.SetViewFlag(2, 1);
    ObjectKey missionKey;
    GetKey(&missionKey);
    const int keySet = person->SetMissionKey(&missionKey, &bothViews);
    ObjectKey seed = seedKeyOf(this);
    const int seedSet = person->SetMissionSeedKey(&seed, ctx);
    bool ok = seedSet && keySet;
    if (GetRecordHidden() != 0) ok = person->SetOnHiddenMission(1, ctx) && ok;
    ok = person->SetOnMandatoryMission((m_flagsA4 >> 2) & 1, ctx) && ok;
    ok = person->SetDecoy(decoy, ctx) && ok;
    ok = person->SetCanResignFromMission(GetRecordCanResign() != 0, ctx) && ok;

    int added;
    if (decoy == 0) {
        if (person->Virtual117() == 0) {
            ObjectKeyValueSet team(m_team);
            team.AddKeyValue(member, 0);
            added = SetTeam(&team, ctx);
        } else {
            ObjectKeyValueSet captured(m_captured);
            captured.AddKeyValue(member, 0);
            added = SetCaptured(&captured, ctx);
        }
    } else {
        ObjectKeyValueSet decoys(m_decoys);
        decoys.AddKeyValue(member, 0);
        added = SetDecoys(&decoys, ctx);
    }
    return added && ok ? 1 : 0;
}

int Mission::RemoveMember(ObjectKey* member, int decoy, NotifyContext* ctx) {
    const int master = IsMasterView();
    if (!master || !isPersonKey(member)) return 0;
    const int inTeam = IsInTeam(member);
    const int inDecoys = IsInDecoys(member);
    const int inCaptured = IsInCaptured(member);
    Role* person = static_cast<Role*>(GetMasterInFamilies_30_40(member));
    if (!person) return 0;

    bool ok = true;
    if (decoy == 0 && (person->m_stateFlags & kEnrouteActive) != 0 && (person->m_stateFlags & kHyperdriveActive) == 0) {
        ok = person->SetEnrouteActive(0, ctx) != 0;
    }
    ok = person->SetMovingBetweenMissions(decoy, ctx) && ok;

    NotifyContext bothViews(*ctx);
    bothViews.SetViewFlag(1, 1);
    bothViews.SetViewFlag(2, 1);
    ObjectKey noMission;
    ok = person->SetMissionKey(&noMission, &bothViews) && ok;
    ObjectKey noSeed;
    noSeed.m_value = 0;
    ok = person->SetMissionSeedKey(&noSeed, ctx) && ok;

    if (inTeam) {
        ObjectKeyValueSet team(m_team);
        team.RemoveKey(member);
        ok = SetTeam(&team, ctx) && ok;
    } else if (inDecoys) {
        ObjectKeyValueSet decoys(m_decoys);
        decoys.RemoveKey(member);
        ok = SetDecoys(&decoys, ctx) && ok;
    } else if (inCaptured) {
        ObjectKeyValueSet captured(m_captured);
        captured.RemoveKey(member);
        ok = SetCaptured(&captured, ctx) && ok;
    }

    ok = UpdatePhaseForTeam(ctx) && ok;
    if (m_leaderKey.m_value == member->m_value) ok = UpdateLeaderAndTeamKeys(ctx) && ok;
    ok = ScheduleTimerEvent(ctx) && ok;
    return ok ? 1 : 0;
}

int Mission::UpdatePhaseForTeam(NotifyContext* ctx) {
    int ok = 1;
    int code = 0;
    StatusCode status(kStatusCategory, kStatusOk);
    if (HasCompletionStatus()) return ok;

    uint32_t anyActive = (m_flagsA4 >> 1) & 1;
    MissionPersonTeamIterator members(this);
    members.Begin();
    while (members.Current() && !anyActive) {
        anyActive = (static_cast<Role*>(members.Current())->m_flags78 & kRemoveOrResignRequests) == 0;
        members.Advance();
    }
    if (anyActive == 0) {
        code = kCompletionNoMembers;
        status = StatusCode(kStatusMissionCategory, kStatusNoPersonOnMission);
    }

    if (status.m_second == static_cast<uint32_t>(kStatusOk)) {
        GameObj* objective = nullptr;
        GameObj* target = nullptr;
        int haveObjective;
        int haveTarget;
        if (!CanAllTeamAct()) {
            haveObjective = GetObjectiveInOwnerView(&objective);
            haveTarget = GetTargetInOwnerView(&target);
        } else {
            haveObjective = GetObjectiveObject(&objective);
            haveTarget = GetTargetObject(&target);
        }
        ok = (haveTarget && haveObjective) ? 1 : 0;

        MissionMemberSummary summary = {};
        if (status.m_second == static_cast<uint32_t>(kStatusOk)) {
            ok = (FillMemberSummaryForMission(&m_team, &m_decoys, &m_captured, &summary, &status) && ok) ? 1 : 0;
            if (status.m_second == static_cast<uint32_t>(kStatusOk)) {
                MissionCheckFunctions checks = {};
                ok = (GetCheckFunctions(&checks) && ok) ? 1 : 0;
                if (status.m_second == static_cast<uint32_t>(kStatusOk)) {
                    ObjectRefList refs;
                    MissionCheckArgs args(0, 1, 1, (m_flags >> 6) & 3, m_record, &refs, &refs, &refs, &m_team, &m_decoys,
                                          &m_captured, &summary, target, objective);
                    if (checks.m_unk04) {
                        const int checked = reinterpret_cast<MissionTargetCheckFn>(checks.m_unk04)(&args, &status, &code);
                        ok = (checked && ok) ? 1 : 0;
                    }
                }
            }
        }
    }

    if (code == 0 && m_phase == kPhaseFinished) code = 1;
    if (code != 0) ok = (CompleteWith(code, ctx) && ok) ? 1 : 0;
    return ok;
}
