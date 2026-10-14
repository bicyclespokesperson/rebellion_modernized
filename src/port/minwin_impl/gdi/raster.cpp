// Colour resolution and the rasteriser: BitBlt / PatBlt / SetDIBitsToDevice, DIB sections and bitmaps, pens (lines, Rectangle) and
// brush fills. All on 8-bit palette-index surfaces; a binary/ternary raster op acts on the index bytes, as in Windows.
#define MINWIN_IMPLEMENTATION 1
#include <cstdlib>
#include <cstring>

#include <vector>

#include "gdi_internal.h"

namespace mwgdi
{

uint8_t ApplyRop2(int rop2, uint8_t p, uint8_t d)
{
  int v = rop2 - 1;
  uint8_t r = 0;
  if (v & 1)
    r |= static_cast<uint8_t>(~p & ~d);
  if (v & 2)
    r |= static_cast<uint8_t>(~p & d);
  if (v & 4)
    r |= static_cast<uint8_t>(p & ~d);
  if (v & 8)
    r |= static_cast<uint8_t>(p & d);
  return r;
}

uint8_t ApplyRop3(uint8_t ropIndex, uint8_t pat, uint8_t src, uint8_t dst)
{
  uint8_t r = 0;
  if (ropIndex & 0x01)
    r |= static_cast<uint8_t>(~pat & ~src & ~dst);
  if (ropIndex & 0x02)
    r |= static_cast<uint8_t>(~pat & ~src & dst);
  if (ropIndex & 0x04)
    r |= static_cast<uint8_t>(~pat & src & ~dst);
  if (ropIndex & 0x08)
    r |= static_cast<uint8_t>(~pat & src & dst);
  if (ropIndex & 0x10)
    r |= static_cast<uint8_t>(pat & ~src & ~dst);
  if (ropIndex & 0x20)
    r |= static_cast<uint8_t>(pat & ~src & dst);
  if (ropIndex & 0x40)
    r |= static_cast<uint8_t>(pat & src & ~dst);
  if (ropIndex & 0x80)
    r |= static_cast<uint8_t>(pat & src & dst);
  return r;
}

uint8_t ResolveColor(const Dc* dc, COLORREF c)
{
  const uint32_t* table = TableOf(dc);
  int count = dc->surface.table ? dc->surface.tableSize : 256;
  bool systemTable = dc->surface.table == nullptr;
  uint32_t kind = c >> 24;
  if ((kind == 1 || kind == 2) && (dc->palette != DefaultPalette() || kind == 1))
  {
    const Palette* pal = dc->palette;
    int n = static_cast<int>(pal->entries.size());
    int index = -1;
    if (kind == 1)
    {
      index = static_cast<int>(c & 0xffff);
      if (index >= n)
        return static_cast<uint8_t>(NearestIndex(table, count, 0));
    }
    else if (n > 0)
    {
      uint32_t rgb = RgbOf(c);
      uint32_t logical[1024];
      int m = n < 1024 ? n : 1024;
      for (int i = 0; i < m; ++i)
        logical[i] = PackRgb(pal->entries[i].peRed, pal->entries[i].peGreen, pal->entries[i].peBlue);
      index = NearestIndex(logical, m, rgb);
    }
    if (index >= 0)
    {
      if (systemTable && index < static_cast<int>(pal->toPhysical.size()))
        return pal->toPhysical[index];
      const PALETTEENTRY& e = pal->entries[index];
      return static_cast<uint8_t>(NearestIndex(table, count, PackRgb(e.peRed, e.peGreen, e.peBlue)));
    }
  }
  return static_cast<uint8_t>(NearestIndex(table, count, RgbOf(c)));
}

void FillSurfaceRect(Dc* dc, int l, int t, int r, int b, uint8_t value, int rop2)
{
  const Surface& s = dc->surface;
  ForEachClipped(dc, l, t, r, b,
                 [&](int x0, int y0, int x1, int y1)
                 {
                   for (int y = y0; y < y1; ++y)
                   {
                     uint8_t* row = s.Row(y);
                     if (rop2 == 13)
                       memset(row + x0, value, x1 - x0);
                     else
                       for (int x = x0; x < x1; ++x)
                         row[x] = ApplyRop2(rop2, value, row[x]);
                   }
                 });
  MirrorDeviceRect(dc, l, t, r, b);
}

void PlotPixel(Dc* dc, int x, int y, uint8_t value, int rop2)
{
  FillSurfaceRect(dc, x, y, x + 1, y + 1, value, rop2);
}

// ---- colour maps between surfaces

// map[i] = index in the destination colour table that displays source entry i. Returns false when the map is the identity.
bool BuildColorMap(const uint32_t* srcTable, int srcCount, const Dc* dst, uint8_t map[256])
{
  const uint32_t* dstTable = TableOf(dst);
  int dstCount = dst->surface.table ? dst->surface.tableSize : 256;
  if (srcTable == dstTable)
    return false;
  bool identity = true;
  const Palette* pal = dst->palette;
  bool systemDst = dst->surface.table == nullptr;
  for (int i = 0; i < 256; ++i)
  {
    uint8_t m;
    if (i >= srcCount)
    {
      m = static_cast<uint8_t>(i);
    }
    else
    {
      uint32_t rgb = srcTable[i] & 0xffffff;
      if (i < dstCount && (dstTable[i] & 0xffffff) == rgb)
      {
        m = static_cast<uint8_t>(i);
      }
      else
      {
        int found = -1;
        if (systemDst)
        {
          for (size_t j = 0; j < pal->entries.size() && j < pal->toPhysical.size(); ++j)
          {
            const PALETTEENTRY& e = pal->entries[j];
            if (PackRgb(e.peRed, e.peGreen, e.peBlue) == rgb && (System().rgb[pal->toPhysical[j]] & 0xffffff) == rgb)
            {
              found = pal->toPhysical[j];
              break;
            }
          }
        }
        m = static_cast<uint8_t>(found >= 0 ? found : NearestIndex(dstTable, dstCount, rgb));
      }
    }
    map[i] = m;
    if (m != i)
      identity = false;
  }
  return !identity;
}

namespace
{

bool RopUsesSource(uint8_t rop)
{
  return ((rop >> 2) & 0x33) != (rop & 0x33);
}
bool RopUsesPattern(uint8_t rop)
{
  return ((rop >> 4) & 0x0f) != (rop & 0x0f);
}

} // namespace

bool BlitRegion(Dc* dst, int dx, int dy, int w, int h, Dc* src, int sx, int sy, DWORD rop)
{
  RefreshDeviceSource(dst);
  auto ropIndex = static_cast<uint8_t>(rop >> 16);
  bool useSrc = RopUsesSource(ropIndex);
  bool usePat = RopUsesPattern(ropIndex);
  if (useSrc && (!src || src->info || !src->surface.bits))
    return false;
  if (!dst->surface.bits)
    return false;
  if (usePat && dst->brush->style == 1)
    return true;
  if (w <= 0 || h <= 0)
    return true;
  uint8_t pat = usePat ? ResolveColor(dst, dst->brush->color) : 0;

  uint8_t map[256];
  bool mapped = false;
  int srcOffX = 0, srcOffY = 0;
  if (useSrc)
  {
    srcOffX = sx - dx;
    srcOffY = sy - dy;
    // clip the destination rectangle to what the source can supply
    int l = dx, t = dy, r = dx + w, b = dy + h;
    if (l + srcOffX + src->originX < 0)
      l = -srcOffX - src->originX;
    if (t + srcOffY + src->originY < 0)
      t = -srcOffY - src->originY;
    if (r + srcOffX + src->originX > src->surface.width)
      r = src->surface.width - srcOffX - src->originX;
    if (b + srcOffY + src->originY > src->surface.height)
      b = src->surface.height - srcOffY - src->originY;
    if (l >= r || t >= b)
      return true;
    dx = l;
    dy = t;
    w = r - l;
    h = b - t;
    mapped = BuildColorMap(TableOf(src), src->surface.table ? src->surface.tableSize : 256, dst, map);
  }

  const int scale = DeviceScale();
  std::vector<uint8_t> deviceSource;
  if (scale > 1 && useSrc)
    RefreshDeviceSource(src);
  const bool sourceDetail = useSrc && (!src->memory || (src->bitmap && !src->bitmap->deviceBits.empty()));
  const uint64_t devicePixels = static_cast<uint64_t>(w) * h * scale * scale;
  const bool copyDeviceSource = (sourceDetail || (useSrc && ropIndex != 0xcc)) &&
                                devicePixels <= 64u * 1024u * 1024u;
  if (scale > 1 && copyDeviceSource && !dst->deviceView)
  {
    deviceSource.resize(static_cast<size_t>(devicePixels));
    if (sourceDetail)
    {
      const Dc sourceView = DeviceView(src);
      for (int y = 0; y < h * scale; ++y)
      {
        const uint8_t* row = sourceView.surface.Row((dy + srcOffY + src->originY) * scale + y);
        uint8_t* out = deviceSource.data() + static_cast<size_t>(y) * w * scale;
        int sourceX = (dx + srcOffX + src->originX) * scale;
        if (mapped)
          for (int x = 0; x < w * scale; ++x)
            out[x] = map[row[sourceX + x]];
        else
          memcpy(out, row + sourceX, w * scale);
      }
    }
    else
    {
      for (int y = 0; y < h * scale; ++y)
      {
        const uint8_t* row = src->surface.Row(dy + srcOffY + src->originY + y / scale);
        uint8_t* out = deviceSource.data() + static_cast<size_t>(y) * w * scale;
        for (int x = 0; x < w * scale; ++x)
        {
          uint8_t index = row[dx + srcOffX + src->originX + x / scale];
          out[x] = mapped ? map[index] : index;
        }
      }
    }
  }
  if (scale > 1 && !dst->deviceView && (!dst->memory ||
      (dst->bitmap && (!dst->bitmap->deviceBits.empty() || copyDeviceSource))))
    (void)DeviceView(dst);

  // The source rows are copied out first, so overlapping blits within one surface behave.
  std::vector<uint8_t> sourcePixels;
  if (useSrc)
  {
    sourcePixels.resize(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y)
    {
      const uint8_t* row = src->surface.Row(dy + y + srcOffY + src->originY) + dx + srcOffX + src->originX;
      uint8_t* out = &sourcePixels[static_cast<size_t>(y) * w];
      if (mapped)
        for (int x = 0; x < w; ++x)
          out[x] = map[row[x]];
      else
        memcpy(out, row, w);
    }
  }
  const Surface& s = dst->surface;
  ForEachClipped(dst, dx, dy, dx + w, dy + h,
                 [&](int x0, int y0, int x1, int y1)
                 {
                   for (int y = y0; y < y1; ++y)
                   {
                     uint8_t* row = s.Row(y);
                     int ry = y - dst->originY - dy;
                     const uint8_t* in =
                       useSrc ? &sourcePixels[static_cast<size_t>(ry) * w + (x0 - dst->originX - dx)] : nullptr;
                     if (ropIndex == 0xcc && in)
                     {
                       memcpy(row + x0, in, x1 - x0);
                     }
                     else
                     {
                       for (int x = x0; x < x1; ++x)
                         row[x] = ApplyRop3(ropIndex, pat, in ? in[x - x0] : 0, row[x]);
                     }
                   }
                 });
  if (scale > 1 && !dst->deviceView && copyDeviceSource &&
      (!dst->memory || (dst->bitmap && !dst->bitmap->deviceBits.empty())))
  {
    RecordDeviceShadow(dst, dx, dy, dx + w, dy + h);
    Dc deviceDst = DeviceView(dst);
    ForEachClipped(dst, dx, dy, dx + w, dy + h, [&](int x0, int y0, int x1, int y1) {
      for (int y = y0 * scale; y < y1 * scale; ++y)
      {
        uint8_t* out = deviceDst.surface.Row(y);
        const uint8_t* in = deviceSource.data() +
                            static_cast<size_t>(y - (dy + dst->originY) * scale) * w * scale +
                            (x0 - dx - dst->originX) * scale;
        if (ropIndex == 0xcc)
          memcpy(out + x0 * scale, in, (x1 - x0) * scale);
        else
          for (int x = x0 * scale; x < x1 * scale; ++x)
            out[x] = ApplyRop3(ropIndex, pat, in[x - x0 * scale], out[x]);
      }
    });
  }
  else
    MirrorDeviceRect(dst, dx, dy, dx + w, dy + h);
  return true;
}

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
                   const BITMAPINFO* bmi)
{
  RefreshDeviceSource(dc);
  const BITMAPINFOHEADER& hd = bmi->bmiHeader;
  if (!dc->surface.bits || hd.biBitCount != 8 || hd.biCompression != 0 || w <= 0 || h <= 0)
    return 0;
  int dibWidth = hd.biWidth;
  int dibHeight = hd.biHeight < 0 ? -hd.biHeight : hd.biHeight;
  bool bottomUp = hd.biHeight > 0;
  int stride = (dibWidth + 3) & ~3;
  int colors = hd.biClrUsed ? static_cast<int>(hd.biClrUsed) : 256;
  if (colors > 256)
    colors = 256;
  const auto* quads = reinterpret_cast<const RGBQUAD*>(reinterpret_cast<const uint8_t*>(bmi) + hd.biSize);
  uint32_t table[256] = {};
  for (int i = 0; i < colors; ++i)
    table[i] = PackRgb(quads[i].rgbRed, quads[i].rgbGreen, quads[i].rgbBlue);
  uint8_t map[256];
  bool mapped = BuildColorMap(table, colors, dc, map);
  int lines = 0;
  int topScan = ySrc + h;
  if (topScan > dibHeight)
    topScan = dibHeight;
  int reported = (topScan < static_cast<int>(startScan + cLines) ? topScan : static_cast<int>(startScan + cLines)) -
                 static_cast<int>(startScan);
  // The source rectangle is in DIB coordinates (origin at the lower left for a bottom-up DIB); scan lines startScan.. are present in `bits`.
  for (int row = 0; row < h; ++row)
  {
    int dibRow =
      bottomUp ? ySrc + (h - 1 - row) : ySrc + row; // scan-line index counted in storage order from the bottom (or top)
    int scan = dibRow;
    if (scan < static_cast<int>(startScan) || scan >= static_cast<int>(startScan + cLines) || dibRow < 0 ||
        dibRow >= dibHeight)
      continue;
    int memRow = dibRow - static_cast<int>(startScan);
    const uint8_t* in = bits + static_cast<size_t>(memRow) * stride;
    ++lines;
    int y = yDest + row;
    ForEachClipped(dc, xDest, y, xDest + w, y + 1,
                   [&](int x0, int y0, int x1, int)
                   {
                     uint8_t* out = dc->surface.Row(y0);
                     for (int x = x0; x < x1; ++x)
                     {
                       int sxp = xSrc + (x - dc->originX - xDest);
                       if (sxp < 0 || sxp >= dibWidth)
                         continue;
                       uint8_t v = in[sxp];
                       out[x] = mapped ? map[v] : v;
                     }
                   });
  }
  (void)lines;
  MirrorDeviceRect(dc, xDest, yDest, xDest + w, yDest + h);
  return reported > 0 ? reported : 0;
}

} // namespace mwgdi
