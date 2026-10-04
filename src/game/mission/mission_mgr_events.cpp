#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gameobj/object_queries.h"
#include "mission/mission_events.h"

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;

MissionMgr* getMissionMgr() { return reinterpret_cast<MissionMgr*>(GetGameRootMember_b8()); }

int succeedWithoutChecks(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    return 1;
}

}  // namespace

// ---- MissionMgrEvent30c (kind 0x30c) ---------------------------------------------------------------------------------

MissionMgrEvent30c::MissionMgrEvent30c() : NotifEventBase() {}

MissionMgrEvent30c::MissionMgrEvent30c(const MissionMgrEvent30c& other) : NotifEventBase(other) {}

MissionMgrEvent30c::~MissionMgrEvent30c() { REB_KEEP_VPTR_STORE(); }

NotifEventBase* MissionMgrEvent30c::Clone() { return NewOrNull<MissionMgrEvent30c>(*this); }

int MissionMgrEvent30c::CheckEnabled(StatusCode* status) { return succeedWithoutChecks(status); }

int MissionMgrEvent30c::CheckValid(StatusCode* status) { return succeedWithoutChecks(status); }

int MissionMgrEvent30c::Execute() {
    NotifyContext ctx(m_context);
    if (MissionMgr* manager = getMissionMgr()) FUN_00546e70(manager, &m_key, m_unk50, &ctx);
    return 0;
}

MissionMgrEvent30c* CreateMissionMgrEvent30c() { return NewOrNull<MissionMgrEvent30c>(); }

// ---- MissionMgrEvent30d (kind 0x30d) ---------------------------------------------------------------------------------

MissionMgrEvent30d::MissionMgrEvent30d() : NotifEventBase() {}

MissionMgrEvent30d::MissionMgrEvent30d(const MissionMgrEvent30d& other) : NotifEventBase(other) {}

MissionMgrEvent30d::~MissionMgrEvent30d() { REB_KEEP_VPTR_STORE(); }

NotifEventBase* MissionMgrEvent30d::Clone() { return NewOrNull<MissionMgrEvent30d>(*this); }

int MissionMgrEvent30d::CheckEnabled(StatusCode* status) { return succeedWithoutChecks(status); }

int MissionMgrEvent30d::CheckValid(StatusCode* status) { return succeedWithoutChecks(status); }

int MissionMgrEvent30d::Execute() {
    NotifyContext ctx(m_context);
    if (MissionMgr* manager = getMissionMgr()) FUN_00546ea0(manager, &m_key, m_unk4c, m_unk50, &ctx);
    return 0;
}

MissionMgrEvent30d* CreateMissionMgrEvent30d() { return NewOrNull<MissionMgrEvent30d>(); }
