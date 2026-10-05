#pragma once
#include <cstddef>
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "core/stream_io.h"
#include "core/utility_classes.h"
#include "gameobj/object_key.h"
#include "orders/order_list.h"

// Minimal declarations of the player-controller classes (0x472-0x4a0, owned by other agents) that GameSession
// drives. Everything here is a stand-in meant to be dropped, or merged into the real headers, once the owners
// publish theirs. Unknown parts are padding; virtual slots are placeholders (VirtualNN = vtable slot NN).
//
//   PlayerController   abstract base, VT 0x65aef0 (0x50 bytes, 96 virtual slots)
//     HumanPlayer      0xc4 bytes, VT 0x65ad70, ctor 0x486120 / 0x4861b0(side)
//     AiPlayer         0x42c bytes, VT 0x65abf0, see ai/ai_player.h
//     RemotePlayer     0x60 bytes, VT 0x65aa70, ctor 0x484df0 / 0x484e70(side)
//
// A GameSession has two controllers, one per side (side 1 = Alliance, 2 = Empire); the "vote" fields are the
// negotiation state of a battle-type vote that is exchanged between the two controllers.

class PlayerController;
class HumanPlayer;
class PlayerMessage;
class PlayerNotice;
class String;

// A 4-byte time value (0x4fcd90 assigns, 0x4fcdd0 / 0x42dbe0 zero it; 0x461c4000 = 10000.0f is stored in it).
struct GameTime {
    float m_value;  // +0x00
};

enum PlayerKind {
    kPlayerHuman = 1,
    kPlayerRemote = 2,
    kPlayerAi = 3,
};

// A queued player message (the order/request objects sent between the two sides). Node of a PlayerMessageTree,
// keyed by its id. Its virtuals are not needed by GameSession (except the destructor).
class PlayerMessage : public IdNode {
public:
    int m_unk1c;   // +0x1c
    int m_side;    // +0x20 the side that sent it (1 or 2)
};

// Notification routed to the controller of its side; only the side and the destructor are used by GameSession.
class PlayerNotice {
public:
    virtual ~PlayerNotice();
    uint8_t m_unk04[0x14];  // +0x04
    int m_side;             // +0x18
};

// Messages that have been sent but not yet delivered (the order module's MessageTree, VT 0x661030).
using PlayerMessageTree = MessageTree;

// The per-player message/state bookkeeping embedded in a human player at +0x6c (ctor 0x4888f0).
class PlayerDispatch {
public:
    int m_side;                 // +0x00
    uint8_t m_unk04[4];         // +0x04
    PersistentInt m_flags;      // +0x08 bit 0x10000000 / 0x20000000 / 8 are set by the 0x48a... methods
    PersistentInt m_defaultState;  // +0x0c
    IdAvlTree m_trees[4];       // +0x10 four trees of VT 0x658da0 (ctor 0x436a40); [0] holds pending orders
    int m_unk40;                // +0x40
    uint8_t m_unk44[0xc];       // +0x44 IdAvlTree of per-key states (ctor 0x5f5ca0)
    int m_unk50;                // +0x50
};

