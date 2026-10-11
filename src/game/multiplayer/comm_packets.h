#pragma once
// The wire and bookkeeping objects of the communication manager (CommMgr, 0x5f7cc0-0x5fa720; these classes are
// 0x5f79b0-0x5f7cc0 and 0x5fa720-0x5fb850).
//
// A DirectPlay message of the game is one datagram of at most 0x200 bytes: a count byte followed by that many
// packets back to back (PacketBuffer). A packet is
//     u32 id | u16 length | u8 reliable | i8 part | i8 partCount | payload (up to 0x1f6 bytes)
// where the high nibble of the low byte of the id's top byte is the packet's category (the "mailbox" the
// message is delivered to) and a message longer than 0x1f6 bytes is cut into `partCount` packets with consecutive
// ids. Reliable packets are re-sent until every other player has acknowledged them (CommAckRecord); an ack is a
// packet with the id of the acknowledged packet whose payload is the sender's name (see CommMgr::SendAck).
#include <cstdint>

#include "core/containers.h"
#include "core/strstream_io.h"
#include "core/game_string.h"
#include "platform/win32.h"

class CommMgr;
class CommPacket;

constexpr int kCommMaxPlayers = 16;
constexpr int kCommPacketBufferSize = 0x200;
constexpr int kCommMaxPayload = 0x1f6;
constexpr int kCommPacketHeaderSize = 9;

// The magic number that starts a CommMessage.
constexpr uint32_t kCommMessageMagic = 0xabcddcba;

// ---- Service provider and session lists -----------------------------------------------------------------------

// A DirectPlay service provider as DirectPlayEnumerate reports it (0x38 bytes, VT 0x66de1c).
// BUG(original): m_guid keeps the pointer the callback received; DirectPlay only guarantees it during the callback.
class ServiceProviderInfo : public IdNode {
public:
    ServiceProviderInfo(const GUID* guid, const char* name, DWORD majorVersion, DWORD minorVersion,
                        DWORD_PTR context);                       // 0x5f79b0
    explicit ServiceProviderInfo(const ServiceProviderInfo& other);  // 0x5f7a50
    ~ServiceProviderInfo() override;                           // 0x5f7ae0 (deleting 0x5f7a30)

    const GUID* m_guid;    // +0x1c
    DWORD m_majorVersion;  // +0x20
    DWORD m_minorVersion;  // +0x24
    DWORD_PTR m_context;   // +0x28
    std::string m_name;         // +0x2c
};

// A game session as IDirectPlay2::EnumSessions reports it (0x38 bytes, VT 0x66de20): the instance GUID and the name.
class SessionInfo : public IdNode {
public:
    explicit SessionInfo(const DPSESSIONDESC2* description);  // 0x5f7b30
    explicit SessionInfo(const SessionInfo& other);           // 0x5f7be0
    ~SessionInfo() override;                                   // 0x5f7c70 (deleting 0x5f7bc0)

    GUID m_instance;  // +0x1c guidInstance of the session
    std::string m_name;    // +0x2c
};

// The two lists of the lobby (globals 0x6be340 and 0x6be330): IdAvlTrees that own their nodes.
class ServiceProviderList : public IdAvlTree {
public:
    ServiceProviderList();           // 0x5fb670
    ~ServiceProviderList() override;  // 0x5fb6b0 (deleting 0x5fb690)
};

class SessionList : public IdAvlTree {
public:
    SessionList();           // 0x5fb6c0
    ~SessionList() override;  // 0x5fb700 (deleting 0x5fb6e0)
};

// What the lobby launcher left for the game (global 0x6be350, VT 0x66de6c, 0x10 bytes). `m_launched` is set by
// CommMgr::QueryLobbyConnection when the game was started from a DirectPlay lobby, `m_host` when that lobby asked the
// game to create (rather than join) the session. The session GUID follows at 0x6be360.
class LobbyConnection {
public:
    LobbyConnection();           // 0x5fb710
    virtual ~LobbyConnection();  // 0x5fb750 (deleting 0x5fb730)

