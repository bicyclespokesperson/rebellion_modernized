#include "multiplayer/comm_mgr.h"
#include "core/icf_stubs.h"

#include <cstdio>
#include <cstring>
#include <new>

#include "core/checked_new.h"

// The shared empty function that stands where the debug build traced; every call keeps its argument.

namespace
{

constexpr char kAckName[] = "!";
constexpr char kPingText[] = "?";
constexpr uint32_t kBroadcastIndex = 0xf;
constexpr uint32_t kNoPlayer = 0xffffffff;
constexpr uint32_t kPingIdLowBits = 0xffffff;
constexpr uint32_t kSystemMessageAddPlayer = 3;
constexpr uint32_t kSystemMessageDeletePlayer = 5;
constexpr int kQuitMessageKind = 4;
constexpr DWORD kPingAfterMs = 30000;
constexpr DWORD kGiveUpAfterMs = 90000;
constexpr int kResendDefaultMs = 1000;

constexpr char kTraceDestroying[] = "CommMgr::~CommMgr():\n";
constexpr char kTraceBufferSplit[] = "Flush() Buffer Split \n ";
constexpr char kTraceErrorSending[] = "Flush() Error Sending ";
constexpr char kTraceBufferEnd[] = "Flush() Buffer End ";
constexpr char kTraceResendError[] = " Error Resending Packet, quitting CheckGuaranteed Packets";
constexpr char kTraceAckError[] = "Error Sending Ack";
constexpr char kTraceAckId[] = "AckID %u, Category %u\n";
constexpr char kTracePlayerLost[] = "CommMgr::DeActivatePlayer():1409 CM_PLAYER_LOST\n";
constexpr char kTracePlayerLeft[] = "CommMgr::ReceiveIncomingCommPacket()1434 CM_PLAYER_LEFT_UNEXCUSED\n";
constexpr char kTraceQuitSession[] = "CommMgr::ReceiveIncomingCommPacket():1590 CM_PLAYER_QUIT_SESSION\n";
constexpr char kTraceAddPlayer[] = "CommMgr::ReceiveIncomingCommPacket() DPSYS_ADDPLAYER\n";
constexpr char kTraceDeletePlayer[] = "CommMgr::ReceiveIncomingCommPacket() DPSYS_DELETEPLAYER\n";
constexpr char kTraceEatQuit[] = "CommMgr::EatMessage():2160 CM_PLAYER_QUIT_SESSION\n";
constexpr char kTracePingStart[] = "CommMgr::PingOpponents 2188 Sending Ping -";
constexpr char kTracePingResult[] = " Pinging Players %u Return Code %u\n";
constexpr char kTraceNewline[] = "\n";

void Trace(CommMgr* comm, const char* text)
{
  IgnoreOneArgument(comm, reinterpret_cast<uintptr_t>(text));
}

void NotifyOwner(CommMgr* comm, int code, uint32_t player)
{
  PostMessageA(comm->m_notifyWindow, kCommNotifyMessage, code, player);
}

uint32_t CategoryOfNode(IdNode* node)
{
  return reinterpret_cast<PartialMessage*>(node)->GetCategory();
}

void DeleteMessage(CommBuffer* message)
{
  if (message->m_data)
    ::operator delete(message->m_data);
  message->m_data = nullptr;
  ::operator delete(message);
}

PartialMessage* FindOrCreatePartial(CommMgr* comm, uint32_t firstId)
{
  auto* partial = static_cast<PartialMessage*>(comm->m_assembling.FindById(firstId));
  if (!partial)
  {
    partial = NewOrNull<PartialMessage>(firstId);
    comm->m_assembling.AddWithId(partial);
  }
  return partial;
}

void RemovePartial(CommMgr* comm, PartialMessage* partial)
{
  partial->ReleaseFragments();
  comm->m_assembling.DeleteById(partial->m_id);
}

} // namespace

// ---- the singleton ---------------------------------------------------------------------------------------------

CommMgr* GetCommMgr()
{
  if (!g_commMgr)
  {
    void* memory = ::operator new(sizeof(CommMgr));
    if (!memory)
    {
      g_commMgr = nullptr;
      return nullptr;
    }
    g_commMgr = new (memory) CommMgr();
    return g_commMgr;
  }
  return g_commMgr;
}

void DestroyCommMgr()
{
  CommMgr* comm = g_commMgr;
  if (comm)
  {
    comm->~CommMgr();
    ::operator delete(comm);
  }
  g_commMgr = nullptr;
  g_serviceProviderList.Clear();
  g_sessionList.Clear();
}

int IsEnumeratingSessions()
{
  return g_commEnumerating;
}

