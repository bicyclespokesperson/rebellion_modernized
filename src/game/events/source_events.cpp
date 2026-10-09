#include "character/character_mgr.h"
#include "events/source_events.h"
#include "events/source_events_decl.h"

#include "events/event_helpers.h"
#include "gameobj/game_obj.h"
#include "story/mission_mgr.h"

using events::kStatusCategory;
using events::kStatusMissingTarget;
using events::kStatusOk;
using events::succeeded;
using events::succeedWithoutChecks;


Event301::Event301() : SourceEvent() {}

Event301::Event301(const Event301& other) : SourceEvent(other) {}

Event301::~Event301() { REB_KEEP_VPTR_STORE(); }

Event301* Event301::Clone() { return NewOrNull<Event301>(*this); }

int Event301::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event301::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event301::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent301(&m_key, &m_keyA, &m_keyB, m_valueB, &ctx);
    return 0;
}

Event301* CreateEvent301() { return NewOrNull<Event301>(); }

Event31d::Event31d() : SourceEvent() {}

Event31d::Event31d(const Event31d& other) : SourceEvent(other) {}

Event31d::~Event31d() { REB_KEEP_VPTR_STORE(); }

Event31d* Event31d::Clone() { return NewOrNull<Event31d>(*this); }

int Event31d::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event31d::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event31d::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent31d(&m_key, &m_keyA, &m_keyB, m_valueB, &ctx);
    return 0;
}

Event31d* CreateEvent31d() { return NewOrNull<Event31d>(); }

Event300::Event300() : SourceEvent() {}

Event300::Event300(const Event300& other) : SourceEvent(other) {}

Event300::~Event300() { REB_KEEP_VPTR_STORE(); }

Event300* Event300::Clone() { return NewOrNull<Event300>(*this); }

int Event300::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event300::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event300::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent300(&m_key, &m_keyA, &m_keyB, m_valueB, &ctx);
    return 0;
}

Event300* CreateEvent300() { return NewOrNull<Event300>(); }

Event308::Event308() : SourceEvent() {}

Event308::Event308(const Event308& other) : SourceEvent(other) {}

Event308::~Event308() { REB_KEEP_VPTR_STORE(); }

Event308* Event308::Clone() { return NewOrNull<Event308>(*this); }

int Event308::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event308::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event308::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent308(&m_key, m_valueB, &ctx);
    return 0;
}

Event308* CreateEvent308() { return NewOrNull<Event308>(); }

Event306::Event306() : SourceEvent() {}

Event306::Event306(const Event306& other) : SourceEvent(other) {}

Event306::~Event306() { REB_KEEP_VPTR_STORE(); }

Event306* Event306::Clone() { return NewOrNull<Event306>(*this); }

int Event306::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event306::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event306::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent306(&m_key, m_valueB, &ctx);
    return 0;
}

Event306* CreateEvent306() { return NewOrNull<Event306>(); }

Event316::Event316() : SourceEvent() {}

Event316::Event316(const Event316& other) : SourceEvent(other) {}

Event316::~Event316() { REB_KEEP_VPTR_STORE(); }

Event316* Event316::Clone() { return NewOrNull<Event316>(*this); }

int Event316::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event316::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event316::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent316(&m_key, m_valueB, &ctx);
    return 0;
}

Event316* CreateEvent316() { return NewOrNull<Event316>(); }

Event307::Event307() : SourceEvent() {}

Event307::Event307(const Event307& other) : SourceEvent(other) {}

Event307::~Event307() { REB_KEEP_VPTR_STORE(); }

Event307* Event307::Clone() { return NewOrNull<Event307>(*this); }

int Event307::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event307::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event307::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent307(&m_key, m_valueB, &ctx);
    return 0;
}

Event307* CreateEvent307() { return NewOrNull<Event307>(); }

Event302::Event302() : SourceEvent() {}

Event302::Event302(const Event302& other) : SourceEvent(other) {}

Event302::~Event302() { REB_KEEP_VPTR_STORE(); }

Event302* Event302::Clone() { return NewOrNull<Event302>(*this); }

int Event302::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event302::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event302::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent302(&m_key, m_valueA, m_valueB, &ctx);
    return 0;
}

Event302* CreateEvent302() { return NewOrNull<Event302>(); }

Event318::Event318() : SourceEvent() {}

Event318::Event318(const Event318& other) : SourceEvent(other) {}

