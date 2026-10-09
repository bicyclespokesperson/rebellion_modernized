// minwin: handles, GUID and IUnknown, the geometry structs, HRESULT and the registry/wait constants shared by every DLL's header.
// Written from the API surface (names, signatures, layouts, values); tools/minwin_check.sh compares them with the SDK.
// Missing names: tools/minwin_scaffold.py drafts the declarations; review them before pasting.
#pragma once
#include "port/minwin/core.h"

struct _RTL_CRITICAL_SECTION_DEBUG;
struct HKEY__;
struct HINSTANCE__;
struct HRSRC__;
struct HWND__;
struct HHOOK__;
struct HACCEL__;
struct HBITMAP__;
struct HBRUSH__;
struct HDC__;
struct HFONT__;
struct HICON__;
struct HMENU__;
struct HPALETTE__;
struct HPEN__;
typedef BOOL *LPBOOL;
typedef BYTE *LPBYTE;
typedef LONG *LPLONG;
typedef DWORD *LPDWORD;
typedef void *LPVOID;
typedef const void *LPCVOID;
typedef wchar_t WCHAR;
typedef const WCHAR *LPCWSTR;
typedef CHAR *LPSTR;
typedef const CHAR *LPCSTR;
typedef void *HANDLE;
typedef LONG HRESULT;
union _LARGE_INTEGER {
    struct {
        DWORD LowPart;
        LONG HighPart;
    };
    struct {
        DWORD LowPart;
        LONG HighPart;
    } u;
    LONGLONG QuadPart;
};
typedef union _LARGE_INTEGER LARGE_INTEGER;
struct _GUID {
    ULONG Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
};
typedef struct _GUID GUID;
typedef const GUID *LPCGUID;
typedef GUID IID;
typedef GUID CLSID;
typedef CLSID *LPCLSID;
#define REFGUID const GUID &
#define REFIID const IID &
#define REFCLSID const CLSID &
typedef DWORD ACCESS_MASK;
typedef struct _RTL_CRITICAL_SECTION_DEBUG *PRTL_CRITICAL_SECTION_DEBUG;
struct _RTL_CRITICAL_SECTION {
    PRTL_CRITICAL_SECTION_DEBUG DebugInfo;
    LONG LockCount;
    LONG RecursionCount;
    HANDLE OwningThread;
    HANDLE LockSemaphore;
    ULONG_PTR SpinCount;
};
typedef struct _RTL_CRITICAL_SECTION RTL_CRITICAL_SECTION;
typedef struct _RTL_CRITICAL_SECTION *PRTL_CRITICAL_SECTION;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef HANDLE HGLOBAL;
typedef WORD ATOM;
typedef struct HKEY__ *HKEY;
typedef HKEY *PHKEY;
typedef struct HINSTANCE__ *HINSTANCE;
typedef HINSTANCE HMODULE;
typedef struct HRSRC__ *HRSRC;
typedef int HFILE;
struct _FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
};
typedef struct _FILETIME FILETIME;
typedef struct HWND__ *HWND;
typedef struct HHOOK__ *HHOOK;
typedef void *HGDIOBJ;
typedef struct HACCEL__ *HACCEL;
typedef struct HBITMAP__ *HBITMAP;
typedef struct HBRUSH__ *HBRUSH;
typedef struct HDC__ *HDC;
typedef struct HFONT__ *HFONT;
typedef struct HICON__ *HICON;
typedef struct HMENU__ *HMENU;
typedef struct HPALETTE__ *HPALETTE;
typedef struct HPEN__ *HPEN;
typedef HICON HCURSOR;
typedef DWORD COLORREF;
struct tagRECT {
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
};
typedef struct tagRECT RECT;
typedef struct tagRECT *LPRECT;
struct tagPOINT {
    LONG x;
    LONG y;
};
typedef struct tagPOINT POINT;
typedef struct tagPOINT *LPPOINT;
struct tagSIZE {
    LONG cx;
    LONG cy;
};
typedef struct tagSIZE SIZE;
typedef struct tagSIZE *LPSIZE;
struct tagPOINTS {
    SHORT x;
    SHORT y;
};
typedef struct tagPOINTS POINTS;
struct IUnknown;
typedef IUnknown *LPUNKNOWN;
struct IUnknown {
    virtual HRESULT REB_STDCALL QueryInterface(const IID & riid, void **ppvObject) = 0;
    virtual ULONG REB_STDCALL AddRef() = 0;
    virtual ULONG REB_STDCALL Release() = 0;
};

#if MINWIN_TARGET_WINDOWS
extern "C" LONG REB_CDECL _InterlockedExchange(volatile LONG *Target, LONG Value);
#else
inline LONG _InterlockedExchange(volatile LONG *Target, LONG Value) { return __atomic_exchange_n(Target, Value, __ATOMIC_SEQ_CST); }
#endif

#define STATUS_WAIT_0 ((DWORD)0)
#define InterlockedExchange _InterlockedExchange
#define READ_CONTROL ((LONG)0x20000)
#define SYNCHRONIZE ((LONG)0x100000)
#define STANDARD_RIGHTS_READ ((LONG)0x20000)
#define STANDARD_RIGHTS_WRITE ((LONG)0x20000)
#define THREAD_BASE_PRIORITY_MAX 2
#define KEY_QUERY_VALUE 1
#define KEY_SET_VALUE 2
#define KEY_CREATE_SUB_KEY 4
#define KEY_ENUMERATE_SUB_KEYS 8
#define KEY_NOTIFY 0x10
#define KEY_READ ((LONG)0x20019)
#define KEY_WRITE ((LONG)0x20006)
#define REG_DWORD ((DWORD)4)
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define HFILE_ERROR -1
#define ERROR_SUCCESS ((LONG)0)
#define ERROR_INVALID_HANDLE ((LONG)6)
#define ERROR_ALREADY_EXISTS ((LONG)0xb7)
#define WAIT_TIMEOUT ((LONG)0x102)
#define E_FAIL ((LONG)0x80004005)
#define S_OK ((LONG)0)
