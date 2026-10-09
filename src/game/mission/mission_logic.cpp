#include "core/reb_abi.h"
#include "character/role.h"
#include "events/event_post.h"
#include "gameobj/object_database.h"
#include "gameobj/object_family.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_queries.h"
#include "mission/mission.h"
#include "mission/mission_external.h"
#include "mission/mission_hub.h"

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusMissionCategory = 0x40;
constexpr int kPhaseLeaderSelect = 2;
constexpr int kPhaseReadyToStart = 4;
constexpr int kPhaseOutcome = 8;
constexpr int kPhaseRepeat = 10;
constexpr int kPhaseFinished = 0xb;
constexpr uint32_t kRemoveOrResignRequests = role_flag::kMissionRemoveRequest | role_flag::kMissionResignRequest;
constexpr uint32_t kDestroyed = 0x8;
constexpr uint32_t kEnrouteActive = 0x20;
constexpr uint32_t kHyperdriveActive = 0x800;
constexpr int kDestroyedReasonMissionDone = 0x14;
constexpr int kEventMissionTimer = 0x38c;
constexpr int kEventMissionPhaseStat = 0x38b;
constexpr int kEventPhaseChanged = 0x30d;
constexpr int kEventReadyForNextPhase = 0x30c;

bool isSystemFamily(GameObjNotifier* object) {
    const uint32_t family = object->GetFamily();
    return family >= 0x90 && family < 0xa0;
}

bool inFamilies(GameObjNotifier* object, uint32_t first, uint32_t last) {
    const uint32_t family = object->GetFamily();
    return family >= first && family < last;
}

MissionData* dataOf(Mission* mission) { return static_cast<MissionData*>(mission->m_data); }

int status(StatusCode* out, int category, int code) {
    *out = StatusCode(category, code);
    return 1;
}

}  // namespace

// ---- queries --------------------------------------------------------------------------------------------

int Mission::CanAllTeamAct() {
    if (GetDataField1c() < 5 && TargetLocationDiffersFromOrigin()) return 0;
    return 1;
}

int Mission::AverageMemberSkill() {
    int total = 0;
    int count = 0;
    MissionPersonIterator members(this);
    members.Begin();
    while (members.Current()) {
        ++count;
        total += static_cast<Role*>(members.Current())->GetLeadership();
        members.Advance();
    }
    return count ? total / count : 0;
}

int Mission::HasMembers() {
    MissionPersonIterator members(this);
    members.Begin();
    return members.HasAny();
}

int Mission::CountMembers() {
    MissionPersonIterator members(this);
    members.Begin();
    return members.Count();
}

uint32_t Mission::GetPhaseLevel() {
    const uint32_t levels = dataOf(this)->m_levels.m_levels;
    if (dataOf(this)->m_unk18 != 0) {
        switch (m_phase) {
            case 2:
                return levels & 0xf;
            case 3:
                return (levels >> 4) & 0xf;
            case 7:
                return (levels >> 8) & 0xf;
            case 9:
                return (levels >> 12) & 0xf;
            default:
                break;
        }
    }
    return 0;
}

int Mission::ComputePhaseLevels(MissionPhaseLevels* out) {
    if (GetRecordDetectionPhases() == 0) {
        out->m_levels &= 0xffff0000;
        return 1;
    }
    out->m_levels = (out->m_levels & 0xffff4110) | 0x4110;
    if (!IsDataField1cAbove6()) {
        if (IsDataField1cAbove4()) out->m_levels &= 0xffffff00;
    } else if (m_completionStatus == 7) {
        out->m_levels = (out->m_levels & 0xffffff00) ^ 3;
    }
    return 1;
}

int Mission::GetOriginSystem(GameObj** out) {
    GameObjNotifier* object = GetMasterInFamilies_01_ff(&m_originLocationKey);
    if (object && isSystemFamily(object)) *out = static_cast<GameObj*>(object);
    return *out != nullptr;
}

int Mission::GetObjectiveObject(GameObj** out) {
    *out = static_cast<GameObj*>(GetMasterInFamilies_01_ff(&m_objectiveKey));
    return *out != nullptr;
}

