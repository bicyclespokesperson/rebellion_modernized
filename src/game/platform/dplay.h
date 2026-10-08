// DirectPlay (DPLAYX.DLL) as REBEXE.EXE uses it: the DirectPlay 6 interface IDirectPlay2A for sessions and players,
// the lobby interface IDirectPlayLobbyA for sessions launched by a lobby, and the three DPLAYX entry points the
// game imports (DirectPlayCreate, DirectPlayEnumerateA, DirectPlayLobbyCreateA). The Windows SDK of the build
// toolchain has no DirectPlay headers, so the declarations the game needs are written out here. The port replaces the
// implementation of these entry points behind this header (docs/platform_layer.md, "multiplayer").
#pragma once
#include <windows.h>
#include <objbase.h>

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

#undef INTERFACE
#define INTERFACE IDirectPlay2A
DECLARE_INTERFACE_(IDirectPlay2A, IUnknown) {
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    STDMETHOD(AddPlayerToGroup)(THIS_ DPID idGroup, DPID idPlayer) PURE;
    STDMETHOD(Close)(THIS) PURE;
    STDMETHOD(CreateGroup)(THIS_ LPDPID lpidGroup, DPNAME * lpGroupName, LPVOID lpData, DWORD dwDataSize,
                           DWORD dwFlags) PURE;
    STDMETHOD(CreatePlayer)(THIS_ LPDPID lpidPlayer, DPNAME * lpPlayerName, HANDLE hEvent, LPVOID lpData,
                            DWORD dwDataSize, DWORD dwFlags) PURE;
    STDMETHOD(DeletePlayerFromGroup)(THIS_ DPID idGroup, DPID idPlayer) PURE;
    STDMETHOD(DestroyGroup)(THIS_ DPID idGroup) PURE;
    STDMETHOD(DestroyPlayer)(THIS_ DPID idPlayer) PURE;
    STDMETHOD(EnumGroupPlayers)(THIS_ DPID idGroup, GUID * lpguidInstance, LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
                                LPVOID lpContext, DWORD dwFlags) PURE;
    STDMETHOD(EnumGroups)(THIS_ GUID * lpguidInstance, LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2, LPVOID lpContext,
                          DWORD dwFlags) PURE;
    STDMETHOD(EnumPlayers)(THIS_ GUID * lpguidInstance, LPDPENUMPLAYERSCALLBACK2 lpEnumPlayersCallback2,
                           LPVOID lpContext, DWORD dwFlags) PURE;
    STDMETHOD(EnumSessions)(THIS_ DPSESSIONDESC2 * lpsd, DWORD dwTimeout, LPDPENUMSESSIONSCALLBACK2 lpEnumSessionsCallback2,
                            LPVOID lpContext, DWORD dwFlags) PURE;
    STDMETHOD(GetCaps)(THIS_ LPVOID lpDPCaps, DWORD dwFlags) PURE;
    STDMETHOD(GetGroupData)(THIS_ DPID idGroup, LPVOID lpData, LPDWORD lpdwDataSize, DWORD dwFlags) PURE;
    STDMETHOD(GetGroupName)(THIS_ DPID idGroup, LPVOID lpData, LPDWORD lpdwDataSize) PURE;
    STDMETHOD(GetMessageCount)(THIS_ DPID idPlayer, LPDWORD lpdwCount) PURE;
    STDMETHOD(GetPlayerAddress)(THIS_ DPID idPlayer, LPVOID lpData, LPDWORD lpdwDataSize) PURE;
    STDMETHOD(GetPlayerCaps)(THIS_ DPID idPlayer, LPVOID lpPlayerCaps, DWORD dwFlags) PURE;
    STDMETHOD(GetPlayerData)(THIS_ DPID idPlayer, LPVOID lpData, LPDWORD lpdwDataSize, DWORD dwFlags) PURE;
    STDMETHOD(GetPlayerName)(THIS_ DPID idPlayer, LPVOID lpData, LPDWORD lpdwDataSize) PURE;
    STDMETHOD(GetSessionDesc)(THIS_ LPVOID lpData, LPDWORD lpdwDataSize) PURE;
    STDMETHOD(Initialize)(THIS_ GUID * lpGUID) PURE;
    STDMETHOD(Open)(THIS_ DPSESSIONDESC2 * lpsd, DWORD dwFlags) PURE;
    STDMETHOD(Receive)(THIS_ LPDPID lpidFrom, LPDPID lpidTo, DWORD dwFlags, LPVOID lpData, LPDWORD lpdwDataSize) PURE;
    STDMETHOD(Send)(THIS_ DPID idFrom, DPID idTo, DWORD dwFlags, LPVOID lpData, DWORD dwDataSize) PURE;
    STDMETHOD(SetGroupData)(THIS_ DPID idGroup, LPVOID lpData, DWORD dwDataSize, DWORD dwFlags) PURE;
    STDMETHOD(SetGroupName)(THIS_ DPID idGroup, DPNAME * lpGroupName, DWORD dwFlags) PURE;
    STDMETHOD(SetPlayerData)(THIS_ DPID idPlayer, LPVOID lpData, DWORD dwDataSize, DWORD dwFlags) PURE;
    STDMETHOD(SetPlayerName)(THIS_ DPID idPlayer, DPNAME * lpPlayerName, DWORD dwFlags) PURE;
    STDMETHOD(SetSessionDesc)(THIS_ DPSESSIONDESC2 * lpSessDesc, DWORD dwFlags) PURE;
};

