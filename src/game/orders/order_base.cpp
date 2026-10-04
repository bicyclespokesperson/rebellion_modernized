#include "orders/order_base.h"

#include <new>

#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_database.h"
#include "orders/order_checks.h"

struct OrderFactoryEntry {
    int m_kind;
    void* (*m_create)();
};

namespace {

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusRefused = 1;
constexpr int kMaxOrderFactories = 0x3ff;

bool isOk(const StatusCode* status) { return status->m_second == static_cast<uint32_t>(kStatusOk); }

void setStatus(StatusCode* status, int code) { *status = StatusCode(kStatusCategory, code); }

bool isTextMessageKind(int kind) { return kind == 1 || kind == 2 || kind == 3 || kind == 5 || kind == 0x11; }

}  // namespace

extern OrderFactoryEntry g_orderFactories[];  // 0x6b6fe0
extern int g_orderFactoryCount;               // 0x6b8fd8

// ---- ActivityRequest ------------------------------------------------------------------------------------------

ActivityRequest::ActivityRequest() : m_side(0), m_unk04(0), m_target(), m_kind(0) {}

ActivityRequest::ActivityRequest(const ActivityRequest& other)
    : m_side(other.m_side), m_unk04(other.m_unk04), m_target(other.m_target), m_kind(other.m_kind) {}

// ---- MessageBase ----------------------------------------------------------------------------------------------

MessageBase::MessageBase() : IdNode() { m_unk1c = 1; }

MessageBase::~MessageBase() { REB_KEEP_VPTR_STORE(); }

int MessageBase::Read(istream* in) {
    // BUG(original): the id is read into the argument slot that held the stream pointer, so a failed read leaves
    // the stream pointer as the id.
    uint32_t id = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(in));
    StreamRead32(in, &id);
    SetId(id);
    StreamRead32(in, &m_unk1c);
    return 1;
}

int MessageBase::Write(ostream* out) {
    uint32_t id = m_id;
    StreamWrite32(out, &id);
    StreamWrite32(out, &m_unk1c);
    return 1;
}

// ---- SidedMessage ---------------------------------------------------------------------------------------------

SidedMessage::SidedMessage() : MessageBase() {
    m_side = 0;
    m_unk28 = 0;
    m_unk24 = 1;
}

SidedMessage::~SidedMessage() { REB_KEEP_VPTR_STORE(); }

int SidedMessage::Read(istream* in) {
    int result = MessageBase::Read(in);
    if (result != 0) {
        StreamRead32(in, &m_side);
        StreamRead32(in, &m_unk24);
        StreamRead32(in, &m_unk28);
    }
    return result;
}

int SidedMessage::Write(ostream* out) {
    int result = MessageBase::Write(out);
    if (result != 0) {
        StreamWrite32(out, &m_side);
        StreamWrite32(out, &m_unk24);
        StreamWrite32(out, &m_unk28);
    }
    return result;
}

void* CreateOrderByKind(int kind) {
    void* (*create)() = nullptr;
    for (int i = 0; i < g_orderFactoryCount && !create; ++i) {
        if (g_orderFactories[i].m_kind == kind) create = g_orderFactories[i].m_create;
    }
    return create ? create() : nullptr;
}

int RegisterOrderFactory(int kind, void* (*factory)()) {
    bool absent = true;
    for (int i = 0; i < g_orderFactoryCount && absent; ++i) absent = g_orderFactories[i].m_kind != kind;
    if (absent && g_orderFactoryCount + 1 < kMaxOrderFactories) {
        g_orderFactories[g_orderFactoryCount].m_kind = kind;
        g_orderFactories[g_orderFactoryCount].m_create = factory;
        ++g_orderFactoryCount;
    }
    return 0;
}

void* CreateMessageByKind(int kind) {
    if (kind < 0x200 && !isTextMessageKind(kind)) return nullptr;
    return CreateOrderByKind(kind);
}

// ---- OrderBase ------------------------------------------------------------------------------------------------

OrderBase::OrderBase() : SidedMessage(), m_team(), m_target() {
    m_arg = 0;
    m_unk3c = 0;
    m_commands = nullptr;
    uint32_t ownsQueue = 0;
    if (m_unk1c != 0) {
        m_commands = NewOrNull<CommandQueue>();
        ownsQueue = m_commands != nullptr;
    }
    m_unk1c = ownsQueue;
}

OrderBase::~OrderBase() {
    REB_KEEP_VPTR_STORE();
    if (m_commands) {
        delete m_commands;
        m_commands = nullptr;
    }
}

void OrderBase::SetTeam(const ObjectKeyValueSet* team) { m_team = *team; }

ObjectKeyValueSet* OrderBase::GetTeam() { return &m_team; }

void OrderBase::SetArg(int arg) { m_arg = arg; }

int OrderBase::GetArg() { return m_arg; }

void OrderBase::SetTarget(const ObjectKey* target) { m_target = *target; }

ObjectKey* OrderBase::GetTarget(ObjectKey* out) {
    *out = m_target;
    return out;
}

ObjectKey* OrderBase::TargetRef() { return &m_target; }

void OrderBase::CopyTarget(ObjectKey* out) { *out = m_target; }

