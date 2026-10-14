// BitBlt / PatBlt / SetDIBitsToDevice, bitmap and DIB-section creation, FillRect.
#define MINWIN_IMPLEMENTATION 1
#include "port/minwin/user32.h"

#include <cstdlib>
#include <cstring>

#include "gdi_internal.h"

using namespace mwgdi;

namespace
{

Bitmap* NewBitmap(int w, int h, bool dib, bool bottomUp)
{
  if (w <= 0 || h <= 0 || static_cast<int64_t>(w) * h > 64 * 1024 * 1024)
    return nullptr;
  auto* b = new Bitmap;
  b->width = w;
  b->height = h;
  b->isDib = dib;
  b->bottomUp = bottomUp;
  b->stride = (w + 3) & ~3;
  b->bits = static_cast<uint8_t*>(calloc(static_cast<size_t>(b->stride) * h, 1));
  if (!b->bits)
  {
    delete b;
    return nullptr;
  }
  Register(b);
  return b;
}

void ReadColorTable(const BITMAPINFO* bmi, Bitmap* b)
{
  const BITMAPINFOHEADER& hd = bmi->bmiHeader;
  int colors = hd.biClrUsed ? static_cast<int>(hd.biClrUsed) : 256;
  if (colors > 256)
    colors = 256;
  const auto* quads = reinterpret_cast<const RGBQUAD*>(reinterpret_cast<const uint8_t*>(bmi) + hd.biSize);
  for (int i = 0; i < colors; ++i)
    b->table[i] = PackRgb(quads[i].rgbRed, quads[i].rgbGreen, quads[i].rgbBlue);
  b->tableSize = colors;
}

COLORREF SystemColor(int index)
{
  switch (index)
  {
    case 0:
      return 0x00c0c0c0; // COLOR_SCROLLBAR
    case 1:
      return 0x00808000; // COLOR_BACKGROUND (desktop)
    case 5:
      return 0x00ffffff; // COLOR_WINDOW
    case 6:
      return 0x00000000; // COLOR_WINDOWFRAME
    case 8:
      return 0x00000000; // COLOR_WINDOWTEXT
    case 15:
      return 0x00c0c0c0; // COLOR_BTNFACE
    default:
      return 0x00c0c0c0;
  }
}

} // namespace

