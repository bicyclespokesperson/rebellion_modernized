// The part of CommMgr that talks to DirectPlay: sessions, players, the datagram send and receive and the lobby
// hand-over. The protocol on top of it is in comm_mgr.cpp.
#include <cstdio>
#include <cstring>
#include <new>
#include <string_view>

#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/icf_stubs.h"
#include "multiplayer/comm_mgr.h"
#include "port/native/native_clock.h"

// The shared empty function that stands where the debug build traced; every call keeps its argument.

namespace
{

constexpr std::string_view kTraceCleanUp = "CommMgr.cpp line 337 CleanUp() Call\n";
constexpr std::string_view kTraceDestroyPlayer = "CommMgr::CleanUp():369 About to call DestroyPlayer.\n";
constexpr std::string_view kTraceQuitSession = "CommMgr::ResetPlayerList():644 CM_PLAYER_QUIT_SESSION\n";
constexpr std::string_view kTraceHostCleanUp = "CommMgr::HostNewSession():1850 Just called CleanUp()\n";
constexpr std::string_view kTraceListInvalid = "CommMgr::GetSessionList():1850 DPERR_INVALIDPARAMS\n";
constexpr std::string_view kTraceJoinCleanUp = "CommMgr::JoinSession():1959 Just Called CleanUp()\n";
constexpr char kTraceBusy[] = "CommMgr::Flush():234 Error Sending Packet! Dplay BUSY - Dplay Error Code : %u ";
constexpr std::string_view kTracePlayerInvalid = "CommMgr::Flush():245 CM_PLAYER_QUIT_SESSION\n";
constexpr std::string_view kTraceReceiveQuit = "CommMgr::ReceiveIncomingCommPacket():1590 CM_PLAYER_QUIT_SESSION\n";
constexpr std::string_view kTraceReceiveLost = "CommMgr::ReceiveIncomingCommPacket()1434 CM_PLAYER_LEFT_UNEXCUSED\n";
constexpr std::string_view kTraceAddPlayer = "CommMgr::ReceiveIncomingCommPacket() DPSYS_ADDPLAYER\n";
constexpr std::string_view kTraceDeletePlayer = "CommMgr::ReceiveIncomingCommPacket() DPSYS_DELETEPLAYER\n";
constexpr char kTraceAckId[] = "AckID %u, Category %u\n";
constexpr std::string_view kNewline = "\n";
constexpr std::string_view kTextFlushing = " Error Flushing Packet!";
constexpr std::string_view kTextDPlayError = "DPlay Error Code : ";
constexpr std::string_view kTextFrom = "From    : ";
constexpr std::string_view kTextTo = "To      : ";
constexpr std::string_view kTextSize = "Size    : ";
constexpr std::string_view kTextMessage = "Message : ";
constexpr std::string_view kTextAddress = "Address : ";
constexpr std::string_view kTextReceiveError = "\nDirect Player Receive Error :";
constexpr std::string_view kTextReceiveFrom = "From     :";
constexpr std::string_view kTextReceiveTo = "To       :";
constexpr std::string_view kTextReceiveBuffer = "BufferSize :";

constexpr std::string_view kMinimumSendLatency = "MinimumSendLatency";
constexpr std::string_view kMaximumSendLatency = "MaximumSendLatency";
constexpr std::string_view kMinimumReceiveLatency = "MinimumReceiveLatency";
constexpr std::string_view kMaximumReceiveLatency = "MaximumReceiveLatency";
constexpr std::string_view kDropSendPercent = "DropSendPercent";
constexpr std::string_view kDropReceivePercent = "DropReceivePercent";
constexpr std::string_view kDupeSendPercent = "DupeSendPercent";
constexpr std::string_view kDupeReceivePercent = "DupeReceivePercent";

constexpr uint32_t kBroadcastIndex = 0xf;
constexpr uint32_t kNoPlayer = 0xffffffff;
constexpr uint32_t kSystemMessageAddPlayer = 3;
constexpr uint32_t kSystemMessageDeletePlayer = 5;
constexpr DWORD kPingAfterMs = 30000;
constexpr DWORD kGiveUpAfterMs = 90000;
constexpr DWORD kReceiveFlags = DPRECEIVE_ALL;
constexpr DWORD kEnumSessionsAvailable = 1;
constexpr DWORD kPlayerCreateFlags = 0;
constexpr DWORD kSessionClosedFlags = DPSESSION_NEWPLAYERSDISABLED | DPSESSION_JOINDISABLED;
constexpr int kIdTextBuffer = 100;
constexpr uint32_t kPingIdLowBits = 0xffffff;

void Trace(CommMgr* comm, std::string_view text)
{
  IgnoreOneArgument(comm, reinterpret_cast<uintptr_t>(text.data()));
}

void NotifyOwner(CommMgr* comm, int code, uint32_t player)
{
  reb::compat::PostMessageA(comm->m_notifyWindow, kCommNotifyMessage, code, player);
}

// The error report the original builds around the trace call: "<tick> Error Flushing Packet!\n", the DirectPlay error
// code, the sender and receiver ids, the size, the message text and its address, each followed by a newline.
class FlushReport
{
  public:
  FlushReport(CommMgr* comm, HRESULT error, int player, const SendBuffer& buffer)
  {
    {
      const std::string newline(kNewline);
      const std::string label(kTextFlushing);
      m_text.append(std::to_string(reb::compat::timeGetTime()));
      m_text += label;
      m_text += newline;
    }
    append(std::string(kNewline), std::string(kTextDPlayError), error);
    append(std::string(kNewline), std::string(kTextFrom), comm->GetPlayerId(comm->GetLocalIndex()));
    append(std::string(kNewline), std::string(kTextTo), comm->GetPlayerId(player));
    append(std::string(kNewline), std::string(kTextSize), buffer.m_used);
    const std::string newline(kNewline);
    const std::string text(reinterpret_cast<const char*>(&buffer.m_count));
    const std::string label(kTextMessage);
    m_text += label;
    m_text += text;
    m_text += newline;
    const std::string addressNewline(kNewline);
    const std::string addressLabel(kTextAddress);
    m_text += addressLabel;
    m_text.append(std::to_string(reinterpret_cast<uintptr_t>(&buffer.m_count)));
    m_text += addressNewline;
  }

