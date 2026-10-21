// Device contexts, the screen and its system palette, logical palettes (select / realize / animate), device caps.
#include <cstdlib>
#include <cstring>

#include "../gdi_api.h"
#include "gdi_internal.h"

namespace mwgdi
{

namespace
{
std::vector<uint8_t> g_deviceBits;
int g_deviceScale = 1;
uint64_t g_deviceScaleEpoch = 0;
int g_screenWidth = minwin_gdi::kScreenWidth;
uint8_t* g_screenBits = static_cast<uint8_t*>(calloc(minwin_gdi::kMaxScreenWidth * minwin_gdi::kScreenHeight, 1));
} // namespace

int DeviceScale()
{
  return g_deviceScale;
}

namespace
{
void PrepareBitmap(Bitmap* bitmap)
{
  const int scale = g_deviceScale;
  if (bitmap->deviceScale == scale && !bitmap->deviceBits.empty()) {
    return;
  }
  const int previousScale = bitmap->deviceScale;
  const size_t logicalPixels = static_cast<size_t>(bitmap->width) * bitmap->height;
  const bool canResample = previousScale > 0 && !bitmap->deviceBits.empty()
                           && bitmap->deviceShadow.size() == logicalPixels
                           && bitmap->deviceBits.size() == logicalPixels * previousScale * previousScale;
  bool logicalPixelsUnchanged = canResample;
  if (logicalPixelsUnchanged) {
    for (int y = 0; y < bitmap->height && logicalPixelsUnchanged; ++y) {
      const uint8_t* src
          = bitmap->bits + static_cast<size_t>(bitmap->bottomUp ? bitmap->height - 1 - y : y) * bitmap->stride;
      const uint8_t* shadow = bitmap->deviceShadow.data() + static_cast<size_t>(y) * bitmap->width;
      logicalPixelsUnchanged = memcmp(src, shadow, bitmap->width) == 0;
    }
  }
  std::vector<uint8_t> previousDeviceBits;
  if (logicalPixelsUnchanged) {
    previousDeviceBits = std::move(bitmap->deviceBits);
  } else if (canResample) {
    ++bitmap->deviceRevision;
  }
  bitmap->deviceScale = scale;
  bitmap->deviceScaleEpoch = g_deviceScaleEpoch;
  const size_t devicePixels = static_cast<size_t>(bitmap->width) * bitmap->height * scale * scale;
  if (devicePixels > 64u * 1024u * 1024u) {
    bitmap->deviceBits.clear();
    bitmap->deviceShadow.clear();
    return;
  }
  bitmap->deviceBits.resize(devicePixels);
  bitmap->deviceShadow.resize(logicalPixels);
  for (int y = 0; y < bitmap->height; ++y) {
    const uint8_t* src
        = bitmap->bits + static_cast<size_t>(bitmap->bottomUp ? bitmap->height - 1 - y : y) * bitmap->stride;
    uint8_t* shadow = bitmap->deviceShadow.data() + static_cast<size_t>(y) * bitmap->width;
    memcpy(shadow, src, bitmap->width);
    for (int dy = 0; dy < scale; ++dy) {
      uint8_t* dst = bitmap->deviceBits.data() + static_cast<size_t>(y * scale + dy) * bitmap->width * scale;
      if (!logicalPixelsUnchanged) {
        for (int x = 0; x < bitmap->width; ++x) {
          memset(dst + x * scale, src[x], scale);
        }
        continue;
      }
      const int sourceY = ((2 * dy + 1) * previousScale) / (2 * scale);
      for (int dx = 0; dx < bitmap->width * scale; ++dx) {
        const int sourceX = ((2 * (dx % scale) + 1) * previousScale) / (2 * scale);
        dst[dx] = previousDeviceBits[static_cast<size_t>(y * previousScale + sourceY) * bitmap->width * previousScale
                                     + (dx / scale) * previousScale + sourceX];
      }
    }
  }
}

void RefreshBitmap(Bitmap* bitmap)
{
  PrepareBitmap(bitmap);
  if (bitmap->deviceBits.empty()) {
    return;
  }
  bool changed = false;
  for (int y = 0; y < bitmap->height; ++y) {
    const uint8_t* src
        = bitmap->bits + static_cast<size_t>(bitmap->bottomUp ? bitmap->height - 1 - y : y) * bitmap->stride;
    const uint8_t* shadow = bitmap->deviceShadow.data() + static_cast<size_t>(y) * bitmap->width;
    if (memcmp(src, shadow, bitmap->width) != 0) {
      changed = true;
      break;
    }
  }
  if (!changed) {
    bitmap->deviceScaleEpoch = g_deviceScaleEpoch;
    return;
  }
  // DIB section bits are writable by the game. Their provenance is unknown after
  // a raw write, so discard the bitmap's high-resolution drawing as a whole.
  bitmap->deviceBits.clear();
  ++bitmap->deviceRevision;
  PrepareBitmap(bitmap);
}
} // namespace

Dc DeviceView(const Dc* dc)
{
  Dc view = *dc;
  const int scale = g_deviceScale;
  view.deviceView = true;
  view.originX *= scale;
  view.originY *= scale;
  for (RECT& clip : view.clip) {
    clip.left *= scale;
    clip.top *= scale;
    clip.right *= scale;
    clip.bottom *= scale;
  }
  if (dc->memory && dc->bitmap) {
    PrepareBitmap(dc->bitmap);
    view.surface.bits = dc->bitmap->deviceBits.data();
    view.surface.width = dc->bitmap->deviceBits.empty() ? 0 : dc->bitmap->width * scale;
    view.surface.height = dc->bitmap->deviceBits.empty() ? 0 : dc->bitmap->height * scale;
    view.surface.bottomUp = false;
  } else {
    view.surface.bits = g_deviceBits.data();
    const minwin_gdi::ScreenSurface screen = minwin_gdi::Screen();
    view.surface.width = screen.width * scale;
    view.surface.height = screen.height * scale;
  }
  view.surface.stride = view.surface.width;
  return view;
}

void MirrorDeviceRect(Dc* dc, int l, int t, int r, int b)
{
  if (dc->info || dc->deviceView || dc->suppressDeviceMirror || g_deviceScale == 1
      || (dc->memory ? !dc->bitmap || dc->bitmap->deviceBits.empty() : g_deviceBits.empty())) {
    return;
  }
  const int scale = g_deviceScale;
  RecordDeviceShadow(dc, l, t, r, b);
  Dc view = DeviceView(dc);
  ForEachClipped(dc, l, t, r, b, [&](int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y) {
      const uint8_t* src = dc->surface.Row(y);
      for (int sy = 0; sy < scale; ++sy) {
        uint8_t* dst = view.surface.Row(y * scale + sy);
        for (int x = x0; x < x1; ++x) {
          memset(dst + x * scale, src[x], scale);
        }
      }
    }
  });
}

