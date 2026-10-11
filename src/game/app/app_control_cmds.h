#pragma once
#include <cstdint>

#include "orders/order_base.h"
#include "core/game_string.h"
#include "core/stream_io.h"

// Multiplayer lobby messages (kinds 0x10-0x12 and 0x14), registered in 0x41ec50 and dispatched by the
// multiplayer window (0x4104b0) and the game-start code (0x485030). The names are provisional: they follow what
// the handlers do. Apply() is not virtual; the dispatcher switches on GetKind() and calls it directly.

// Kind 0x14: the table of six player-slot values (global table at 0x6b1220, indexed 1..6).
class MpSlotTableMsg : public MessageBase {
public:
    MpSlotTableMsg();
    ~MpSlotTableMsg() override;
    int Read(istream* in) override;
    int Write(ostream* out) override;
    int GetKind() override;

    // Copies the values into the global slot table. Always returns 0.
    int Apply();

    uint32_t m_unk20;  // +0x20, zeroed but not serialised
    int m_values[6];   // +0x24
};

// Kind 0x10: the lobby settings the host chose; the fields are copies of the multiplayer window's +0x130, +0x120,
// +0x124 and +0x128.
class MpLobbySettingsMsg : public MessageBase {
public:
    MpLobbySettingsMsg(int setting130, int setting120, int setting124, int setting128, int startGame);
    ~MpLobbySettingsMsg() override;
    int Read(istream* in) override;
    int Write(ostream* out) override;
    int GetKind() override;

    int m_setting130;  // +0x20
    int m_setting120;  // +0x24
    int m_setting124;  // +0x28
    int m_setting128;  // +0x2c
    int m_startGame;   // +0x30
};

// Kind 0x11: a chat line (the text is in TextMessage::m_text). Adds no fields.
class MpChatMsg : public TextMessage {
public:
    MpChatMsg();
    ~MpChatMsg() override;
    int Read(istream* in) override;
    int Write(ostream* out) override;
    int GetKind() override;
    int CheckEnabled(StatusCode* status) override { return 1; }  // shared 0x51ebb0
    int CheckValid(StatusCode* status) override { return 1; }    // shared 0x51ebb0
    // Always reports success: {1, -1}.
    int Execute(StatusCode* status) override;
};

// Kind 0x12: a player's identity. m_action 1 sets the local player's id, name and a flag in the multiplayer
// session (global at 0x6be3b8), m_action 2 sets its peer id.
class MpPlayerInfoMsg : public MessageBase {
public:
    MpPlayerInfoMsg();
    ~MpPlayerInfoMsg() override;
    int Read(istream* in) override;
    int Write(ostream* out) override;
    int GetKind() override;

    // Returns 1 if the action was recognised, else 0.
    int Apply();

    int m_action;         // +0x20
    int m_playerId;       // +0x24
    String m_playerName;  // +0x28
    int m_extra;          // +0x34
};

// Kind 0x16 has no constructor or registration in the binary; only its handler was found (it sets the
// multiplayer flag word at 0x6b123c). The class is declared just far enough to hold the handler.
class MpFlagsMsg : public MessageBase {
public:
    int Apply();  // always returns 0

    uint32_t m_flags;  // +0x20
};

MessageBase* CreateMpSlotTableMsg();
MessageBase* CreateMpLobbySettingsMsg();
MessageBase* CreateMpChatMsg();
MessageBase* CreateMpPlayerInfoMsg();

// Static initialisers: a width computed from a constant rectangle {left, top, right, bottom} in .rdata.
void ComputeRectAWidth();
void ComputeRectBWidth();