int Mission::GetTargetObject(GameObj** out) {
    *out = static_cast<GameObj*>(GetMasterInFamilies_01_ff(&m_targetKey));
    return *out != nullptr;
}

int Mission::GetTargetLocationSystem(GameObj** out) {
    GameObjNotifier* object = GetMasterInFamilies_01_ff(&m_targetLocationKey);
    if (object && isSystemFamily(object)) *out = static_cast<GameObj*>(object);
    return *out != nullptr;
}

namespace {
int viewOfControlSide(uint32_t flags) {
    const uint32_t side = (flags >> 6) & 3;
    return side == 1 ? 1 : side == 2 ? 2 : 0;
}
}  // namespace

int Mission::GetObjectiveInOwnerView(GameObj** out) {
    *out = nullptr;
    const int view = viewOfControlSide(m_flags);
    if (view) *out = static_cast<GameObj*>(GetObjectInFamilies_01_ff(view, &m_objectiveKey));
    return *out != nullptr;
}

int Mission::GetTargetInOwnerView(GameObj** out) {
    *out = nullptr;
    const int view = viewOfControlSide(m_flags);
    if (view) *out = static_cast<GameObj*>(GetObjectInFamilies_01_ff(view, &m_targetKey));
    return *out != nullptr;
}

int Mission::GetMemberTransitFlags(Role* person, int* a, int* b, int* c) {
    *a = 0;
    *b = 0;
    *c = 0;
    if (!HasCompletionStatus() && (person->m_flags78 & kRemoveOrResignRequests) == 0) return 1;
    bool sameSide = false;
    GameObjNotifier* parent = nullptr;
    person->GetParent(&parent);
    if (parent) sameSide = ((person->m_flags ^ parent->m_flags) & 0xc0) == 0;
    *a = 1;
    const int field68 = GetRecordField68();
    const bool flag = (person->m_stateFlags & kHyperdriveActive) != 0 || field68 == 2 || (field68 == 1 && !sameSide);
    *b = flag;
    *c = flag && GetRecordField68() != 2;
    return 1;
}

int REB_STDCALL ChooseMissionLeader(ObjectKeyValueSet* team, ObjectKey* leader, NullableKey* seed) {
    leader->Reset();
    seed->m_value = 0;
    int bestLeadership = 0;
    int bestRank = 1;
    for (AvlNode* node = team->First(); node; node = node->m_successor) {
        ObjectKey key;
        static_cast<ObjectKeyNode*>(node)->GetKey(&key);
        if (!(key.Family() >= 0x30 && key.Family() < 0x40)) continue;
        Role* person = static_cast<Role*>(GetMasterInFamilies_30_40(&key));
        if (!person || (person->m_flags78 & kRemoveOrResignRequests) != 0 || (person->m_stateFlags & kDestroyed) != 0 ||
            person->Virtual117() != 0) {
            continue;
        }
        const int rank = person->Virtual127();
        const bool better = !leader->IsSet() || bestRank < rank ||
                            (person->Virtual127() == bestRank && bestLeadership < person->GetLeadership());
        if (!better) continue;
        person->GetKey(leader);
        if (person->m_record) {
            ObjectKey recordKey;
            recordKey.m_value = (person->m_record->m_third << 24) | (person->m_record->m_id & 0xffffff);
            seed->m_value = recordKey.m_value;
        }
        bestLeadership = person->GetLeadership();
        bestRank = person->Virtual127();
    }
    return 1;
}

// ---- activity checks and completion ---------------------------------------------------------------------

int Mission::GetCheckFunctions(MissionCheckFunctions* out) {
    out->m_unk00 = reinterpret_cast<void*>(&Mission::CheckMemberDefault);
    out->m_unk04 = reinterpret_cast<void*>(&Mission::CheckTargetDefault);
    return 1;
}

