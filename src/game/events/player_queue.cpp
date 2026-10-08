#include <new>

#include "app/player_controller_decl.h"
#include "core/checked_new.h"
#include "core/vptr_store.h"
#include "events/game_clock_hooks.h"
#include "events/net_messages.h"
#include "orders/order_base.h"
#include "strategy_ui/game_ui_facade.h"

// ---- PlayerQueue ----------------------------------------------------------------------------------------------

PlayerQueue::PlayerQueue() : m_pending(), m_state(0) {
    m_side = 0;
    m_syncMessageId = 0;
    m_sync = nullptr;
    m_deliveredId = 0;
    m_syncId = 0;
    m_maxSyncPoint = 0;
}

PlayerQueue::PlayerQueue(int side) : m_pending(), m_state(0) {
    m_side = side;
    m_syncMessageId = 0;
    m_deliveredId = 0;
    m_sync = nullptr;
    m_syncId = 0;
    m_maxSyncPoint = 0;
}

PlayerQueue::~PlayerQueue() {
    delete m_sync;
    REB_KEEP_VPTR_STORE();
}

void PlayerQueue::Read(istream* in) {
    ReadValue32(in, &m_side);
    ReadValuePersistentInt(in, &m_state);
    ReadValue32(in, &m_syncMessageId);
    ReadValue32(in, &m_deliveredId);
    if (m_syncMessageId != 0) {
        NetMsg1* sync = NewOrNull<NetMsg1>();
        m_sync = sync;
        sync->Read(in);
    }
    ReadValue32(in, &m_syncId);
    ReadValue32(in, &m_maxSyncPoint);
    m_pending.Read(in);
}

void PlayerQueue::Write(ostream* out) {
    WriteValue32(out, &m_side);
    WriteValuePersistentInt(out, &m_state);
    WriteValue32(out, &m_syncMessageId);
    WriteValue32(out, &m_deliveredId);
    if (m_sync) m_sync->Write(out);
    WriteValue32(out, &m_syncId);
    WriteValue32(out, &m_maxSyncPoint);
    m_pending.Write(out);
}

void PlayerQueue::Virtual07(PlayerMessage* message) {
    if (!message) return;
    MessageBase* queued = reinterpret_cast<MessageBase*>(message);
    if (queued->GetKind() == 1) {
        unsigned syncPoint = static_cast<NetMsg1*>(queued)->m_syncPoint;
        if (m_maxSyncPoint < syncPoint) m_maxSyncPoint = syncPoint;
    }
    m_pending.Adopt(message);
}

int PlayerQueue::Virtual11() {
    int result = 1;
    if ((m_state.m_value & 4) != 0) return result;
    IdNode* node = static_cast<IdNode*>(m_pending.First());
    if (!node) return result;
    for (;;) {
        if (node->m_id > m_deliveredId) break;
        IdNode* old = m_pending.Detach(node->m_id);
        if (old) delete old;
        node = static_cast<IdNode*>(m_pending.First());
        if (!node) return result;
    }
    m_deliveredId = node->m_id;
    IdNode* message = m_pending.Detach(node->m_id);
    result = reinterpret_cast<MessageBase*>(message)->GetKind() == 1;
    if (result) m_state.m_value |= 4;
    UiAddManagerNotice(message);
    return result;
}

int PlayerQueue::Virtual13() {
    m_state.m_value = 0;
    return 1;
}

// ---- PlayerController -----------------------------------------------------------------------------------------

PlayerController::PlayerController() : PlayerQueue(), m_voteKey() {
    m_voteTime.m_value = 0;
    m_nextVoteId = 0;
    m_voteState = 0;
    m_voteId = 0;
    m_syncTarget = 0;
    m_unk4c = 0;
}

PlayerController::PlayerController(int side) : PlayerQueue(side), m_voteKey() {
    m_voteTime.m_value = 0;
    PlayerController::Virtual05(side == 1 ? 1 : 0x77359401);
    m_voteState = 0;
    m_voteId = 0;
    m_syncTarget = 0;
    m_unk4c = 0;
}

