// DirectPlay (DPLAYX.DLL) as REBEXE.EXE uses it: the DirectPlay 6 interface IDirectPlay2A for sessions and players,
// the lobby interface IDirectPlayLobbyA for sessions launched by a lobby, and the three DPLAYX entry points the
// game imports (DirectPlayCreate, DirectPlayEnumerateA, DirectPlayLobbyCreateA). The Windows SDK of the build
// toolchain has no DirectPlay headers, so the declarations the game needs are written out here. The port replaces the
// implementation of these entry points behind this header (docs/platform_layer.md, "multiplayer").
#pragma once
#include "core/reb_abi.h"
#include "platform/win32.h"

typedef DWORD DPID;
typedef DPID* LPDPID;

constexpr DPID DPID_ALLPLAYERS = 0;
constexpr DPID DPID_SYSMSG = 0;

// DPSESSIONDESC2::dwFlags
constexpr DWORD DPSESSION_NEWPLAYERSDISABLED = 0x00000001;
constexpr DWORD DPSESSION_JOINDISABLED = 0x00000020;

// IDirectPlay2::Open
constexpr DWORD DPOPEN_JOIN = 0x00000001;
constexpr DWORD DPOPEN_CREATE = 0x00000002;

// IDirectPlay2::Receive
constexpr DWORD DPRECEIVE_ALL = 0x00000001;

// IDirectPlay2::Send
constexpr DWORD DPSEND_GUARANTEED = 0x00000001;

// DPLCONNECTION::dwFlags are DPOPEN_JOIN / DPOPEN_CREATE.

constexpr HRESULT DPERR_ALREADYINITIALIZED = 0x88770005;
constexpr HRESULT DPERR_BUFFERTOOSMALL = 0x8877001e;
constexpr HRESULT DPERR_INVALIDOBJECT = 0x88770082;
constexpr HRESULT DPERR_INVALIDPLAYER = 0x88770096;
constexpr HRESULT DPERR_NOCONNECTION = 0x887700aa;
constexpr HRESULT DPERR_NOMESSAGES = 0x887700be;
constexpr HRESULT DPERR_BUSY = 0x8877010e;
constexpr HRESULT DPERR_USERCANCEL = 0x88770118;
constexpr HRESULT DPERR_CONNECTING = 0x8877015e;
constexpr HRESULT DPERR_INVALIDPARAMS = 0x80070057;

struct DPNAME {
    DWORD dwSize;
    DWORD dwFlags;
    LPSTR lpszShortNameA;
    LPSTR lpszLongNameA;
};

struct DPSESSIONDESC2 {
    DWORD dwSize;             // +0x00 0x50
    DWORD dwFlags;            // +0x04
    GUID guidInstance;        // +0x08
    GUID guidApplication;     // +0x18
    DWORD dwMaxPlayers;       // +0x28
    DWORD dwCurrentPlayers;   // +0x2c
    LPSTR lpszSessionNameA;   // +0x30
    LPSTR lpszPasswordA;      // +0x34
    DWORD dwReserved1;        // +0x38
    DWORD dwReserved2;        // +0x3c
    DWORD dwUser1;            // +0x40
    DWORD dwUser2;            // +0x44
    DWORD dwUser3;            // +0x48
    DWORD dwUser4;            // +0x4c
};

struct DPLCONNECTION {
    DWORD dwSize;                    // +0x00
    DWORD dwFlags;                   // +0x04 DPOPEN_JOIN or DPOPEN_CREATE
    DPSESSIONDESC2* lpSessionDesc;   // +0x08
    DPNAME* lpPlayerName;            // +0x0c
    GUID guidSP;                     // +0x10
    LPVOID lpAddress;                // +0x20
    DWORD dwAddressSize;             // +0x24
};

static_assert(sizeof(DPSESSIONDESC2) == 0x50 || sizeof(void*) != 4, "DPSESSIONDESC2 layout");

typedef BOOL(FAR PASCAL* LPDPENUMDPCALLBACKA)(GUID* lpguidSP, LPSTR lpSPName, DWORD dwMajorVersion,
                                              DWORD dwMinorVersion, LPVOID lpContext);
typedef BOOL(FAR PASCAL* LPDPENUMSESSIONSCALLBACK2)(const DPSESSIONDESC2* lpThisSD, LPDWORD lpdwTimeOut,
                                                    DWORD dwFlags, LPVOID lpContext);
typedef BOOL(FAR PASCAL* LPDPENUMPLAYERSCALLBACK2)(DPID dpId, DWORD dwPlayerType, const DPNAME* lpName,
                                                   DWORD dwFlags, LPVOID lpContext);

