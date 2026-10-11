#pragma once
// CommMgr, the communication manager of the multiplayer game (0x5f7cc0-0x5fa720, the singleton g_commMgr at
// 0x6be3a8; "CommMgr.cpp" in the original's debug strings). It turns the game's messages into packets
// (comm_packets.h), keeps them in per-player datagram buffers, resends reliable ones until every player
// acknowledged them and reassembles incoming ones into messages the game takes out by category. Underneath it uses
// DirectPlay 6 (IDirectPlay2A through platform/dplay.h) for the session and its players; the methods that call
// DirectPlay are in comm_dplay.cpp, the protocol is in comm_mgr.cpp.
//
// The game talks to it through the thin wrappers of tactical/comm2.h; the lobby (MultiPlayerWindow) also uses the
// session functions. Player indices run 0..15 (one nibble of a packet id): index 0xf is the broadcast buffer.
#include <cstdint>

#include "core/containers.h"
#include "core/game_string.h"
#include "core/string_map.h"
#include "multiplayer/comm_packets.h"
#include "platform/win32.h"

// Notification codes posted to the owner window (message 0x40c, wParam = code, lParam = player index).
enum CommNotify : int {
    kCommPlayerLeft = 1,        // DeactivatePlayer: a player was dropped
    kCommPlayerLost = 3,        // no packet from the other side for 90 seconds
    kCommSessionEnded = 4,      // the session is gone (quit message, failed send, last opponent left)
};
constexpr UINT kCommNotifyMessage = 0x40c;

// Result codes of the session functions.
enum CommResult : int {
    kCommOk = 0,
    kCommNotConnected = 1,       // no DirectPlay session
    kCommBadParameter = 3,
    kCommProviderFailed = 5,
    kCommSessionListFailed = 6,
    kCommSessionFull = 7,
    kCommSessionNotAllowed = 8,
    kCommPlayerCreateFailed = 9,
    kCommNoOtherPlayers = 0xb,
    kCommSendFailed = 0xc,
    kCommSendToSelf = 0xd,
    kCommWaitingForAcks = 0xe,
    kCommBufferEmpty = 0x10,
    kCommNoCommMgr = 0x11,
    kCommNameRequired = 0x12,
};

class CommMgr {
public:
    CommMgr();   // 0x5f8410
    ~CommMgr();  // 0x5f8740

    // ---- messages --------------------------------------------------------------------------------------------
    // Cuts `data` into packets of `category` and queues them for every player; returns the message id of a reliable
    // message (0 for an unreliable one or when there is nobody to send to).
    uint32_t QueueMessage(const void* data, uint32_t size, uint32_t category, int reliable);  // 0x5f7d90
    // The next complete message of `category` (a heap CommBuffer the caller owns), null if there is none.
    CommBuffer* TakeMessage(char category);   // 0x5f9550
    // Drops the pending incoming and outgoing messages of `category`.
    void ClearCategory(char category);        // 0x5fa720
    // Deletes the nodes of `tree` (PartialMessages or CommOutgoings) that belong to `category`.
    void ClearCategoryFrom(IdAvlTree* tree, char category);  // 0x5fa750

    // ---- packets ---------------------------------------------------------------------------------------------
    // Puts `packet` into player `player`'s datagram buffer (flushing it first when full); 0 on success.
    int QueuePacket(CommPacket* packet, int player, int markReliable);  // 0x5f9090
    // Same for the broadcast buffer, and for a reliable packet it marks every other player as pending.
    int BroadcastPacket(CommPacket* packet, int reliable);              // 0x5f91c0
    // Sends one datagram buffer; returns a CommResult (0 on success).
    int FlushPlayer(int player);   // 0x5f7ea0
    // Sends them all; true if the broadcast buffer went out.
    int FlushAll();                // 0x5f9030
    // Resends the reliable packets whose acknowledgement is overdue; returns kCommWaitingForAcks (0xe) when
    // none is outstanding.
    int ResendUnacked();           // 0x5f92e0
    // Acknowledges a received reliable packet to its sender; true on success.
    int SendAck(ReceivedPacket* packet);  // 0x5f93d0
    // Handles the acknowledgement packet `packet` from player `from`; returns 1 if it was ours to handle.
    int HandleAck(ReceivedPacket* packet, uint32_t from);  // 0x5f8eb0
    // Moves every complete received packet into its PartialMessage.
    void RegroupReceived();        // 0x5f94a0
    // The next received packet that is new (acknowledging it); null if the receive queue is empty.
    ReceivedPacket* ReceiveIncoming();  // 0x5f9860
    // Handles a quit message arriving as a regular message; true if it was one.
    int EatMessage(CommBuffer* message);  // 0x5fa350
    // Sends a keep-alive packet to the other players when nothing was heard for a while.
    void PingOpponents();          // 0x5fa400

