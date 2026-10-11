#include "multiplayer/comm_packets.h"
#include "core/icf_stubs.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

#include "core/checked_new.h"
#include "core/stream_io.h"
#include "multiplayer/comm_mgr.h"


// The shared empty function the release build left in place of the debug trace (it pops one argument).

namespace {

constexpr int kLongMessageLength = 0x1ff;
constexpr char kLongMessageFormat[] = "Long Message %u \n";

CommMgr* TraceTarget() { return GetCommMgr(); }

}  // namespace

// ---- ServiceProviderInfo / SessionInfo ------------------------------------------------------------------------

ServiceProviderInfo::ServiceProviderInfo(const GUID* guid, const char* name, DWORD majorVersion, DWORD minorVersion,
                                         DWORD_PTR context)
    : IdNode(), m_name() {
    m_guid = guid;
    m_name = name;
    m_context = context;
    m_minorVersion = minorVersion;
    m_majorVersion = majorVersion;
}

ServiceProviderInfo::ServiceProviderInfo(const ServiceProviderInfo& other) : IdNode(), m_name() {
    m_guid = other.m_guid;
    m_name = other.m_name.m_text;
    m_majorVersion = other.m_majorVersion;
    m_minorVersion = other.m_minorVersion;
    m_context = other.m_context;
}

ServiceProviderInfo::~ServiceProviderInfo() = default;

SessionInfo::SessionInfo(const DPSESSIONDESC2* description) : IdNode(), m_name() {
    if (description) {
        m_instance = description->guidInstance;
        m_name = description->lpszSessionNameA;
    }
}

SessionInfo::SessionInfo(const SessionInfo& other) : IdNode(), m_name() {
    m_instance = other.m_instance;
    m_name.Assign(other.m_name);
}

SessionInfo::~SessionInfo() = default;

ServiceProviderList::ServiceProviderList() : IdAvlTree() {}

ServiceProviderList::~ServiceProviderList() = default;

SessionList::SessionList() : IdAvlTree() {}

SessionList::~SessionList() = default;

LobbyConnection::LobbyConnection() : m_launched(0), m_host(0), m_dplay(nullptr) {}

LobbyConnection::~LobbyConnection() = default;

// ---- CommMessage -----------------------------------------------------------------------------------------------

CommMessage::CommMessage(const char* buffer) : m_magic(0), m_kind(0), m_arg(0), m_a(0), m_b(0), m_c(0) {
    ostrstream_ctor(&m_stream, 1);
    if (!buffer) return;
    IStrStream in;
    istrstream_ctor_str(&in, const_cast<char*>(buffer), 1);
    StreamRead32(in.In(), &m_magic);
    StreamRead32(in.In(), &m_kind);
    StreamRead32(in.In(), &m_arg);
    StreamRead32(in.In(), &m_a);
    StreamRead32(in.In(), &m_b);
    StreamRead32(in.In(), &m_c);
    istrstream_dtor(in.Ios());
    ios_dtor(in.Ios());
}

CommMessage::CommMessage(int kind, int arg, int a, int b, int c) {
    ostrstream_ctor(&m_stream, 1);
    m_kind = kind;
    m_arg = arg;
    m_b = b;
    m_magic = kCommMessageMagic;
    m_a = a;
    m_c = c;
}

CommMessage::~CommMessage() {
    ostrstream_dtor(m_stream.Ios());
    ios_dtor(m_stream.Ios());

}

char* CommMessage::Serialize() {
    ostream* out = m_stream.Out();
    ClearIosState(out);
    ostream_seekp_pos(out, 0);
    StreamWrite32(out, &m_magic);
    StreamWrite32(out, &m_kind);
    StreamWrite32(out, &m_arg);
    StreamWrite32(out, &m_a);
    StreamWrite32(out, &m_b);
    StreamWrite32(out, &m_c);
    return strstreambuf_str(StreambufOf(out));
}

// ---- CommAckRecord ---------------------------------------------------------------------------------------------

CommAckRecord::CommAckRecord(uint32_t packetId) {
    m_packetId = packetId;
    for (int player = 0; player < kCommMaxPlayers; ++player) {
        m_sendTime[player] = 0;
        m_state[player] = 0;
    }
}

int CommAckRecord::Acknowledge(int player) {
    if (m_state[player] != 1) return 0;
    m_state[player] = 3;
    return 1;
}

int CommAckRecord::SetState(uint32_t player, int state) {
    if (player >= kCommMaxPlayers) return 0;
    m_state[player] = state;
    return 1;
}