    int m_launched;           // +0x04 0x6be354
    int m_host;               // +0x08 0x6be358
    IDirectPlay2A* m_dplay;   // +0x0c 0x6be35c
};

// ---- Messages exchanged through CommMgr -----------------------------------------------------------------------

// A small control message (VT 0x66de30, 0x74 bytes) serialised through an ostrstream: the magic number and five
// ints. Kind 4 is "player quit"; `m_arg` is the sender's player index.
class CommMessage {
public:
    explicit CommMessage(const char* buffer);                                      // 0x5fa920 (null buffer: all zero)
    CommMessage(int kind, int arg, int a, int b, int c);                            // 0x5faa80
    virtual ~CommMessage();                                                        // 0x5faad0 (deleting 0x5fa900)

    // Writes the six ints into the stream and returns the stream's buffer (frozen: the caller deletes it).
    char* Serialize();  // 0x5faaf0

    uint32_t m_magic;  // +0x04 kCommMessageMagic
    int m_kind;        // +0x08
    int m_arg;         // +0x0c
    int m_a;           // +0x10
    int m_b;           // +0x14
    int m_c;           // +0x18
    OStrStream m_stream;  // +0x1c
};

// The delivery state of one reliable packet (0x88 bytes, no vtable): for each player when it was sent and the
// state: 0 never sent, 1 sent and unacknowledged, 2 the player left, 3 acknowledged.
class CommAckRecord {
public:
    explicit CommAckRecord(uint32_t packetId);  // 0x5fb2d0

    // 1 -> 3; returns whether the state was 1.
    int Acknowledge(int player);                 // 0x5fb300
    int SetState(uint32_t player, int state);   // 0x5fb320: false for players of 16 and up
    // True if no player is still pending and at least one is settled (every other player but ourselves).
    int IsSettled();                             // 0x5fb340
    int GetState(int player) const;              // 0x5fb390
    uint32_t GetSendTime(int player) const;      // 0x5fb3a0
    void MarkSent(int player);                   // 0x5fb3b0: timeGetTime, state 1

    uint32_t m_unknown00;              // +0x00 never written
    uint32_t m_packetId;               // +0x04
    uint32_t m_sendTime[kCommMaxPlayers];  // +0x08
    int m_state[kCommMaxPlayers];      // +0x48
};

// The part of a packet that goes on the wire (0x200 bytes).
struct PacketWire {
    uint32_t m_id;                         // +0x00
    uint16_t m_length;                     // +0x04 payload bytes
    uint8_t m_reliable;                    // +0x06
    int8_t m_part;                         // +0x07 1-based
    int8_t m_partCount;                    // +0x08
    uint8_t m_payload[kCommMaxPayload + 1];  // +0x09
};

// A packet this machine sends (0x220 bytes, VT 0x66de34). The node id is the packet id; reliable packets register
// themselves in the CommMgr's pending tree on construction, the others are added by CommOutgoing.
class CommPacket : public IdNode {
public:
    // Category variant: takes the next id from the global counter and puts `category` into its top nibble.
    CommPacket(const void* data, uint32_t size, uint8_t category, char part, char partCount, int reliable);  // 0x5faba0
    // Explicit id variant.
    CommPacket(const void* data, uint32_t size, uint32_t id, char part, char partCount, int reliable);       // 0x5fadf0
    ~CommPacket() override;  // 0x5facc0 (deleting 0x5faca0)

    // Id of the first packet of the message this one belongs to.
    int FirstFragmentId();  // 0x5fad20
    // Copies header and payload to `out`; returns the byte count, 0 if they do not fit into `capacity`.
    int Pack(void* out, uint16_t capacity);  // 0x5fad40

    CommAckRecord* m_ack;  // +0x1c owned
    PacketWire m_wire;     // +0x20
};

// A packet taken out of a PacketBuffer (0x220 bytes, VT 0x66de70).
class ReceivedPacket : public IdNode {
public:
    ReceivedPacket(const void* wire, uint32_t size);  // 0x5fb760
    ~ReceivedPacket() override;                        // 0x5fb800 (deleting 0x5fb7e0)