// Abstract controller of one side of a game session. A virtual slot is vtable offset / 4.
class PlayerController {
public:
    PlayerController();                   // 0x488460
    explicit PlayerController(int side);  // 0x4884f0
    virtual ~PlayerController();          // 0x488580
    virtual void Read(istream* in);
    virtual void Write(ostream* out);
    virtual int GetKind() { return 0; }  // PlayerKind: 1 human, 2 remote, 3 AI (the original's slots 3, 4, 9, 10, 14 and 17-94 are pure)
    virtual int Virtual04() { return 0; }
    virtual void Virtual05();
    virtual void Virtual06();
    virtual void Virtual07(PlayerMessage* message);
    virtual int Virtual08(PlayerMessage* message);
    virtual int Virtual09() { return 0; }
    virtual void Virtual10() {}
    virtual int Virtual11();
    virtual int Virtual12(unsigned syncId);
    virtual int Virtual13();
    virtual void Virtual14() {}
    virtual int Virtual15(int* outId, ObjectKey* outKey, GameTime* outTime);  // returns the vote state
    virtual int Virtual16(int* outId, ObjectKey* outKey);  // returns the vote state
    virtual void Virtual17(int id, ObjectKey* key) {}
    virtual void Virtual18(int id, ObjectKey* key, int state, GameTime* time) {}
    virtual void Virtual19(int id, ObjectKey* key) {}
    virtual void Virtual20(PlayerNotice* notice) {}
    virtual void Virtual21() {}
    virtual void Virtual22(int a, int b) {}
    virtual void Virtual23(int value) {}
    virtual void Virtual24(int value) {}
    virtual void Virtual25(int value) {}
    virtual void Virtual26(int value) {}
    virtual void Virtual27(int value) {}
    virtual void Virtual28(int value) {}
    virtual void Virtual29(unsigned flags) {}
    virtual void Virtual30(const ObjectKey* key, int arg) {}
    virtual void Virtual31(const ObjectKey* key, int arg) {}
    virtual void Virtual32(const ObjectKey* key, int arg) {}
    virtual void Virtual33(const ObjectKey* key, int arg) {}
    virtual void Virtual34(const ObjectKey* key, int arg) {}
    virtual void Virtual35() {}
    virtual void Virtual36() {}
    virtual void Virtual37(const ObjectKey* key, int arg) {}
    virtual void Virtual38(const ObjectKey* key, int arg) {}
    virtual void Virtual39(const ObjectKey* key, int arg) {}
    virtual void Virtual40(const ObjectKey* key, int arg) {}
    virtual void Virtual41(const ObjectKey* key, int arg) {}
    virtual void Virtual42(const ObjectKey* key, int arg) {}
    virtual void Virtual43(const ObjectKey* key, int sound) {}
    virtual void Virtual44(const ObjectKey* key, int sound) {}
    virtual void Virtual45(const ObjectKey* key, int arg) {}
    virtual void Virtual46(const ObjectKey* key, int arg) {}
    virtual void Virtual47(const ObjectKey* key, int arg) {}
    virtual void Virtual48(const ObjectKey* key, int arg) {}
    virtual void Virtual49(const ObjectKey* key, int arg) {}
    virtual void Virtual50(const ObjectKey* key, int arg) {}
    virtual void Virtual51(const ObjectKey* manager, int unused) {}
    virtual void Virtual52(const ObjectKey* manager, int position) {}
    virtual void Virtual53(const ObjectKey* manager, const ObjectKey* character) {}
    virtual void Virtual54(const ObjectKey* manager, String* text) {}
    virtual void Virtual55() {}
    virtual void Virtual56() {}
    virtual void Virtual57() {}
    virtual void Virtual58(const ObjectKey* key, int arg) {}
    virtual void Virtual59(int a, int b) {}
    virtual void Virtual60(int a, int b) {}
    virtual void Virtual61(int a, int b) {}
    virtual void Virtual62(const ObjectKey* key, int redraw) {}
    virtual void Virtual63(const ObjectKey* key, int redraw) {}
    virtual void Virtual64() {}
    virtual void Virtual65(const ObjectKey* key, int redraw) {}
    virtual void Virtual66(const ObjectKey* key, int arg) {}
    virtual void Virtual67(const ObjectKey* key, int arg) {}
    virtual void Virtual68(const ObjectKey* key, int arg) {}
    virtual void Virtual69(const ObjectKey* key, int arg) {}
    virtual void Virtual70(const ObjectKey* key, int arg) {}
    virtual void Virtual71(const ObjectKey* key, int arg) {}
    virtual void Virtual72(const ObjectKey* key, int arg) {}
    virtual void Virtual73(const ObjectKey* key, int arg) {}
    virtual void Virtual74(const ObjectKey* key, int arg) {}
    virtual void Virtual75(const ObjectKey* key, int arg) {}
    virtual void Virtual76(const ObjectKey* key, int arg, const ObjectKey* system) {}
    virtual void Virtual77(const ObjectKey* key, int arg) {}
    virtual void Virtual78(const ObjectKey* key, int arg) {}
    virtual void Virtual79(const ObjectKey* key, int arg) {}
    virtual void Virtual80(const ObjectKey* key, int arg) {}
    virtual void Virtual81(const ObjectKey* key, const ObjectKey* oldKey, const ObjectKey* newKey) {}
    virtual void Virtual82(const ObjectKey* key) {}
    virtual void Virtual83(const ObjectKey* key, const ObjectKey* newParent, int arg) {}
    virtual void Virtual84(const ObjectKey* key, int arg) {}
    virtual void Virtual85() {}
    virtual void Virtual86(const ObjectKey* key, int arg) {}
    virtual void Virtual87(const ObjectKey* key, int name0, int name1, int name2) {}
    virtual void Virtual88(const ObjectKey* key, int arg) {}
    virtual void Virtual89(const ObjectKey* key, int arg) {}
    virtual void Virtual90(const ObjectKey* key) {}
    virtual void Virtual91(const ObjectKey* key, const ObjectKey* newParent) {}
    virtual void Virtual92(int a, const ObjectKey* key) {}
    virtual void Virtual93(unsigned target) {}
    virtual void Virtual94(int side) {}
    virtual void Virtual95();

