// minwin: handles, GUID and IUnknown, the geometry structs, HRESULT and the registry/wait constants shared by every
// DLL's header. Written from the API surface (names, signatures, layouts, values) of the Windows SDK.
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
using LPBOOL = BOOL*;
using LPBYTE = BYTE*;
using LPLONG = LONG*;
using LPDWORD = DWORD*;
using LPVOID = void*;
using LPCVOID = const void*;
using WCHAR = wchar_t;
using LPCWSTR = const WCHAR*;
using LPSTR = CHAR*;
using LPCSTR = const CHAR*;
using HANDLE = void*;
using HRESULT = LONG;
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
using LARGE_INTEGER = union _LARGE_INTEGER;
struct _GUID {
  ULONG Data1;
  unsigned short Data2;
  unsigned short Data3;
  unsigned char Data4[8];
};
using GUID = struct _GUID;
using LPCGUID = const GUID*;
using IID = GUID;
using CLSID = GUID;
using LPCLSID = CLSID*;
#define REFGUID const GUID&
#define REFIID const IID&
#define REFCLSID const CLSID&
using ACCESS_MASK = DWORD;
using PRTL_CRITICAL_SECTION_DEBUG = struct _RTL_CRITICAL_SECTION_DEBUG*;
struct _RTL_CRITICAL_SECTION {
  PRTL_CRITICAL_SECTION_DEBUG DebugInfo;
  LONG LockCount;
  LONG RecursionCount;
  HANDLE OwningThread;
  HANDLE LockSemaphore;
  ULONG_PTR SpinCount;
};
using RTL_CRITICAL_SECTION = struct _RTL_CRITICAL_SECTION;
using PRTL_CRITICAL_SECTION = struct _RTL_CRITICAL_SECTION*;
using WPARAM = UINT_PTR;
using LPARAM = LONG_PTR;
using LRESULT = LONG_PTR;
using HGLOBAL = HANDLE;
using ATOM = WORD;
using HKEY = struct HKEY__*;
using PHKEY = HKEY*;
using HINSTANCE = struct HINSTANCE__*;
using HMODULE = HINSTANCE;
using HRSRC = struct HRSRC__*;
using HFILE = int;
struct _FILETIME {
  DWORD dwLowDateTime;
  DWORD dwHighDateTime;
};
using FILETIME = struct _FILETIME;
using HWND = struct HWND__*;
using HHOOK = struct HHOOK__*;
using HGDIOBJ = void*;
using HACCEL = struct HACCEL__*;
using HBITMAP = struct HBITMAP__*;
using HBRUSH = struct HBRUSH__*;
using HDC = struct HDC__*;
using HFONT = struct HFONT__*;
using HICON = struct HICON__*;
using HMENU = struct HMENU__*;
using HPALETTE = struct HPALETTE__*;
using HPEN = struct HPEN__*;
using HCURSOR = HICON;
using COLORREF = DWORD;
struct tagRECT {
  LONG left;
  LONG top;
  LONG right;
  LONG bottom;
};
using RECT = struct tagRECT;
using LPRECT = struct tagRECT*;
struct tagPOINT {
  LONG x;
  LONG y;
};
using POINT = struct tagPOINT;
using LPPOINT = struct tagPOINT*;
struct tagSIZE {
  LONG cx;
  LONG cy;
};
using SIZE = struct tagSIZE;
using LPSIZE = struct tagSIZE*;
struct tagPOINTS {
  SHORT x;
  SHORT y;
};
using POINTS = struct tagPOINTS;
struct IUnknown;
using LPUNKNOWN = IUnknown*;
struct IUnknown {
  virtual HRESULT REB_STDCALL QueryInterface(const IID& riid, void** ppvObject) = 0;
  virtual ULONG REB_STDCALL AddRef() = 0;
  virtual ULONG REB_STDCALL Release() = 0;
};

#define STATUS_WAIT_0 ((DWORD)0)
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
#define MAKELONG(a, b)                                                                                                 \
  ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define HFILE_ERROR -1
#define ERROR_SUCCESS ((LONG)0)
#define ERROR_INVALID_HANDLE ((LONG)6)
#define ERROR_ALREADY_EXISTS ((LONG)0xb7)
#define WAIT_TIMEOUT ((LONG)0x102)
#define E_FAIL ((LONG)0x80004005)
#define S_OK ((LONG)0)