  const char* text() const
  {
    return m_text.c_str();
  }

  private:
  void append(const std::string& newline, const std::string& label, uint32_t value)
  {
    m_text += label;
    m_text.append(std::to_string(value));
    m_text += newline;
  }

  std::string m_text;
};

} // namespace

// ---- the DPLAYX callbacks --------------------------------------------------------------------------------------

BOOL CommEnumServiceProviderCallback(GUID* guid, LPSTR name, DWORD majorVersion, DWORD minorVersion, LPVOID context)
{ // 0x5f88d0
  auto* info
      = NewOrNull<ServiceProviderInfo>(guid, name, majorVersion, minorVersion, reinterpret_cast<DWORD_PTR>(context));
  if (info && !g_serviceProviderList.AddWithId(info)) {
    delete info;
  }
  return TRUE;
}

BOOL CommEnumSessionsCallback(const DPSESSIONDESC2* description, LPDWORD timeout, DWORD flags, LPVOID context)
{ // 0x5f8950
  if (!description) {
    return FALSE;
  }
  auto* info = NewOrNull<SessionInfo>(description);
  if (info && !g_sessionList.AddWithId(info)) {
    delete info;
  }
  return TRUE;
}

BOOL CommEnumPlayersCallback(DPID id, DWORD playerType, const DPNAME* name, DWORD flags, LPVOID context)
{ // 0x5f89c0
  CommMgr* comm = GetCommMgr();
  if (comm->GetEnumeratedCount() >= g_commMaxPlayers) {
    return FALSE;
  }
  if (id == 0) {
    return TRUE;
  }
  comm->m_playerIds[comm->GetEnumeratedCount()] = id;
  ++comm->m_enumeratedCount;
  ++comm->m_playerCount;
  return TRUE;
}

