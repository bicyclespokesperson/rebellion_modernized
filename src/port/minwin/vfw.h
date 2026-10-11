// minwin: AVIFIL32 -- the three AVIFile calls the game uses to read the properties of a movie file.
// Written from the API surface (names, signatures, layouts, values); tools/minwin_check.sh compares them with the SDK.
// Missing names: tools/minwin_scaffold.py drafts the declarations; review them before pasting.
#pragma once
#include "port/minwin/base.h"
#include "port/minwin/winmm.h"

struct IAVIFile;
struct _AVIFILEINFOA
{
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
typedef struct _AVIFILEINFOA AVIFILEINFOA;
typedef struct _AVIFILEINFOA* LPAVIFILEINFOA;
typedef IAVIFile* PAVIFILE;

extern "C" {
MINWIN_IMPORT ULONG REB_STDCALL AVIFileRelease(PAVIFILE pfile);
MINWIN_IMPORT HRESULT REB_STDCALL AVIFileOpenA(PAVIFILE* ppfile, LPCSTR szFile, UINT uMode, LPCLSID lpHandler);
MINWIN_IMPORT HRESULT REB_STDCALL AVIFileInfoA(PAVIFILE pfile, LPAVIFILEINFOA pfi, LONG lSize);
}