// ---- construction ----------------------------------------------------------------------------------------------

CommMgr::CommMgr()
{
  m_isHost = 0;
  g_commEnumerating = 0;
  m_playerCount = 0;
  m_enumeratedCount = 0;
  m_latencyAverage = 0;
  m_latencyCount = 0;
  m_latencyTotal = 0;
  m_lastReceiveTime = timeGetTime();
  sprintf(m_ackName, "%s", kAckName);
  m_pingSent = 0;
  m_lastLatency = 0;
  m_unknownF8[0] = 0;
  m_unknownF8[1] = 0;
  m_unknownF8[2] = 0;
  m_unknownF8[3] = 0;
  m_minSendLatency = 0;
  m_maxSendLatency = 0;
  m_minReceiveLatency = 0;
  m_maxReceiveLatency = 0;
  m_dupeSendPercent = 0;
  m_dupeReceivePercent = 0;
  m_dropSendPercent = 0;
  m_dropReceivePercent = 0;
  m_receiveEvent = nullptr;
  m_resendInterval = kResendDefaultMs;
  g_commMgr = this;
  m_notifyWindow = nullptr;
  m_localIndex = -1;
  g_commMaxPlayers = 0;
  m_receiveBuffer.Reset();
  m_dplay = nullptr;
}

CommMgr::~CommMgr()
{
  Trace(this, kTraceDestroying);
  g_commMgr = nullptr;
  g_commEnumerating = 0;
}

// ---- trivial accessors -----------------------------------------------------------------------------------------

int CommMgr::GetLastLatency() const
{
  return static_cast<int>(m_lastLatency);
}

int CommMgr::GetLocalIndex() const
{
  return m_localIndex;
}

uint32_t CommMgr::GetEnumeratedCount() const
{
  return m_enumeratedCount;
}

uint32_t CommMgr::GetPlayerId(int index) const
{
  return m_playerIds[index];
}

uint32_t CommMgr::FindPlayerIndex(uint32_t id) const
{
  for (uint32_t index = 0; index <= g_commMaxPlayers; ++index)
  {
    if (m_playerIds[index] == id)
      return index;
  }
  return kNoPlayer;
}

// ---- sending ---------------------------------------------------------------------------------------------------

uint32_t CommMgr::QueueMessage(const void* data, uint32_t size, uint32_t category, int reliable)
{
  uint32_t result = 0;
  if (m_playerCount < 2)
    return result;
  CommMgr* comm = GetCommMgr();
  CommOutgoing* outgoing = NewOrNull<CommOutgoing>(data, size, category, reliable);
  auto* packet = static_cast<CommPacket*>(comm->m_pending.FindById(outgoing->m_firstPacketId));
  while (packet && m_dplay)
  {
    if (BroadcastPacket(packet, reliable) != 0)
      return 0;
    if (!m_dplay)
      return 1;
    packet = static_cast<CommPacket*>(packet->m_successor);
  }
  if (reliable)
  {
    outgoing->SetId(outgoing->m_id);
    result = outgoing->m_id;
    comm->m_outgoing.AddWithId(outgoing);
  }
  else if (outgoing)
  {
    delete outgoing;
  }
  return result;
}

int CommMgr::QueuePacket(CommPacket* packet, int player, int markReliable)
{
  if (!m_dplay)
    return kCommNotConnected;
  if (m_playerCount < 2)
    return kCommNoOtherPlayers;
  const uint32_t id = GetPlayerId(player);
  if (id == 0 || id == kNoPlayer)
    return kCommNoOtherPlayers;
  if (id == m_localId || player == m_localIndex)
    return kCommSendToSelf;
  if (!packet)
    return kCommBufferEmpty;
  if (markReliable == 1)
    packet->m_wire.m_reliable = 1;
  SendBuffer& buffer = m_sendBuffers[player];
  if (!buffer.Append(packet))
  {
    const int flushed = FlushPlayer(player);
    if (!m_dplay)
      return kCommNotConnected;
    Trace(this, kTraceBufferSplit);
    if (flushed != 0)
      return flushed;
    if (!buffer.Append(packet))
    {
      Trace(this, kTraceErrorSending);
      return kCommSendFailed;
    }
  }
  packet->m_ack->MarkSent(player);
  return 0;
}