// ---- session ---------------------------------------------------------------------------------------------------

int CommMgr::Init(HWND window, GUID application, uint32_t maxPlayers)
{
  int result = 0;
  g_commEnumerating = 0;
  m_lastReceiveTime = reb::compat::timeGetTime();
  m_pingSent = 0;
  m_unknownF8[0] = 0;
  m_unknownF8[1] = 0;
  m_unknownF8[2] = 0;
  m_unknownF8[3] = 0;
  {
    const std::string key(kMinimumSendLatency);
    g_commSettings.FindInText(&key, &m_minSendLatency);
  }
  {
    const std::string key(kMaximumSendLatency);
    g_commSettings.FindInText(&key, &m_maxSendLatency);
  }
  {
    const std::string key(kMinimumReceiveLatency);
    g_commSettings.FindInText(&key, &m_minReceiveLatency);
  }
  {
    const std::string key(kMaximumReceiveLatency);
    g_commSettings.FindInText(&key, &m_maxReceiveLatency);
  }
  {
    const std::string key(kDropSendPercent);
    g_commSettings.FindFloat(&key, &m_dropSendPercent);
  }
  {
    const std::string key(kDropReceivePercent);
    g_commSettings.FindFloat(&key, &m_dropReceivePercent);
  }
  {
    const std::string key(kDupeSendPercent);
    g_commSettings.FindFloat(&key, &m_dupeSendPercent);
  }
  {
    const std::string key(kDupeReceivePercent);
    g_commSettings.FindFloat(&key, &m_dupeReceivePercent);
  }
  m_notifyWindow = window;
  m_isHost = 0;
  m_application = application;
  m_clientMode = 1;
  m_receiveBuffer.Reset();
  g_commProviderGuid = nullptr;
  m_dplay = nullptr;
  m_localIndex = -1;
  m_playerCount = 0;
  m_enumeratedCount = 0;
  m_unknownA4 = 0;
  m_unknownA8 = 0x3000;
  m_unknownAC = native_clock::TickMs() + m_unknownA8;
  g_commMaxPlayers = maxPlayers;
  m_resendInterval = 1000;
  for (uint32_t player = 0; player < g_commMaxPlayers; ++player) {
    m_playerIds[player] = kNoPlayer;
  }
  if (g_lobbyConnection.m_launched != 0) {
    m_dplay = g_lobbyConnection.m_dplay;
    g_commSessionGuid = g_lobbySessionGuid;
    m_isHost = g_lobbyConnection.m_host;
    result = StartPlayerCreation();
  }
  return result;
}

int CommMgr::CleanUp()
{
  g_commEnumerating = 0;
  g_lobbyConnection.m_launched = 0;
  Trace(this, kTraceCleanUp);
  m_lastReceiveTime = reb::compat::timeGetTime();
  m_pingSent = 0;
  m_isHost = 0;
  g_commPacketCounter = 0;
  m_seen.clear();
  m_unknown198.Clear();
  m_unknown1a4.Clear();
  m_playerCount = 0;
  m_enumeratedCount = 0;
  g_serviceProviderList.Clear();
  g_sessionList.Clear();
  ClearOwnedRegistries();
  if (!m_dplay) {
    return 1;
  }
  if (m_localIndex != -1) {
    Trace(this, kTraceDestroyPlayer);
    m_dplay->DestroyPlayer(m_playerIds[m_localIndex]);
    m_localIndex = -1;
  }
  m_receiveBuffer.Reset();
  const HRESULT closeResult = m_dplay->Close();
  const ULONG references = m_dplay->Release();
  m_dplay = nullptr;
  g_commProviderGuid = nullptr;
  if (closeResult != 0 && references == 0) {
    return 0;
  }
  return 1;
}

int CommMgr::CreateLocalPlayer()
{
  if (!m_dplay) {
    return 0;
  }
  if (m_dplay->CreatePlayer(&m_localId, nullptr, nullptr, nullptr, 0, kPlayerCreateFlags) != 0) {
    return 0;
  }
  m_enumeratedCount = 1;
  m_playerCount = 1;
  m_playerIds[0] = m_localId;
  return 1;
}

