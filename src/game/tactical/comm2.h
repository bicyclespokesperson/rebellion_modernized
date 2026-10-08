#pragma once
#include <cstdint>

#include "core/game_string.h"

// Multiplayer glue between the tactical engine (and the lobby) and the communication manager (CommMgr, a library class
// at 0x5f7xxx reached through GetCommMgr, 0x5f7cc0): thin null-checked wrappers (0x596fa0-0x597530, "2Comm.cpp"). The
// wrappers return the CommMgr's own result, or 0x11 (no connection) / 0 / 1 without a CommMgr, as noted.

class CommMgr;

// A GUID passed by value (the address of a player or session).
struct CommGuid {
    uint32_t m_data[4];
};

// Sends `bufferSize` bytes: through the session (`toSession` set) addressed to `target`, else to a player. Returns 0x11
// without a CommMgr.
int __cdecl CommSend(uint32_t guid0, uint32_t guid1, uint32_t guid2, uint32_t guid3, int sendMode, int flags, int sessionFlags,
                     int toSession, int player0, int player1, int player2, int player3);  // 0x596fa0

int CommShutdown();                                     // 0x597030
int CommReceiveAll();                                   // 0x597040
int CommPoll();                                         // 0x597050
int CommIsConnected();                                  // 0x597060
int CommFlush();                                        // 0x597080
int CommGetPlayerCount();                               // 0x597090
void __cdecl CommClearCategory(int category);           // 0x5970a0
int __cdecl CommSendMessage(int a, int b, int c, int unused);  // 0x5970d0 (0 without a connection)
int __cdecl CommQueryPlayer(int a);                     // 0x597110 (0 without a connection)
int CommIsSessionHost();                                // 0x597140 (1 without a connection)
int CommGetSessionState();                              // 0x597170 (1 without a connection)
void __cdecl CommSetField194(int value);                // 0x597190
int CommGetLocalId();                                   // 0x5971a0
int CommGetSessionCount();                              // 0x5971c0
int __cdecl CommSetSessionOption(int option);           // 0x5971e0 (0x11 without a CommMgr)
int __cdecl CommGetPlayerName(int player, int* status);  // 0x597210
int __cdecl CommJoinSession(int session);               // 0x597240 (0x11 without a CommMgr)
int __cdecl CommLeaveSession(int session);              // 0x597270 (0x11 without a CommMgr)
void __cdecl CommSetPlayerName(String name);            // 0x5972a0
void CommSyncState();                                   // 0x597310
void __cdecl CommTrace(const char* text);               // 0x597340
void CommSendPlayerQuit();                              // 0x597360
void __cdecl CommSetField2c(int value);                 // 0x597400
void __cdecl CommSetGlobal6be3b4(int value);            // 0x597420
void* __thiscall CommGetSelf(void* self);               // 0x597440
int CommGetCounter();                                   // 0x597460
int CommGetFlag354();                                   // 0x597480
int CommGetFlag358();                                   // 0x5974a0
void CommClearFlag354();                                // 0x5974c0
void __thiscall CommAssignName(CommMgr* comm, String name);  // 0x5974e0