void RecordDeviceShadow(Dc* dc, int l, int t, int r, int b)
{
  if (!dc->memory || dc->deviceView || g_deviceScale == 1 || !dc->bitmap || dc->bitmap->deviceBits.empty()) {
    return;
  }
  PrepareBitmap(dc->bitmap);
  ++dc->bitmap->deviceRevision;
  ForEachClipped(dc, l, t, r, b, [&](int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; ++y) {
      memcpy(dc->bitmap->deviceShadow.data() + static_cast<size_t>(y) * dc->bitmap->width + x0, dc->surface.Row(y) + x0,
             x1 - x0);
    }
  });
}

void RefreshDeviceSource(Dc* dc)
{
  if (dc && dc->memory && dc->bitmap && !dc->bitmap->deviceBits.empty() && g_deviceScale > 1) {
    RefreshBitmap(dc->bitmap);
  }
}

// The 20 static colours Windows reserves: indices 0-9 and 246-255 (0x00RRGGBB).
const uint32_t kStaticColors[20] = {
    0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0, 0xc0dcc0, 0xa6caf0,
    0xfffbf0, 0xa0a0a4, 0x808080, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,
};

SystemPalette& System()
{
  static SystemPalette* p = [] {
    auto* s = new SystemPalette;
    memset(s->rgb, 0, sizeof s->rgb);
    memset(s->flags, 0, sizeof s->flags);
    for (int i = 0; i < 10; ++i) {
      s->rgb[i] = kStaticColors[i];
      s->rgb[246 + i] = kStaticColors[10 + i];
    }
    return s;
  }();
  return *p;
}