#undef INTERFACE
#define INTERFACE IDirectPlayLobbyA
DECLARE_INTERFACE_(IDirectPlayLobbyA, IUnknown) {
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, LPVOID * ppvObj) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;
    STDMETHOD(Connect)(THIS_ DWORD dwFlags, IUnknown * *lplpDP, IUnknown * pUnk) PURE;
    STDMETHOD(CreateAddress)(THIS_ REFGUID guidSP, REFGUID guidDataType, LPCVOID lpData, DWORD dwDataSize,
                             LPVOID lpAddress, LPDWORD lpdwAddressSize) PURE;
    STDMETHOD(EnumAddress)(THIS_ LPVOID lpEnumAddressCallback, LPCVOID lpAddress, DWORD dwAddressSize,
                           LPVOID lpContext) PURE;
    STDMETHOD(EnumAddressTypes)(THIS_ LPVOID lpEnumAddressTypeCallback, REFGUID guidSP, LPVOID lpContext,
                                DWORD dwFlags) PURE;
    STDMETHOD(EnumLocalApplications)(THIS_ LPVOID lpEnumLocalAppCallback, LPVOID lpContext, DWORD dwFlags) PURE;
    STDMETHOD(GetConnectionSettings)(THIS_ DWORD dwAppID, LPVOID lpData, LPDWORD lpdwDataSize) PURE;
    STDMETHOD(ReceiveLobbyMessage)(THIS_ DWORD dwFlags, DWORD dwAppID, LPDWORD lpdwMessageFlags, LPVOID lpData,
                                   LPDWORD lpdwDataSize) PURE;
    STDMETHOD(RunApplication)(THIS_ DWORD dwFlags, LPDWORD lpdwAppID, DPLCONNECTION * lpConn, HANDLE hReceiveEvent) PURE;
    STDMETHOD(SendLobbyMessage)(THIS_ DWORD dwFlags, DWORD dwAppID, LPVOID lpData, DWORD dwDataSize) PURE;
    STDMETHOD(SetConnectionSettings)(THIS_ DWORD dwFlags, DWORD dwAppID, DPLCONNECTION * lpConn) PURE;
    STDMETHOD(SetLobbyMessageEvent)(THIS_ DWORD dwFlags, DWORD dwAppID, HANDLE hReceiveEvent) PURE;
};
#undef INTERFACE

extern "C" {
// DPLAYX.DLL, bound through the game's own import table (the original calls them by ordinal 1, 2 and 4).
__declspec(dllimport) HRESULT __stdcall DirectPlayCreate(GUID* lpGUID, IUnknown** lplpDP, IUnknown* pUnk);
__declspec(dllimport) HRESULT __stdcall DirectPlayEnumerateA(LPDPENUMDPCALLBACKA lpEnumCallback, LPVOID lpContext);
__declspec(dllimport) HRESULT __stdcall DirectPlayLobbyCreateA(GUID* lpGUIDSP, IDirectPlayLobbyA** lplpDPL,
                                                               IUnknown* lpUnk, LPVOID lpData, DWORD dwDataSize);
}

// IID_IDirectPlay2A, the interface CommMgr asks the DirectPlay object for (REBEXE.EXE data at 0x66e488).
extern const GUID g_iidDirectPlay2A;