PlayerController::~PlayerController() { REB_KEEP_VPTR_STORE(); }

void PlayerController::Virtual05(int firstVoteId) {
    m_nextVoteId = firstVoteId;
    m_lastVoteId = firstVoteId - 1;
}

int PlayerController::Virtual06() { return m_nextVoteId++; }

int PlayerController::Virtual15(int* outId, ObjectKey* outKey, GameTime* outTime) {
    *outId = m_voteId;
    new (outKey) ObjectKey(m_voteKey);
    AssignGameTime(outTime, &m_voteTime);
    return m_voteState;
}

int PlayerController::Virtual16(int* outId, ObjectKey* outKey) {
    *outId = m_voteId;
    new (outKey) ObjectKey(m_voteKey);
    return m_voteState;
}

void PlayerController::Read(istream* in) {
    PlayerQueue::Read(in);
    StreamRead32(in, &m_nextVoteId);
    StreamRead32(in, &m_lastVoteId);
    StreamRead32(in, &m_voteState);
    StreamRead32(in, &m_syncTarget);
    StreamRead32(in, &m_unk4c);
    StreamRead32(in, &m_voteId);
    m_voteKey.Read(in);
    ReadGameTime(&m_voteTime, in);
}

void PlayerController::Write(ostream* out) {
    PlayerQueue::Write(out);
    StreamWrite32(out, &m_nextVoteId);
    StreamWrite32(out, &m_lastVoteId);
    StreamWrite32(out, &m_voteState);
    StreamWrite32(out, &m_syncTarget);
    StreamWrite32(out, &m_unk4c);
    StreamWrite32(out, &m_voteId);
    m_voteKey.Write(out);
    StreamWrite32(out, &m_voteTime);
}

int PlayerController::Virtual12(unsigned syncId) {
    bool done = false;
    int result = 0;
    if (m_syncId != syncId) {
        m_syncId = syncId;
        return result;
    }
    if ((m_state.m_value & 2) != 0) {
        if (m_syncId <= m_maxSyncPoint) {
            m_syncMessageId = 0;
            result = 1;
        }
        return result;
    }
    if ((m_state.m_value & 1) == 0) {
        NetMsg1* sync = NewOrNull<NetMsg1>(static_cast<int>(syncId));
        m_sync = sync;
        if (sync) {
            sync->SetId(Virtual06());
            sync->m_side = m_side;
            ChecksumRecord current;
            CopyCurrentSnapshot(reinterpret_cast<GameStateSnapshot*>(&current));
            sync->m_checksums = current;
            m_syncMessageId = sync->m_id;
            m_state.m_value |= 1;
        }
        return result;
    }
    IdNode* node = reinterpret_cast<IdNode*>(UiGetFirstMessage());
    do {
        unsigned id = node ? node->m_id : 0;
        if (static_cast<unsigned>(m_lastVoteId) < id && id < static_cast<unsigned>(m_syncMessageId)) {
            done = true;
        } else if (id == 0) {
            Send(m_sync);
            m_state.m_value |= 2;
            m_sync = nullptr;
            done = true;
        }
        if (node) node = static_cast<IdNode*>(node->m_successor);
    } while (!done);
    return result;
}

void PlayerController::Virtual95() { ++m_unk4c; }

void PlayerController::Send(MessageBase* message) {
    if (!message) return;
    if (message->m_id == 0) message->SetId(Virtual06());
    UiRouteMessage(reinterpret_cast<PlayerMessage*>(message));
}

void PlayerController::SendKeyRequest(const ObjectKey* key) {
    OrderBase* order = static_cast<OrderBase*>(CreateMessageByKind(0x205));
    if (!order) return;
    ObjectKeyValueSet team;
    team.AddKeyValue(key, 0);
    order->m_side = m_side;
    order->SetTeam(&team);
    Send(order);
}

int PlayerController::IsSynced() { return m_syncTarget == 0; }

void PlayerController::SetSyncTarget(unsigned target) { m_syncTarget = target; }