    uint8_t m_unk04[0x20];   // +0x04
    int m_side;              // +0x24 1 or 2
    unsigned m_syncId;       // +0x28
    uint8_t m_unk2c[0xc];    // +0x2c
    int m_voteState;         // +0x38 0 none, 1 accepted, 2 declined, 3 confirmed (returned by Virtual15/16)
    GameTime m_voteTime;     // +0x3c
    ObjectKey m_voteKey;     // +0x40
    int m_voteId;            // +0x44
    unsigned m_syncTarget;   // +0x48 non-zero while waiting for the other side to reach this sync point
    int m_unk4c;             // +0x4c
};

class RemotePlayer : public PlayerController {
public:
    uint8_t m_unk50[0x60 - 0x50];  // +0x50
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(GameTime, 0x04);
REB_ASSERT_SIZE(PlayerMessageTree, 0x10);
REB_ASSERT_OFFSET(PlayerMessage, m_side, 0x20);
REB_ASSERT_OFFSET(PlayerNotice, m_side, 0x18);
REB_ASSERT_SIZE(PlayerDispatch, 0x54);
REB_ASSERT_OFFSET(PlayerDispatch, m_flags, 0x08);
REB_ASSERT_OFFSET(PlayerDispatch, m_defaultState, 0x0c);
REB_ASSERT_OFFSET(PlayerDispatch, m_trees, 0x10);
REB_ASSERT_SIZE(PlayerController, 0x50);
REB_ASSERT_OFFSET(PlayerController, m_side, 0x24);
REB_ASSERT_OFFSET(PlayerController, m_syncId, 0x28);
REB_ASSERT_OFFSET(PlayerController, m_voteState, 0x38);
REB_ASSERT_OFFSET(PlayerController, m_voteTime, 0x3c);
REB_ASSERT_OFFSET(PlayerController, m_voteKey, 0x40);
REB_ASSERT_OFFSET(PlayerController, m_syncTarget, 0x48);
REB_ASSERT_OFFSET(PlayerController, m_unk4c, 0x4c);
REB_ASSERT_SIZE(RemotePlayer, 0x60);
#pragma clang diagnostic pop

// ---- constructors (the original allocates with operator new first; one variant takes the side) ----------------
RemotePlayer* __thiscall FUN_00484df0(void* memory);
RemotePlayer* __thiscall FUN_00484e70(void* memory, int side);

// ---- non-virtual PlayerController methods --------------------------------------------------------------------
int __thiscall FUN_004888c0(PlayerController* player);  // m_syncTarget == 0
void __thiscall FUN_004888d0(PlayerController* player, unsigned syncTarget);
void __thiscall FUN_0048aa00(PlayerController* player, int argument);

// ---- PlayerDispatch (the +0x6c sub-object of HumanPlayer) ----------------------------------------------------
void __thiscall FUN_0048a190(PlayerDispatch* dispatch, int id);
void __thiscall FUN_0048a610(PlayerDispatch* dispatch, int on);
void __thiscall FUN_0048a210(PlayerDispatch* dispatch, int key, int value);
unsigned __thiscall FUN_0048a1c0(PlayerDispatch* dispatch, int key);
void __thiscall FUN_0048a1f0(PlayerDispatch* dispatch, int defaultState);
void __thiscall FUN_0048a470(PlayerDispatch* dispatch, int a, int b);
void __thiscall FUN_00488ca0(PlayerDispatch* dispatch, PlayerMessage* message);
int __thiscall FUN_0048a500(PlayerDispatch* dispatch);
void __thiscall FUN_0048a530(PlayerDispatch* dispatch, unsigned mask);

#include "strategy_ui/human_player.h"
