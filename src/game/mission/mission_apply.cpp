#include "gameobj/game_hooks.h"
#include "mission/mission.h"

namespace {

constexpr int kMasterView = 0;

bool wantsNotification(Mission* view, NotifyContext* ctx) {
    return CallGameHook2AF0() && ctx->GetViewFlag(static_cast<int>(view->ViewSide()));
}

}  // namespace

#define REB_MISSION_INT_APPLY(Name, field)                                              \
    int Mission::Apply##Name(NotifyContext* ctx) {                                      \
        int result = IsSideView();                                                      \
        if (result) {                                                                   \
            Mission* master = ViewAsMission(kMasterView);                               \
            if (MatchesSideMask(ctx->m_flag08)) {                                       \
                const int oldValue = field;                                             \
                if (oldValue != master->field) {                                        \
                    field = master->field;                                              \
                    if (wantsNotification(this, ctx)) result = On##Name##Notified(oldValue, field, ctx); \
                }                                                                       \
            }                                                                           \
        }                                                                               \
        return result;                                                                  \
    }

REB_MISSION_INT_APPLY(UserId, m_userId)
REB_MISSION_INT_APPLY(UserId2, m_userId2)
REB_MISSION_INT_APPLY(TaskStatus, m_taskStatus)
REB_MISSION_INT_APPLY(CompletionStatus, m_completionStatus)
REB_MISSION_INT_APPLY(Phase, m_phase)
#undef REB_MISSION_INT_APPLY

#define REB_MISSION_KEY_APPLY(Name, field)                                              \
    int Mission::Apply##Name(NotifyContext* ctx) {                                      \
        int result = IsSideView();                                                      \
        if (result) {                                                                   \
            Mission* master = ViewAsMission(kMasterView);                               \
            if (MatchesSideMask(ctx->m_flag08)) {                                       \
                if (field.m_value != master->field.m_value) {                           \
                    auto oldKey = field;                                                \
                    field = master->field;                                              \
                    if (wantsNotification(this, ctx)) result = On##Name##Notified(&oldKey, &field, ctx); \
                }                                                                       \
            }                                                                           \
        }                                                                               \
        return result;                                                                  \
    }

REB_MISSION_KEY_APPLY(OriginLocationKey, m_originLocationKey)
REB_MISSION_KEY_APPLY(ObjectiveKey, m_objectiveKey)
REB_MISSION_KEY_APPLY(TargetKey, m_targetKey)
REB_MISSION_KEY_APPLY(TargetLocationKey, m_targetLocationKey)
REB_MISSION_KEY_APPLY(LeaderKey, m_leaderKey)
REB_MISSION_KEY_APPLY(LeaderSeedKey, m_leaderSeedKey)
#undef REB_MISSION_KEY_APPLY

#define REB_MISSION_SET_APPLY(Name, field)                                              \
    int Mission::Apply##Name(NotifyContext* ctx) {                                      \
        int result = IsSideView();                                                      \
        if (result) {                                                                   \
            Mission* master = ViewAsMission(kMasterView);                               \
            if (MatchesSideMask(ctx->m_flag08)) {                                       \
                if (master->field != field) {                                           \
                    ObjectKeyValueSet oldSet(field);                                    \
                    field = master->field;                                              \
                    if (wantsNotification(this, ctx)) result = On##Name##Notified(&oldSet, &field, ctx); \
                }                                                                       \
            }                                                                           \
        }                                                                               \
        return result;                                                                  \
    }

REB_MISSION_SET_APPLY(Team, m_team)
REB_MISSION_SET_APPLY(Decoys, m_decoys)
REB_MISSION_SET_APPLY(Captured, m_captured)
REB_MISSION_SET_APPLY(FinishedMembers, m_finishedMembers)
#undef REB_MISSION_SET_APPLY

#define REB_MISSION_FLAG_APPLY(Name, mask)                                              \
    int Mission::Apply##Name(NotifyContext* ctx) {                                      \
        int result = IsSideView();                                                      \
        if (result) {                                                                   \
            Mission* master = ViewAsMission(kMasterView);                               \
            if (MatchesSideMask(ctx->m_flag08)) {                                       \
                if (UpdateFlagBits(mask, master->m_flagsA4, &m_flagsA4)) {              \
                    if (wantsNotification(this, ctx)) result = On##Name##Notified(m_flagsA4 & mask, ctx); \
                }                                                                       \
            }                                                                           \
        }                                                                               \
        return result;                                                                  \
    }

REB_MISSION_FLAG_APPLY(ReadyForNextPhase, mission_flag::kReadyForNextPhase)
REB_MISSION_FLAG_APPLY(ImpliedTeam, mission_flag::kImpliedTeam)
REB_MISSION_FLAG_APPLY(Mandatory, mission_flag::kMandatory)
#undef REB_MISSION_FLAG_APPLY