uint32_t CommMgr::ResetPlayerList()
{
  if (m_dplay) {
    for (int slot = 0; slot < 15; ++slot) {
      m_playerIds[slot] = kNoPlayer;
    }
    const uint32_t previousCount = m_playerCount;
    m_enumeratedCount = 0;
    m_playerCount = 0;
    m_dplay->EnumPlayers(&g_commSessionGuid, CommEnumPlayersCallback, nullptr, 0);
    for (uint32_t first = 0; first < m_enumeratedCount; ++first) {
      for (uint32_t other = first + 1; other < m_enumeratedCount; ++other) {
        if (m_playerIds[other] < m_playerIds[first]) {
          const uint32_t swapped = m_playerIds[first];
          m_playerIds[first] = m_playerIds[other];
          m_playerIds[other] = swapped;
        }
      }
    }
    m_localIndex = -1;
    for (uint32_t player = 0; player < g_commMaxPlayers; ++player) {
      if (m_playerIds[player] == m_localId) {
        m_localIndex = static_cast<int>(player);
        break;
      }
    }
    m_playerIds[15] = 0;
    m_playerCount = m_enumeratedCount;
    FlushAll();
    if (m_playerCount != previousCount) {
      g_commPacketCounter = static_cast<uint32_t>(GetLocalIndex()) << 28;
      m_seen.clear();
      if (m_playerCount < previousCount && m_playerCount == 1) {
        CleanUp();
        Trace(this, kTraceQuitSession);
        NotifyOwner(this, kCommSessionEnded, 0);
      }
    }
  }
  return m_enumeratedCount;
}

ServiceProviderList* CommMgr::EnumerateServiceProviders()
{
  g_serviceProviderList.Clear();
  if (reb::compat::DirectPlayEnumerateA(CommEnumServiceProviderCallback, nullptr) != 0) {
    g_serviceProviderList.Clear();
  }
  return &g_serviceProviderList;
}

int CommMgr::OpenProvider(ServiceProviderInfo* provider)
{
  if (!provider) {
    return kCommBadParameter;
  }
  g_commProviderGuid = &provider->m_guid;
  IUnknown* created = nullptr;
  HRESULT result = reb::compat::DirectPlayCreate(&provider->m_guid, &created, nullptr);
  if (created) {
    result = created->QueryInterface(g_iidDirectPlay2A, reinterpret_cast<void**>(&m_dplay));
    created->Release();
  }
  if (result == 0 && m_dplay) {
    return kCommOk;
  }
  g_commProviderGuid = nullptr;
  m_dplay = nullptr;
  return kCommProviderFailed;
}

int CommMgr::FillSessionDesc(DPSESSIONDESC2* description, GUID instance, uint32_t flags, const char* sessionName)
{
  int result = 0;
  if (description) {
    memset(description, 0, sizeof(*description));
    description->dwSize = sizeof(*description);
    description->dwMaxPlayers = g_commMaxPlayers;
    description->dwFlags = flags;
    description->guidApplication = m_application;
    description->lpszSessionNameA = const_cast<char*>(sessionName);
    description->guidInstance = instance;
  }
  if (flags == 2 && !sessionName) {
    result = kCommNameRequired;
  }
  return result;
}

int CommMgr::StartPlayerCreation()
{
  if (!CreateLocalPlayer()) {
    m_dplay->Release();
    m_dplay = nullptr;
    g_commProviderGuid = nullptr;
    return kCommPlayerCreateFailed;
  }
  ResetPlayerList();
  return 0;
}