Event318::~Event318() { REB_KEEP_VPTR_STORE(); }

Event318* Event318::Clone() { return NewOrNull<Event318>(*this); }

int Event318::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event318::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event318::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent318(&m_key, m_valueA, m_valueB, &ctx);
    return 0;
}

Event318* CreateEvent318() { return NewOrNull<Event318>(); }

Event315::Event315() : SourceEvent() {}

Event315::Event315(const Event315& other) : SourceEvent(other) {}

Event315::~Event315() { REB_KEEP_VPTR_STORE(); }

Event315* Event315::Clone() { return NewOrNull<Event315>(*this); }

int Event315::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event315::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event315::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent315(&m_key, m_valueA, m_valueB, &ctx);
    return 0;
}

Event315* CreateEvent315() { return NewOrNull<Event315>(); }

Event30a::Event30a() : SourceEvent() {}

Event30a::Event30a(const Event30a& other) : SourceEvent(other) {}

Event30a::~Event30a() { REB_KEEP_VPTR_STORE(); }

Event30a* Event30a::Clone() { return NewOrNull<Event30a>(*this); }

int Event30a::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event30a::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event30a::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent30a(&m_key, m_valueA, m_valueB, &ctx);
    return 0;
}

Event30a* CreateEvent30a() { return NewOrNull<Event30a>(); }

Event320::Event320() : TwoKeyEvent() {}

Event320::Event320(const Event320& other) : TwoKeyEvent(other) {}

Event320::~Event320() { REB_KEEP_VPTR_STORE(); }

Event320* Event320::Clone() { return NewOrNull<Event320>(*this); }

int Event320::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event320::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event320::Execute() {
    NotifyContext ctx(m_context);
    ApplyEvent320(&m_key, &m_keyC, &m_keyD, &ctx);
    return 0;
}

Event320* CreateEvent320() { return NewOrNull<Event320>(); }

Event313::Event313() : SourceEvent() {}

Event313::Event313(const Event313& other) : SourceEvent(other) {}

Event313::~Event313() { REB_KEEP_VPTR_STORE(); }

Event313* Event313::Clone() { return NewOrNull<Event313>(*this); }

int Event313::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event313::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event313::Execute() {
    NotifyContext ctx(m_context);
    GameObjNotifier* manager = GetGameRootMember_b8();
    if (manager) static_cast<MissionMgr*>(manager)->MissionMgr::RunStoryPipelineForStoryKey2(&m_key, m_valueB, &ctx);
    return 0;
}

Event313* CreateEvent313() { return NewOrNull<Event313>(); }

Event31a::Event31a() : SourceEvent() {}

Event31a::Event31a(const Event31a& other) : SourceEvent(other) {}

Event31a::~Event31a() { REB_KEEP_VPTR_STORE(); }

Event31a* Event31a::Clone() { return NewOrNull<Event31a>(*this); }

int Event31a::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event31a::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event31a::Execute() {
    GameObj* object = static_cast<GameObj*>(GetMasterInFamilies_01_ff(&m_key));
    if (object) DeleteObjectViews(object);
    return 0;
}

Event31a* CreateEvent31a() { return NewOrNull<Event31a>(); }

Event317::Event317() : SourceEvent() {}

Event317::Event317(const Event317& other) : SourceEvent(other) {}

Event317::~Event317() { REB_KEEP_VPTR_STORE(); }

Event317* Event317::Clone() { return NewOrNull<Event317>(*this); }

int Event317::CheckEnabled(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event317::CheckValid(StatusCode* status) {
    return succeedWithoutChecks(status);
}

int Event317::Execute() {
    StatusCode status(kStatusCategory, kStatusOk);
    GameObjNotifier* owner = nullptr;
    int ok = CheckValid(&status);
    if (ok && succeeded(status)) {
        const StatusCode missing(kStatusCategory, kStatusMissingTarget);
        ok = ResolveMasterInFamilies_30_3c(&m_key, &owner, &missing, &status);
    }
    CharacterMgr* manager = nullptr;
    if (ok && succeeded(status)) {
        manager = GetCharacterManager();
        ok = manager != nullptr;
    }
    if (ok && succeeded(status)) {
        NotifyContext ctx(m_context);
        manager->RollCaptiveEscape(static_cast<Character*>(owner), m_valueB, &ctx);
    }
    return 0;
}

Event317* CreateEvent317() { return NewOrNull<Event317>(); }
