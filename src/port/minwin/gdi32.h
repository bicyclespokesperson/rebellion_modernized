// minwin: GDI32 -- device contexts, bitmaps and DIB sections, palettes, pens, brushes, fonts and text output.
// Written from the API surface (names, signatures, layouts, values) of the Windows SDK.
#pragma once
#include "port/minwin/base.h"

struct _devicemodeA;
struct tagBITMAP {
  LONG bmType;
  LONG bmWidth;
  LONG bmHeight;
  LONG bmWidthBytes;
  WORD bmPlanes;
  WORD bmBitsPixel;
  LPVOID bmBits;
};
using BITMAP = struct tagBITMAP;
struct tagRGBQUAD {
  BYTE rgbBlue;
  BYTE rgbGreen;
  BYTE rgbRed;
  BYTE rgbReserved;
};
using RGBQUAD = struct tagRGBQUAD;
struct tagBITMAPINFOHEADER {
  DWORD biSize;
  LONG biWidth;
  LONG biHeight;
  WORD biPlanes;
  WORD biBitCount;
  DWORD biCompression;
  DWORD biSizeImage;
  LONG biXPelsPerMeter;
  LONG biYPelsPerMeter;
  DWORD biClrUsed;
  DWORD biClrImportant;
};
using BITMAPINFOHEADER = struct tagBITMAPINFOHEADER;
struct tagBITMAPINFO {
  BITMAPINFOHEADER bmiHeader;
  RGBQUAD bmiColors[1];
};
using BITMAPINFO = struct tagBITMAPINFO;
struct tagTEXTMETRICA {
  LONG tmHeight;
  LONG tmAscent;
  LONG tmDescent;
  LONG tmInternalLeading;
  LONG tmExternalLeading;
  LONG tmAveCharWidth;
  LONG tmMaxCharWidth;
  LONG tmWeight;
  LONG tmOverhang;
  LONG tmDigitizedAspectX;
  LONG tmDigitizedAspectY;
  BYTE tmFirstChar;
  BYTE tmLastChar;
  BYTE tmDefaultChar;
  BYTE tmBreakChar;
  BYTE tmItalic;
  BYTE tmUnderlined;
  BYTE tmStruckOut;
  BYTE tmPitchAndFamily;
  BYTE tmCharSet;
};
using TEXTMETRICA = struct tagTEXTMETRICA;
using LPTEXTMETRICA = struct tagTEXTMETRICA*;
struct tagLOGBRUSH {
  UINT lbStyle;
  COLORREF lbColor;
  ULONG_PTR lbHatch;
};
using LOGBRUSH = struct tagLOGBRUSH;
struct tagPALETTEENTRY {
  BYTE peRed;
  BYTE peGreen;
  BYTE peBlue;
  BYTE peFlags;
};
using PALETTEENTRY = struct tagPALETTEENTRY;
using LPPALETTEENTRY = struct tagPALETTEENTRY*;
struct tagLOGPALETTE {
  WORD palVersion;
  WORD palNumEntries;
  PALETTEENTRY palPalEntry[1];
};
using LOGPALETTE = struct tagLOGPALETTE;
struct tagLOGFONTA {
  LONG lfHeight;
  LONG lfWidth;
  LONG lfEscapement;
  LONG lfOrientation;
  LONG lfWeight;
  BYTE lfItalic;
  BYTE lfUnderline;
  BYTE lfStrikeOut;
  BYTE lfCharSet;
  BYTE lfOutPrecision;
  BYTE lfClipPrecision;
  BYTE lfQuality;
  BYTE lfPitchAndFamily;
  CHAR lfFaceName[32];
};
using LOGFONTA = struct tagLOGFONTA;
using DEVMODEA = struct _devicemodeA;