int CommAckRecord::IsSettled() {
    int settled = 0;
    CommMgr* comm = GetCommMgr();
    for (uint32_t player = 0; player < 15; ++player) {
        if (comm && static_cast<uint32_t>(comm->GetLocalIndex()) == player) continue;
        const int state = m_state[player];
        if (state == 1) return 0;
        if (state == 3 || state == 2) settled = 1;
    }
    return settled;
}

int CommAckRecord::GetState(int player) const { return m_state[player]; }

uint32_t CommAckRecord::GetSendTime(int player) const { return m_sendTime[player]; }

void CommAckRecord::MarkSent(int player) {
    GetCommMgr();
    m_sendTime[player] = timeGetTime();
    m_state[player] = 1;
}

// ---- CommPacket ------------------------------------------------------------------------------------------------

CommPacket::CommPacket(const void* data, uint32_t size, uint8_t category, char part, char partCount, int reliable)
    : IdNode() {
    CommMgr* comm = GetCommMgr();
    m_ack = nullptr;
    memcpy(m_wire.m_payload, data, size & 0xffff);
    m_wire.m_partCount = partCount;
    m_wire.m_part = part;
    m_wire.m_length = static_cast<uint16_t>(size);
    m_wire.m_reliable = static_cast<uint8_t>(reliable);
    ++g_commPacketCounter;
    const uint32_t id = (g_commPacketCounter & 0xf0ffffff) | (static_cast<uint32_t>(category & 0xf) << 24);
    m_ack = NewOrNull<CommAckRecord>(id);
    SetId(id);
    if (reliable) comm->m_pending.AddWithId(this);
    m_wire.m_id = id;
}

CommPacket::CommPacket(const void* data, uint32_t size, uint32_t id, char part, char partCount, int reliable)
    : IdNode() {
    CommMgr* comm = GetCommMgr();
    m_ack = nullptr;
    memcpy(m_wire.m_payload, data, size & 0xffff);
    m_wire.m_part = part;
    m_wire.m_partCount = partCount;
    m_wire.m_length = static_cast<uint16_t>(size);
    m_wire.m_reliable = static_cast<uint8_t>(reliable);
    m_ack = NewOrNull<CommAckRecord>(id);
    SetId(id);
    if (reliable) comm->m_pending.AddWithId(this);
    m_wire.m_id = id;
}

CommPacket::~CommPacket() {
    ::operator delete(m_ack);

}

int CommPacket::FirstFragmentId() {
    if (m_ack) return static_cast<int>(m_ack->m_packetId) - m_wire.m_part + 1;
    return 1 - m_wire.m_part;
}

int CommPacket::Pack(void* out, uint16_t capacity) {
    if (capacity < 4) return 0;
    const uint32_t total = m_wire.m_length + kCommPacketHeaderSize;
    if (total > capacity) return 0;
    memcpy(out, &m_wire, total);
    return m_wire.m_length + kCommPacketHeaderSize;
}

// ---- ReceivedPacket --------------------------------------------------------------------------------------------

ReceivedPacket::ReceivedPacket(const void* wire, uint32_t size) : IdNode() {
    memcpy(&m_wire, wire, size & 0xffff);
    SetId(m_wire.m_id);
}

ReceivedPacket::~ReceivedPacket() = default;

uint16_t ReceivedPacket::CopyPayload(void* out, uint16_t capacity) const {
    if (m_wire.m_length > capacity) return 0;
    memcpy(out, m_wire.m_payload, m_wire.m_length);
    return m_wire.m_length;
}

// ---- CommOutgoing ----------------------------------------------------------------------------------------------

CommOutgoing::CommOutgoing(const void* data, uint32_t size, uint32_t category, int reliable) : IdNode() {
    CommMgr* comm = GetCommMgr();
    m_firstPacketId = 0;
    m_unused1d = 0;
    m_partCount = static_cast<int8_t>((size + 0x1f5) / 0x1f6);
    char part = 1;
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    while (size > 0) {
        const bool lastPart = size <= kCommMaxPayload;
        const uint32_t length = lastPart ? size : kCommMaxPayload;
        CommPacket* packet =
            NewOrNull<CommPacket>(bytes, length, static_cast<uint8_t>(category), part, m_partCount, reliable);
        if (m_firstPacketId == 0) m_firstPacketId = packet->m_ack ? packet->m_ack->m_packetId : 0;
        if (!packet->m_wire.m_reliable) comm->m_pending.AddWithId(packet);
        if (lastPart) {
            size = 0;
        } else {
            bytes += kCommMaxPayload;
            size -= kCommMaxPayload;
            ++part;
        }
    }
    SetId(m_firstPacketId);
}

CommOutgoing::~CommOutgoing() = default;

