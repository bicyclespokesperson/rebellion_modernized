#include "events/person_events.h"

#include "character/role.h"
#include "core/checked_new.h"
#include "gameobj/object_queries.h"
#include "story/mission_mgr.h"
#include "rules/event_rules.h"

// Methods of the object GetGameRootMember_b8 returns (the mission manager, not reconstructed here); `manager` is the
// thiscall receiver.

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusNoRole = 0x12;
constexpr int kLockFamilyFirst = 0x40;
constexpr int kLockFamilyEnd = 0x80;

void setOk(StatusCode* status) { *status = StatusCode(kStatusCategory, kStatusOk); }

bool isOk(const StatusCode& status) { return status.m_second == static_cast<uint32_t>(kStatusOk); }

bool isLockable(const ObjectKey& key) { return key.IsSet() && key.Family() >= kLockFamilyFirst && key.Family() < kLockFamilyEnd; }

}  // namespace

// ---- RoleActivityCommand (0x230) --------------------------------------------------------------------------

RoleActivityCommand::RoleActivityCommand() : Command() {}

RoleActivityCommand::RoleActivityCommand(const RoleActivityCommand& other) : Command(other) {}

RoleActivityCommand::~RoleActivityCommand() {  }

Command* RoleActivityCommand::Clone() { return NewOrNull<RoleActivityCommand>(*this); }

int RoleActivityCommand::GetKind() { return 0x230; }

void* CreateRoleActivityCommand() { return NewOrNull<RoleActivityCommand>(); }

int RoleActivityCommand::CheckEnabled(StatusCode* status) {
    int ok = 1;
    GameObjNotifier* role = nullptr;
    setOk(status);
    if (isOk(*status)) {
        StatusCode missing(kStatusCategory, kStatusNoRole);
        ok = ResolveMasterInFamilies_30_40(&m_key, &role, &missing, status);
    }
    if (ok && isOk(*status)) ok = static_cast<Role*>(role)->CheckActivity(&m_request, status);
    return ok;
}

int RoleActivityCommand::CheckValid(StatusCode* status) {
    int ok = 1;
    GameObjNotifier* role = nullptr;
    setOk(status);
    if (isOk(*status)) {
        StatusCode missing(kStatusCategory, kStatusNoRole);
        ok = ResolveMasterInFamilies_30_40(&m_key, &role, &missing, status);
    }
    if (ok && isOk(*status)) ok = static_cast<Role*>(role)->CheckActivityStatus(&m_request, status);
    return ok;
}

int RoleActivityCommand::Execute() {
    NotifyContext ctx(m_context);
    GameObjNotifier* role = nullptr;
    StatusCode status(kStatusCategory, kStatusOk);
    StatusCode missing(kStatusCategory, kStatusNoRole);
    int ok = ResolveMasterInFamilies_30_40(&m_key, &role, &missing, &status);
    if (ok && isOk(status)) static_cast<Role*>(role)->Virtual128(&m_request, &ctx);
    return 0;
}

// ---- RemoveRequestEvent (0x30b) ---------------------------------------------------------------------------

RemoveRequestEvent::RemoveRequestEvent() : SourceEvent() {}

RemoveRequestEvent::RemoveRequestEvent(const RemoveRequestEvent& other) : SourceEvent(other) {}

RemoveRequestEvent::~RemoveRequestEvent() {  }

EventBase* RemoveRequestEvent::Clone() { return NewOrNull<RemoveRequestEvent>(*this); }

int RemoveRequestEvent::GetKind() { return 0x30b; }

int RemoveRequestEvent::GetPriority() { return 0x150; }

void* CreateRemoveRequestEvent() { return NewOrNull<RemoveRequestEvent>(); }

int RemoveRequestEvent::CheckEnabled(StatusCode* status) {
    setOk(status);
    return 1;
}

int RemoveRequestEvent::CheckValid(StatusCode* status) {
    setOk(status);
    return 1;
}

int RemoveRequestEvent::Execute() {
    NotifyContext ctx(m_context);
    if (GameObjNotifier* manager = GetGameRootMember_b8()) static_cast<MissionMgr*>(manager)->MissionMgr::ProcessRemoveRequest(&m_key, m_valueB, &ctx);
    return 0;
}

// ---- RoleStateEvent (0x30e) -------------------------------------------------------------------------------

RoleStateEvent::RoleStateEvent() : SourceEvent() {}

RoleStateEvent::RoleStateEvent(const RoleStateEvent& other) : SourceEvent(other) {}

RoleStateEvent::~RoleStateEvent() {  }

EventBase* RoleStateEvent::Clone() { return NewOrNull<RoleStateEvent>(*this); }

