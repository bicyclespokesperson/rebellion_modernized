// minwin: ADVAPI32 -- registry values (the game's configuration) and the user name.
// Written from the API surface (names, signatures, layouts, values); tools/minwin_check.sh compares them with the SDK.
// Missing names: tools/minwin_scaffold.py drafts the declarations; review them before pasting.
#pragma once
#include "port/minwin/base.h"

using LSTATUS = LONG;
using REGSAM = ACCESS_MASK;

extern "C" {
MINWIN_IMPORT BOOL REB_STDCALL GetUserNameA(LPSTR lpBuffer, LPDWORD pcbBuffer);
MINWIN_IMPORT LSTATUS REB_STDCALL RegCloseKey(HKEY hKey);
MINWIN_IMPORT LSTATUS REB_STDCALL
RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
MINWIN_IMPORT LSTATUS REB_STDCALL
RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
MINWIN_IMPORT LSTATUS REB_STDCALL
RegSetValueExA(HKEY hKey, LPCSTR lpValueName, DWORD Reserved, DWORD dwType, const BYTE* lpData, DWORD cbData);
}

#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)((LONG)0x80000002))