int Mission::CheckActivity(const ActivityRequest* request, StatusCode* out) {
    status(out, kStatusCategory, kStatusOk);
    const int result = CheckActivityRequest(request, out);
    if (out->m_second == static_cast<uint32_t>(kStatusOk)) {
        if (IsPhase4()) status(out, kStatusMissionCategory, 0x40);
        if (out->m_second == static_cast<uint32_t>(kStatusOk) && !IsPhase8()) status(out, kStatusMissionCategory, 0x41);
    }
    return result;
}

int Mission::CheckActivityStatus(const ActivityRequest* request, StatusCode* out) {
    status(out, kStatusCategory, kStatusOk);
    return CheckActivity(request, out);
}

int Mission::TryComplete(const ActivityRequest* request, NotifyContext* ctx) {
    StatusCode result(kStatusCategory, kStatusOk);
    int allowed = CheckActivityStatus(request, &result);
    if (allowed && result.m_second == static_cast<uint32_t>(kStatusOk)) allowed = CompleteIfOutcomePhase(ctx);
    return allowed;
}

int Mission::CompleteIfOutcomePhase(NotifyContext* ctx) {
    if (IsPhase8()) return CompleteWith(2, ctx);
    return 0;
}

int Mission::CompleteWith(int completionStatus, NotifyContext* ctx) {
    NotifyContext copy(*ctx);
    copy.m_reserved14 = CallGameHook2AFC();
    return SetCompletionStatus(completionStatus, &copy) != 0;
}

int REB_CDECL Mission::CheckMemberDefault(int unused, int value, int* out, StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    *out = value;
    return 1;
}

int REB_CDECL Mission::CheckTargetDefault(MissionCheckArgs* args, StatusCode* statusOut, int* code) {
    *statusOut = StatusCode(kStatusCategory, kStatusOk);
    *code = 0;
    int ok = 1;
    if (args->m_flag0c != 0 && (args->m_objective == nullptr || args->m_target == nullptr)) ok = 0;
    if (args->m_flag04 != 0) {
        if (ok) ok = CheckSummaryAgainstRecord(args->m_record, args->m_summary, statusOut);
        if (args->m_flag0c == 0) return ok;
        if (ok && statusOut->m_second == static_cast<uint32_t>(kStatusOk) &&
            (args->m_target->m_stateFlags & 0x10) != 0) {
            *statusOut = StatusCode(kStatusMissionCategory, 0x13);
            *code = -1;
        }
    }
    if (args->m_flag0c != 0) {
        auto* record = static_cast<MissionSD*>(args->m_record);
        if (ok && statusOut->m_second == static_cast<uint32_t>(kStatusOk) && record->m_containerLossEnds != 0 &&
            (args->m_target->m_stateFlags & kDestroyed) != 0) {
            *statusOut = StatusCode(kStatusMissionCategory, 0x12);
            *code = 7;
        }
        if (ok && statusOut->m_second == static_cast<uint32_t>(kStatusOk)) {
            const uint32_t side = args->m_controlSide;
            const uint32_t objectiveSide = (args->m_objective->m_flags >> 6) & 3;
            int allowed;
            if (side == objectiveSide) {
                allowed = record->m_ownSideTarget;
            } else {
                const uint32_t opposite = side == 3 ? 3 : 2 - (side != 1);
                allowed = opposite == objectiveSide ? record->m_opponentTarget : record->m_otherSideTarget;
            }
            if (allowed == 0) {
                *statusOut = StatusCode(kStatusMissionCategory, 0x23);
                *code = 8;
            }
        }
    }
    return ok;
}

// ---- phase machinery ------------------------------------------------------------------------------------

int Mission::AssignPhaseLevels(const MissionPhaseLevels* levels) {
    const int result = IsMasterView();
    dataOf(this)->m_levels = *levels;
    return result;
}

int Mission::ComputeAllTeamCanAct(uint32_t* out) {
    *out = 1;
    MissionPersonIterator members(this);
    members.Begin();
    uint32_t canAct = *out;
    while (canAct != 0 && members.Current()) {
        *out = (~members.Current()->m_stateFlags >> 11) & 1;
        members.Advance();
        canAct = *out;
    }
    dataOf(this)->m_unk18 = *out;
    return 1;
}