int CommMgr::HostNewSession(const char* sessionName)
{
  int result = kCommBadParameter;
  if (!m_dplay) {
    return result;
  }
  DPSESSIONDESC2 description;
  GUID unused = {};
  result = FillSessionDesc(&description, unused, g_commSessionFlags, sessionName);
  if (result != 0) {
    return result;
  }
  const HRESULT opened = m_dplay->Open(&description, DPOPEN_CREATE);
  if (opened != 0) {
    CleanUp();
    Trace(this, kTraceHostCleanUp);
    return (opened == DPERR_USERCANCEL) + kCommSessionFull;
  }
  g_commSessionGuid = description.guidInstance;
  result = StartPlayerCreation();
  if (result == 0) {
    m_isHost = 1;
  }
  return result;
}

SessionList* CommMgr::GetSessionList(uint32_t timeoutMs, int* status)
{
  *status = kCommBadParameter;
  if (m_dplay && IsEnumeratingSessions() == 0) {
    g_sessionList.Clear();
    DPSESSIONDESC2 description;
    GUID unused = {};
    *status = FillSessionDesc(&description, unused, 0, nullptr);
    if (*status == 0) {
      g_commEnumerating = 1;
      const int listed = static_cast<int>(
          m_dplay->EnumSessions(&description, timeoutMs, CommEnumSessionsCallback, nullptr, kEnumSessionsAvailable));
      if (listed == 0) {
        *status = 0;
      } else if (listed > static_cast<int>(DPERR_ALREADYINITIALIZED)) {
        if (listed == static_cast<int>(DPERR_INVALIDOBJECT)) {
          CleanUp();
          Trace(this, kTraceListInvalid);
          *status = kCommSessionListFailed;
        } else if (listed == static_cast<int>(DPERR_USERCANCEL) || listed == static_cast<int>(DPERR_CONNECTING)) {
          *status = kCommSessionNotAllowed;
        }
      } else if (listed == static_cast<int>(DPERR_ALREADYINITIALIZED)) {
        *status = kCommSessionListFailed;
      } else if (listed == static_cast<int>(DPERR_INVALIDPARAMS)) {
        CleanUp();
        Trace(this, kTraceListInvalid);
        *status = kCommSessionListFailed;
      }
    }
  }
  if (*status == 0) {
    g_commEnumerating = 0;
    return &g_sessionList;
  }
  g_commEnumerating = 1;
  g_sessionList.Clear();
  return &g_sessionList;
}

int CommMgr::JoinSession(SessionInfo* session)
{
  if (!m_dplay || !session) {
    return kCommBadParameter;
  }
  DPSESSIONDESC2 description;
  FillSessionDesc(&description, session->m_instance, g_commSessionFlags, session->m_name.c_str());
  const HRESULT opened = m_dplay->Open(&description, DPOPEN_JOIN);
  if (opened != 0) {
    CleanUp();
    Trace(this, kTraceJoinCleanUp);
    return (opened == DPERR_USERCANCEL) + kCommSessionFull;
  }
  g_commSessionGuid = session->m_instance;
  return StartPlayerCreation();
}

int CommMgr::CloseSessionToNewPlayers()
{
  if (m_dplay && m_isHost) {
    DWORD size = 0;
    m_dplay->GetSessionDesc(nullptr, &size);
    auto* description = static_cast<DPSESSIONDESC2*>(::operator new(size));
    memset(description, 0, size);
    if (m_dplay->GetSessionDesc(description, &size) == 0) {
      description->dwMaxPlayers = g_commMaxPlayers;
      description->dwFlags |= kSessionClosedFlags;
      m_dplay->SetSessionDesc(description, 0);
    }
    ::operator delete(description);
  }
  return 0;
}

