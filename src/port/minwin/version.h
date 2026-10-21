// minwin: VERSION -- file version resources (the CD/executable version check).
// Written from the API surface (names, signatures, layouts, values) of the Windows SDK.
#pragma once
#include "port/minwin/base.h"

namespace reb::compat
{
DWORD REB_STDCALL GetFileVersionInfoSizeA(LPCSTR lptstrFilename, LPDWORD lpdwHandle);
BOOL REB_STDCALL GetFileVersionInfoA(LPCSTR lptstrFilename, DWORD dwHandle, DWORD dwLen, LPVOID lpData);
BOOL REB_STDCALL VerQueryValueA(LPCVOID pBlock, LPCSTR lpSubBlock, LPVOID* lplpBuffer, PUINT puLen);
}