int Mission::ReadyUnlessPhase4Or8(NotifyContext* ctx) {
    int result = 1;
    if (!IsPhase4() && !IsPhase8()) result = SetReadyForNextPhase(1, ctx);
    return result;
}

int Mission::ReadyIfPhase8(NotifyContext* ctx) {
    int result = IsPhase8();
    if (result) result = SetReadyForNextPhase(1, ctx);
    return result;
}

int Mission::DestroyIfPhase11(NotifyContext* ctx) {
    if (m_phase != kPhaseFinished) return 0;
    return ChangeDestroyedReason(kDestroyedReasonMissionDone, ctx) != 0;
}

int Mission::ScheduleTimerEvent(NotifyContext* ctx) {
    int result = 1;
    if (!HasMembers() && m_completionStatus != 0) {
        result = ArmTimerAfterSetFlag(kEventMissionTimer, 1, this, &dataOf(this)->m_statB, ctx) != 0;
    }
    return result;
}

int Mission::AdvancePhase(NotifyContext* ctx) {
    if (HasCompletionStatus()) {
        SetPhase(kPhaseFinished, ctx);
        return 1;
    }
    int result = 1;
    if (m_flagsA4 & mission_flag::kReadyForNextPhase) {
        const int cleared = SetReadyForNextPhase(0, ctx);
        int next = m_phase + 1;
        if (m_phase == kPhaseRepeat) {
            if (GetRecordRepeats() != 0) next = kPhaseOutcome;
        } else if (m_phase == kPhaseFinished) {
            next = kPhaseFinished;
        }
        const int advanced = SetPhase(next, ctx);
        if (advanced && cleared) return 1;
        result = 0;
    }
    return result;
}

int Mission::UpdateLeaderAndTeamKeys(NotifyContext* ctx) {
    int ok = 1;
    if (m_phase != 1 && m_phase != kPhaseFinished) {
        ObjectKey leader;
        NullableKey seed;
        seed.m_value = 0;
        const int chosen = ChooseMissionLeader(&m_team, &leader, &seed);
        const int leaderSet = SetLeaderKey(&leader, ctx);
        ok = leaderSet && chosen;
        const int seedSet = SetLeaderSeedKey(&seed, ctx);
        ok = seedSet && ok;
        if (ok && (m_flagsA4 & mission_flag::kImpliedTeam) == 0) ok = leader.IsSet() != 0;
    }
    return ok;
}

int Mission::UpdateTeamAndTarget(NotifyContext* ctx) {
    int ok = 1;
    int markReady = 0;
    if (IsPhase4()) {
        uint32_t allCanAct = 1;
        ok = ComputeAllTeamCanAct(&allCanAct) != 0;
        if (allCanAct != 0) {
            GameObj* target = nullptr;
            const int haveTarget = GetTargetObject(&target);
            ok = haveTarget && ok;
            if (target) {
                ObjectKey owner;
                const int gotOwner = target->GetOwnerKey(&owner);
                ok = gotOwner && ok;
                const int locationSet = SetTargetLocationKey(&owner, ctx);
                ok = locationSet && ok;
            }
            int anyMoving = 1;
            int anyStopped = 1;
            RulesMgr* rules = GetRulesMgr();
            ok = rules != nullptr && ok;
            if (rules) {
                MissionPersonIterator members(this);
                members.Begin();
                while (members.Current()) {
                    uint32_t arrived = 1;
                    if (members.Current()->m_stateFlags & kEnrouteActive) {
                        const int evaluated = rules->ResumeTrip(members.Current(), &arrived, ctx);
                        ok = evaluated && ok;
                    }
                    if (arrived == 0) {
                        anyMoving = 0;
                    } else {
                        anyStopped = 0;
                    }
                    members.Advance();
                }
                if (anyMoving == 0 && anyStopped == 0) anyMoving = 1;
            }
            if (anyMoving != 0 && !HasCompletionStatus()) markReady = 1;
        }
    }
    if (markReady) {
        const int set = SetReadyForNextPhase(1, ctx);
        ok = set && ok;
    }
    return ok;
}