void OrderBase::BuildRequest(ActivityRequest* request) {
    request->m_side = m_side;
    ObjectKey target;
    request->m_target = *GetTarget(&target);
    request->m_kind = GetKind();
    request->m_unk04 = GetArg();
}

int OrderBase::CollectTeamDependencies(ObjectKeyValueSet* out) {
    out->Clear();
    ActivityRequest request;
    BuildRequest(&request);
    StatusCode status(kStatusCategory, kStatusOk);
    return CollectKeyDependencies(&request, GetTeam(), out, &status);
}

int OrderBase::IsChecked() {
    int ownsQueue = static_cast<int>(m_unk1c);
    StatusCode status(kStatusCategory, kStatusOk);
    ActivityRequest request;
    BuildRequest(&request);
    CheckEnabled(&status);
    int anyChecked = 0;
    int anyUnchecked = 0;
    int ok = ownsQueue;
    if (ok != 0) ok = ClassifyCommands(&request, m_commands, &anyChecked, &anyUnchecked);
    int result = 0;
    if (ok != 0) {
        if (Virtual21() == 0)
            result = anyChecked;
        else
            result = anyChecked != 0 && anyUnchecked == 0;
    }
    return result;
}

int OrderBase::CheckEnabled(StatusCode* status) {
    int ownsQueue = static_cast<int>(m_unk1c);
    *status = StatusCode(kStatusCategory, kStatusOk);
    ActivityRequest request;
    BuildRequest(&request);
    if (ownsQueue != 0) {
        if (isOk(status)) ownsQueue = CheckCommand(status);
        if (ownsQueue != 0) {
            if (isOk(status)) ownsQueue = CheckCommandsEnabled(&request, m_commands, status);
            if (ownsQueue != 0) return isOk(status);
        }
    }
    setStatus(status, kStatusRefused);
    return isOk(status);
}

int OrderBase::CheckValid(StatusCode* status) {
    int ownsQueue = static_cast<int>(m_unk1c);
    *status = StatusCode(kStatusCategory, kStatusOk);
    ActivityRequest request;
    BuildRequest(&request);
    CheckEnabled(status);
    if (ownsQueue != 0) {
        if (isOk(status)) ownsQueue = CheckDestination(status);
        if (ownsQueue != 0) {
            if (isOk(status)) ownsQueue = CheckCommandsValid(&request, m_commands, status);
            if (ownsQueue != 0) return isOk(status);
        }
    }
    setStatus(status, kStatusRefused);
    return isOk(status);
}

int OrderBase::Execute(StatusCode* status) {
    int ownsQueue = static_cast<int>(m_unk1c);
    *status = StatusCode(kStatusCategory, kStatusOk);
    CheckValid(status);
    if (ownsQueue != 0) {
        if (isOk(status)) SetArg(CallGameHook2AFC());
        if (ownsQueue != 0 && isOk(status)) {
            ActivityRequest request;
            BuildRequest(&request);
            ExecuteCommands(&request, m_commands);
        }
    }
    return 1;
}

int OrderBase::CheckCommand(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    Command* command = CreateCommand();
    int result = 0;
    if (command != nullptr) {
        ActivityRequest request;
        BuildRequest(&request);
        command->SetRequest(&request);
        result = CheckCommandForTeam(command, &m_team, m_commands, status);
        delete command;
    }
    if (result == 0) setStatus(status, kStatusRefused);
    return result;
}

int OrderBase::CheckDestination(StatusCode* status) {
    int ownsQueue = static_cast<int>(m_unk1c);
    *status = StatusCode(kStatusCategory, kStatusOk);
    ActivityRequest request;
    BuildRequest(&request);
    if (ownsQueue == 0) setStatus(status, kStatusRefused);
    return ownsQueue;
}

// ---- TextMessage ----------------------------------------------------------------------------------------------

TextMessage::TextMessage() : OrderBase(), m_text() {}

TextMessage::~TextMessage() { REB_KEEP_VPTR_STORE(); }

int TextMessage::Read(istream* in) {
    int result = OrderBase::Read(in);
    if (result != 0) m_text.ReadFrom(in);
    return result;
}

int TextMessage::Write(ostream* out) {
    int result = OrderBase::Write(out);
    if (result != 0) m_text.WriteTo(out);
    return result;
}

int TextMessage::CheckEnabled(StatusCode* status) { return OrderBase::CheckEnabled(status); }

int TextMessage::CheckValid(StatusCode* status) { return OrderBase::CheckValid(status); }

int TextMessage::Execute(StatusCode* status) {
    *status = StatusCode(kStatusCategory, kStatusOk);
    return OrderBase::Execute(status);
}

int TextMessage::CheckCommand(StatusCode* status) { return OrderBase::CheckCommand(status); }

int TextMessage::CheckDestination(StatusCode* status) { return OrderBase::CheckDestination(status); }

int OrderBase::Read(istream* in) {
    int result = SidedMessage::Read(in);
    if (result != 0) {
        StreamRead32(in, &m_arg);
        StreamRead32(in, &m_unk3c);
        m_target.Read(in);
        m_team.Read(in);
    }
    return result;
}

int OrderBase::Write(ostream* out) {
    int result = SidedMessage::Write(out);
    if (result != 0) {
        StreamWrite32(out, &m_arg);
        StreamWrite32(out, &m_unk3c);
        m_target.Write(out);
        m_team.Write(out);
    }
    return result;
}
