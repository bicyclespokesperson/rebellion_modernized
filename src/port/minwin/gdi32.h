// minwin: GDI32 -- device contexts, bitmaps and DIB sections, palettes, pens, brushes, fonts and text output.
// Written from the API surface (names, signatures, layouts, values); tools/minwin_check.sh compares them with the SDK.
// Missing names: tools/minwin_scaffold.py drafts the declarations; review them before pasting.
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
typedef struct tagBITMAP BITMAP;
struct tagRGBQUAD {
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
};
typedef struct tagRGBQUAD RGBQUAD;
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
typedef struct tagBITMAPINFOHEADER BITMAPINFOHEADER;
struct tagBITMAPINFO {
    BITMAPINFOHEADER bmiHeader;
    RGBQUAD bmiColors[1];
};
typedef struct tagBITMAPINFO BITMAPINFO;
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
typedef struct tagTEXTMETRICA TEXTMETRICA;
typedef struct tagTEXTMETRICA *LPTEXTMETRICA;
struct tagLOGBRUSH {
    UINT lbStyle;
    COLORREF lbColor;
    ULONG_PTR lbHatch;
};
typedef struct tagLOGBRUSH LOGBRUSH;
struct tagPALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
};
typedef struct tagPALETTEENTRY PALETTEENTRY;
typedef struct tagPALETTEENTRY *LPPALETTEENTRY;
struct tagLOGPALETTE {
    WORD palVersion;
    WORD palNumEntries;
    PALETTEENTRY palPalEntry[1];
};
typedef struct tagLOGPALETTE LOGPALETTE;
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
typedef struct tagLOGFONTA LOGFONTA;
typedef struct _devicemodeA DEVMODEA;

extern "C" {
MINWIN_IMPORT BOOL REB_STDCALL BitBlt(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop);
MINWIN_IMPORT HBRUSH REB_STDCALL CreateBrushIndirect(const LOGBRUSH *plbrush);
MINWIN_IMPORT HBITMAP REB_STDCALL CreateCompatibleBitmap(HDC hdc, int cx, int cy);
MINWIN_IMPORT HDC REB_STDCALL CreateCompatibleDC(HDC hdc);
MINWIN_IMPORT HBITMAP REB_STDCALL CreateDIBitmap(HDC hdc, const BITMAPINFOHEADER *pbmih, DWORD flInit, const void *pjBits, const BITMAPINFO *pbmi, UINT iUsage);
MINWIN_IMPORT HFONT REB_STDCALL CreateFontIndirectA(const LOGFONTA *lplf);
MINWIN_IMPORT HDC REB_STDCALL CreateICA(LPCSTR pszDriver, LPCSTR pszDevice, LPCSTR pszPort, const DEVMODEA *pdm);
MINWIN_IMPORT HPALETTE REB_STDCALL CreatePalette(const LOGPALETTE *plpal);
MINWIN_IMPORT HPEN REB_STDCALL CreatePen(int iStyle, int cWidth, COLORREF color);
MINWIN_IMPORT HBRUSH REB_STDCALL CreateSolidBrush(COLORREF color);
MINWIN_IMPORT BOOL REB_STDCALL DeleteDC(HDC hdc);
MINWIN_IMPORT BOOL REB_STDCALL DeleteObject(HGDIOBJ ho);
MINWIN_IMPORT int REB_STDCALL GetDeviceCaps(HDC hdc, int index);
MINWIN_IMPORT UINT REB_STDCALL GetPaletteEntries(HPALETTE hpal, UINT iStart, UINT cEntries, LPPALETTEENTRY pPalEntries);
MINWIN_IMPORT HGDIOBJ REB_STDCALL GetStockObject(int i);
MINWIN_IMPORT UINT REB_STDCALL GetSystemPaletteEntries(HDC hdc, UINT iStart, UINT cEntries, LPPALETTEENTRY pPalEntries);
MINWIN_IMPORT BOOL REB_STDCALL GetTextExtentPoint32A(HDC hdc, LPCSTR lpString, int c, LPSIZE psizl);
MINWIN_IMPORT BOOL REB_STDCALL LineTo(HDC hdc, int x, int y);
MINWIN_IMPORT BOOL REB_STDCALL PatBlt(HDC hdc, int x, int y, int w, int h, DWORD rop);
MINWIN_IMPORT BOOL REB_STDCALL Rectangle(HDC hdc, int left, int top, int right, int bottom);
MINWIN_IMPORT UINT REB_STDCALL RealizePalette(HDC hdc);
MINWIN_IMPORT BOOL REB_STDCALL ResizePalette(HPALETTE hpal, UINT n);
MINWIN_IMPORT HGDIOBJ REB_STDCALL SelectObject(HDC hdc, HGDIOBJ h);
MINWIN_IMPORT HPALETTE REB_STDCALL SelectPalette(HDC hdc, HPALETTE hPal, BOOL bForceBkgd);
MINWIN_IMPORT COLORREF REB_STDCALL SetBkColor(HDC hdc, COLORREF color);
MINWIN_IMPORT int REB_STDCALL SetBkMode(HDC hdc, int mode);
MINWIN_IMPORT int REB_STDCALL SetDIBitsToDevice(HDC hdc, int xDest, int yDest, DWORD w, DWORD h, int xSrc, int ySrc, UINT StartScan, UINT cLines, const void *lpvBits, const BITMAPINFO *lpbmi, UINT ColorUse);
MINWIN_IMPORT UINT REB_STDCALL SetPaletteEntries(HPALETTE hpal, UINT iStart, UINT cEntries, const PALETTEENTRY *pPalEntries);
MINWIN_IMPORT int REB_STDCALL SetROP2(HDC hdc, int rop2);
MINWIN_IMPORT int REB_STDCALL SetStretchBltMode(HDC hdc, int mode);
MINWIN_IMPORT UINT REB_STDCALL SetSystemPaletteUse(HDC hdc, UINT use);
MINWIN_IMPORT COLORREF REB_STDCALL SetTextColor(HDC hdc, COLORREF color);
MINWIN_IMPORT UINT REB_STDCALL SetTextAlign(HDC hdc, UINT align);
MINWIN_IMPORT BOOL REB_STDCALL GetTextMetricsA(HDC hdc, LPTEXTMETRICA lptm);
MINWIN_IMPORT HBITMAP REB_STDCALL CreateDIBSection(HDC hdc, const BITMAPINFO *pbmi, UINT usage, void **ppvBits, HANDLE hSection, DWORD offset);
MINWIN_IMPORT int REB_STDCALL GetObjectA(HANDLE h, int c, LPVOID pv);
MINWIN_IMPORT BOOL REB_STDCALL MoveToEx(HDC hdc, int x, int y, LPPOINT lppt);
MINWIN_IMPORT BOOL REB_STDCALL TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c);
MINWIN_IMPORT BOOL REB_STDCALL DPtoLP(HDC hdc, LPPOINT lppt, int c);
MINWIN_IMPORT BOOL REB_STDCALL Polyline(HDC hdc, const POINT *apt, int cpt);
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
#define GetObject GetObjectA