uint32_t RgbOf(COLORREF c)
{
  return PackRgb(c & 0xff, (c >> 8) & 0xff, (c >> 16) & 0xff);
}

int NearestIndex(const uint32_t* table, int count, uint32_t rgb)
{
  for (int i = 0; i < count; ++i) {
    if ((table[i] & 0xffffff) == rgb) {
      return i;
    }
  }
  int best = 0;
  long bestDistance = -1;
  int r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;
  for (int i = 0; i < count; ++i) {
    int dr = int((table[i] >> 16) & 0xff) - r, dg = int((table[i] >> 8) & 0xff) - g, db = int(table[i] & 0xff) - b;
    long d = long(dr) * dr + long(dg) * dg + long(db) * db;
    if (bestDistance < 0 || d < bestDistance) {
      bestDistance = d;
      best = i;
    }
  }
  return best;
}

void UpdateMapping(Palette* p)
{
  if (p->realized && p->toPhysical.size() == p->entries.size()) {
    return;
  }
  p->toPhysical.resize(p->entries.size());
  SystemPalette& sys = System();
  for (size_t i = 0; i < p->entries.size(); ++i) {
    p->toPhysical[i] = static_cast<uint8_t>(
        NearestIndex(sys.rgb, 256, PackRgb(p->entries[i].peRed, p->entries[i].peGreen, p->entries[i].peBlue)));
  }
}

namespace
{

constexpr int kPcReserved = 1, kPcExplicit = 2, kPcNoCollapse = 4;

bool IsStaticSlot(const SystemPalette& s, int i)
{
  return s.staticColors && (i < 10 || i >= 246);
}
bool IsFixedSlot(const SystemPalette& s, int i)
{
  return IsStaticSlot(s, i) || i == 0 || i == 255;
}

void SetSlot(SystemPalette& s, int i, uint32_t rgb, int& changed)
{
  if (s.rgb[i] != rgb) {
    s.rgb[i] = rgb;
    ++changed;
  }
}

} // namespace

// RealizePalette's result when no system palette slot changed colour: the entries of the logical palette that map to a
// different system palette index than at its previous realization (every entry, for a palette realized the first time).
// Wine's user32 reports a realization that only re-maps entries to the colours the system palette already holds (a
// movie palette equal to the previous one) with a WM_PALETTECHANGED broadcast as well.
int MappedDifferently(const std::vector<uint8_t>& before, const std::vector<uint8_t>& after)
{
  int different = 0;
  for (size_t i = 0; i < after.size(); ++i) {
    if (i >= before.size() || before[i] != after[i]) {
      ++different;
    }
  }
  return different;
}

