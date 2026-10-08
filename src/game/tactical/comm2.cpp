#include "tactical/comm2.h"

#include "tactical/tactical_externals.h"

// The communication manager is a library class; only the entry points and fields this glue uses are named here.
CommMgr* FUN_005f7cc0();  // GetCommMgr
int FUN_005f7d30();
struct CommTraceSink;
void __thiscall FUN_005f4490(CommTraceSink* sink, int value);
int __thiscall FUN_005f8b80(CommMgr* comm, int flags, CommGuid target, int sessionFlags);
int __thiscall FUN_005ff810(CommMgr* comm, int a, int b, int c, int d, int e);
int __thiscall FUN_005f8600(CommMgr* comm);
int FUN_005fa7a0();
int __thiscall FUN_005f88c0(CommMgr* comm);
int __thiscall FUN_005fa720(CommMgr* comm, int category);
int __thiscall FUN_005f7d90(CommMgr* comm, int a, int b, int c, int d);
int __thiscall FUN_005f9550(CommMgr* comm, int a);
int __thiscall FUN_005f92e0(CommMgr* comm);
int __thiscall FUN_005f9030(CommMgr* comm);
void __thiscall FUN_005f94a0(CommMgr* comm);
int __thiscall FUN_005f8400(CommMgr* comm);
int __thiscall FUN_005f9eb0(CommMgr* comm);
int __thiscall FUN_005f9f70(CommMgr* comm, int a);
int __thiscall FUN_005fa050(CommMgr* comm, int a, int* status);
int __thiscall FUN_005fa180(CommMgr* comm, int a);
int __thiscall FUN_005f9ee0(CommMgr* comm, int a);
void* __thiscall FUN_005fa540(CommMgr* comm);
int FUN_005fa5e0();

struct CommMessage;
CommMessage* __thiscall FUN_005faa80(void* memory, int kind, int id, int a, int b, int c);
void* __thiscall FUN_005faaf0(CommMessage* message);
void __thiscall FUN_005faad0(CommMessage* message);

extern int g_commFlag354;     // 0x6be354
extern int g_commFlag358;     // 0x6be358
extern int g_commGlobal3b4;   // 0x6be3b4
extern CommTraceSink g_commTraceSink;  // 0x6be370

namespace {

constexpr int kNoConnection = 0x11;
constexpr int kStateOffset = 0x108;
constexpr int kPlayerCountOffset = 0x94;
constexpr int kField2cOffset = 0x2c;
constexpr int kField194Offset = 0x194;
constexpr int kHostStateWanted = 0xe;
constexpr int kPlayerQuitMessageKind = 4;
constexpr int kPlayerQuitSize = 0x74;
constexpr char kClearCategoryCalled[] = "ClearCategory Called\n";
constexpr char kClearCategoryReturned[] = "ClearCategory Returned\n";
constexpr char kPlayerQuitTrace[] = "2Comm.cpp line 394 CM_PLAYER_QUIT_SESSION\n";

template <class T>
T& fieldOf(CommMgr* comm, int offset) {
    return *reinterpret_cast<T*>(reinterpret_cast<char*>(comm) + offset);
}

}  // namespace

int __cdecl CommSend(uint32_t guid0, uint32_t guid1, uint32_t guid2, uint32_t guid3, int sendMode, int flags,
                     int sessionFlags, int toSession, int player0, int player1, int player2, int player3) {
    int result = kNoConnection;
    FUN_005f7d30();
    CommMgr* comm = FUN_005f7cc0();
    FUN_005f4490(&g_commTraceSink, sendMode);
    if (comm) {
        if (toSession != 0) {
            CommGuid target = {{guid0, guid1, guid2, guid3}};
            result = FUN_005f8b80(comm, flags, target, sessionFlags);
            return result;
        }
        result = FUN_005ff810(comm, player0, player1, player2, flags, player3);
    }
    return result;
}

int CommShutdown() {
    CommPoll();
    return FUN_005f7d30();
}

int CommReceiveAll() {
    FUN_005f7cc0();
    return FUN_005fa7a0();
}

int CommPoll() { return FUN_005f8600(FUN_005f7cc0()); }

int CommIsConnected() { return fieldOf<int>(FUN_005f7cc0(), kStateOffset) != 0; }

int CommFlush() { return FUN_005f88c0(FUN_005f7cc0()); }

int CommGetPlayerCount() { return fieldOf<int>(FUN_005f7cc0(), kPlayerCountOffset); }

void __cdecl CommClearCategory(int category) {
    CommTrace(kClearCategoryCalled);
    FUN_005fa720(FUN_005f7cc0(), category);
    CommTrace(kClearCategoryReturned);
}

