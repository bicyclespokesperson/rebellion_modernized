#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "events/event_helpers.h"
#include "gameobj/object_queries.h"
#include "mission/mission_events.h"
#include "story/mission_mgr.h"

namespace {

using events::succeedWithoutChecks;

MissionMgr* getMissionMgr() { return reinterpret_cast<MissionMgr*>(GetGameRootMember_b8()); }

}  // namespace

// ---- MissionMgrEvent30c (kind 0x30c) ---------------------------------------------------------------------------------

MissionMgrEvent30c::MissionMgrEvent30c() : SourceEvent() {}

MissionMgrEvent30c::MissionMgrEvent30c(const MissionMgrEvent30c& other) : SourceEvent(other) {}

MissionMgrEvent30c::~MissionMgrEvent30c() { REB_KEEP_VPTR_STORE(); }

MissionMgrEvent30c* MissionMgrEvent30c::Clone() { return NewOrNull<MissionMgrEvent30c>(*this); }

int MissionMgrEvent30c::CheckEnabled(StatusCode* status) { return succeedWithoutChecks(status); }

int MissionMgrEvent30c::CheckValid(StatusCode* status) { return succeedWithoutChecks(status); }

int MissionMgrEvent30c::Execute() {
    NotifyContext ctx(m_context);
    if (MissionMgr* manager = getMissionMgr()) manager->AdvanceMissionPhase(&m_key, m_valueB, &ctx);
    return 0;
}

MissionMgrEvent30c* CreateMissionMgrEvent30c() { return NewOrNull<MissionMgrEvent30c>(); }

// ---- MissionMgrEvent30d (kind 0x30d) ---------------------------------------------------------------------------------

MissionMgrEvent30d::MissionMgrEvent30d() : SourceEvent() {}

MissionMgrEvent30d::MissionMgrEvent30d(const MissionMgrEvent30d& other) : SourceEvent(other) {}

MissionMgrEvent30d::~MissionMgrEvent30d() { REB_KEEP_VPTR_STORE(); }

MissionMgrEvent30d* MissionMgrEvent30d::Clone() { return NewOrNull<MissionMgrEvent30d>(*this); }

int MissionMgrEvent30d::CheckEnabled(StatusCode* status) { return succeedWithoutChecks(status); }

int MissionMgrEvent30d::CheckValid(StatusCode* status) { return succeedWithoutChecks(status); }

int MissionMgrEvent30d::Execute() {
    NotifyContext ctx(m_context);
    if (MissionMgr* manager = getMissionMgr()) manager->RunMissionPhase(&m_key, m_valueA, m_valueB, &ctx);
    return 0;
}

MissionMgrEvent30d* CreateMissionMgrEvent30d() { return NewOrNull<MissionMgrEvent30d>(); }
