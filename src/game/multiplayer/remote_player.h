#pragma once
// RemotePlayer (0x484df0-0x4856f0, VT 0x65aa70, 0x60 bytes): the controller of a side that another machine plays.
// Instead of deciding things it forwards: what the local game wants to tell the other player (orders, votes and sync
// messages) is serialised with a MessageCodec and sent through CommSendMessage on category 2, and the messages that
// arrive on categories 2 and 4 are routed to the UI, the vote history or the sync protocol (Virtual10, once per frame).
// The base class (PlayerController / PlayerQueue, app/player_controller_decl.h) holds the queue of messages the other
// player sent and the vote state; the history of votes the other player announced is the AiMissionHistory it borrows.
#include <cstdint>

#include "ai/ai_task_manager.h"
#include "app/player_controller_decl.h"
#include "gameobj/object_key.h"

class MessageBase;

class RemotePlayer : public PlayerController {
public:
    RemotePlayer();                   // 0x484df0
    explicit RemotePlayer(int side);  // 0x484e70
    virtual ~RemotePlayer();          // 0x484ed0 (scalar deleting destructor 0x484e50)

    virtual void Read(istream* in) override;    // 1: 0x484f20
    virtual void Write(ostream* out) override;  // 2: 0x484f60
    virtual int GetKind() override { return kPlayerRemote; }  // 3, shared 0x4e8740
    // Forgets the announced votes.
    virtual int Virtual04() override;                          // 4: 0x484fa0
    // The other player sent `message`: remember its sync point, then queue it and acknowledge it.
    virtual void Virtual07(PlayerMessage* message) override;   // 7: 0x484fb0
    // Sends one of our messages to the other player; 0 for the messages that went out.
    virtual int Virtual08(PlayerMessage* message) override;    // 8: 0x485000
    virtual int Virtual09() override { return 1; }             // 9, shared 0x40f340
    // Takes one message from the network and acts on it.
    virtual void Virtual10() override;                         // 10: 0x485030
    virtual int Virtual12(unsigned syncId) override;           // 12: 0x485370
    virtual int Virtual15(int* outId, ObjectKey* outKey, GameTime* outTime) override;  // 15: 0x485180
    virtual int Virtual16(int* outId, ObjectKey* outKey) override;                     // 16: 0x485250
    virtual void Virtual17(int id, ObjectKey* key) override;                           // 17: 0x485120
    virtual void Virtual18(int id, ObjectKey* key, int state, GameTime* time) override;  // 18: 0x485150
    virtual void Virtual19(int id, ObjectKey* key) override;                           // 19: 0x485300
    virtual void Virtual20(PlayerNotice* notice) override;                             // 20: 0x485340
    virtual void Virtual87(const ObjectKey* key, StringWords name) override;  // 87: 0x485360
    virtual void Virtual93(unsigned target) override;                                  // 93: 0x4853a0
    virtual void Virtual94(int side) override;                                         // 94: 0x4853f0
    virtual void Virtual95() override {}                                               // 95, shared 0x619730 (no vote counting)

    // Hands a message that is not a vote or sync message to the UI.
    void RouteMessage(MessageBase* message);  // 0x485440
    // Tells the other player that `message` arrived (a NetMsg2 with its id and side).
    void SendAck(MessageBase* message);       // 0x485520
    // Serialises the message and sends it reliably on category 2; the caller keeps the message.
    void SendToPeer(MessageBase* message);   // 0x4855b0
    // Sends one vote (a NetMsg4).
    void SendVote(int id, ObjectKey* key, int state, GameTime* time);  // 0x485640

    uint32_t m_voteSent;       // +0x50 set when our vote went out; cleared when a new vote starts
    uint32_t m_unknown54;      // +0x54 only saved and loaded
    AiMissionHistory m_history;  // +0x58 the votes the other player announced
};

// Takes the next message of category 2 (else 4) from CommMgr and decodes it; null if there is none.
MessageBase* ReceiveRemoteMessage();  // 0x485460