int QueryLobbyConnection()
{
  int found = 0;
  IDirectPlayLobbyA* lobby = nullptr;
  DPLCONNECTION* settings = nullptr;
  DWORD size = 0;
  g_lobbyConnection.m_launched = 0;
  if (reb::compat::DirectPlayLobbyCreateA(nullptr, &lobby, nullptr, nullptr, 0) != 0) {
    return found;
  }
  if (lobby->GetConnectionSettings(0, nullptr, &size) != DPERR_BUFFERTOOSMALL || size == 0) {
    return found;
  }
  settings = static_cast<DPLCONNECTION*>(::operator new(size));
  if (!settings) {
    return found;
  }
  if (lobby->GetConnectionSettings(0, settings, &size) != 0) {
    return found;
  }
  settings->lpSessionDesc->dwFlags = g_commSessionFlags;
  settings->lpSessionDesc->dwMaxPlayers = g_commMaxPlayers;
  if (lobby->SetConnectionSettings(0, 0, settings) != 0) {
    return found;
  }
  IUnknown* connected = nullptr;
  if (lobby->Connect(0, &connected, nullptr) != 0) {
    return found;
  }
  IDirectPlay2A* dplay = nullptr;
  if (connected->QueryInterface(g_iidDirectPlay2A, reinterpret_cast<void**>(&dplay)) != 0) {
    return found;
  }
  found = 1;
  g_lobbyConnection.m_launched = 1;
  g_lobbyConnection.m_dplay = dplay;
  g_lobbySessionGuid = settings->lpSessionDesc->guidInstance;
  g_lobbyConnection.m_host = (settings->dwFlags & DPOPEN_CREATE) != 0;
  return found;
}

// ---- the datagram pump -----------------------------------------------------------------------------------------

int CommMgr::FlushPlayer(int player)
{
  int result = 0;
  if (!m_dplay) {
    return kCommNotConnected;
  }
  if (m_playerCount < 2) {
    return kCommNoOtherPlayers;
  }
  if (player == GetLocalIndex()) {
    return kCommSendToSelf;
  }
  SendBuffer& buffer = m_sendBuffers[player];
  if (buffer.GetCount() == 0) {
    return kCommBufferEmpty;
  }
  if (!m_dplay) {
    return kCommNotConnected;
  }
  const HRESULT sent
      = m_dplay->Send(GetPlayerId(GetLocalIndex()), GetPlayerId(player), 0, &buffer.m_count, buffer.m_used);
  if (sent == 0) {
    buffer.Reset();
    return 0;
  }
  if (sent == DPERR_BUSY) {
    char text[kIdTextBuffer];
    sprintf(text, kTraceBusy, sent);
    Trace(this, text);
    buffer.Reset();
    return 0;
  }
  {
    const FlushReport report(this, sent, player, buffer);
    Trace(this, report.text());
  }
  if (sent == DPERR_INVALIDPLAYER && player != static_cast<int>(kBroadcastIndex)) {
    Trace(this, kTracePlayerInvalid);
    NotifyOwner(this, kCommSessionEnded, player);
    result = kCommNoOtherPlayers;
  } else {
    result = kCommSendFailed;
  }
  NotifyOwner(this, kCommSessionEnded, player);
  CleanUp();
  return result;
}