// Foreground realization of the DC's palette (the model: one active palette owns every non-static slot).
int RealizePaletteIn(Dc* dc)
{
  Palette* pal = dc->palette;
  SystemPalette& sys = System();
  if (pal == DefaultPalette()) {
    return 0;
  }
  int n = static_cast<int>(pal->entries.size());
  const std::vector<uint8_t> previousMapping = pal->realized ? pal->toPhysical : std::vector<uint8_t>();
  pal->toPhysical.assign(n, 0);
  if (dc->memory) {
    pal->realized = false;
    UpdateMapping(pal);
    return 0;
  }
  uint8_t used[256] = {};
  for (int i = 0; i < 256; ++i) {
    used[i] = IsFixedSlot(sys, i) ? 1 : 0;
  }
  bool owned[256] = {};
  int changed = 0;
  if (!sys.staticColors) {
    sys.rgb[0] = 0x000000;
    sys.rgb[255] = 0xffffff;
  }
  for (int i = 0; i < n; ++i) {
    const PALETTEENTRY& e = pal->entries[i];
    uint32_t rgb = PackRgb(e.peRed, e.peGreen, e.peBlue);
    if (e.peFlags & kPcExplicit) {
      pal->toPhysical[i] = static_cast<uint8_t>((e.peRed | (e.peGreen << 8)) & 0xff);
      continue;
    }
    int slot = -1;
    if (!(e.peFlags & (kPcNoCollapse | kPcReserved))) {
      for (int k = 0; k < 256 && slot < 0; ++k) {
        if ((used[k] || owned[k]) && sys.rgb[k] == rgb) {
          slot = k;
        }
      }
    }
    if (slot < 0) {
      for (int k = 0; k < 256; ++k) {
        if (!used[k] && !owned[k]) {
          slot = k;
          owned[k] = true;
          SetSlot(sys, k, rgb, changed);
          break;
        }
      }
    }
    if (slot < 0) {
      slot = NearestIndex(sys.rgb, 256, rgb);
    }
    pal->toPhysical[i] = static_cast<uint8_t>(slot);
  }
  for (int k = 0; k < 256; ++k) {
    if (!used[k] && !owned[k] && sys.rgb[k] != 0) { // released by the previous palette
      sys.rgb[k] = 0;
      ++changed;
    }
  }
  pal->realized = true;
  if (changed) {
    ++sys.generation;
  }
  return changed ? changed : MappedDifferently(previousMapping, pal->toPhysical);
}

Dc* DcOf(HDC hdc)
{
  return As<Dc>(hdc, ObjType::Dc);
}

const uint32_t* TableOf(const Dc* dc)
{
  return dc->surface.table ? dc->surface.table : System().rgb;
}

Surface SurfaceOfDc(const Dc* dc)
{
  Surface s;
  if (dc->window || (!dc->memory && !dc->info)) {
    minwin_gdi::ScreenSurface scr = minwin_gdi::Screen();
    s.bits = scr.bits;
    s.width = scr.width;
    s.height = scr.height;
    s.stride = scr.pitch;
    return s;
  }
  if (dc->memory && dc->bitmap) {
    const Bitmap* b = dc->bitmap;
    s.bits = b->bits;
    s.width = b->width;
    s.height = b->height;
    s.stride = b->stride;
    s.bottomUp = b->bottomUp;
    if (b->isDib) {
      s.table = b->table;
      s.tableSize = b->tableSize;
    }
  }
  return s;
}

void ResetClipToSurface(Dc* dc)
{
  dc->clip.clear();
  if (dc->surface.bits) {
    dc->clip.push_back({0, 0, dc->surface.width, dc->surface.height});
  }
}

namespace
{

Dc* NewDc()
{
  Dc* dc = new Dc;
  dc->pen = StockPen(7);
  dc->brush = StockBrush(0);
  dc->font = static_cast<Font*>(reb::compat::GetStockObject(13));
  dc->palette = DefaultPalette();
  ++dc->pen->selectCount;
  ++dc->brush->selectCount;
  ++dc->font->selectCount;
  Register(dc);
  return dc;
}

Bitmap* MonoDefaultBitmap()
{
  auto* b = new Bitmap;
  b->width = b->height = 1;
  b->stride = 4;
  b->bits = static_cast<uint8_t*>(calloc(1, 4));
  b->stock = true;
  Register(b);
  return b;
}

} // namespace

} // namespace mwgdi

using namespace mwgdi;