int RoleStateEvent::GetKind() { return 0x30e; }

int RoleStateEvent::GetPriority() { return 0x200; }

void* CreateRoleStateEvent() { return NewOrNull<RoleStateEvent>(); }

int RoleStateEvent::CheckEnabled(StatusCode* status) {
    setOk(status);
    return 1;
}

int RoleStateEvent::CheckValid(StatusCode* status) {
    setOk(status);
    return 1;
}

int RoleStateEvent::Execute() {
    NotifyContext ctx(m_context);
    ApplyRoleStateEvent(&m_key, m_valueB, &ctx);
    return 0;
}

// ---- PersonRemovedEvent (0x312) ---------------------------------------------------------------------------

PersonRemovedEvent::PersonRemovedEvent() : SourceEvent() {}

PersonRemovedEvent::PersonRemovedEvent(const PersonRemovedEvent& other) : SourceEvent(other) {}

PersonRemovedEvent::~PersonRemovedEvent() {  }

EventBase* PersonRemovedEvent::Clone() { return NewOrNull<PersonRemovedEvent>(*this); }

int PersonRemovedEvent::GetKind() { return 0x312; }

void* CreatePersonRemovedEvent() { return NewOrNull<PersonRemovedEvent>(); }

int PersonRemovedEvent::CheckEnabled(StatusCode* status) {
    setOk(status);
    return 1;
}

int PersonRemovedEvent::CheckValid(StatusCode* status) {
    setOk(status);
    return 1;
}

int PersonRemovedEvent::Execute() {
    NotifyContext ctx(m_context);
    if (GameObjNotifier* manager = GetGameRootMember_b8()) static_cast<MissionMgr*>(manager)->MissionMgr::OnPersonRemoved(&m_key, m_valueB, &ctx);
    return 0;
}

// ---- ResignRequestEvent (0x314) ---------------------------------------------------------------------------

ResignRequestEvent::ResignRequestEvent() : SourceEvent() {}

ResignRequestEvent::ResignRequestEvent(const ResignRequestEvent& other) : SourceEvent(other) {}

ResignRequestEvent::~ResignRequestEvent() {  }

EventBase* ResignRequestEvent::Clone() { return NewOrNull<ResignRequestEvent>(*this); }

int ResignRequestEvent::GetKind() { return 0x314; }

int ResignRequestEvent::GetPriority() { return 0x152; }

void* CreateResignRequestEvent() { return NewOrNull<ResignRequestEvent>(); }

int ResignRequestEvent::CheckEnabled(StatusCode* status) {
    setOk(status);
    return 1;
}

int ResignRequestEvent::CheckValid(StatusCode* status) {
    setOk(status);
    return 1;
}

int ResignRequestEvent::Execute() {
    NotifyContext ctx(m_context);
    if (GameObjNotifier* manager = GetGameRootMember_b8()) static_cast<MissionMgr*>(manager)->MissionMgr::ProcessResignRequest(&m_key, m_valueB, &ctx);
    return 0;
}

// ---- LockTransferEvent (0x321) ----------------------------------------------------------------------------

LockTransferEvent::LockTransferEvent() : TwoKeyEvent() {}

LockTransferEvent::LockTransferEvent(const LockTransferEvent& other) : TwoKeyEvent(other) {}

LockTransferEvent::~LockTransferEvent() {  }

EventBase* LockTransferEvent::Clone() { return NewOrNull<LockTransferEvent>(*this); }

int LockTransferEvent::GetKind() { return 0x321; }

void* CreateLockTransferEvent() { return NewOrNull<LockTransferEvent>(); }

int LockTransferEvent::CheckEnabled(StatusCode* status) {
    setOk(status);
    return 1;
}

int LockTransferEvent::CheckValid(StatusCode* status) {
    setOk(status);
    return 1;
}

int LockTransferEvent::Execute() {
    NotifyContext ctx(m_context);
    if (isLockable(m_keyD)) {
        if (GameObjNotifier* target = GetMasterInFamilies_40_80(&m_keyD)) static_cast<GameObj*>(target)->IncrementLock(&ctx);
    }
    if (isLockable(m_keyC)) {
        if (GameObjNotifier* target = GetMasterInFamilies_40_80(&m_keyC)) static_cast<GameObj*>(target)->DecrementLock(&ctx);
    }
    return 0;
}

// ---- shared slot forwarders -------------------------------------------------------------------------------

void SourceEventRead(SourceEvent* event, istream* in) { event->ReadSource(in); }

void SourceEventChecksum(SourceEvent* event, Crc32* crc) { event->ChecksumSource(crc); }
