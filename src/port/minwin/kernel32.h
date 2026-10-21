// minwin: KERNEL32 -- the handle, resource, module and timing calls the game still makes.
// Written from the API surface (names, signatures, layouts, values) of the Windows SDK.
#pragma once
#include "port/minwin/base.h"

struct _SECURITY_ATTRIBUTES;
using LPSECURITY_ATTRIBUTES = struct _SECURITY_ATTRIBUTES*;
using CRITICAL_SECTION = RTL_CRITICAL_SECTION;
using LPCRITICAL_SECTION = PRTL_CRITICAL_SECTION;

extern "C" {
BOOL REB_STDCALL CloseHandle(HANDLE hObject);
DWORD REB_STDCALL GetLastError(void);
BOOL REB_STDCALL QueryPerformanceCounter(LARGE_INTEGER* lpPerformanceCount);
BOOL REB_STDCALL QueryPerformanceFrequency(LARGE_INTEGER* lpFrequency);
DWORD REB_STDCALL GetCurrentThreadId(void);
}

namespace reb::compat
{
BOOL REB_STDCALL FreeLibrary(HMODULE hLibModule);
BOOL REB_STDCALL FreeResource(HGLOBAL hResData);
HMODULE REB_STDCALL GetModuleHandleA(LPCSTR lpModuleName);
HGLOBAL REB_STDCALL LoadResource(HMODULE hModule, HRSRC hResInfo);
LPVOID REB_STDCALL LockResource(HGLOBAL hResData);
HMODULE REB_STDCALL LoadLibraryA(LPCSTR lpLibFileName);
DWORD REB_STDCALL SizeofResource(HMODULE hModule, HRSRC hResInfo);
HRSRC REB_STDCALL FindResourceA(HMODULE hModule, LPCSTR lpName, LPCSTR lpType);
DWORD REB_STDCALL VerLanguageNameA(DWORD wLang, LPSTR szLang, DWORD cchLang);
}

#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR) - 1)
