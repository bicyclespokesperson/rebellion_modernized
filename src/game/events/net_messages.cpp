#include "events/net_messages.h"

#include "core/checked_new.h"
#include "events/event_helpers.h"
#include "economy/side.h"
#include "events/game_clock_hooks.h"
#include "gameobj/object_queries.h"
#include "gameobj/side_bits.h"

using events::succeedWithoutChecks;

// ---- NetMsg1 --------------------------------------------------------------------------------------------------

NetMsg1::NetMsg1() : SidedMessage(), m_syncPoint(0), m_checksums() {}

NetMsg1::NetMsg1(int syncPoint) : SidedMessage(), m_syncPoint(syncPoint), m_checksums() {}

NetMsg1::~NetMsg1() {  }

int NetMsg1::Read(istream* in) {
    int result = SidedMessage::Read(in);
    if (!result) return result;
    StreamRead32(in, &m_syncPoint);
    return m_checksums.Load(in);
}

int NetMsg1::Write(ostream* out) {
    int result = SidedMessage::Write(out);
    if (!result) return result;
    StreamWrite32(out, &m_syncPoint);
    return m_checksums.Save(out);
}

int NetMsg1::CheckEnabled(StatusCode* status) { return succeedWithoutChecks(status); }

int NetMsg1::CheckValid(StatusCode* status) { return succeedWithoutChecks(status); }

int NetMsg1::Execute(StatusCode* status) {
    succeedWithoutChecks(status);
    return SetSideSnapshot(reinterpret_cast<GameStateSnapshot*>(&m_checksums), m_side);
}

void* CreateNetMsg1() { return NewOrNull<NetMsg1>(); }

// ---- NetMsg2 --------------------------------------------------------------------------------------------------

NetMsg2::NetMsg2() : SidedMessage(), m_syncPoint(0) {}

NetMsg2::~NetMsg2() {  }

int NetMsg2::Read(istream* in) {
    int result = SidedMessage::Read(in);
    if (result) StreamRead32(in, &m_syncPoint);
    return result;
}

int NetMsg2::Write(ostream* out) {
    int result = SidedMessage::Write(out);
    if (result) StreamWrite32(out, &m_syncPoint);
    return result;
}

int NetMsg2::CheckEnabled(StatusCode* status) { return succeedWithoutChecks(status); }

int NetMsg2::CheckValid(StatusCode* status) { return succeedWithoutChecks(status); }

int NetMsg2::Execute(StatusCode* status) { return succeedWithoutChecks(status); }

void* CreateNetMsg2() { return NewOrNull<NetMsg2>(); }

// ---- NetMsg280 ------------------------------------------------------------------------------------------------

NetMsg280::NetMsg280() : SidedMessage() {}

NetMsg280::~NetMsg280() {  }

int NetMsg280::Read(istream* in) {
    int result = SidedMessage::Read(in);
    if (result) StreamRead32(in, &m_value);
    return result;
}

int NetMsg280::Write(ostream* out) {
    int result = SidedMessage::Write(out);
    if (result) StreamWrite32(out, &m_value);
    return result;
}

int NetMsg280::GetKind() { return 0x280; }

int NetMsg280::CheckEnabled(StatusCode* status) { return succeedWithoutChecks(status); }

int NetMsg280::CheckValid(StatusCode* status) { return succeedWithoutChecks(status); }

int NetMsg280::Execute(StatusCode* status) {
    succeedWithoutChecks(status);
    Side* side = static_cast<Side*>(GetGameRootSideMemberThunk(m_side));
    return side ? side->SetUnknownC4(m_value) : 0;
}

void* CreateNetMsg280() { return NewOrNull<NetMsg280>(); }

// ---- NetMsg4 --------------------------------------------------------------------------------------------------

NetMsg4::NetMsg4() : MessageBase(), m_voteId(0), m_voteState(0), m_voteKey() { m_voteTime.m_value = 0; }

NetMsg4::~NetMsg4() {  }

int NetMsg4::Read(istream* in) {
    int result = MessageBase::Read(in);
    StreamRead32(in, &m_voteId);
    StreamRead32(in, &m_voteState);
    ReadGameTime(&m_voteTime, in);
    reinterpret_cast<SideBits*>(&m_voteKey)->Read(in);
    return result;
}

int NetMsg4::Write(ostream* out) {
    int result = MessageBase::Write(out);
    StreamWrite32(out, &m_voteId);
    StreamWrite32(out, &m_voteState);
    StreamWrite32(out, &m_voteTime);
    reinterpret_cast<SideBits*>(&m_voteKey)->Write(out);
    return result;
}

void* CreateNetMsg4() { return NewOrNull<NetMsg4>(); }

// ---- NetMsg5 --------------------------------------------------------------------------------------------------

NetMsg5::NetMsg5() : SidedMessage(), m_value(0) {}

NetMsg5::~NetMsg5() {  }

int NetMsg5::Read(istream* in) {
    int result = SidedMessage::Read(in);
    StreamRead32(in, &m_value);
    return result;
}

int NetMsg5::Write(ostream* out) {
    int result = SidedMessage::Write(out);
    StreamWrite32(out, &m_value);
    return result;
}

// BUG(original): Execute has no return statement; EAX still holds the object pointer.
int NetMsg5::Execute(StatusCode*) { return static_cast<int>(reinterpret_cast<uintptr_t>(this)); }

void* CreateNetMsg5() { return NewOrNull<NetMsg5>(); }

// ---- NetMsg13 -------------------------------------------------------------------------------------------------

NetMsg13::NetMsg13() : MessageBase() {}

NetMsg13::~NetMsg13() {  }

int NetMsg13::Read(istream* in) { return MessageBase::Read(in); }

int NetMsg13::Write(ostream* out) { return MessageBase::Write(out); }

void* CreateNetMsg13() { return NewOrNull<NetMsg13>(); }

// ---- NetMsg15 -------------------------------------------------------------------------------------------------

NetMsg15::NetMsg15() : MessageBase(), m_valueA(0), m_valueB(0), m_valueC(0) {}

NetMsg15::~NetMsg15() {  }

int NetMsg15::Read(istream* in) {
    int result = MessageBase::Read(in);
    StreamRead32(in, &m_valueA);
    StreamRead32(in, &m_valueB);
    StreamRead32(in, &m_valueC);
    return result;
}

int NetMsg15::Write(ostream* out) {
    int result = MessageBase::Write(out);
    StreamWrite32(out, &m_valueA);
    StreamWrite32(out, &m_valueB);
    StreamWrite32(out, &m_valueC);
    return result;
}

void* CreateNetMsg15() { return NewOrNull<NetMsg15>(); }
