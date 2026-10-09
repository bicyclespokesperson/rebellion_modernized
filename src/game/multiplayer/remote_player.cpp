#include "multiplayer/remote_player.h"

#include <new>

#include "app/app_control_cmds.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "events/game_clock_hooks.h"
#include "events/net_messages.h"
#include "multiplayer/comm_mgr.h"
#include "multiplayer/message_codec.h"
#include "orders/order_base.h"
#include "strategy_ui/game_ui_facade.h"
#include "tactical/comm2.h"

// The shared empty function the release build left where a debug assertion failed (0x619730).
void FUN_00619730();

namespace {

constexpr int kRemoteMessageCategory = 2;
constexpr int kVoteMessageCategory = 4;
constexpr int kSyncPointKind = 1;
constexpr int kAckKind = 2;
constexpr int kVoteKind = 4;
constexpr int kSingleValueKind = 5;
constexpr int kPlayerInfoKind = 0x12;
constexpr int kSyncRequestKind = 0x15;
constexpr int kHistoryByVoteId = 1;
constexpr int kHistoryByVoteIdOnly = 2;

MessageBase* AsMessage(PlayerMessage* message) { return reinterpret_cast<MessageBase*>(message); }

}  // namespace

RemotePlayer::RemotePlayer() : PlayerController(), m_history() {
    m_voteSent = 0;
    m_unknown54 = 0;
}

RemotePlayer::RemotePlayer(int side) : PlayerController(side), m_history() {
    m_voteSent = 0;
    m_unknown54 = 0;
}

RemotePlayer::~RemotePlayer() { REB_KEEP_VPTR_STORE(); }

void RemotePlayer::Read(istream* in) {
    PlayerController::Read(in);
    ReadValue32(in, &m_voteSent);
    ReadValue32(in, &m_unknown54);
    m_history.Load(in);
}

void RemotePlayer::Write(ostream* out) {
    PlayerController::Write(out);
    WriteValue32(out, &m_voteSent);
    WriteValue32(out, &m_unknown54);
    m_history.Save(out);
}

int RemotePlayer::Virtual04() {
    m_history.Clear();
    return 1;
}

void RemotePlayer::Virtual07(PlayerMessage* message) {
    if (!message) return;
    MessageBase* received = AsMessage(message);
    if (received->GetKind() == kSyncPointKind) {
        const unsigned syncPoint = static_cast<NetMsg1*>(received)->m_syncPoint;
        if (m_maxSyncPoint < syncPoint) m_maxSyncPoint = syncPoint;
    } else if (received->GetKind() == kSingleValueKind) {
        PlayerController::Virtual95();
    }
    m_pending.Adopt(received);
    SendAck(received);
}

int RemotePlayer::Virtual08(PlayerMessage* message) {
    if (reinterpret_cast<SidedMessage*>(message)->m_unk24 == 1) {
        SendToPeer(AsMessage(message));
        return 0;
    }
    return 1;
}

void RemotePlayer::Virtual10() {
    MessageBase* message = ReceiveRemoteMessage();
    CommAllPacketsAcknowledged();
    if (!message) return;
    switch (message->GetKind()) {
    case kAckKind: {
        auto* ack = static_cast<NetMsg2*>(message);
        if (ack->m_side != m_side) {
            UiDeliverQueuedMessage(ack->m_syncPoint);
            delete message;
            return;
        }
        break;
    }
    case kVoteKind:
        m_history.Add(message);
        delete message;
        return;
    case kPlayerInfoKind:
        static_cast<MpPlayerInfoMsg*>(message)->Apply();
        delete message;
        return;
    case kSyncRequestKind: {
        auto* request = static_cast<NetMsg15*>(message);
        if (request->m_valueA != 0) {
            m_syncTarget = request->m_valueB;
            UiBeginSync(request->m_valueB);
            delete message;
            return;
        }
        UiAckSync(request->m_valueC);
        break;
    }
    default:
        RouteMessage(message);
        return;
    }
    delete message;
}

int RemotePlayer::Virtual12(unsigned syncId) {
    if (m_syncId != syncId) {
        m_syncId = syncId;
        return 0;
    }
    return m_syncId <= m_maxSyncPoint;
}

int RemotePlayer::Virtual15(int* outId, ObjectKey* outKey, GameTime* outTime) {
    if (m_voteState != 0) {
        *outId = m_voteId;
        new (outKey) ObjectKey(m_voteKey);
        AssignGameTime(outTime, &m_voteTime);
        return m_voteState;
    }
    AiMissionHistoryEntry* entry =
        m_history.Find(reinterpret_cast<const int*>(&m_voteKey), static_cast<uint32_t>(m_voteId), kHistoryByVoteId);
    if (!entry) {
        *outId = 0;
        outKey->Reset();
        ClearGameTime(outTime);
        return m_voteState;
    }
    m_voteState = entry->m_x1c;
    AssignGameTime(&m_voteTime, &entry->m_time);
    AssignGameTime(outTime, &m_voteTime);
    *outId = static_cast<int>(entry->m_id);
    new (outKey) ObjectKey(entry->m_key);
    m_history.Remove(entry);
    delete entry;
    return m_voteState;
}