int __cdecl CommSendMessage(int a, int b, int c, int unused) {
    int result = 0;
    CommMgr* comm = FUN_005f7cc0();
    if (CommIsConnected() != 0) result = FUN_005f7d90(comm, a, b, c, 1);
    return result;
}

int __cdecl CommQueryPlayer(int a) {
    int result = 0;
    CommMgr* comm = FUN_005f7cc0();
    if (CommIsConnected() != 0) result = FUN_005f9550(comm, a);
    return result;
}

int CommIsSessionHost() {
    bool host = true;
    CommMgr* comm = FUN_005f7cc0();
    if (comm && fieldOf<int>(comm, kStateOffset) != 0) host = FUN_005f92e0(comm) == kHostStateWanted;
    return host;
}

int CommGetSessionState() {
    CommMgr* comm = FUN_005f7cc0();
    int result = 1;
    if (comm && fieldOf<int>(comm, kStateOffset) != 0) result = FUN_005f9030(comm);
    return result;
}

void __cdecl CommSetField194(int value) { fieldOf<int>(FUN_005f7cc0(), kField194Offset) = value; }

int CommGetLocalId() {
    CommMgr* comm = FUN_005f7cc0();
    if (comm) return FUN_005f8400(comm);
    return 0;
}

int CommGetSessionCount() {
    int result = 0;
    CommMgr* comm = FUN_005f7cc0();
    if (comm) result = FUN_005f9eb0(comm);
    return result;
}

int __cdecl CommSetSessionOption(int option) {
    int result = kNoConnection;
    CommMgr* comm = FUN_005f7cc0();
    if (comm) result = FUN_005f9f70(comm, option);
    return result;
}

int __cdecl CommGetPlayerName(int player, int* status) {
    int result = 0;
    *status = kNoConnection;
    CommMgr* comm = FUN_005f7cc0();
    if (comm) result = FUN_005fa050(comm, player, status);
    return result;
}

int __cdecl CommJoinSession(int session) {
    int result = kNoConnection;
    CommMgr* comm = FUN_005f7cc0();
    if (comm) result = FUN_005fa180(comm, session);
    return result;
}

int __cdecl CommLeaveSession(int session) {
    int result = kNoConnection;
    CommMgr* comm = FUN_005f7cc0();
    if (comm) result = FUN_005f9ee0(comm, session);
    return result;
}

void __cdecl CommSetPlayerName(String name) {
    CommMgr* comm = FUN_005f7cc0();
    if (comm) CommAssignName(comm, name);
}

void CommSyncState() {
    CommMgr* comm = FUN_005f7cc0();
    if (comm && fieldOf<int>(comm, kStateOffset) != 0) {
        FUN_005f94a0(comm);
        FUN_005f92e0(comm);
        FUN_005f9030(comm);
    }
}

void __cdecl CommTrace(const char* text) {
    if (FUN_005f7cc0()) FUN_005ae1e0(const_cast<char*>(text));
}

void CommSendPlayerQuit() {
    CommTrace(kPlayerQuitTrace);
    const int flushed = CommFlush();
    alignas(8) char storage[kPlayerQuitSize];
    CommMessage* message = FUN_005faa80(storage, kPlayerQuitMessageKind, flushed, 0, 0, 0);
    void* buffer = FUN_005faaf0(message);
    CommSendMessage(reinterpret_cast<int>(buffer), kPlayerQuitSize, 0, 1);
    CommGetSessionState();
    operator delete(buffer);
    FUN_005faad0(message);
}

void __cdecl CommSetField2c(int value) {
    CommMgr* comm = FUN_005f7cc0();
    if (comm) fieldOf<int>(comm, kField2cOffset) = value;
}

void __cdecl CommSetGlobal6be3b4(int value) {
    if (FUN_005f7cc0()) g_commGlobal3b4 = value;
}

void* __thiscall CommGetSelf(void* self) {
    CommMgr* comm = FUN_005f7cc0();
    if (comm) return FUN_005fa540(comm);
    return self;
}

int CommGetCounter() {
    int result = 0;
    if (FUN_005f7cc0()) result = FUN_005fa5e0();
    return result;
}

int CommGetFlag354() {
    int result = 0;
    if (FUN_005f7cc0()) result = g_commFlag354;
    return result;
}

int CommGetFlag358() {
    int result = 0;
    if (FUN_005f7cc0()) result = g_commFlag358;
    return result;
}

void CommClearFlag354() {
    if (FUN_005f7cc0()) g_commFlag354 = 0;
}

void __thiscall CommAssignName(CommMgr* comm, String name) { fieldOf<String>(comm, 0x98).Assign(name); }