namespace minwin_gdi
{

ScreenSurface Screen()
{
  return {g_screenBits, g_screenWidth, kScreenHeight, g_screenWidth};
}

bool SetScreenSize(int width, int height)
{
  if (width < kScreenWidth || width > kMaxScreenWidth || height != kScreenHeight || !g_screenBits
      || width == g_screenWidth) {
    return false;
  }
  const int overlap = width < g_screenWidth ? width : g_screenWidth;
  if (width > g_screenWidth) {
    for (int y = height - 1; y >= 0; --y) {
      uint8_t* destination = g_screenBits + static_cast<size_t>(y) * width;
      memmove(destination, g_screenBits + static_cast<size_t>(y) * g_screenWidth, overlap);
      memset(destination + overlap, 0, width - overlap);
    }
  } else {
    for (int y = 0; y < height; ++y) {
      memmove(g_screenBits + static_cast<size_t>(y) * width, g_screenBits + static_cast<size_t>(y) * g_screenWidth,
              overlap);
    }
  }
  g_screenWidth = width;
  g_deviceBits.clear();
  DeviceScreen(g_deviceScale);
  return true;
}

ScreenSurface DeviceScreen(int scale)
{
  if (scale < 1) {
    scale = 1;
  }
  if (scale > 4) {
    scale = 4;
  }
  const ScreenSurface logical = Screen();
  if (scale == 1) {
    if (g_deviceScale != 1) {
      ++g_deviceScaleEpoch;
    }
    g_deviceScale = 1;
    g_deviceBits.clear();
    return logical;
  }
  if (g_deviceScale != scale || g_deviceBits.empty()) {
    if (g_deviceScale != scale) {
      ++g_deviceScaleEpoch;
    }
    g_deviceScale = scale;
    g_deviceBits.resize(static_cast<size_t>(logical.width * scale) * logical.height * scale);
    for (int y = 0; y < logical.height; ++y) {
      const uint8_t* src = logical.bits + static_cast<size_t>(y) * logical.pitch;
      for (int sy = 0; sy < scale; ++sy) {
        uint8_t* dst = g_deviceBits.data() + static_cast<size_t>(y * scale + sy) * logical.width * scale;
        for (int x = 0; x < logical.width; ++x) {
          memset(dst + x * scale, src[x], scale);
        }
      }
    }
  }
  return {g_deviceBits.data(), logical.width * scale, logical.height * scale, logical.width * scale};
}

void InvalidateDeviceBitmap(HBITMAP handle)
{
  if (auto* bitmap = mwgdi::As<mwgdi::Bitmap>(handle, mwgdi::ObjType::Bitmap)) {
    bitmap->deviceBits.clear();
    bitmap->deviceShadow.clear();
    ++bitmap->deviceRevision;
  }
}

int DeviceScale()
{
  return mwgdi::DeviceScale();
}

uint64_t DeviceBitmapRevision(HBITMAP handle)
{
  auto* bitmap = mwgdi::As<mwgdi::Bitmap>(handle, mwgdi::ObjType::Bitmap);
  if (bitmap && mwgdi::DeviceScale() > 1 && bitmap->deviceScaleEpoch != mwgdi::g_deviceScaleEpoch) {
    mwgdi::RefreshBitmap(bitmap);
  }
  return bitmap ? bitmap->deviceRevision : 0;
}

bool CopyDeviceBitmapPixels(HBITMAP handle, DeviceBitmapPixels& out)
{
  auto* bitmap = mwgdi::As<mwgdi::Bitmap>(handle, mwgdi::ObjType::Bitmap);
  if (!bitmap || mwgdi::DeviceScale() <= 1) {
    return false;
  }
  mwgdi::RefreshBitmap(bitmap);
  if (bitmap->deviceBits.empty()) {
    return false;
  }
  out.bits = bitmap->deviceBits;
  out.scale = bitmap->deviceScale;
  out.revision = bitmap->deviceRevision;
  return true;
}

uint64_t SetDeviceBitmapPixels(HBITMAP handle, int scale, std::span<const uint8_t> pixels, int x, int y, int width,
                               int height)
{
  auto* bitmap = mwgdi::As<mwgdi::Bitmap>(handle, mwgdi::ObjType::Bitmap);
  const int targetScale = mwgdi::DeviceScale();
  if (!bitmap || scale < 1 || scale > 4 || targetScale <= 1) {
    return 0;
  }
  const size_t logicalPixels = static_cast<size_t>(bitmap->width) * bitmap->height;
  if (logicalPixels > 64u * 1024u * 1024u / (scale * scale) || pixels.size() != logicalPixels * scale * scale) {
    return 0;
  }
  if (width == 0 && height == 0) {
    width = bitmap->width;
    height = bitmap->height;
  }
  if (x < 0 || y < 0 || width <= 0 || height <= 0 || x > bitmap->width - width || y > bitmap->height - height) {
    return 0;
  }
  mwgdi::PrepareBitmap(bitmap);
  if (bitmap->deviceBits.empty()) {
    return 0;
  }
  const int targetWidth = bitmap->width * targetScale;
  for (int targetY = y * targetScale; targetY < (y + height) * targetScale; ++targetY) {
    const int sourceY = (2 * (targetY % targetScale) + 1) * scale / (2 * targetScale) + (targetY / targetScale) * scale;
    uint8_t* destination = bitmap->deviceBits.data() + static_cast<size_t>(targetY) * targetWidth;
    for (int targetX = x * targetScale; targetX < (x + width) * targetScale; ++targetX) {
      const int sourceX
          = (2 * (targetX % targetScale) + 1) * scale / (2 * targetScale) + (targetX / targetScale) * scale;
      destination[targetX] = pixels[static_cast<size_t>(sourceY) * bitmap->width * scale + sourceX];
    }
  }
  for (int row = y; row < y + height; ++row) {
    const uint8_t* source
        = bitmap->bits + static_cast<size_t>(bitmap->bottomUp ? bitmap->height - 1 - row : row) * bitmap->stride;
    memcpy(bitmap->deviceShadow.data() + static_cast<size_t>(row) * bitmap->width + x, source + x, width);
  }
  return ++bitmap->deviceRevision;
}

void CurrentPalette(uint32_t rgb[256], uint32_t* generation)
{
  SystemPalette& s = System();
  memcpy(rgb, s.rgb, sizeof s.rgb);
  if (generation) {
    *generation = s.generation;
  }
}

HDC CreateWindowDc(const WindowDcDesc& desc)
{
  Dc* dc = NewDc();
  dc->window = desc.window;
  dc->originX = desc.originX;
  dc->originY = desc.originY;
  dc->surface = SurfaceOfDc(dc);
  for (int i = 0; i < desc.clipCount; ++i) {
    dc->clip.push_back(desc.clip[i]);
  }
  return reinterpret_cast<HDC>(dc);
}

void DestroyWindowDc(HDC hdc)
{
  Dc* dc = DcOf(hdc);
  if (!dc) {
    return;
  }
  --dc->pen->selectCount;
  --dc->brush->selectCount;
  --dc->font->selectCount;
  Unregister(dc);
  delete dc;
}

} // namespace minwin_gdi