    // The name the other players see.
    void SetPlayerName(std::string name);          // 0x5974e0 (tactical/comm2.cpp)

    // ---- players ---------------------------------------------------------------------------------------------
    int GetLastLatency() const;               // 0x5f8400
    int GetLocalIndex() const;                // 0x5f88c0
    uint32_t GetEnumeratedCount() const;      // 0x5f8a20
    uint32_t GetPlayerId(int index) const;    // 0x5f8ff0 DirectPlay id of the player
    // Index of the player with DirectPlay id `id` (0xffffffff if none).
    uint32_t FindPlayerIndex(uint32_t id) const;  // 0x5f9000
    // Marks player `index` as gone, settles their pending acknowledgements; returns 0 or kCommNoOtherPlayers.
    int DeactivatePlayer(uint32_t index);     // 0x5f9790

    // ---- DirectPlay (comm_dplay.cpp) -------------------------------------------------------------------------
    // Connects the manager to the owner window: remembers the application GUID, the number of players and the
    // network settings; when the game was launched from a lobby it also joins that session. Returns 0 or an error.
    int Init(HWND window, GUID application, uint32_t maxPlayers);  // 0x5f8b80
    // Closes the session and forgets everything about it; false if closing DirectPlay failed.
    int CleanUp();                            // 0x5f8600
    // Creates the local DirectPlay player (with a receive event).
    int CreateLocalPlayer();                  // 0x5f8850
    // Asks DirectPlay for the players of the session, sorts their ids and finds our index; returns their count.
    uint32_t ResetPlayerList();               // 0x5f8a30
    // The service providers DirectPlay offers (the lobby's first list).
    ServiceProviderList* EnumerateServiceProviders();  // 0x5f9eb0
    int OpenProvider(ServiceProviderInfo* provider);   // 0x5f9ee0 (kCommBadParameter without a provider)
    int HostNewSession(const char* sessionName);       // 0x5f9f70
    // The sessions a joined provider offers (g_sessionList); *status gets a CommResult.
    SessionList* GetSessionList(uint32_t timeoutMs, int* status);  // 0x5fa050
    int JoinSession(SessionInfo* session);             // 0x5fa180
    // After the session was opened: creates the local player and lists the players.
    int StartPlayerCreation();                         // 0x5fa270
    // Fills a DPSESSIONDESC2 for Open/CreateSession; kCommNameRequired when creating without a name.
    int FillSessionDesc(DPSESSIONDESC2* description, GUID instance, uint32_t flags, const char* sessionName);  // 0x5fa2b0
    // Closes the session to newcomers and fixes its player limit (host only).
    int CloseSessionToNewPlayers();                    // 0x5fa540

