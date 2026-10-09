// The only header through which reconstructed code reaches Win32 / DirectX / Smacker declarations.
// Hybrid build: the real Windows SDK. Port: a native subset implemented on SDL (docs/platform_layer.md).
#pragma once
#include "core/reb_abi.h"

#if defined(REB_MINWIN)
#include "port/minwin/minwin.h"
#else
#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <vfw.h>
#include <ddraw.h>
#include <dsound.h>
#include <winver.h>

// rpcndr.h defines `small` as char, which breaks parameters of that name.
#undef small

// The SDK declares these without dllimport (it links the import libraries). The original reaches them through the
// game's import address table, which is also how the hybrid build resolves them (imports.csv).
extern "C" {
__declspec(dllimport) HRESULT WINAPI DirectDrawCreate(GUID FAR* lpGUID, LPDIRECTDRAW FAR* lplpDD, IUnknown FAR* pUnkOuter);
__declspec(dllimport) HRESULT WINAPI DirectDrawEnumerateA(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext);
__declspec(dllimport) HRESULT WINAPI DirectSoundCreate(LPCGUID lpcGuidDevice, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter);
__declspec(dllimport) ULONG STDAPICALLTYPE AVIFileRelease(PAVIFILE pfile);
__declspec(dllimport) HRESULT STDAPICALLTYPE AVIFileInfoA(PAVIFILE pfile, AVIFILEINFOA FAR* pfi, LONG lSize);
__declspec(dllimport) HRESULT STDAPICALLTYPE AVIFileOpenA(PAVIFILE FAR* ppfile, LPCSTR szFile, UINT uMode, LPCLSID lpHandler);
__declspec(dllimport) DWORD REB_STDCALL GetFileVersionInfoSizeA(LPCSTR fileName, LPDWORD handle);
__declspec(dllimport) BOOL REB_STDCALL GetFileVersionInfoA(LPCSTR fileName, DWORD handle, DWORD length, LPVOID data);
__declspec(dllimport) DWORD REB_STDCALL VerLanguageNameA(DWORD language, LPSTR buffer, DWORD size);
__declspec(dllimport) BOOL REB_STDCALL VerQueryValueA(LPCVOID block, LPCSTR subBlock, LPVOID* buffer, PUINT length);
}
#endif

#include "platform/dplay.h"