    // Copies the payload to `out` if it fits into `capacity`; returns its length, 0 if it does not fit.
    uint16_t CopyPayload(void* out, uint16_t capacity) const;  // 0x5fb810

    uint32_t m_unknown1c;  // +0x1c never written
    PacketWire m_wire;     // +0x20
};

// A message being sent (0x24 bytes, VT 0x66de38): cuts the data into packets of CommPacket and keeps their ids.
// The node id is the id of the first packet.
class CommOutgoing : public IdNode {
public:
    CommOutgoing(const void* data, uint32_t size, uint32_t category, int reliable);  // 0x5faed0
    ~CommOutgoing() override;                                                        // 0x5fb080 (deleting 0x5fb060)

    int8_t m_partCount;        // +0x1c
    int8_t m_unused1d;         // +0x1d
    uint32_t m_firstPacketId;  // +0x20
};

// A message handed out by CommMgr::GetMessage: its size and a heap copy of the bytes; both are the caller's.
struct CommBuffer {
    uint32_t m_size;  // +0x00
    void* m_data;     // +0x04
};

// A message being received (0x28 bytes, VT 0x66de48): collects the packets of one message in the CommMgr's received
// tree. The node id is the id of the first packet.
class PartialMessage : public IdNode {
public:
    explicit PartialMessage(uint32_t firstPacketId);  // 0x5fb410
    ~PartialMessage() override;                        // 0x5fb3e0 (deleting 0x5fb3f0)

    int IsComplete() const;                // 0x5fb470
    uint32_t GetCategory() const;          // 0x5fb490
    // Files the packet unless it is already known; true once every part has arrived.
    int AddFragment(ReceivedPacket* packet);  // 0x5fb4a0
    // The concatenated payloads (a heap CommBuffer the caller owns).
    CommBuffer* Assemble();         // 0x5fb500
    // Removes the packets from the CommMgr's received tree (and deletes them).
    int ReleaseFragments();                // 0x5fb630

    int8_t m_partCount;   // +0x1c parts the message has
    int8_t m_partsSeen;   // +0x1d parts that arrived
    uint32_t m_firstId;   // +0x20
    uint32_t m_length;    // +0x24 sum of the payload lengths
};

// ---- The datagram buffers -------------------------------------------------------------------------------------

// A buffer holding one DirectPlay message under construction or just received: m_count packets in m_data.
// Slot 0 (Reset) is pure; the derived classes add the virtual destructor as slot 1.
class PacketBuffer {
public:
    virtual void Reset() = 0;  // 0

    int8_t GetCount() const;          // 0x5fb0c0
    uint8_t* GetWritePointer() const;  // 0x5fb170: where the next packet goes / DirectPlay receives into
    // Takes the first packet out of the buffer (null if it holds none), moving the rest down.
    ReceivedPacket* PopPacket();      // 0x5fb180
    // Appends the packet if it fits; false if the buffer is full.
    int Append(CommPacket* packet);   // 0x5fb280

    int8_t m_count;            // +0x04
    uint8_t m_data[0x1ff];     // +0x05
    uint16_t m_used;           // +0x204 bytes in use, including the count byte
    uint8_t* m_write;          // +0x208

protected:
    PacketBuffer();    // 0x5fb090
    ~PacketBuffer();   // 0x5fb0b0 (stores the pure vtable of 0x66de3c)
};

// The outgoing buffer of one player (VT 0x66de28); an array of 17 of them lives in the CommMgr.
class SendBuffer : public PacketBuffer {
public:
    SendBuffer();   // 0x5f85c0
    void Reset() override;  // 0: 0x5fb240
    virtual ~SendBuffer();          // 1: 0x5fb270 (deleting 0x5f85e0)
};

// The buffer DirectPlay receives into (VT 0x66de40, 0x210 bytes).
class ReceiveBuffer : public PacketBuffer {
public:
    ReceiveBuffer();  // 0x5fb0d0
    void Reset() override;  // 0: 0x5fb140
    virtual ~ReceiveBuffer();       // 1: 0x5fb130 (deleting 0x5fb110)

    int m_fromPlayer;  // +0x20c index of the player the last message came from
};