int RemotePlayer::Virtual16(int* outId, ObjectKey* outKey) {
    if (m_voteState != 0) {
        *outId = m_voteId;
        new (outKey) ObjectKey(m_voteKey);
        return m_voteState;
    }
    AiMissionHistoryEntry* entry =
        m_history.Find(reinterpret_cast<const int*>(&m_voteKey), static_cast<uint32_t>(m_voteId), kHistoryByVoteIdOnly);
    if (!entry) {
        *outId = 0;
        outKey->Reset();
        return m_voteState;
    }
    m_voteState = entry->m_x1c;
    AssignGameTime(&m_voteTime, &entry->m_time);
    *outId = static_cast<int>(entry->m_id);
    new (outKey) ObjectKey(entry->m_key);
    m_history.Remove(entry);
    delete entry;
    return m_voteState;
}

void RemotePlayer::Virtual17(int id, ObjectKey* key) {
    ClearGameTime(&m_voteTime);
    m_voteState = 0;
    m_voteSent = 0;
    new (&m_voteKey) ObjectKey(*key);
    m_voteId = id;
}

void RemotePlayer::Virtual18(int id, ObjectKey* key, int state, GameTime* time) {
    if (m_voteSent == 0) {
        SendVote(id, key, state, time);
        m_voteSent = 1;
    }
}

void RemotePlayer::Virtual19(int id, ObjectKey* key) {
    m_voteState = 0;
    ClearGameTime(&m_voteTime);
    new (&m_voteKey) ObjectKey(*key);
    m_voteSent = 0;
    m_voteId = id;
}

void RemotePlayer::Virtual20(PlayerNotice* notice) {
    if (notice) delete notice;
}

void RemotePlayer::Virtual87(const ObjectKey* key, StringWords name) {
    // The String that was passed by value in the argument words is destroyed by the callee.
    static_assert(sizeof(String) <= sizeof(StringWords), "a String must fit in the argument words");
    reinterpret_cast<String*>(&name)->String::~String();
}

void RemotePlayer::Virtual93(unsigned target) {
    if (target <= m_syncTarget) return;
    auto* request = static_cast<NetMsg15*>(CreateOrderByKind(kSyncRequestKind));
    if (!request) return;
    request->m_valueA = 1;
    request->m_valueB = static_cast<int>(target);
    SendToPeer(request);
    delete request;
    m_syncTarget = target;
}

void RemotePlayer::Virtual94(int side) {
    if (m_side == side) {
        m_syncTarget = 0;
        return;
    }
    if (side == 0) return;
    auto* request = static_cast<NetMsg15*>(CreateOrderByKind(kSyncRequestKind));
    if (!request) return;
    request->m_valueC = side;
    SendToPeer(request);
    delete request;
}

void RemotePlayer::RouteMessage(MessageBase* message) {
    if (message) UiRouteMessage(reinterpret_cast<PlayerMessage*>(message));
}

void RemotePlayer::SendAck(MessageBase* message) {
    void* memory = ::operator new(sizeof(NetMsg2));
    if (!memory) FUN_00619730();
    NetMsg2* ack = memory ? new (memory) NetMsg2() : nullptr;
    if (!ack) return;
    ack->m_syncPoint = static_cast<int>(message->m_id);
    ack->m_side = reinterpret_cast<SidedMessage*>(message)->m_side;
    SendToPeer(ack);
    delete ack;
}

void RemotePlayer::SendToPeer(MessageBase* message) {
    if (!message) return;
    MessageCodec codec;
    codec.Serialize(message);
    if (codec.m_valid) {
        CommSendMessage(codec.m_buffer, codec.m_size, kRemoteMessageCategory, 1);
        CommFlushAll();
    }
}

void RemotePlayer::SendVote(int id, ObjectKey* key, int state, GameTime* time) {
    void* memory = ::operator new(sizeof(NetMsg4));
    if (!memory) FUN_00619730();
    NetMsg4* vote = memory ? new (memory) NetMsg4() : nullptr;
    if (!vote) return;
    vote->m_voteState = state;
    vote->m_voteId = id;
    AssignGameTime(&vote->m_voteTime, time);
    new (&vote->m_voteKey) ObjectKey(*key);
    SendToPeer(vote);
    delete vote;
}

MessageBase* ReceiveRemoteMessage() {
    MessageBase* result = nullptr;
    CommBuffer* buffer = CommTakeMessage(kRemoteMessageCategory);
    if (!buffer) {
        buffer = CommTakeMessage(kVoteMessageCategory);
        if (!buffer) return result;
    }
    MessageCodec codec;
    codec.SetData(buffer->m_data, buffer->m_size);
    codec.Deserialize(&result);
    if (buffer->m_data) ::operator delete(buffer->m_data);
    buffer->m_data = nullptr;
    ::operator delete(buffer);
    return result;
}
