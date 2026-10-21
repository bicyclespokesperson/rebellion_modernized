#pragma once
#include <cstdint>
#include <string_view>

#include "core/game_string.h"
#include "core/string_map.h"
#include "multiplayer/comm_mgr.h"
#include "platform/win32.h"

// Multiplayer glue between the lobby, strategic UI and the communication manager (CommMgr,
// multiplayer/comm_mgr.h): thin null-checked wrappers (0x596fa0-0x597530, "2Comm.cpp"). They return the CommMgr's own
// result, or 0x11 (no connection) / 0 / 1 without a CommMgr, as noted.

// A GUID passed by value (the address of a player or session).
struct CommGuid {
  uint32_t m_data[4];
};

// Starts the communication layer: keeps `settings` as the network test settings and either initialises the CommMgr for
// `window` with the application GUID and `maxPlayers` (`connect` set), or calls the (stub) alternative transport.
// Returns 0x11 without a CommMgr.
int CommInit(uint32_t guid0, uint32_t guid1, uint32_t guid2, uint32_t guid3, StringMap* settings, HWND window,
             int maxPlayers, int connect, int unused9, int unused10, int unused11,
             int unused12); // 0x596fa0

void CommShutdown();                  // 0x597030
int CommIsEnumerating();              // 0x597040 (the session list is being collected)
int CommCleanUp();                    // 0x597050
int CommIsConnected();                // 0x597060
int CommGetLocalIndex();              // 0x597080
int CommGetPlayerCount();             // 0x597090
void CommClearCategory(int category); // 0x5970a0
uint32_t CommSendMessage(const void* data, uint32_t size, int category,
                         int reliable);                           // 0x5970d0 (0 without a connection)
CommBuffer* CommTakeMessage(int category);                        // 0x597110 (null without a connection)
int CommAllPacketsAcknowledged();                                 // 0x597140 (1 without a connection)
int CommFlushAll();                                               // 0x597170 (1 without a connection)
void CommSetResendInterval(int milliseconds);                     // 0x597190
int CommGetLastLatency();                                         // 0x5971a0
ServiceProviderList* CommEnumerateServiceProviders();             // 0x5971c0 (null without a CommMgr)
int CommHostNewSession(const char* sessionName);                  // 0x5971e0 (0x11 without a CommMgr)
SessionList* CommGetSessionList(uint32_t timeoutMs, int* status); // 0x597210 (null without a CommMgr)
int CommJoinSession(SessionInfo* session);                        // 0x597240 (0x11 without a CommMgr)
int CommOpenProvider(ServiceProviderInfo* provider);              // 0x597270 (0x11 without a CommMgr)
void CommSetPlayerName(std::string name);                         // 0x5972a0
void CommSyncState();                                             // 0x597310
void CommTrace(std::string_view text);                            // 0x597340
inline constexpr std::string_view kCancelMultiPlayerTrace
    = "MultiPlayerWindow::DoCancelMultiPlayer(): About to call PlayerQuitSession()!\n";
void CommSendPlayerQuit();                                        // 0x597360
void CommSetNotifyWindow(HWND window);                            // 0x597400
void CommSetSessionFlags(int flags);                              // 0x597420
void* CommCloseSessionToNewPlayers(void* self); // 0x597440 (returns the argument when there is no CommMgr)
int CommQueryLobbyConnection();                 // 0x597460
int CommLaunchedFromLobby();                    // 0x597480
int CommLobbyIsHost();                          // 0x5974a0
void CommClearLobbyLaunch();                    // 0x5974c0