    // ---- state -----------------------------------------------------------------------------------------------
    IdAvlTree m_pending;      // +0x00 CommPackets by id: everything queued and not yet settled
    IdAvlTree m_received;     // +0x0c ReceivedPackets by id: the parts of messages being received
    IdAvlTree m_seen;         // +0x18 IdNodes: ids of the packets delivered already
    int m_isHost;             // +0x24
    int m_clientMode;         // +0x28 1 after Init
    HWND m_notifyWindow;      // +0x2c
    HANDLE m_receiveEvent;    // +0x30 signalled by DirectPlay when a message arrives
    uint32_t m_playerIds[16];  // +0x34 DirectPlay ids by player index, 0xffffffff for none; [15] is cleared to 0
    uint32_t m_unknown74;     // +0x74
    IdAvlTree m_assembling;   // +0x78 PartialMessages by id of their first packet
    IdAvlTree m_outgoing;     // +0x84 CommOutgoings of the reliable messages by id of their first packet
    uint32_t m_enumeratedCount;  // +0x90 players DirectPlay listed
    uint32_t m_playerCount;   // +0x94 players in the session
    std::string m_playerName;      // +0x98
    uint32_t m_unknownA4;     // +0xa4
    uint32_t m_unknownA8;     // +0xa8 0x3000
    uint32_t m_unknownAC;     // +0xac GetTickCount() + m_unknownA8 at Init
    GUID m_application;       // +0xb0 application GUID of the session description
    uint32_t m_lastLatency;   // +0xc0 round trip of the last acknowledged packet (ms)
    uint32_t m_latencyCount;  // +0xc4
    uint32_t m_latencyTotal;  // +0xc8
    uint32_t m_latencyAverage;  // +0xcc
    uint32_t m_lastReceiveTime;  // +0xd0 timeGetTime() of the last datagram
    int m_pingSent;           // +0xd4
    int m_minSendLatency;     // +0xd8 the network test settings, read from the settings map at Init
    int m_maxSendLatency;     // +0xdc
    int m_minReceiveLatency;  // +0xe0
    int m_maxReceiveLatency;  // +0xe4
    float m_dropSendPercent;  // +0xe8
    float m_dropReceivePercent;  // +0xec
    float m_dupeSendPercent;  // +0xf0
    float m_dupeReceivePercent;  // +0xf4
    int m_unknownF8[4];       // +0xf8
    IDirectPlay2A* m_dplay;   // +0x108 null when not connected
    int m_localIndex;         // +0x10c our player index, -1 before the player list is known
    DPID m_localId;           // +0x110
    char m_ackName[0x80];     // +0x114 payload of acknowledgements
    uint32_t m_resendInterval;  // +0x194 ms before an unacknowledged packet is sent again
    StringMap m_unknown198;   // +0x198
    StringMap m_unknown1a4;   // +0x1a4
    SendBuffer m_sendBuffers[17];  // +0x1b0 one per player index; [15] is the broadcast buffer
    ReceiveBuffer m_receiveBuffer;  // +0x247c
};


// The singleton: created on first use, null after DestroyCommMgr.
CommMgr* GetCommMgr();     // 0x5f7cc0
void DestroyCommMgr();     // 0x5f7d30
// True while a session list is being collected (g_commEnumerating).
int IsEnumeratingSessions();  // 0x5fa7a0
// Asks DirectPlay whether the game was started from a lobby; fills g_lobbyConnection (see comm_packets.h) and
// returns whether it was.
int QueryLobbyConnection();  // 0x5fa5e0

extern CommMgr* g_commMgr;                       // 0x6be3a8
extern int g_commEnumerating;                    // 0x6be3a4
extern uint32_t g_commPacketCounter;             // 0x6be39c next packet sequence number (top byte: player index)
extern const GUID* g_commProviderGuid;           // 0x6be3ac the service provider the session runs on
extern int g_commSessionFlags;                   // 0x6be3b4 DPSESSIONDESC2::dwFlags for new sessions
extern GUID g_commSessionGuid;                   // 0x6be380 instance GUID of the session we joined/created
extern uint32_t g_commMaxPlayers;                // 0x6ac294
extern ServiceProviderList g_serviceProviderList;  // 0x6be340
extern SessionList g_sessionList;                // 0x6be330
extern LobbyConnection g_lobbyConnection;        // 0x6be350
extern GUID g_lobbySessionGuid;                  // 0x6be360 instance GUID the lobby launched us into
extern StringMap g_commSettings;                 // 0x6be370 network test settings ("MinimumSendLatency" ...)
