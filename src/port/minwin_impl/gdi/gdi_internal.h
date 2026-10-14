// minwin_impl/gdi: internal model of the 8-bit GDI subset. Logical and device surfaces store palette indices.
//
// Surfaces: the one 640x480 screen (window DCs), DIB sections (own colour table, bottom-up unless the height was negative) and
// device-dependent bitmaps (indices into the system palette). A DC resolves COLORREFs to indices of its surface's colour table.
#pragma once
#include <stdint.h>

#include <vector>

#include "port/minwin/minwin.h"

namespace mwgdi
{

enum class ObjType : uint32_t
{
  Pen = 0x50454e31,
  Brush,
  Font,
  Bitmap,
  Palette,
  Dc
};

struct Object
{
  ObjType type;
  bool stock = false;
  bool deleteRequested = false;
  int selectCount = 0;
  explicit Object(ObjType t) : type(t)
  {
  }
  virtual ~Object() = default;
};

struct Pen : Object
{
  int style = 0; // PS_SOLID 0, PS_DASH 1, PS_DOT 2, PS_NULL 5
  int width = 1;
  COLORREF color = 0;
  Pen() : Object(ObjType::Pen)
  {
  }
};

struct Brush : Object
{
  int style = 0; // BS_SOLID 0, BS_NULL 1
  COLORREF color = 0;
  Brush() : Object(ObjType::Brush)
  {
  }
};

struct Font : Object
{
  LOGFONTA lf = {};
  void* textCache = nullptr; // owned by the text module
  void* scaledTextCache[5] = {};
  Font() : Object(ObjType::Font)
  {
  }
  ~Font() override;
};

struct Dc;

struct Bitmap : Object
{
  int width = 0, height = 0;
  bool isDib = false; // a DIB section: has its own colour table and its bits are visible to the application
  bool bottomUp = false;
  int stride = 0;
  uint8_t* bits = nullptr;
  uint32_t table[256] = {}; // 0x00RRGGBB, DIB sections only
  int tableSize = 0;
  Dc* selectedIn = nullptr;
  std::vector<uint8_t> deviceBits;
  std::vector<uint8_t> deviceShadow;
  int deviceScale = 1;
  uint64_t deviceScaleEpoch = 0;
  uint64_t deviceRevision = 0;
  Bitmap() : Object(ObjType::Bitmap)
  {
  }
  ~Bitmap() override;
};

struct Palette : Object
{
  std::vector<PALETTEENTRY> entries;
  std::vector<uint8_t> toPhysical; // logical index -> system palette index, as of the last Select/Realize
  bool realized = false;
  Palette() : Object(ObjType::Palette)
  {
  }
};

// A drawing target: rows of palette indices.
struct Surface
{
  uint8_t* bits = nullptr;
  int width = 0, height = 0;
  int stride = 0;
  bool bottomUp = false;
  const uint32_t* table = nullptr; // null: the system palette
  int tableSize = 256;
  uint8_t* Row(int y) const
  {
    return bits + static_cast<ptrdiff_t>(bottomUp ? height - 1 - y : y) * stride;
  }
};

struct Dc : Object
{
  bool memory = false;
  bool info = false; // CreateIC: no drawing surface
  HWND window = nullptr;
  Bitmap* bitmap = nullptr; // memory DC: the selected bitmap
  Surface surface;
  int originX = 0, originY = 0; // surface coordinates of the DC's (0,0)
  std::vector<RECT> clip; // surface coordinates, non-overlapping; empty = nothing visible
  Pen* pen;
  Brush* brush;
  Font* font;
  Palette* palette; // never null: the default palette when none is selected
  COLORREF textColor = 0, bkColor = 0x00ffffff;
  int bkMode = 2, rop2 = 13, textAlign = 0, stretchMode = 1;
  int curX = 0, curY = 0;
  bool deviceView = false;
  bool suppressDeviceMirror = false;
  Dc() : Object(ObjType::Dc)
  {
  }
};

// ---- objects.cpp
extern void (*g_fontCacheFree)(void*); // set by the text module
bool IsLive(const void* handle, ObjType type);
template <typename T>
T* As(const void* handle, ObjType type)
{
  return IsLive(handle, type) ? static_cast<T*>(const_cast<void*>(handle)) : nullptr;
}
void Register(Object* o);
void Unregister(Object* o);
Palette* DefaultPalette();
Pen* StockPen(int which);
Brush* StockBrush(int which);

// ---- system palette (dc.cpp)
struct SystemPalette
{
  uint32_t rgb[256];
  uint8_t flags[256];
  bool staticColors = true;
  uint32_t generation = 1;
};
SystemPalette& System();
extern const uint32_t kStaticColors[20];
int NearestIndex(const uint32_t* table, int count, uint32_t rgb);
uint32_t RgbOf(COLORREF c); // 0x00RRGGBB from a COLORREF (0x00BBGGRR)
int RealizePaletteIn(Dc* dc);
void UpdateMapping(Palette* p);

// ---- colour and drawing (raster.cpp)
inline uint32_t PackRgb(uint8_t r, uint8_t g, uint8_t b)
{
  return (uint32_t(r) << 16) | (uint32_t(g) << 8) | b;
}
Dc* DcOf(HDC hdc);
Surface SurfaceOfDc(const Dc* dc);
const uint32_t* TableOf(const Dc* dc);
uint8_t ResolveColor(const Dc* dc, COLORREF c);
void ResetClipToSurface(Dc* dc);
int DeviceScale();
Dc DeviceView(const Dc* dc);
void MirrorDeviceRect(Dc* dc, int l, int t, int r, int b);
void RecordDeviceShadow(Dc* dc, int l, int t, int r, int b);
void RefreshDeviceSource(Dc* dc);
// Applies a binary raster op (R2_*) to one byte.
uint8_t ApplyRop2(int rop2, uint8_t pen, uint8_t dst);
uint8_t ApplyRop3(uint8_t ropIndex, uint8_t pattern, uint8_t src, uint8_t dst);
// Calls fn(x0,y0,x1,y1) for the parts of [l,r)x[t,b) (DC coordinates) that are inside the clip.
template <typename Fn>
void ForEachClipped(const Dc* dc, int l, int t, int r, int b, Fn fn)
{
  l += dc->originX;
  r += dc->originX;
  t += dc->originY;
  b += dc->originY;
  for (const RECT& c : dc->clip)
  {
    int x0 = l > c.left ? l : c.left, y0 = t > c.top ? t : c.top;
    int x1 = r < c.right ? r : c.right, y1 = b < c.bottom ? b : c.bottom;
    if (x0 < x1 && y0 < y1)
      fn(x0, y0, x1, y1);
  }
}
bool BuildColorMap(const uint32_t* srcTable, int srcCount, const Dc* dst, uint8_t map[256]);
// BitBlt core: `rop` is the full 32-bit ternary raster op. False when the op needs a source and there is none.
bool BlitRegion(Dc* dst, int dx, int dy, int w, int h, Dc* src, int sx, int sy, DWORD rop);
int SetDibToDevice(Dc* dc,
                   int xDest,
                   int yDest,
                   int w,
                   int h,
                   int xSrc,
                   int ySrc,
                   UINT startScan,
                   UINT cLines,
                   const uint8_t* bits,
                   const BITMAPINFO* bmi);
void FillSurfaceRect(Dc* dc, int l, int t, int r, int b, uint8_t value, int rop2); // DC coordinates
void PlotPixel(Dc* dc, int x, int y, uint8_t value, int rop2); // DC coordinates, clipped

} // namespace mwgdi
