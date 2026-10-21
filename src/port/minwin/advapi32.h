// minwin: ADVAPI32 -- registry values (the game's configuration) and the user name.
// Written from the API surface (names, signatures, layouts, values) of the Windows SDK.
#pragma once
#include "port/minwin/base.h"

using LSTATUS = LONG;
using REGSAM = ACCESS_MASK;

namespace reb::compat
{
BOOL REB_STDCALL GetUserNameA(LPSTR lpBuffer, LPDWORD pcbBuffer);
LSTATUS REB_STDCALL RegCloseKey(HKEY hKey);
LSTATUS REB_STDCALL RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired,
                                                PHKEY phkResult);
LSTATUS REB_STDCALL RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType,
                                                   LPBYTE lpData, LPDWORD lpcbData);
LSTATUS REB_STDCALL RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType,
                                                 const BYTE* lpData, DWORD cbData);
}

#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)((LONG)0x80000002))