int CommMgr::BroadcastPacket(CommPacket* packet, int reliable)
{
  if (!m_dplay)
    return kCommNotConnected;
  if (m_playerCount < 2)
    return kCommNoOtherPlayers;
  if (!packet)
    return kCommBufferEmpty;
  SendBuffer& broadcast = m_sendBuffers[kBroadcastIndex];
  if (!broadcast.Append(packet))
  {
    const int flushed = FlushPlayer(kBroadcastIndex);
    if (!m_dplay)
      return kCommNotConnected;
    Trace(this, kTraceBufferSplit);
    if (flushed != 0)
      return flushed;
    if (!broadcast.Append(packet))
    {
      Trace(this, kTraceBufferEnd);
      return kCommSendFailed;
    }
  }
  if (!m_dplay)
    return kCommNotConnected;
  if (reliable == 1)
  {
    for (uint32_t player = 0; player < GetEnumeratedCount(); ++player)
    {
      if (player != static_cast<uint32_t>(GetLocalIndex()) && player < 0x10 && m_playerIds[player] != kNoPlayer)
        packet->m_ack->MarkSent(player);
    }
  }
  return 0;
}

int CommMgr::FlushAll()
{
  if (!m_dplay)
    return 0;
  int result = 0;
  if (m_playerCount >= 2)
  {
    for (uint32_t player = 0; player < g_commMaxPlayers; ++player)
      FlushPlayer(player);
    result = FlushPlayer(kBroadcastIndex) == 0;
  }
  return result;
}

int CommMgr::ResendUnacked()
{
  int result = 0;
  const DWORD now = timeGetTime();
  bool anyPending = false;
  auto* packet = static_cast<CommPacket*>(m_pending.First());
  for (;;)
  {
    if (!packet || !m_dplay)
    {
      if (!anyPending)
        result = kCommWaitingForAcks;
      return result;
    }
    if (packet->m_wire.m_reliable)
    {
      for (uint32_t player = 0; player < GetEnumeratedCount(); ++player)
      {
        if (packet->m_ack->GetState(player) != 1)
          continue;
        const uint32_t sent = packet->m_ack->GetSendTime(player);
        anyPending = true;
        if (m_resendInterval < now - sent)
        {
          result = QueuePacket(packet, player, 0);
          FlushAll();
          if (result != 0)
          {
            Trace(this, kTraceResendError);
            return result;
          }
        }
      }
    }
    if (!m_dplay)
      break;
    packet = static_cast<CommPacket*>(packet->m_successor);
  }
  return 1;
}

int CommMgr::SendAck(ReceivedPacket* packet)
{
  const uint32_t id = packet->m_wire.m_id;
  CommPacket ack(m_ackName, static_cast<uint32_t>(strlen(m_ackName) + 1), id, 1, 1, 0);
  const int result = QueuePacket(&ack, id >> 28, 0);
  if (result != 0)
  {
    Trace(this, kTraceAckError);
    return 0;
  }
  return 1;
}

int CommMgr::HandleAck(ReceivedPacket* packet, uint32_t from)
{
  CommMgr* comm = GetCommMgr();
  const uint32_t id = packet->m_wire.m_id;
  if (id == 0 || (id >> 28) != static_cast<uint32_t>(m_localIndex))
    return 0;
  auto* sent = static_cast<CommPacket*>(comm->m_pending.FindById(id));
  if (!sent)
    return 1;
  const DWORD now = timeGetTime();
  const uint32_t latency = now - sent->m_ack->GetSendTime(from);
  ++m_latencyCount;
  m_lastLatency = latency;
  m_latencyTotal += latency;
  if (m_latencyCount == 0)
    m_latencyCount = 1;
  m_latencyAverage = m_latencyTotal / m_latencyCount;
  if (sent->m_ack->GetState(from) == 3)
    return 0;
  if (sent->m_ack->GetState(from) != 1)
    return 0;
  if (sent->m_ack->Acknowledge(from) == 1 && sent->m_ack->IsSettled() == 1)
  {
    const uint32_t firstId = sent->FirstFragmentId();
    auto* outgoing = static_cast<CommOutgoing*>(m_outgoing.FindById(firstId));
    if (outgoing)
    {
      ++outgoing->m_unused1d;
      if (outgoing->m_partCount <= outgoing->m_unused1d)
        m_outgoing.DeleteById(firstId);
    }
    comm->m_pending.DeleteById(id);
  }
  return 1;
}

// ---- receiving -------------------------------------------------------------------------------------------------

void CommMgr::RegroupReceived()
{
  ReceivedPacket* packet = ReceiveIncoming();
  while (packet)
  {
    const uint32_t firstId = packet->m_wire.m_id - packet->m_wire.m_part + 1;
    PartialMessage* partial = FindOrCreatePartial(this, firstId);
    if (partial)
      partial->AddFragment(packet);
    packet = ReceiveIncoming();
  }
}

