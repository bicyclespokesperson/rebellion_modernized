// minwin: VERSION -- file version resources (the CD/executable version check).
// Written from the API surface (names, signatures, layouts, values); tools/minwin_check.sh compares them with the SDK.
// Missing names: tools/minwin_scaffold.py drafts the declarations; review them before pasting.
#pragma once
#include "port/minwin/base.h"

extern "C" {
MINWIN_IMPORT DWORD REB_STDCALL GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle);
MINWIN_IMPORT BOOL REB_STDCALL GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);
MINWIN_IMPORT BOOL REB_STDCALL VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen);
}