HDC REB_STDCALL reb::compat::CreateCompatibleDC(HDC hdc)
{
  (void)hdc;
  static Bitmap* defaultBitmap = MonoDefaultBitmap();
  Dc* dc = NewDc();
  dc->memory = true;
  dc->bitmap = defaultBitmap;
  dc->surface = SurfaceOfDc(dc);
  ResetClipToSurface(dc);
  return reinterpret_cast<HDC>(dc);
}

HDC REB_STDCALL reb::compat::CreateICA(LPCSTR, LPCSTR, LPCSTR, const DEVMODEA*)
{
  Dc* dc = NewDc();
  dc->info = true;
  return reinterpret_cast<HDC>(dc);
}

BOOL REB_STDCALL reb::compat::DeleteDC(HDC hdc)
{
  Dc* dc = DcOf(hdc);
  if (!dc) {
    return FALSE;
  }
  if (dc->bitmap) {
    dc->bitmap->selectedIn = nullptr;
  }
  --dc->pen->selectCount;
  --dc->brush->selectCount;
  --dc->font->selectCount;
  Unregister(dc);
  delete dc;
  return TRUE;
}

COLORREF REB_STDCALL reb::compat::SetTextColor(HDC hdc, COLORREF color)
{
  Dc* dc = DcOf(hdc);
  if (!dc) {
    return 0xffffffff;
  }
  COLORREF old = dc->textColor;
  dc->textColor = color;
  return old;
}

