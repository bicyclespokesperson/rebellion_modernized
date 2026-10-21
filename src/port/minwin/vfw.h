// minwin: AVIFIL32 -- the three AVIFile calls the game uses to read the properties of a movie file.
// Written from the API surface (names, signatures, layouts, values) of the Windows SDK.
#pragma once
#include "port/minwin/base.h"
#include "port/minwin/winmm.h"

struct IAVIFile;
struct _AVIFILEINFOA {
  DWORD dwMaxBytesPerSec;
  DWORD dwFlags;
  DWORD dwCaps;
  DWORD dwStreams;
  DWORD dwSuggestedBufferSize;
  DWORD dwWidth;
  DWORD dwHeight;
  DWORD dwScale;
  DWORD dwRate;
  DWORD dwLength;
  DWORD dwEditCount;
  char szFileType[64];
};
using AVIFILEINFOA = struct _AVIFILEINFOA;
using LPAVIFILEINFOA = struct _AVIFILEINFOA*;
using PAVIFILE = IAVIFile*;

namespace reb::compat
{
ULONG REB_STDCALL AVIFileRelease(PAVIFILE pfile);
HRESULT REB_STDCALL AVIFileOpenA(PAVIFILE* ppfile, LPCSTR szFile, UINT uMode, LPCLSID lpHandler);
HRESULT REB_STDCALL AVIFileInfoA(PAVIFILE pfile, LPAVIFILEINFOA pfi, LONG lSize);
}
