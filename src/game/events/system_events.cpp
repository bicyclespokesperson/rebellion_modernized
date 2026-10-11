#include "events/system_events.h"

#include "events/event_helpers.h"
#include "events/system_events_decl.h"
#include "galaxy/system.h"
#include "gameobj/side_bits.h"

using events::succeedWithoutChecks;


SystemEvent303::SystemEvent303() : SourceEvent() {}

SystemEvent303::SystemEvent303(const SystemEvent303& other)  = default;

SystemEvent303::~SystemEvent303() = default;

SystemEvent303* SystemEvent303::Clone() { return NewOrNull<SystemEvent303>(*this); }

int SystemEvent303::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent303::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent303::Execute() {
    NotifyContext ctx(m_context);
    ApplySystemEvent303(&m_key, m_valueB, &ctx);
    return 0;
}

SystemEvent303* CreateSystemEvent303() { return NewOrNull<SystemEvent303>(); }

SystemEvent304::SystemEvent304() : SourceEvent() {}

SystemEvent304::SystemEvent304(const SystemEvent304& other)  = default;

SystemEvent304::~SystemEvent304() = default;

SystemEvent304* SystemEvent304::Clone() { return NewOrNull<SystemEvent304>(*this); }

int SystemEvent304::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent304::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent304::Execute() {
    NotifyContext ctx(m_context);
    ApplySystemEvent304(&m_key, m_valueB, &ctx);
    return 0;
}

SystemEvent304* CreateSystemEvent304() { return NewOrNull<SystemEvent304>(); }

SystemEvent305::SystemEvent305() : SourceEvent() {}

SystemEvent305::SystemEvent305(const SystemEvent305& other)  = default;

SystemEvent305::~SystemEvent305() = default;

SystemEvent305* SystemEvent305::Clone() { return NewOrNull<SystemEvent305>(*this); }

int SystemEvent305::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent305::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent305::Execute() {
    NotifyContext ctx(m_context);
    ApplySystemEvent305(&m_key, m_valueB, &ctx);
    return 0;
}

SystemEvent305* CreateSystemEvent305() { return NewOrNull<SystemEvent305>(); }

SystemEvent309::SystemEvent309() : SourceEvent() {}

SystemEvent309::SystemEvent309(const SystemEvent309& other)  = default;

SystemEvent309::~SystemEvent309() = default;

SystemEvent309* SystemEvent309::Clone() { return NewOrNull<SystemEvent309>(*this); }

int SystemEvent309::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent309::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent309::Execute() {
    NotifyContext ctx(m_context);
    ApplySystemEvent309(&m_key, m_valueB, &ctx);
    return 0;
}

SystemEvent309* CreateSystemEvent309() { return NewOrNull<SystemEvent309>(); }

SystemEvent310::SystemEvent310() : SourceEvent() {}

SystemEvent310::SystemEvent310(const SystemEvent310& other)  = default;

SystemEvent310::~SystemEvent310() = default;

SystemEvent310* SystemEvent310::Clone() { return NewOrNull<SystemEvent310>(*this); }

int SystemEvent310::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent310::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent310::Execute() {
    NotifyContext ctx(m_context);
    ApplySystemEvent310(&m_key, m_valueB, &ctx);
    return 0;
}

SystemEvent310* CreateSystemEvent310() { return NewOrNull<SystemEvent310>(); }

SystemEvent30f::SystemEvent30f() : SourceEvent() {}

SystemEvent30f::SystemEvent30f(const SystemEvent30f& other)  = default;

SystemEvent30f::~SystemEvent30f() = default;

SystemEvent30f* SystemEvent30f::Clone() { return NewOrNull<SystemEvent30f>(*this); }

int SystemEvent30f::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent30f::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent30f::Execute() {
    NotifyContext ctx(m_context);
    ApplySystemEvent30f(&m_key, m_valueB, &ctx);
    return 0;
}

SystemEvent30f* CreateSystemEvent30f() { return NewOrNull<SystemEvent30f>(); }

SystemEvent31b::SystemEvent31b() : SourceEvent() {}

SystemEvent31b::SystemEvent31b(const SystemEvent31b& other)  = default;

SystemEvent31b::~SystemEvent31b() = default;

SystemEvent31b* SystemEvent31b::Clone() { return NewOrNull<SystemEvent31b>(*this); }

int SystemEvent31b::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent31b::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent31b::Execute() {
    NotifyContext ctx(m_context);
    SideBits first;
    SideBits second;
    new (&first) SideBits(m_valueA);
    new (&second) SideBits(m_valueB);
    ApplySystemEvent31b(&m_key, &first, &second, &ctx);
    return 0;
}

SystemEvent31b* CreateSystemEvent31b() { return NewOrNull<SystemEvent31b>(); }

SystemEvent311::SystemEvent311() : SourceEvent() {}

SystemEvent311::SystemEvent311(const SystemEvent311& other)  = default;

SystemEvent311::~SystemEvent311() = default;

SystemEvent311* SystemEvent311::Clone() { return NewOrNull<SystemEvent311>(*this); }

int SystemEvent311::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent311::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent311::Execute() {
    NotifyContext ctx(m_context);
    ApplySystemEvent311(&m_key, m_valueB, &ctx);
    return 0;
}

SystemEvent311* CreateSystemEvent311() { return NewOrNull<SystemEvent311>(); }

SystemEvent319::SystemEvent319() : SourceEvent() {}

SystemEvent319::SystemEvent319(const SystemEvent319& other)  = default;

SystemEvent319::~SystemEvent319() = default;

SystemEvent319* SystemEvent319::Clone() { return NewOrNull<SystemEvent319>(*this); }

int SystemEvent319::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent319::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int SystemEvent319::Execute() {
    NotifyContext ctx(m_context);
    ObjectKey key;
    GetKey(&key);
    System* system = static_cast<System*>(GetMasterInFamilies_90_98(&key));
    if (system) {
        system->SyncViewForEvent(0xb, 1, &ctx);
        system->SyncViewForEvent(0xb, 2, &ctx);
    }
    return 0;
}

ObjectKey* SystemEvent319::GetKey(ObjectKey* out) {
    *out = m_key;
    return out;
}

SystemEvent319* CreateSystemEvent319() { return NewOrNull<SystemEvent319>(); }