COLORREF REB_STDCALL reb::compat::SetBkColor(HDC hdc, COLORREF color)
{
  Dc* dc = DcOf(hdc);
  if (!dc) {
    return 0xffffffff;
  }
  COLORREF old = dc->bkColor;
  dc->bkColor = color;
  return old;
}

int REB_STDCALL reb::compat::SetBkMode(HDC hdc, int mode)
{
  Dc* dc = DcOf(hdc);
  if (!dc || (mode != 1 && mode != 2)) {
    return 0;
  }
  int old = dc->bkMode;
  dc->bkMode = mode;
  return old;
}

int REB_STDCALL reb::compat::SetROP2(HDC hdc, int rop2)
{
  Dc* dc = DcOf(hdc);
  if (!dc || rop2 < 1 || rop2 > 16) {
    return 0;
  }
  int old = dc->rop2;
  dc->rop2 = rop2;
  return old;
}

int REB_STDCALL reb::compat::SetStretchBltMode(HDC hdc, int mode)
{
  Dc* dc = DcOf(hdc);
  if (!dc) {
    return 0;
  }
  int old = dc->stretchMode;
  dc->stretchMode = mode;
  return old;
}

UINT REB_STDCALL reb::compat::SetTextAlign(HDC hdc, UINT align)
{
  Dc* dc = DcOf(hdc);
  if (!dc) {
    return 0xffffffffu;
  }
  UINT old = dc->textAlign;
  dc->textAlign = align;
  return old;
}

BOOL REB_STDCALL reb::compat::DPtoLP(HDC hdc, LPPOINT, int)
{
  return DcOf(hdc) ? TRUE : FALSE;
}

int REB_STDCALL reb::compat::GetDeviceCaps(HDC hdc, int index)
{
  if (!DcOf(hdc)) {
    return 0;
  }
  switch (index) {
    case 0:
      return 0x0300; // DRIVERVERSION
    case 2:
      return 1; // TECHNOLOGY: DT_RASDISPLAY
    case 4:
      return 169; // HORZSIZE (mm)
    case 6:
      return 127; // VERTSIZE
    case 8:
      return minwin_gdi::Screen().width; // HORZRES
    case 10:
      return minwin_gdi::Screen().height; // VERTRES
    case 12:
      return 8; // BITSPIXEL
    case 14:
      return 1; // PLANES
    case 16:
      return -1; // NUMBRUSHES
    case 18:
      return -1; // NUMPENS
    case 22:
      return 0; // NUMFONTS
    case 24:
      return 20; // NUMCOLORS: the static colours of a palettised device
    case 26:
      return 0; // PDEVICESIZE
    case 28:
      return 0x1ff; // CURVECAPS
    case 30:
      return 0x1ff; // LINECAPS
    case 32:
      return 0xff; // POLYGONALCAPS
    case 34:
      return 0x1ff; // TEXTCAPS
    case 36:
      return 0; // CLIPCAPS
    case 38:
      return 0x7e99 | 0x100; // RASTERCAPS: bitblt, bitmap64, di bitmap, palette, dibtodev, stretchblt, stretchdib...
    case 40:
      return 36; // ASPECTX
    case 42:
      return 36; // ASPECTY
    case 44:
      return 51; // ASPECTXY
    case 88:
      return 96; // LOGPIXELSX
    case 90:
      return 96; // LOGPIXELSY
    case 104:
      return 256; // SIZEPALETTE
    case 106:
      return 20; // NUMRESERVED
    case 108:
      return 18; // COLORRES
    default:
      return 0;
  }
}

UINT REB_STDCALL reb::compat::GetSystemPaletteEntries(HDC hdc, UINT iStart, UINT cEntries, LPPALETTEENTRY out)
{
  if (!DcOf(hdc) || iStart >= 256) {
    return 0;
  }
  SystemPalette& s = System();
  UINT n = cEntries;
  if (iStart + n > 256) {
    n = 256 - iStart;
  }
  if (!out) {
    return 256;
  }
  for (UINT i = 0; i < n; ++i) {
    uint32_t c = s.rgb[iStart + i];
    out[i] = {uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c), 0};
  }
  return n;
}

