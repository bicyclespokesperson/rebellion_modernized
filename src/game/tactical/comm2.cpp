#include "tactical/comm2.h"

#include <cstring>

#include "multiplayer/comm_mgr.h"

// The shared empty function that stands where the debug build traced (it pops one argument).
void __thiscall FUN_005ae1e0(void* self, const char* text);
// The alternative transport of CommInit: GameWindowObject slot 13's shared "return 0" body.
int __thiscall FUN_005ff810(CommMgr* comm, int a, int b, int c, int d, int e);

namespace {

constexpr int kNoConnection = 0x11;
constexpr int kPlayerQuitMessageKind = 4;
constexpr int kPlayerQuitSize = 0x74;
constexpr char kClearCategoryCalled[] = "ClearCategory Called\n";
constexpr char kClearCategoryReturned[] = "ClearCategory Returned\n";
constexpr char kPlayerQuitTrace[] = "2Comm.cpp line 394 CM_PLAYER_QUIT_SESSION\n";

}  // namespace

int __cdecl CommInit(uint32_t guid0, uint32_t guid1, uint32_t guid2, uint32_t guid3, StringMap* settings, HWND window,
                     int maxPlayers, int connect, int unused9, int unused10, int unused11, int unused12) {
    int result = kNoConnection;
    DestroyCommMgr();
    CommMgr* comm = GetCommMgr();
    g_commSettings = *settings;
    if (comm) {
        if (connect != 0) {
            const uint32_t words[4] = {guid0, guid1, guid2, guid3};
            GUID application;
            memcpy(&application, words, sizeof(application));
            return comm->Init(window, application, maxPlayers);
        }
        result = FUN_005ff810(comm, unused9, unused10, unused11, reinterpret_cast<int>(window), unused12);
    }
    return result;
}

void CommShutdown() {
    CommCleanUp();
    DestroyCommMgr();
}

int CommIsEnumerating() {
    GetCommMgr();
    return IsEnumeratingSessions();
}

int CommCleanUp() { return GetCommMgr()->CleanUp(); }

int CommIsConnected() { return GetCommMgr()->m_dplay != nullptr; }

int CommGetLocalIndex() { return GetCommMgr()->GetLocalIndex(); }

int CommGetPlayerCount() { return GetCommMgr()->m_playerCount; }

void __cdecl CommClearCategory(int category) {
    CommTrace(kClearCategoryCalled);
    GetCommMgr()->ClearCategory(static_cast<char>(category));
    CommTrace(kClearCategoryReturned);
}

uint32_t __cdecl CommSendMessage(const void* data, uint32_t size, int category, int reliable) {
    uint32_t result = 0;
    CommMgr* comm = GetCommMgr();
    if (CommIsConnected() != 0) result = comm->QueueMessage(data, size, category, 1);
    return result;
}

CommBuffer* __cdecl CommTakeMessage(int category) {
    CommBuffer* result = nullptr;
    CommMgr* comm = GetCommMgr();
    if (CommIsConnected() != 0) result = comm->TakeMessage(static_cast<char>(category));
    return result;
}

int CommAllPacketsAcknowledged() {
    bool acknowledged = true;
    CommMgr* comm = GetCommMgr();
    if (comm && comm->m_dplay) acknowledged = comm->ResendUnacked() == kCommWaitingForAcks;
    return acknowledged;
}

int CommFlushAll() {
    CommMgr* comm = GetCommMgr();
    int result = 1;
    if (comm && comm->m_dplay) result = comm->FlushAll();
    return result;
}

void __cdecl CommSetResendInterval(int milliseconds) { GetCommMgr()->m_resendInterval = milliseconds; }

int CommGetLastLatency() {
    CommMgr* comm = GetCommMgr();
    if (comm) return comm->GetLastLatency();
    return 0;
}

ServiceProviderList* CommEnumerateServiceProviders() {
    ServiceProviderList* result = nullptr;
    CommMgr* comm = GetCommMgr();
    if (comm) result = comm->EnumerateServiceProviders();
    return result;
}

int __cdecl CommHostNewSession(const char* sessionName) {
    int result = kNoConnection;
    CommMgr* comm = GetCommMgr();
    if (comm) result = comm->HostNewSession(sessionName);
    return result;
}

SessionList* __cdecl CommGetSessionList(uint32_t timeoutMs, int* status) {
    SessionList* result = nullptr;
    *status = kNoConnection;
    CommMgr* comm = GetCommMgr();
    if (comm) result = comm->GetSessionList(timeoutMs, status);
    return result;
}

int __cdecl CommJoinSession(SessionInfo* session) {
    int result = kNoConnection;
    CommMgr* comm = GetCommMgr();
    if (comm) result = comm->JoinSession(session);
    return result;
}

int __cdecl CommOpenProvider(ServiceProviderInfo* provider) {
    int result = kNoConnection;
    CommMgr* comm = GetCommMgr();
    if (comm) result = comm->OpenProvider(provider);
    return result;
}

void __cdecl CommSetPlayerName(String name) {
    CommMgr* comm = GetCommMgr();
    if (comm) comm->SetPlayerName(name);
}

void CommMgr::SetPlayerName(String name) { m_playerName.Assign(name); }

void CommSyncState() {
    CommMgr* comm = GetCommMgr();
    if (comm && comm->m_dplay) {
        comm->RegroupReceived();
        comm->ResendUnacked();
        comm->FlushAll();
    }
}

void __cdecl CommTrace(const char* text) {
    if (CommMgr* comm = GetCommMgr()) FUN_005ae1e0(comm, text);
}

void CommSendPlayerQuit() {
    CommTrace(kPlayerQuitTrace);
    const int localIndex = CommGetLocalIndex();
    CommMessage message(kPlayerQuitMessageKind, localIndex, 0, 0, 0);
    char* buffer = message.Serialize();
    CommSendMessage(buffer, kPlayerQuitSize, 0, 1);
    CommFlushAll();
    operator delete(buffer);
}

void __cdecl CommSetNotifyWindow(HWND window) {
    CommMgr* comm = GetCommMgr();
    if (comm) comm->m_notifyWindow = window;
}

void __cdecl CommSetSessionFlags(int flags) {
    if (GetCommMgr()) g_commSessionFlags = flags;
}

void* __thiscall CommCloseSessionToNewPlayers(void* self) {
    CommMgr* comm = GetCommMgr();
    if (comm) return reinterpret_cast<void*>(comm->CloseSessionToNewPlayers());
    return self;
}

int CommQueryLobbyConnection() {
    int result = 0;
    if (GetCommMgr()) result = QueryLobbyConnection();
    return result;
}

int CommLaunchedFromLobby() {
    GetCommMgr();
    int result = 0;
    if (GetCommMgr()) result = g_lobbyConnection.m_launched;
    return result;
}

int CommLobbyIsHost() {
    int result = 0;
    if (GetCommMgr()) result = g_lobbyConnection.m_host;
    return result;
}

void CommClearLobbyLaunch() {
    if (GetCommMgr()) g_lobbyConnection.m_launched = 0;
}