namespace reb::compat
{
BOOL REB_STDCALL BitBlt(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop);
HBRUSH REB_STDCALL CreateBrushIndirect(const LOGBRUSH* plbrush);
HBITMAP REB_STDCALL CreateCompatibleBitmap(HDC hdc, int cx, int cy);
HDC REB_STDCALL CreateCompatibleDC(HDC hdc);
HBITMAP REB_STDCALL CreateDIBitmap(HDC hdc, const BITMAPINFOHEADER* pbmih, DWORD flInit,
                                                 const void* pjBits, const BITMAPINFO* pbmi, UINT iUsage);
HFONT REB_STDCALL CreateFontIndirectA(const LOGFONTA* lplf);
HDC REB_STDCALL CreateICA(LPCSTR pszDriver, LPCSTR pszDevice, LPCSTR pszPort, const DEVMODEA* pdm);
HPALETTE REB_STDCALL CreatePalette(const LOGPALETTE* plpal);
HPEN REB_STDCALL CreatePen(int iStyle, int cWidth, COLORREF color);
HBRUSH REB_STDCALL CreateSolidBrush(COLORREF color);
BOOL REB_STDCALL DeleteDC(HDC hdc);
BOOL REB_STDCALL DeleteObject(HGDIOBJ ho);
int REB_STDCALL GetDeviceCaps(HDC hdc, int index);
UINT REB_STDCALL GetPaletteEntries(HPALETTE hpal, UINT iStart, UINT cEntries, LPPALETTEENTRY pPalEntries);
HGDIOBJ REB_STDCALL GetStockObject(int i);
UINT REB_STDCALL GetSystemPaletteEntries(HDC hdc, UINT iStart, UINT cEntries, LPPALETTEENTRY pPalEntries);
BOOL REB_STDCALL GetTextExtentPoint32A(HDC hdc, LPCSTR lpString, int c, LPSIZE psizl);
BOOL REB_STDCALL LineTo(HDC hdc, int x, int y);
BOOL REB_STDCALL PatBlt(HDC hdc, int x, int y, int w, int h, DWORD rop);
BOOL REB_STDCALL Rectangle(HDC hdc, int left, int top, int right, int bottom);
UINT REB_STDCALL RealizePalette(HDC hdc);
BOOL REB_STDCALL ResizePalette(HPALETTE hpal, UINT n);
HGDIOBJ REB_STDCALL SelectObject(HDC hdc, HGDIOBJ h);
HPALETTE REB_STDCALL SelectPalette(HDC hdc, HPALETTE hPal, BOOL bForceBkgd);
COLORREF REB_STDCALL SetBkColor(HDC hdc, COLORREF color);
int REB_STDCALL SetBkMode(HDC hdc, int mode);
int REB_STDCALL SetDIBitsToDevice(HDC hdc, int xDest, int yDest, DWORD w, DWORD h, int xSrc, int ySrc,
                                                UINT StartScan, UINT cLines, const void* lpvBits,
                                                const BITMAPINFO* lpbmi, UINT ColorUse);
UINT REB_STDCALL SetPaletteEntries(HPALETTE hpal, UINT iStart, UINT cEntries,
                                                 const PALETTEENTRY* pPalEntries);
int REB_STDCALL SetROP2(HDC hdc, int rop2);
int REB_STDCALL SetStretchBltMode(HDC hdc, int mode);
UINT REB_STDCALL SetSystemPaletteUse(HDC hdc, UINT use);
COLORREF REB_STDCALL SetTextColor(HDC hdc, COLORREF color);
UINT REB_STDCALL SetTextAlign(HDC hdc, UINT align);
BOOL REB_STDCALL GetTextMetricsA(HDC hdc, LPTEXTMETRICA lptm);
HBITMAP REB_STDCALL CreateDIBSection(HDC hdc, const BITMAPINFO* pbmi, UINT usage, void** ppvBits,
                                                   HANDLE hSection, DWORD offset);
int REB_STDCALL GetObjectA(HANDLE h, int c, LPVOID pv);
BOOL REB_STDCALL MoveToEx(HDC hdc, int x, int y, LPPOINT lppt);
BOOL REB_STDCALL TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c);
BOOL REB_STDCALL DPtoLP(HDC hdc, LPPOINT lppt, int c);
BOOL REB_STDCALL Polyline(HDC hdc, const POINT* apt, int cpt);
}

#define R2_XORPEN 7
#define SRCCOPY ((DWORD)0xcc0020)
#define PATCOPY ((DWORD)0xf00021)
#define BLACKNESS ((DWORD)0x42)
#define PROOF_QUALITY 2
#define FW_NORMAL 0x190
#define FW_BOLD 0x2bc
#define FW_HEAVY 0x384
#define FW_BLACK 0x384
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define PALETTERGB(r, g, b) (0x02000000 | RGB(r, g, b))
#define PC_NOCOLLAPSE 4
#define TRANSPARENT 1
#define OPAQUE 2
#define WHITE_BRUSH 0
#define GRAY_BRUSH 2
#define BLACK_BRUSH 4
#define NULL_BRUSH 5
#define WHITE_PEN 6
#define BLACK_PEN 7
#define NULL_PEN 8
#define BS_SOLID 0
#define PS_SOLID 0
#define PS_DOT 2
#define HORZRES 8
#define VERTRES 0xa
#define BITSPIXEL 0xc
#define PLANES 0xe
#define NUMCOLORS 0x18
#define RASTERCAPS 0x26
#define SIZEPALETTE 0x68
#define RC_PALETTE 0x100
#define DIB_RGB_COLORS 0
#define SYSPAL_STATIC 1
#define SYSPAL_NOSTATIC 2
#define CBM_INIT ((LONG)4)