CommBuffer* CommMgr::TakeMessage(char category)
{
  CommBuffer* message = nullptr;
  auto* partial = static_cast<PartialMessage*>(m_assembling.First());
  while (partial && m_dplay)
  {
    auto* next = static_cast<PartialMessage*>(partial->m_successor);
    if (partial->IsComplete())
    {
      if (static_cast<char>(partial->GetCategory()) == 0)
      {
        message = partial->Assemble();
        if (message)
        {
          RemovePartial(this, partial);
          if (EatMessage(message))
          {
            DeleteMessage(message);
            message = nullptr;
          }
          if (!m_dplay)
            return nullptr;
        }
      }
      else if (static_cast<char>(partial->GetCategory()) == category)
      {
        message = partial->Assemble();
        if (message)
        {
          RemovePartial(this, partial);
          return message;
        }
      }
    }
    partial = next;
  }
  ReceivedPacket* packet = ReceiveIncoming();
  for (;; packet = ReceiveIncoming())
  {
    if (!packet)
      return message;
    const uint32_t firstId = packet->m_wire.m_id - packet->m_wire.m_part + 1;
    PartialMessage* completed = FindOrCreatePartial(this, firstId);
    if (!completed->AddFragment(packet))
    {
      if (message)
        return message;
      continue;
    }
    if (static_cast<char>(completed->GetCategory()) != 0)
    {
      if (static_cast<char>(completed->GetCategory()) == category)
      {
        message = completed->Assemble();
        if (!message)
          continue;
        RemovePartial(this, completed);
      }
      if (message)
        return message;
      continue;
    }
    message = completed->Assemble();
    if (!message)
      continue;
    RemovePartial(this, completed);
    if (EatMessage(message))
    {
      DeleteMessage(message);
      message = nullptr;
    }
    if (!m_dplay)
      return nullptr;
    if (message)
      return message;
  }
}

int CommMgr::EatMessage(CommBuffer* message)
{
  int isCommMessage = 0;
  if (message)
  {
    CommMessage parsed(static_cast<const char*>(message->m_data));
    isCommMessage = parsed.m_magic == kCommMessageMagic;
    if (isCommMessage && parsed.m_kind == kQuitMessageKind)
    {
      const int player = parsed.m_arg;
      CleanUp();
      Trace(this, kTraceEatQuit);
      NotifyOwner(this, kCommSessionEnded, player);
    }
  }
  return isCommMessage;
}

void CommMgr::PingOpponents()
{
  if (m_playerCount < 2)
  {
    m_lastReceiveTime = timeGetTime();
    m_pingSent = 0;
    return;
  }
  const int local = GetLocalIndex();
  Trace(this, kTracePingStart);
  const uint32_t id = (static_cast<uint32_t>(local) << 28) | kPingIdLowBits;
  CommPacket* packet = NewOrNull<CommPacket>(kPingText, static_cast<uint32_t>(sizeof(kPingText)), id, 1, 1, 1);
  const int result = BroadcastPacket(packet, 1);
  FlushAll();
  m_lastReceiveTime = timeGetTime();
  m_pingSent = 1;
  char text[100];
  sprintf(text, kTracePingResult, id, result);
  Trace(this, text);
  Trace(this, kTraceNewline);
}

void CommMgr::ClearCategory(char category)
{
  if (category == 0)
    return;
  ClearCategoryFrom(&m_assembling, category);
  ClearCategoryFrom(&m_outgoing, category);
}

void CommMgr::ClearCategoryFrom(IdAvlTree* tree, char category)
{
  if (!m_dplay)
    return;
  AvlNode* node = tree->First();
  while (node)
  {
    AvlNode* next = node->m_successor;
    if (static_cast<char>(CategoryOfNode(static_cast<IdNode*>(node))) == category)
      tree->DeleteById(static_cast<IdNode*>(node)->m_id);
    node = next;
  }
}

int CommMgr::DeactivatePlayer(uint32_t index)
{
  if (index >= g_commMaxPlayers || m_playerIds[index] == kNoPlayer)
    return kCommNoOtherPlayers;
  m_playerIds[index] = kNoPlayer;
  --m_playerCount;
  auto* packet = static_cast<CommPacket*>(m_pending.First());
  while (packet)
  {
    if (packet->m_ack->GetState(index) == 1)
      packet->m_ack->SetState(index, 2);
    auto* next = static_cast<CommPacket*>(packet->m_successor);
    if (packet->m_ack->IsSettled() == 1)
      m_pending.DeleteById(packet->m_id);
    packet = next;
  }
  if (index != kNoPlayer && index != kBroadcastIndex)
  {
    CleanUp();
    Trace(this, kTracePlayerLost);
    NotifyOwner(this, kCommPlayerLeft, index);
  }
  return 0;
}