UINT REB_STDCALL reb::compat::SetSystemPaletteUse(HDC hdc, UINT use)
{
  Dc* dc = DcOf(hdc);
  if (!dc || (use != SYSPAL_STATIC && use != SYSPAL_NOSTATIC)) {
    return 0;
  }
  SystemPalette& s = System();
  UINT old = s.staticColors ? SYSPAL_STATIC : SYSPAL_NOSTATIC;
  bool wantStatic = use == SYSPAL_STATIC;
  if (wantStatic != s.staticColors) {
    s.staticColors = wantStatic;
    if (wantStatic) {
      for (int i = 0; i < 10; ++i) {
        s.rgb[i] = kStaticColors[i];
        s.rgb[246 + i] = kStaticColors[10 + i];
      }
    } else {
      s.rgb[0] = 0x000000;
      s.rgb[255] = 0xffffff;
    }
    ++s.generation;
  }
  return old;
}

HPALETTE REB_STDCALL reb::compat::CreatePalette(const LOGPALETTE* plpal)
{
  if (!plpal) {
    return nullptr;
  }
  auto* p = new Palette;
  p->entries.assign(plpal->palPalEntry, plpal->palPalEntry + plpal->palNumEntries);
  UpdateMapping(p);
  Register(p);
  return reinterpret_cast<HPALETTE>(p);
}

BOOL REB_STDCALL reb::compat::ResizePalette(HPALETTE hpal, UINT n)
{
  auto* p = As<Palette>(hpal, ObjType::Palette);
  if (!p || n == 0 || n > 1024) {
    return FALSE;
  }
  p->entries.resize(n);
  p->realized = false;
  UpdateMapping(p);
  return TRUE;
}

UINT REB_STDCALL reb::compat::SetPaletteEntries(HPALETTE hpal, UINT iStart, UINT cEntries, const PALETTEENTRY* e)
{
  auto* p = As<Palette>(hpal, ObjType::Palette);
  if (!p || p->stock || iStart >= p->entries.size() || !e) {
    return 0;
  }
  UINT n = cEntries;
  if (iStart + n > p->entries.size()) {
    n = static_cast<UINT>(p->entries.size()) - iStart;
  }
  for (UINT i = 0; i < n; ++i) {
    p->entries[iStart + i] = e[i];
  }
  p->realized = false;
  UpdateMapping(p);
  return n;
}

UINT REB_STDCALL reb::compat::GetPaletteEntries(HPALETTE hpal, UINT iStart, UINT cEntries, LPPALETTEENTRY out)
{
  auto* p = As<Palette>(hpal, ObjType::Palette);
  if (!p) {
    return 0;
  }
  if (!out) {
    return static_cast<UINT>(p->entries.size());
  }
  if (iStart >= p->entries.size()) {
    return 0;
  }
  UINT n = cEntries;
  if (iStart + n > p->entries.size()) {
    n = static_cast<UINT>(p->entries.size()) - iStart;
  }
  for (UINT i = 0; i < n; ++i) {
    out[i] = p->entries[iStart + i];
  }
  return n;
}

HPALETTE REB_STDCALL reb::compat::SelectPalette(HDC hdc, HPALETTE hPal, BOOL)
{
  Dc* dc = DcOf(hdc);
  auto* p = As<Palette>(hPal, ObjType::Palette);
  if (!dc || !p) {
    return nullptr;
  }
  Palette* old = dc->palette;
  dc->palette = p;
  UpdateMapping(p);
  return reinterpret_cast<HPALETTE>(old);
}

UINT REB_STDCALL reb::compat::RealizePalette(HDC hdc)
{
  Dc* dc = DcOf(hdc);
  if (!dc) {
    return 0xffffffffu;
  }
  int changed = RealizePaletteIn(dc);
  if (changed && !dc->memory) {
    minwin_user::NotifyPaletteChanged(dc->window);
  }
  return static_cast<UINT>(changed);
}
