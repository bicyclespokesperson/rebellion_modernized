#include "events/plain_events.h"
#include "events/plain_events_decl.h"

#include "economy/side.h"
#include "events/event_helpers.h"
#include "gameobj/game_obj.h"

using events::kStatusCategory;
using events::kStatusMissingTarget;
using events::kStatusOk;
using events::succeeded;
using events::succeedWithoutChecks;


RefineryQueueEvent::RefineryQueueEvent() : PlainEvent() {}

RefineryQueueEvent::RefineryQueueEvent(const RefineryQueueEvent& other) : PlainEvent(other) {}

RefineryQueueEvent::~RefineryQueueEvent() { REB_KEEP_VPTR_STORE(); }

RefineryQueueEvent* RefineryQueueEvent::Clone() { return NewOrNull<RefineryQueueEvent>(*this); }

int RefineryQueueEvent::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int RefineryQueueEvent::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int RefineryQueueEvent::Execute() {
    NotifyContext ctx(m_context);
    int more = 0;
    GameObjNotifier* owner = nullptr;
    StatusCode status(kStatusCategory, kStatusOk);
    const StatusCode missing(kStatusCategory, kStatusMissingTarget);
    int ok = ResolveMasterInFamilies_f3_f4(&m_key, &owner, &missing, &status);
    if (ok && succeeded(status)) static_cast<Side*>(owner)->ServeRefineryQueue(&more, &ctx);
    return more;
}

RefineryQueueEvent* CreateRefineryQueueEvent() { return NewOrNull<RefineryQueueEvent>(); }

ManufacturerQueueEvent::ManufacturerQueueEvent() : PlainEvent() {}

ManufacturerQueueEvent::ManufacturerQueueEvent(const ManufacturerQueueEvent& other) : PlainEvent(other) {}

ManufacturerQueueEvent::~ManufacturerQueueEvent() { REB_KEEP_VPTR_STORE(); }

ManufacturerQueueEvent* ManufacturerQueueEvent::Clone() { return NewOrNull<ManufacturerQueueEvent>(*this); }

int ManufacturerQueueEvent::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int ManufacturerQueueEvent::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int ManufacturerQueueEvent::Execute() {
    NotifyContext ctx(m_context);
    int more = 0;
    GameObjNotifier* owner = nullptr;
    StatusCode status(kStatusCategory, kStatusOk);
    const StatusCode missing(kStatusCategory, kStatusMissingTarget);
    int ok = ResolveMasterInFamilies_f3_f4(&m_key, &owner, &missing, &status);
    if (ok && succeeded(status)) static_cast<Side*>(owner)->ServeManufacturerQueue(&more, &ctx);
    return more;
}

ManufacturerQueueEvent* CreateManufacturerQueueEvent() { return NewOrNull<ManufacturerQueueEvent>(); }

ReparentToRootEvent::ReparentToRootEvent() : PlainEvent() {}

ReparentToRootEvent::ReparentToRootEvent(const ReparentToRootEvent& other) : PlainEvent(other) {}

ReparentToRootEvent::~ReparentToRootEvent() { REB_KEEP_VPTR_STORE(); }

ReparentToRootEvent* ReparentToRootEvent::Clone() { return NewOrNull<ReparentToRootEvent>(*this); }

int ReparentToRootEvent::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int ReparentToRootEvent::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int ReparentToRootEvent::Execute() {
    NotifyContext ctx(m_context);
    GameObjNotifier* owner = nullptr;
    StatusCode status(kStatusCategory, kStatusOk);
    const StatusCode missing(kStatusCategory, kStatusMissingTarget);
    int ok = ResolveMasterInFamilies_01_ff(&m_key, &owner, &missing, &status);
    if (ok && owner) {
        GameObjNotifier* parent = GetGameRootMember_b0();
        ObjectKey currentParent;
        static_cast<GameObj*>(owner)->GetValidParentKey(&currentParent);
        if (parent) {
            ObjectKey wanted;
            if (FUN_004025b0(parent, &wanted)->m_value != currentParent.m_value) {
                ObjectKey target;
                static_cast<GameObj*>(owner)->Reparent(FUN_004025b0(parent, &target), &ctx);
            }
        }
    }
    return 0;
}

ReparentToRootEvent* CreateReparentToRootEvent() { return NewOrNull<ReparentToRootEvent>(); }