// ---- PartialMessage --------------------------------------------------------------------------------------------

PartialMessage::PartialMessage(uint32_t firstPacketId) : IdNode() {
    m_partsSeen = 0;
    m_partCount = 0;
    m_length = 0;
    m_firstId = firstPacketId;
    SetId(firstPacketId);
}

PartialMessage::~PartialMessage() = default;

int PartialMessage::IsComplete() const { return m_partsSeen == m_partCount; }

uint32_t PartialMessage::GetCategory() const { return (m_firstId >> 24) & 0xf; }

int PartialMessage::AddFragment(ReceivedPacket* packet) {
    CommMgr* comm = GetCommMgr();
    if (comm && packet) {
        if (!comm->m_received.FindById(packet->m_wire.m_id)) {
            m_partCount = packet->m_wire.m_partCount;
            ++m_partsSeen;
            m_length += packet->m_wire.m_length;
            comm->m_received.AddWithId(packet);
        }
    }
    return IsComplete();
}

CommBuffer* PartialMessage::Assemble() {
    CommMgr* comm = GetCommMgr();
    uint32_t total = 0;
    for (int8_t part = 1; part <= m_partsSeen; ++part) {
        auto* packet = static_cast<ReceivedPacket*>(comm->m_received.FindById(part + m_firstId - 1));
        total += packet->m_wire.m_length;
    }
    if (total >= kLongMessageLength) {
        char text[100];
        sprintf(text, kLongMessageFormat, total);
        IgnoreOneArgument(TraceTarget(), reinterpret_cast<uintptr_t>(text));
    }
    CommBuffer* buffer = static_cast<CommBuffer*>(::operator new(sizeof(CommBuffer)));
    if (buffer) {
        buffer->m_size = total;
        void* data = nullptr;
        if (total != 0) data = ::operator new(total);
        buffer->m_data = data;
    }
    uint8_t* out = static_cast<uint8_t*>(buffer->m_data);
    for (int8_t part = 1; part <= m_partsSeen; ++part) {
        auto* packet = static_cast<ReceivedPacket*>(comm->m_received.FindById(part + m_firstId - 1));
        const uint16_t copied = packet->CopyPayload(out, static_cast<uint16_t>(total));
        out += copied;
        total -= copied;
    }
    return buffer;
}

int PartialMessage::ReleaseFragments() {
    CommMgr* comm = GetCommMgr();
    for (int8_t part = 0; part < m_partsSeen; ++part) comm->m_received.DeleteById(m_firstId + part);
    return 1;
}

// ---- PacketBuffer ----------------------------------------------------------------------------------------------

PacketBuffer::PacketBuffer() : m_count(0), m_used(1) { m_write = m_data; }

PacketBuffer::~PacketBuffer() = default;

int8_t PacketBuffer::GetCount() const { return m_count; }

uint8_t* PacketBuffer::GetWritePointer() const { return m_write; }

ReceivedPacket* PacketBuffer::PopPacket() {
    if (GetCount() <= 0) return nullptr;
    const uint32_t length = *reinterpret_cast<uint16_t*>(m_data + 4) + kCommPacketHeaderSize;
    if (length > kCommPacketBufferSize - 1) {
        Reset();
        return nullptr;
    }
    ReceivedPacket* packet = NewOrNull<ReceivedPacket>(m_data, length);
    --m_count;
    memmove(m_data, m_data + length, kCommPacketBufferSize - 1 - length);
    return packet;
}

int PacketBuffer::Append(CommPacket* packet) {
    const int added = packet->Pack(m_write, static_cast<uint16_t>(kCommPacketBufferSize - m_used));
    if (added == 0) return 0;
    m_used = static_cast<uint16_t>(m_used + added);
    ++m_count;
    m_write += added;
    return 1;
}

// ---- SendBuffer / ReceiveBuffer --------------------------------------------------------------------------------

SendBuffer::SendBuffer() : PacketBuffer() {}

void SendBuffer::Reset() {
    memset(&m_count, 0, kCommPacketBufferSize);
    m_write = m_data;
    m_used = 1;
}

SendBuffer::~SendBuffer() = default;

ReceiveBuffer::ReceiveBuffer() : PacketBuffer() {
    m_write = reinterpret_cast<uint8_t*>(&m_count);
    m_used = 0;
    m_count = 0;
    m_fromPlayer = -1;
}

void ReceiveBuffer::Reset() {
    memset(&m_count, 0, kCommPacketBufferSize);
    m_count = 0;
    m_write = reinterpret_cast<uint8_t*>(&m_count);
    m_used = 0;
}

ReceiveBuffer::~ReceiveBuffer() = default;