struct IDirectPlay2A : public IUnknown {
    HRESULT REB_STDCALL QueryInterface(REFIID riid, LPVOID * ppvObj) override = 0;
    ULONG REB_STDCALL AddRef() override = 0;
    ULONG REB_STDCALL Release() override = 0;
    virtual HRESULT REB_STDCALL AddPlayerToGroup(DPID idGroup, DPID idPlayer) = 0;
    virtual HRESULT REB_STDCALL Close() = 0;
    virtual HRESULT REB_STDCALL CreateGroup(LPDPID lpidGroup, DPNAME * lpGroupName, LPVOID lpData, DWORD dwDataSize,
                           DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL CreatePlayer(LPDPID lpidPlayer, DPNAME * lpPlayerName, HANDLE hEvent, LPVOID lpData,
                            DWORD dwDataSize, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL DeletePlayerFromGroup(DPID idGroup, DPID idPlayer) = 0;
    virtual HRESULT REB_STDCALL DestroyGroup(DPID idGroup) = 0;
    virtual HRESULT REB_STDCALL DestroyPlayer(DPID idPlayer) = 0;
    virtual HRESULT REB_STDCALL EnumGroupPlayers(DPID idGroup, GUID * lpguidInstance, LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
                                LPVOID lpContext, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL EnumGroups(GUID * lpguidInstance, LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2, LPVOID lpContext,
                          DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL EnumPlayers(GUID * lpguidInstance, LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
                           LPVOID lpContext, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL EnumSessions(DPSESSIONDESC2 * lpsd, DWORD dwTimeout, LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
                            LPVOID lpContext, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL GetCaps(LPVOID lpDPCaps, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL GetGroupData(DPID idGroup, LPVOID lpData, LPDWORD lpdwDataSize, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL GetGroupName(DPID idGroup, LPVOID lpData, LPDWORD lpdwDataSize) = 0;
    virtual HRESULT REB_STDCALL GetMessageCount(DPID idPlayer, LPDWORD lpdwCount) = 0;
    virtual HRESULT REB_STDCALL GetPlayerAddress(DPID idPlayer, LPVOID lpData, LPDWORD lpdwDataSize) = 0;
    virtual HRESULT REB_STDCALL GetPlayerCaps(DPID idPlayer, LPVOID lpPlayerCaps, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL GetPlayerData(DPID idPlayer, LPVOID lpData, LPDWORD lpdwDataSize, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL GetPlayerName(DPID idPlayer, LPVOID lpData, LPDWORD lpdwDataSize) = 0;
    virtual HRESULT REB_STDCALL GetSessionDesc(LPVOID lpData, LPDWORD lpdwDataSize) = 0;
    virtual HRESULT REB_STDCALL Initialize(GUID * lpGUID) = 0;
    virtual HRESULT REB_STDCALL Open(DPSESSIONDESC2 * lpsd, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL Receive(LPDPID lpidFrom, LPDPID lpidTo, DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize) = 0;
    virtual HRESULT REB_STDCALL Send(DPID idFrom, DPID idTo, DWORD dwFlags, LPVOID lpData, DWORD dwDataSize) = 0;
    virtual HRESULT REB_STDCALL SetGroupData(DPID idGroup, LPVOID lpData, DWORD dwDataSize, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL SetGroupName(DPID idGroup, DPNAME * lpGroupName, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL SetPlayerData(DPID idPlayer, LPVOID lpData, DWORD dwDataSize, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL SetPlayerName(DPID idPlayer, DPNAME * lpPlayerName, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL SetSessionDesc(DPSESSIONDESC2 * lpSessDesc, DWORD dwFlags) = 0;
};

struct IDirectPlayLobbyA : public IUnknown {
    HRESULT REB_STDCALL QueryInterface(REFIID riid, LPVOID * ppvObj) override = 0;
    ULONG REB_STDCALL AddRef() override = 0;
    ULONG REB_STDCALL Release() override = 0;
    virtual HRESULT REB_STDCALL Connect(DWORD dwFlags, IUnknown * *lplpDP, IUnknown * pUnk) = 0;
    virtual HRESULT REB_STDCALL CreateAddress(REFGUID guidSP, REFGUID guidDataType, LPCVOID lpData, DWORD dwDataSize,
                             LPVOID lpAddress, LPDWORD lpdwAddressSize) = 0;
    virtual HRESULT REB_STDCALL EnumAddress(LPVOID lpEnumAddressCallback, LPCVOID lpAddress, DWORD dwAddressSize,
                           LPVOID lpContext) = 0;
    virtual HRESULT REB_STDCALL EnumAddressTypes(LPVOID lpEnumAddressTypeCallback, REFGUID guidSP, LPVOID lpContext,
                                DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL EnumLocalApplications(LPVOID lpEnumLocalAppCallback, LPVOID lpContext, DWORD dwFlags) = 0;
    virtual HRESULT REB_STDCALL GetConnectionSettings(DWORD dwAppID, LPVOID lpData, LPDWORD lpdwDataSize) = 0;
    virtual HRESULT REB_STDCALL ReceiveLobbyMessage(DWORD dwFlags, DWORD dwAppID, LPDWORD lpdwMessageFlags, LPVOID lpData,
                                   LPDWORD lpdwDataSize) = 0;
    virtual HRESULT REB_STDCALL RunApplication(DWORD dwFlags, LPDWORD lpdwAppID, DPLCONNECTION * lpConn, HANDLE hReceiveEvent) = 0;
    virtual HRESULT REB_STDCALL SendLobbyMessage(DWORD dwFlags, DWORD dwAppID, LPVOID lpData, DWORD dwDataSize) = 0;
    virtual HRESULT REB_STDCALL SetConnectionSettings(DWORD dwFlags, DWORD dwAppID, DPLCONNECTION * lpConn) = 0;
    virtual HRESULT REB_STDCALL SetLobbyMessageEvent(DWORD dwFlags, DWORD dwAppID, HANDLE hReceiveEvent) = 0;
};

extern "C" {
// DPLAYX.DLL, bound through the game's own import table (the original calls them by ordinal 1, 2 and 4).
REB_DLLIMPORT HRESULT REB_STDCALL DirectPlayCreate(GUID* lpGUID, IUnknown** lplpDP, IUnknown* pUnk);
REB_DLLIMPORT HRESULT REB_STDCALL DirectPlayEnumerateA(LPDPENUMDPCALLBACKA lpEnumCallback, LPVOID lpContext);
REB_DLLIMPORT HRESULT REB_STDCALL DirectPlayLobbyCreateA(GUID* lpGUIDSP, IDirectPlayLobbyA** lplpDPL,
                                                               IUnknown* lpUnk, LPVOID lpData, DWORD dwDataSize);
}

// IID_IDirectPlay2A, the interface CommMgr asks the DirectPlay object for (REBEXE.EXE data at 0x66e488).
extern const GUID g_iidDirectPlay2A;
