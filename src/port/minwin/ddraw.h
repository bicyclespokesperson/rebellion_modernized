// minwin: DDRAW -- DirectDraw as used for the 640x480x8 mode switch; the surface interfaces are only named.
// Written from the API surface (names, signatures, layouts, values) of the Windows SDK.
#pragma once
#include "port/minwin/base.h"
#include "port/minwin/gdi32.h"

struct _DDCAPS_DX7;
struct IDirectDrawPalette;
struct IDirectDrawClipper;
struct IDirectDrawSurface;
namespace reb::compat
{
extern const GUID IID_IDirectDraw2;
}
using LPDIRECTDRAW = struct IDirectDraw*;
using LPDIRECTDRAWSURFACE = struct IDirectDrawSurface*;
using LPDIRECTDRAWPALETTE = struct IDirectDrawPalette*;
using LPDIRECTDRAWCLIPPER = struct IDirectDrawClipper*;
using LPDDSURFACEDESC = struct _DDSURFACEDESC*;
using LPDDENUMCALLBACKA = BOOL(REB_STDCALL*)(GUID*, LPSTR, LPSTR, LPVOID);
using LPDDENUMMODESCALLBACK = HRESULT(REB_STDCALL*)(LPDDSURFACEDESC, LPVOID);
using LPDDENUMSURFACESCALLBACK = HRESULT(REB_STDCALL*)(LPDIRECTDRAWSURFACE, LPDDSURFACEDESC, LPVOID);
struct _DDCOLORKEY {
  DWORD dwColorSpaceLowValue;
  DWORD dwColorSpaceHighValue;
};
using DDCOLORKEY = struct _DDCOLORKEY;
struct _DDSCAPS {
  DWORD dwCaps;
};
using DDSCAPS = struct _DDSCAPS;
using LPDDSCAPS = DDSCAPS*;
struct _DDCAPS_DX5 {
  DWORD dwSize;
  DWORD dwCaps;
  DWORD dwCaps2;
  DWORD dwCKeyCaps;
  DWORD dwFXCaps;
  DWORD dwFXAlphaCaps;
  DWORD dwPalCaps;
  DWORD dwSVCaps;
  DWORD dwAlphaBltConstBitDepths;
  DWORD dwAlphaBltPixelBitDepths;
  DWORD dwAlphaBltSurfaceBitDepths;
  DWORD dwAlphaOverlayConstBitDepths;
  DWORD dwAlphaOverlayPixelBitDepths;
  DWORD dwAlphaOverlaySurfaceBitDepths;
  DWORD dwZBufferBitDepths;
  DWORD dwVidMemTotal;
  DWORD dwVidMemFree;
  DWORD dwMaxVisibleOverlays;
  DWORD dwCurrVisibleOverlays;
  DWORD dwNumFourCCCodes;
  DWORD dwAlignBoundarySrc;
  DWORD dwAlignSizeSrc;
  DWORD dwAlignBoundaryDest;
  DWORD dwAlignSizeDest;
  DWORD dwAlignStrideAlign;
  DWORD dwRops[8];
  DDSCAPS ddsCaps;
  DWORD dwMinOverlayStretch;
  DWORD dwMaxOverlayStretch;
  DWORD dwMinLiveVideoStretch;
  DWORD dwMaxLiveVideoStretch;
  DWORD dwMinHwCodecStretch;
  DWORD dwMaxHwCodecStretch;
  DWORD dwReserved1;
  DWORD dwReserved2;
  DWORD dwReserved3;
  DWORD dwSVBCaps;
  DWORD dwSVBCKeyCaps;
  DWORD dwSVBFXCaps;
  DWORD dwSVBRops[8];
  DWORD dwVSBCaps;
  DWORD dwVSBCKeyCaps;
  DWORD dwVSBFXCaps;
  DWORD dwVSBRops[8];
  DWORD dwSSBCaps;
  DWORD dwSSBCKeyCaps;
  DWORD dwSSBFXCaps;
  DWORD dwSSBRops[8];
  DWORD dwMaxVideoPorts;
  DWORD dwCurrVideoPorts;
  DWORD dwSVBCaps2;
  DWORD dwNLVBCaps;
  DWORD dwNLVBCaps2;
  DWORD dwNLVBCKeyCaps;
  DWORD dwNLVBFXCaps;
  DWORD dwNLVBRops[8];
};
using DDCAPS_DX5 = struct _DDCAPS_DX5;
using DDCAPS_DX7 = struct _DDCAPS_DX7;
using DDCAPS = DDCAPS_DX7;
using LPDDCAPS = DDCAPS*;
struct _DDPIXELFORMAT {
  DWORD dwSize;
  DWORD dwFlags;
  DWORD dwFourCC;
  union {
    DWORD dwRGBBitCount;
    DWORD dwYUVBitCount;
    DWORD dwZBufferBitDepth;
    DWORD dwAlphaBitDepth;
    DWORD dwLuminanceBitCount;
    DWORD dwBumpBitCount;
    DWORD dwPrivateFormatBitCount;
  };
  union {
    DWORD dwRBitMask;
    DWORD dwYBitMask;
    DWORD dwStencilBitDepth;
    DWORD dwLuminanceBitMask;
    DWORD dwBumpDuBitMask;
    DWORD dwOperations;
  };
  union {
    DWORD dwGBitMask;
    DWORD dwUBitMask;
    DWORD dwZBitMask;
    DWORD dwBumpDvBitMask;
    struct {
      WORD wFlipMSTypes;
      WORD wBltMSTypes;
    } MultiSampleCaps;
  };
  union {
    DWORD dwBBitMask;
    DWORD dwVBitMask;
    DWORD dwStencilBitMask;
    DWORD dwBumpLuminanceBitMask;
  };
  union {
    DWORD dwRGBAlphaBitMask;
    DWORD dwYUVAlphaBitMask;
    DWORD dwLuminanceAlphaBitMask;
    DWORD dwRGBZBitMask;
    DWORD dwYUVZBitMask;
  };
};
using DDPIXELFORMAT = struct _DDPIXELFORMAT;
struct _DDBLTFX;
struct _DDOVERLAYFX;
struct _DDBLTBATCH;
using DDBLTFX = struct _DDBLTFX;
using LPDDBLTFX = DDBLTFX*;
using DDOVERLAYFX = struct _DDOVERLAYFX;
using LPDDOVERLAYFX = DDOVERLAYFX*;
using DDBLTBATCH = struct _DDBLTBATCH;
using LPDDBLTBATCH = DDBLTBATCH*;
using LPDDCOLORKEY = DDCOLORKEY*;
using LPDDPIXELFORMAT = DDPIXELFORMAT*;
struct IDirectDrawSurface : public IUnknown {
  HRESULT REB_STDCALL QueryInterface(const IID& riid, LPVOID* ppvObj) override = 0;
  ULONG REB_STDCALL AddRef() override = 0;
  ULONG REB_STDCALL Release() override = 0;
  virtual HRESULT REB_STDCALL AddAttachedSurface(LPDIRECTDRAWSURFACE) = 0;
  virtual HRESULT REB_STDCALL AddOverlayDirtyRect(LPRECT) = 0;
  virtual HRESULT REB_STDCALL Blt(LPRECT, LPDIRECTDRAWSURFACE, LPRECT, DWORD, LPDDBLTFX) = 0;
  virtual HRESULT REB_STDCALL BltBatch(LPDDBLTBATCH, DWORD, DWORD) = 0;
  virtual HRESULT REB_STDCALL BltFast(DWORD, DWORD, LPDIRECTDRAWSURFACE, LPRECT, DWORD) = 0;
  virtual HRESULT REB_STDCALL DeleteAttachedSurface(DWORD, LPDIRECTDRAWSURFACE) = 0;
  virtual HRESULT REB_STDCALL EnumAttachedSurfaces(LPVOID, LPDDENUMSURFACESCALLBACK) = 0;
  virtual HRESULT REB_STDCALL EnumOverlayZOrders(DWORD, LPVOID, LPDDENUMSURFACESCALLBACK) = 0;
  virtual HRESULT REB_STDCALL Flip(LPDIRECTDRAWSURFACE, DWORD) = 0;
  virtual HRESULT REB_STDCALL GetAttachedSurface(LPDDSCAPS, LPDIRECTDRAWSURFACE*) = 0;
  virtual HRESULT REB_STDCALL GetBltStatus(DWORD) = 0;
  virtual HRESULT REB_STDCALL GetCaps(LPDDSCAPS) = 0;
  virtual HRESULT REB_STDCALL GetClipper(LPDIRECTDRAWCLIPPER*) = 0;
  virtual HRESULT REB_STDCALL GetColorKey(DWORD, LPDDCOLORKEY) = 0;
  virtual HRESULT REB_STDCALL GetDC(HDC*) = 0;
  virtual HRESULT REB_STDCALL GetFlipStatus(DWORD) = 0;
  virtual HRESULT REB_STDCALL GetOverlayPosition(LPLONG, LPLONG) = 0;
  virtual HRESULT REB_STDCALL GetPalette(LPDIRECTDRAWPALETTE*) = 0;
  virtual HRESULT REB_STDCALL GetPixelFormat(LPDDPIXELFORMAT) = 0;
  virtual HRESULT REB_STDCALL GetSurfaceDesc(LPDDSURFACEDESC) = 0;
  virtual HRESULT REB_STDCALL Initialize(LPDIRECTDRAW, LPDDSURFACEDESC) = 0;
  virtual HRESULT REB_STDCALL IsLost() = 0;
  virtual HRESULT REB_STDCALL Lock(LPRECT, LPDDSURFACEDESC, DWORD, HANDLE) = 0;
  virtual HRESULT REB_STDCALL ReleaseDC(HDC) = 0;
  virtual HRESULT REB_STDCALL Restore() = 0;
  virtual HRESULT REB_STDCALL SetClipper(LPDIRECTDRAWCLIPPER) = 0;
  virtual HRESULT REB_STDCALL SetColorKey(DWORD, LPDDCOLORKEY) = 0;
  virtual HRESULT REB_STDCALL SetOverlayPosition(LONG, LONG) = 0;
  virtual HRESULT REB_STDCALL SetPalette(LPDIRECTDRAWPALETTE) = 0;
  virtual HRESULT REB_STDCALL Unlock(LPVOID) = 0;
  virtual HRESULT REB_STDCALL UpdateOverlay(LPRECT, LPDIRECTDRAWSURFACE, LPRECT, DWORD, LPDDOVERLAYFX) = 0;
  virtual HRESULT REB_STDCALL UpdateOverlayDisplay(DWORD) = 0;
  virtual HRESULT REB_STDCALL UpdateOverlayZOrder(DWORD, LPDIRECTDRAWSURFACE) = 0;
};
struct IDirectDraw : public IUnknown {
  HRESULT REB_STDCALL QueryInterface(const IID& riid, LPVOID* ppvObj) override = 0;
  ULONG REB_STDCALL AddRef() override = 0;
  ULONG REB_STDCALL Release() override = 0;
  virtual HRESULT REB_STDCALL Compact() = 0;
  virtual HRESULT REB_STDCALL CreateClipper(DWORD, LPDIRECTDRAWCLIPPER*, IUnknown*) = 0;
  virtual HRESULT REB_STDCALL CreatePalette(DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE*, IUnknown*) = 0;
  virtual HRESULT REB_STDCALL CreateSurface(LPDDSURFACEDESC, LPDIRECTDRAWSURFACE*, IUnknown*) = 0;
  virtual HRESULT REB_STDCALL DuplicateSurface(LPDIRECTDRAWSURFACE, LPDIRECTDRAWSURFACE*) = 0;
  virtual HRESULT REB_STDCALL EnumDisplayModes(DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMMODESCALLBACK) = 0;
  virtual HRESULT REB_STDCALL EnumSurfaces(DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMSURFACESCALLBACK) = 0;
  virtual HRESULT REB_STDCALL FlipToGDISurface() = 0;
  virtual HRESULT REB_STDCALL GetCaps(LPDDCAPS, LPDDCAPS) = 0;
  virtual HRESULT REB_STDCALL GetDisplayMode(LPDDSURFACEDESC) = 0;
  virtual HRESULT REB_STDCALL GetFourCCCodes(LPDWORD, LPDWORD) = 0;
  virtual HRESULT REB_STDCALL GetGDISurface(LPDIRECTDRAWSURFACE*) = 0;
  virtual HRESULT REB_STDCALL GetMonitorFrequency(LPDWORD) = 0;
  virtual HRESULT REB_STDCALL GetScanLine(LPDWORD) = 0;
  virtual HRESULT REB_STDCALL GetVerticalBlankStatus(LPBOOL) = 0;
  virtual HRESULT REB_STDCALL Initialize(GUID*) = 0;
  virtual HRESULT REB_STDCALL RestoreDisplayMode() = 0;
  virtual HRESULT REB_STDCALL SetCooperativeLevel(HWND, DWORD) = 0;
  virtual HRESULT REB_STDCALL SetDisplayMode(DWORD, DWORD, DWORD) = 0;
  virtual HRESULT REB_STDCALL WaitForVerticalBlank(DWORD, HANDLE) = 0;
};
struct IDirectDraw2 : public IUnknown {
  HRESULT REB_STDCALL QueryInterface(const IID& riid, LPVOID* ppvObj) override = 0;
  ULONG REB_STDCALL AddRef() override = 0;
  ULONG REB_STDCALL Release() override = 0;
  virtual HRESULT REB_STDCALL Compact() = 0;
  virtual HRESULT REB_STDCALL CreateClipper(DWORD, LPDIRECTDRAWCLIPPER*, IUnknown*) = 0;
  virtual HRESULT REB_STDCALL CreatePalette(DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE*, IUnknown*) = 0;
  virtual HRESULT REB_STDCALL CreateSurface(LPDDSURFACEDESC, LPDIRECTDRAWSURFACE*, IUnknown*) = 0;
  virtual HRESULT REB_STDCALL DuplicateSurface(LPDIRECTDRAWSURFACE, LPDIRECTDRAWSURFACE*) = 0;
  virtual HRESULT REB_STDCALL EnumDisplayModes(DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMMODESCALLBACK) = 0;
  virtual HRESULT REB_STDCALL EnumSurfaces(DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMSURFACESCALLBACK) = 0;
  virtual HRESULT REB_STDCALL FlipToGDISurface() = 0;
  virtual HRESULT REB_STDCALL GetCaps(LPDDCAPS, LPDDCAPS) = 0;
  virtual HRESULT REB_STDCALL GetDisplayMode(LPDDSURFACEDESC) = 0;
  virtual HRESULT REB_STDCALL GetFourCCCodes(LPDWORD, LPDWORD) = 0;
  virtual HRESULT REB_STDCALL GetGDISurface(LPDIRECTDRAWSURFACE*) = 0;
  virtual HRESULT REB_STDCALL GetMonitorFrequency(LPDWORD) = 0;
  virtual HRESULT REB_STDCALL GetScanLine(LPDWORD) = 0;
  virtual HRESULT REB_STDCALL GetVerticalBlankStatus(LPBOOL) = 0;
  virtual HRESULT REB_STDCALL Initialize(GUID*) = 0;
  virtual HRESULT REB_STDCALL RestoreDisplayMode() = 0;
  virtual HRESULT REB_STDCALL SetCooperativeLevel(HWND, DWORD) = 0;
  virtual HRESULT REB_STDCALL SetDisplayMode(DWORD, DWORD, DWORD, DWORD, DWORD) = 0;
  virtual HRESULT REB_STDCALL WaitForVerticalBlank(DWORD, HANDLE) = 0;
  virtual HRESULT REB_STDCALL GetAvailableVidMem(LPDDSCAPS, LPDWORD, LPDWORD) = 0;
};
struct _DDSURFACEDESC {
  DWORD dwSize;
  DWORD dwFlags;
  DWORD dwHeight;
  DWORD dwWidth;
  union {
    LONG lPitch;
    DWORD dwLinearSize;
  };
  DWORD dwBackBufferCount;
  union {
    DWORD dwMipMapCount;
    DWORD dwZBufferBitDepth;
    DWORD dwRefreshRate;
  };
  DWORD dwAlphaBitDepth;
  DWORD dwReserved;
  LPVOID lpSurface;
  DDCOLORKEY ddckCKDestOverlay;
  DDCOLORKEY ddckCKDestBlt;
  DDCOLORKEY ddckCKSrcOverlay;
  DDCOLORKEY ddckCKSrcBlt;
  DDPIXELFORMAT ddpfPixelFormat;
  DDSCAPS ddsCaps;
};
using DDSURFACEDESC = struct _DDSURFACEDESC;

namespace reb::compat
{
HRESULT REB_STDCALL DirectDrawEnumerateA(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext);
HRESULT REB_STDCALL DirectDrawCreate(GUID* lpGUID, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter);
}

#define DDCAPS_3D ((LONG)1)
#define DDENUMSURFACES_DOESEXIST ((LONG)0x10)
#define DDSCL_FULLSCREEN ((LONG)1)
#define DDSCL_NORMAL ((LONG)8)
#define DDSCL_EXCLUSIVE ((LONG)0x10)
#define DDSCL_ALLOWMODEX ((LONG)0x40)