extern "C" {

BOOL REB_STDCALL BitBlt(HDC hdc, int x, int y, int cx, int cy, HDC hdcSrc, int x1, int y1, DWORD rop)
{
  Dc* dst = DcOf(hdc);
  if (!dst)
    return FALSE;
  Dc* src = hdcSrc ? DcOf(hdcSrc) : nullptr;
  if (hdcSrc && !src)
    return FALSE;
  return BlitRegion(dst, x, y, cx, cy, src, x1, y1, rop) ? TRUE : FALSE;
}

BOOL REB_STDCALL PatBlt(HDC hdc, int x, int y, int w, int h, DWORD rop)
{
  Dc* dc = DcOf(hdc);
  if (!dc)
    return FALSE;
  if (w < 0)
  { // a negative extent mirrors the rectangle around its inclusive start
    x += w + 1;
    w = -w;
  }
  if (h < 0)
  {
    y += h + 1;
    h = -h;
  }
  return BlitRegion(dc, x, y, w, h, nullptr, 0, 0, rop) ? TRUE : FALSE;
}

int REB_STDCALL SetDIBitsToDevice(HDC hdc,
                                  int xDest,
                                  int yDest,
                                  DWORD w,
                                  DWORD h,
                                  int xSrc,
                                  int ySrc,
                                  UINT StartScan,
                                  UINT cLines,
                                  const void* lpvBits,
                                  const BITMAPINFO* lpbmi,
                                  UINT ColorUse)
{
  Dc* dc = DcOf(hdc);
  if (!dc || !lpvBits || !lpbmi || ColorUse != DIB_RGB_COLORS)
    return 0;
  return SetDibToDevice(dc, xDest, yDest, static_cast<int>(w), static_cast<int>(h), xSrc, ySrc, StartScan, cLines,
                        static_cast<const uint8_t*>(lpvBits), lpbmi);
}

HBITMAP REB_STDCALL CreateCompatibleBitmap(HDC hdc, int cx, int cy)
{
  Dc* dc = DcOf(hdc);
  if (!dc)
    return nullptr;
  bool dibLike = dc->memory && dc->bitmap && dc->bitmap->isDib;
  Bitmap* b = NewBitmap(cx, cy, dibLike, dibLike);
  if (b && dibLike)
  {
    memcpy(b->table, dc->bitmap->table, sizeof b->table);
    b->tableSize = dc->bitmap->tableSize;
  }
  return reinterpret_cast<HBITMAP>(b);
}

HBITMAP REB_STDCALL CreateDIBSection(HDC, const BITMAPINFO* pbmi, UINT usage, void** ppvBits, HANDLE, DWORD)
{
  if (ppvBits)
    *ppvBits = nullptr;
  if (!pbmi || usage != DIB_RGB_COLORS || pbmi->bmiHeader.biBitCount != 8 || pbmi->bmiHeader.biCompression != 0)
    return nullptr;
  int w = pbmi->bmiHeader.biWidth;
  int h = pbmi->bmiHeader.biHeight;
  bool bottomUp = h > 0;
  Bitmap* b = NewBitmap(w, h < 0 ? -h : h, true, bottomUp);
  if (!b)
    return nullptr;
  ReadColorTable(pbmi, b);
  if (ppvBits)
    *ppvBits = b->bits;
  return reinterpret_cast<HBITMAP>(b);
}

HBITMAP REB_STDCALL CreateDIBitmap(HDC hdc,
                                   const BITMAPINFOHEADER* pbmih,
                                   DWORD flInit,
                                   const void* pjBits,
                                   const BITMAPINFO* pbmi,
                                   UINT iUsage)
{
  Dc* dc = DcOf(hdc);
  if (!dc || !pbmih || pbmih->biBitCount != 8)
    return nullptr;
  int w = pbmih->biWidth;
  int h = pbmih->biHeight < 0 ? -pbmih->biHeight : pbmih->biHeight;
  Bitmap* b = NewBitmap(w, h, false, false);
  if (!b)
    return nullptr;
  if ((flInit & CBM_INIT) && pjBits && pbmi && iUsage == DIB_RGB_COLORS && pbmi->bmiHeader.biBitCount == 8)
  {
    Bitmap source; // only its colour table is wanted
    ReadColorTable(pbmi, &source);
    uint8_t map[256];
    bool mapped = BuildColorMap(source.table, source.tableSize, dc, map);
    int srcW = pbmi->bmiHeader.biWidth, srcH = pbmi->bmiHeader.biHeight;
    bool bottomUp = srcH > 0;
    if (srcH < 0)
      srcH = -srcH;
    int srcStride = (srcW + 3) & ~3;
    for (int y = 0; y < h && y < srcH; ++y)
    {
      const uint8_t* in =
        static_cast<const uint8_t*>(pjBits) + static_cast<size_t>(bottomUp ? srcH - 1 - y : y) * srcStride;
      uint8_t* out = b->bits + static_cast<size_t>(y) * b->stride;
      for (int x = 0; x < w && x < srcW; ++x)
        out[x] = mapped ? map[in[x]] : in[x];
    }
  }
  return reinterpret_cast<HBITMAP>(b);
}

int REB_STDCALL FillRect(HDC hdc, const RECT* lprc, HBRUSH hbr)
{
  Dc* dc = DcOf(hdc);
  if (!dc || !lprc)
    return 0;
  RefreshDeviceSource(dc);
  COLORREF color;
  int style = 0;
  auto raw = reinterpret_cast<uintptr_t>(hbr);
  if (raw > 0 && raw < 32)
  {
    color = SystemColor(static_cast<int>(raw) - 1);
  }
  else if (auto* b = As<Brush>(hbr, ObjType::Brush))
  {
    color = b->color;
    style = b->style;
  }
  else
  {
    return 0;
  }
  if (style == 1)
    return 1;
  int l = lprc->left, t = lprc->top, r = lprc->right, bt = lprc->bottom;
  if (r < l)
  { // as PatBlt with negative extents (the behaviour of Wine's FillRect)
    int n = l - r;
    l = r + 1;
    r = l + n;
  }
  if (bt < t)
  {
    int n = t - bt;
    t = bt + 1;
    bt = t + n;
  }
  FillSurfaceRect(dc, l, t, r, bt, ResolveColor(dc, color), 13);
  return 1;
}

} // extern "C"