ReceivedPacket* CommMgr::ReceiveIncoming()
{
  const DWORD now = reb::compat::timeGetTime();
  if (m_playerCount < 2) {
    m_lastReceiveTime = now;
    m_pingSent = 0;
  } else {
    const DWORD silent = now - m_lastReceiveTime;
    if (silent > kPingAfterMs - 1) {
      if (m_pingSent == 0) {
        PingOpponents();
      } else if (silent > kGiveUpAfterMs - 1) {
        CleanUp();
        Trace(this, kTraceReceiveLost);
        NotifyOwner(this, kCommPlayerLost, 0);
        return nullptr;
      }
    }
  }
  if (!m_dplay) {
    return nullptr;
  }
  for (;;) {
    ReceivedPacket* packet = nullptr;
    while (m_receiveBuffer.GetCount() != 0) {
      packet = m_receiveBuffer.PopPacket();
      const uint32_t id = packet->m_wire.m_id;
      if ((id >> 28) == static_cast<uint32_t>(GetLocalIndex())) {
        HandleAck(packet, m_receiveBuffer.m_fromPlayer);
      } else {
        if (packet->m_wire.m_reliable != 0 && id != 0) {
          SendAck(packet);
          FlushAll();
        }
        const uint32_t packetId = packet->m_wire.m_id;
        if ((packetId & kPingIdLowBits) != kPingIdLowBits) {
          if (packetId != 0 && !m_seen.contains(packetId)) {
            try {
              m_seen.insert(packetId);
            } catch (const std::bad_alloc&) {
            }
            char text[kIdTextBuffer];
            sprintf(text, kTraceAckId, packetId & 0xffffff, (packetId >> 24) & 0xf);
            return packet;
          }
        }
      }
      if (packet) {
        delete packet;
      }
    }
    m_receiveBuffer.Reset();
    if (!m_dplay) {
      return nullptr;
    }
    DPID from = 0;
    DPID to = 0;
    DWORD size = kCommPacketBufferSize;
    uint8_t* writePointer = m_receiveBuffer.GetWritePointer();
    const HRESULT received = m_dplay->Receive(&from, &to, kReceiveFlags, writePointer, &size);
    if (received != 0) {
      if (received != DPERR_NOMESSAGES) {
        std::string report;
        {
          const std::string newline1(kNewline);
          const std::string bufferLabel(kTextReceiveBuffer);
          const std::string newline2(kNewline);
          const std::string toLabel(kTextReceiveTo);
          const std::string newline3(kNewline);
          const std::string fromLabel(kTextReceiveFrom);
          const std::string newline4(kNewline);
          const std::string errorLabel(kTextReceiveError);
          report += errorLabel;
          report.append(std::to_string(received));
          report += newline4;
          report += fromLabel;
          report.append(std::to_string(from));
          report += newline3;
          report += toLabel;
          report.append(std::to_string(to));
          report += newline2;
          report += bufferLabel;
          report.append(std::to_string(size));
          report += newline1;
        }
        Trace(this, report.c_str());
        return nullptr;
      }
      native_clock::TickMs();
      return nullptr;
    }
    m_lastReceiveTime = reb::compat::timeGetTime();
    m_pingSent = 0;
    if (from == 0) {
      const auto* message = reinterpret_cast<const uint32_t*>(writePointer);
      if (message[0] == kSystemMessageAddPlayer) {
        Trace(this, kTraceAddPlayer);
        ResetPlayerList();
      } else if (message[0] == kSystemMessageDeletePlayer) {
        Trace(this, kTraceDeletePlayer);
        DeactivatePlayer(FindPlayerIndex(message[1]));
        if (ResetPlayerList() < 2) {
          CleanUp();
          Trace(this, kTraceReceiveQuit);
          NotifyOwner(this, kCommSessionEnded, 0);
          return nullptr;
        }
      }
      m_receiveBuffer.Reset();
      continue;
    }
    if (from == m_localId) {
      m_receiveBuffer.Reset();
      continue;
    }
    uint32_t sender = FindPlayerIndex(from);
    if (m_clientMode == 1 && sender == kNoPlayer) {
      ResetPlayerList();
      sender = FindPlayerIndex(from);
    }
    m_receiveBuffer.m_fromPlayer = static_cast<int>(sender);
    packet = m_receiveBuffer.PopPacket();
    const uint32_t firstId = packet->m_wire.m_id;
    if ((firstId >> 28) == static_cast<uint32_t>(GetLocalIndex())) {
      HandleAck(packet, m_receiveBuffer.m_fromPlayer);
    } else if (sender == (packet->m_wire.m_id >> 28)) {
      if (packet->m_wire.m_reliable != 0 && packet->m_wire.m_id != 0) {
        SendAck(packet);
        FlushAll();
      }
      const uint32_t packetId = packet->m_wire.m_id;
      if ((packetId & kPingIdLowBits) != kPingIdLowBits) {
        if (packetId != 0 && !m_seen.contains(packetId)) {
          try {
            m_seen.insert(packetId);
          } catch (const std::bad_alloc&) {
          }
          char text[kIdTextBuffer];
          sprintf(text, kTraceAckId, packet->m_wire.m_id & 0xffffff, (packet->m_wire.m_id >> 24) & 0xf);
          return packet;
        }
      }
    }
    if (packet) {
      delete packet;
    }
  }
}
