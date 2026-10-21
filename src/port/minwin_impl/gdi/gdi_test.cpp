#include "port/minwin/gdi32.h"
#include "port/minwin/user32.h"

#include <cstring>

#include <vector>

#include "../gdi_api.h"
#include "../tests/mw_test.h"

extern int g_paletteNotifications;

namespace
{

struct Quad256 {
  RGBQUAD q[256];
};

void SetQuad(RGBQUAD& q, uint32_t rgb)
{
  q.rgbRed = (rgb >> 16) & 255;
  q.rgbGreen = (rgb >> 8) & 255;
  q.rgbBlue = rgb & 255;
  q.rgbReserved = 0;
}

HBITMAP MakeDib(int w, int h, const Quad256& table, uint8_t** bits)
{
  struct {
    BITMAPINFOHEADER header;
    RGBQUAD colors[256];
  } info;
  memset(&info, 0, sizeof info);
  info.header.biSize = sizeof(BITMAPINFOHEADER);
  info.header.biWidth = w;
  info.header.biHeight = h;
  info.header.biPlanes = 1;
  info.header.biBitCount = 8;
  info.header.biClrUsed = 256;
  memcpy(info.colors, table.q, sizeof table.q);
  void* p = nullptr;
  HBITMAP bm = reb::compat::CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS, &p, nullptr, 0);
  *bits = static_cast<uint8_t*>(p);
  return bm;
}

// The palette code of the game (CoolWindow::SetPalette): copy the system statics to both ends, NOCOLLAPSE the rest.
HPALETTE MakeIdentityPalette(HDC dc, const uint32_t middle[236])
{
  struct {
    WORD version, count;
    PALETTEENTRY entries[256];
  } lp;
  memset(&lp, 0, sizeof lp);
  PALETTEENTRY sys[256];
  reb::compat::SetSystemPaletteUse(dc, SYSPAL_NOSTATIC);
  reb::compat::SetSystemPaletteUse(dc, SYSPAL_STATIC);
  reb::compat::GetSystemPaletteEntries(dc, 0, 256, sys);
  for (int i = 0; i < 10; ++i) {
    lp.entries[i] = sys[i];
    lp.entries[246 + i] = sys[246 + i];
  }
  for (int i = 10; i < 246; ++i) {
    lp.entries[i].peRed = (middle[i - 10] >> 16) & 255;
    lp.entries[i].peGreen = (middle[i - 10] >> 8) & 255;
    lp.entries[i].peBlue = middle[i - 10] & 255;
    lp.entries[i].peFlags = PC_NOCOLLAPSE;
  }
  lp.version = 0x300;
  lp.count = 1;
  HPALETTE p = reb::compat::CreatePalette(reinterpret_cast<LOGPALETTE*>(&lp));
  reb::compat::ResizePalette(p, 256);
  reb::compat::SetPaletteEntries(p, 0, 256, lp.entries);
  return p;
}

HDC ScreenDc(const RECT& visible)
{
  minwin_gdi::WindowDcDesc d = {nullptr, 0, 0, &visible, 1};
  return minwin_gdi::CreateWindowDc(d);
}

uint8_t ScreenPixel(int x, int y)
{
  return minwin_gdi::Screen().bits[y * minwin_gdi::Screen().pitch + x];
}

void ClearScreen()
{
  memset(minwin_gdi::Screen().bits, 0, 640 * 480);
}

} // namespace

TEST(device_caps_describe_a_palette_device)
{
  HDC dc = reb::compat::CreateCompatibleDC(nullptr);
  CHECK_EQ(reb::compat::GetDeviceCaps(dc, HORZRES), 640);
  CHECK_EQ(reb::compat::GetDeviceCaps(dc, VERTRES), 480);
  CHECK_EQ(reb::compat::GetDeviceCaps(dc, BITSPIXEL), 8);
  CHECK_EQ(reb::compat::GetDeviceCaps(dc, PLANES), 1);
  CHECK_EQ(reb::compat::GetDeviceCaps(dc, NUMCOLORS), 20);
  CHECK_EQ(reb::compat::GetDeviceCaps(dc, SIZEPALETTE), 256);
  CHECK((reb::compat::GetDeviceCaps(dc, RASTERCAPS) & RC_PALETTE) != 0);
  reb::compat::DeleteDC(dc);
  HDC info = reb::compat::CreateICA("DISPLAY", nullptr, nullptr, nullptr);
  CHECK_EQ(reb::compat::GetDeviceCaps(info, HORZRES), 640);
  reb::compat::DeleteDC(info);
}

TEST(screen_resize_preserves_pixels_and_exposes_wider_canvas)
{
  minwin_gdi::DeviceScreen(1);
  ClearScreen();
  minwin_gdi::Screen().bits[479 * 640 + 639] = 47;
  CHECK(minwin_gdi::SetScreenSize(853, 480));
  const minwin_gdi::ScreenSurface wide = minwin_gdi::Screen();
  CHECK_EQ(wide.width, 853);
  CHECK_EQ(wide.pitch, 853);
  CHECK_EQ(ScreenPixel(639, 479), 47);
  CHECK_EQ(ScreenPixel(852, 479), 0);
  RECT visible = {850, 470, 853, 480};
  HDC dc = ScreenDc(visible);
  CHECK_EQ(reb::compat::GetDeviceCaps(dc, HORZRES), 853);
  HBRUSH brush = reb::compat::CreateSolidBrush(RGB(255, 255, 255));
  CHECK(reb::compat::FillRect(dc, &visible, brush));
  CHECK_EQ(ScreenPixel(852, 479), 255);
  CHECK_EQ(minwin_gdi::DeviceScreen(2).width, 1706);
  reb::compat::DeleteObject(brush);
  minwin_gdi::DestroyWindowDc(dc);
  CHECK(minwin_gdi::SetScreenSize(640, 480));
  CHECK_EQ(ScreenPixel(639, 479), 47);
  CHECK_EQ(minwin_gdi::Screen().pitch, 640);
  minwin_gdi::DeviceScreen(1);
}

TEST(device_bitmap_import_preserves_unedited_detail)
{
  for (int scale : {3, 4}) {
    minwin_gdi::DeviceScreen(scale);
    Quad256 table{};
    uint8_t* bits = nullptr;
    HBITMAP bitmap = MakeDib(4, 2, table, &bits);
    CHECK(bitmap != nullptr);
    memset(bits, 7, 8);
    std::vector<uint8_t> detail(4 * 2 * scale * scale, 7);
    detail[0] = 1;
    CHECK(minwin_gdi::SetDeviceBitmapPixels(bitmap, scale, detail) != 0);
    detail[scale] = 9;
    CHECK(minwin_gdi::SetDeviceBitmapPixels(bitmap, scale, detail, 1, 0, 1, 1) != 0);
    minwin_gdi::DeviceBitmapPixels copied;
    CHECK(minwin_gdi::CopyDeviceBitmapPixels(bitmap, copied));
    CHECK_EQ(copied.scale, scale);
    CHECK_EQ(copied.bits[0], 1);
    CHECK_EQ(copied.bits[scale], 9);
    CHECK_EQ(copied.bits[2 * scale], 7);
    const uint64_t beforeScaleChange = copied.revision;
    minwin_gdi::DeviceScreen(1);
    CHECK_EQ(minwin_gdi::DeviceBitmapRevision(bitmap), beforeScaleChange);
    minwin_gdi::DeviceScreen(scale);
    CHECK(minwin_gdi::CopyDeviceBitmapPixels(bitmap, copied));
    CHECK_EQ(copied.bits[0], 1);
    CHECK_EQ(copied.bits[scale], 9);
    minwin_gdi::DeviceScreen(1);
    bits[4 + 1] = 10;
    minwin_gdi::DeviceScreen(scale);
    CHECK(minwin_gdi::DeviceBitmapRevision(bitmap) > beforeScaleChange);
    CHECK(minwin_gdi::CopyDeviceBitmapPixels(bitmap, copied));
    CHECK_EQ(copied.bits[scale], 10);
    minwin_gdi::InvalidateDeviceBitmap(bitmap);
    CHECK(minwin_gdi::CopyDeviceBitmapPixels(bitmap, copied));
    CHECK_EQ(copied.bits[0], 7);
    reb::compat::DeleteObject(bitmap);
  }
  minwin_gdi::DeviceScreen(1);
}

TEST(device_dib_blit_maps_source_palette_at_both_resolutions)
{
  minwin_gdi::DeviceScreen(1);
  ClearScreen();
  minwin_gdi::DeviceScreen(4);
  RECT visible = {0, 0, 640, 480};
  HDC screen = ScreenDc(visible);
  reb::compat::SetSystemPaletteUse(screen, SYSPAL_STATIC);
  Quad256 table{};
  SetQuad(table.q[1], 0xff0000);
  SetQuad(table.q[2], 0x0000ff);
  uint8_t* bits = nullptr;
  HBITMAP bitmap = MakeDib(2, 1, table, &bits);
  CHECK(bitmap != nullptr);
  bits[0] = bits[1] = 1;
  std::vector<uint8_t> detail(2 * 4 * 4, 1);
  detail[0] = 2;
  CHECK(minwin_gdi::SetDeviceBitmapPixels(bitmap, 4, detail) != 0);
  HDC memory = reb::compat::CreateCompatibleDC(screen);
  HGDIOBJ oldBitmap = reb::compat::SelectObject(memory, bitmap);
  CHECK(reb::compat::BitBlt(screen, 0, 0, 2, 1, memory, 0, 0, SRCCOPY));
  uint32_t palette[256];
  minwin_gdi::CurrentPalette(palette, nullptr);
  CHECK_EQ(palette[ScreenPixel(0, 0)], 0xff0000u);
  const minwin_gdi::ScreenSurface device = minwin_gdi::DeviceScreen(4);
  CHECK_EQ(palette[device.bits[0]], 0x0000ffu);
  CHECK_EQ(palette[device.bits[1]], 0xff0000u);
  reb::compat::SelectObject(memory, oldBitmap);
  reb::compat::DeleteObject(bitmap);
  reb::compat::DeleteDC(memory);
  minwin_gdi::DestroyWindowDc(screen);
  minwin_gdi::DeviceScreen(1);
}

TEST(device_screen_scales_clipped_drawing_without_changing_logical_readback)
{
  ClearScreen();
  minwin_gdi::DeviceScreen(4);
  RECT visible = {10, 10, 12, 12};
  HDC screen = ScreenDc(visible);
  HBRUSH white = reb::compat::CreateSolidBrush(RGB(255, 255, 255));
  RECT fill = {9, 9, 13, 13};
  CHECK(reb::compat::FillRect(screen, &fill, white));
  CHECK_EQ(ScreenPixel(10, 10), 255);
  CHECK_EQ(ScreenPixel(9, 10), 0);
  const minwin_gdi::ScreenSurface device = minwin_gdi::DeviceScreen(4);
  CHECK_EQ(device.width, 2560);
  CHECK_EQ(device.height, 1920);
  for (int y = 40; y < 48; ++y) {
    for (int x = 40; x < 48; ++x) {
      CHECK_EQ(device.bits[y * device.pitch + x], 255);
    }
  }
  CHECK_EQ(device.bits[40 * device.pitch + 39], 0);

  HDC memory = reb::compat::CreateCompatibleDC(screen);
  HBITMAP bitmap = reb::compat::CreateCompatibleBitmap(screen, 4, 4);
  HGDIOBJ oldBitmap = reb::compat::SelectObject(memory, bitmap);
  CHECK(reb::compat::BitBlt(memory, 0, 0, 4, 4, screen, 9, 9, SRCCOPY));
  RECT all = {0, 0, 640, 480};
  HDC readback = ScreenDc(all);
  CHECK(reb::compat::BitBlt(readback, 20, 20, 4, 4, memory, 0, 0, SRCCOPY));
  CHECK_EQ(ScreenPixel(21, 21), 255);
  CHECK_EQ(ScreenPixel(20, 20), 0);
  minwin_gdi::DestroyWindowDc(readback);
  reb::compat::SelectObject(memory, oldBitmap);
  reb::compat::DeleteObject(bitmap);
  reb::compat::DeleteDC(memory);
  reb::compat::DeleteObject(white);
  minwin_gdi::DestroyWindowDc(screen);
  minwin_gdi::DeviceScreen(1);
}

TEST(device_line_uses_subpixel_geometry_and_xor_restores_it)
{
  ClearScreen();
  minwin_gdi::DeviceScreen(1);
  minwin_gdi::DeviceScreen(4);
  RECT all = {0, 0, 640, 480};
  HDC dc = ScreenDc(all);
  HPEN pen = reb::compat::CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
  HGDIOBJ oldPen = reb::compat::SelectObject(dc, pen);
  reb::compat::SetROP2(dc, R2_XORPEN);
  CHECK(reb::compat::MoveToEx(dc, 2, 2, nullptr));
  CHECK(reb::compat::LineTo(dc, 14, 8));
  const minwin_gdi::ScreenSurface device = minwin_gdi::DeviceScreen(4);
  int detailedBlocks = 0, lit = 0;
  for (int y = 0; y < 12; ++y) {
    for (int x = 0; x < 18; ++x) {
      const int xx = x * 4, yy = y * 4;
      const uint8_t a = device.bits[yy * device.pitch + xx];
      for (int sy = 0; sy < 4; ++sy) {
        for (int sx = 0; sx < 4; ++sx) {
          const uint8_t pixel = device.bits[(yy + sy) * device.pitch + xx + sx];
          detailedBlocks += pixel != a;
          lit += pixel != 0;
        }
      }
    }
  }
  CHECK(lit > 0);
  CHECK(detailedBlocks > 0);
  CHECK(reb::compat::MoveToEx(dc, 2, 2, nullptr));
  CHECK(reb::compat::LineTo(dc, 14, 8));
  for (int y = 0; y < 48; ++y) {
    for (int x = 0; x < 72; ++x) {
      CHECK_EQ(device.bits[y * device.pitch + x], 0);
    }
  }
  CHECK_EQ(ScreenPixel(2, 2), 0);
  RECT shiftedClip = {30, 40, 60, 60};
  minwin_gdi::WindowDcDesc shiftedDesc = {nullptr, 30, 40, &shiftedClip, 1};
  HDC shifted = minwin_gdi::CreateWindowDc(shiftedDesc);
  reb::compat::SelectObject(shifted, pen);
  CHECK(reb::compat::MoveToEx(shifted, 2, 2, nullptr));
  CHECK(reb::compat::LineTo(shifted, 14, 8));
  CHECK_EQ(ScreenPixel(32, 42), 255);
  CHECK_EQ(device.bits[(42 * 4) * device.pitch + 32 * 4], 255);
  CHECK_EQ(device.bits[(2 * 4) * device.pitch + 2 * 4], 0);
  minwin_gdi::DestroyWindowDc(shifted);

  HDC memory = reb::compat::CreateCompatibleDC(dc);
  HBITMAP bitmap = reb::compat::CreateCompatibleBitmap(dc, 32, 32);
  HGDIOBJ oldBitmap = reb::compat::SelectObject(memory, bitmap);
  reb::compat::SelectObject(memory, pen);
  CHECK(reb::compat::MoveToEx(memory, 2, 2, nullptr));
  CHECK(reb::compat::LineTo(memory, 14, 8));
  CHECK(reb::compat::BitBlt(dc, 100, 100, 32, 32, memory, 0, 0, SRCCOPY));
  int offscreenDetail = 0;
  for (int y = 100; y < 116; ++y) {
    for (int x = 100; x < 118; ++x) {
      const int xx = x * 4, yy = y * 4;
      const uint8_t a = device.bits[yy * device.pitch + xx];
      for (int sy = 0; sy < 4; ++sy) {
        for (int sx = 0; sx < 4; ++sx) {
          offscreenDetail += a != device.bits[(yy + sy) * device.pitch + xx + sx];
        }
      }
    }
  }
  CHECK(offscreenDetail > 0);
  reb::compat::SelectObject(memory, oldBitmap);
  reb::compat::DeleteObject(bitmap);
  reb::compat::DeleteDC(memory);
  reb::compat::SelectObject(dc, oldPen);
  reb::compat::DeleteObject(pen);
  minwin_gdi::DestroyWindowDc(dc);
  minwin_gdi::DeviceScreen(1);
}

TEST(system_palette_static_colours_and_use)
{
  RECT all = {0, 0, 640, 480};
  HDC dc = ScreenDc(all);
  PALETTEENTRY e[256];
  CHECK_EQ(reb::compat::GetSystemPaletteEntries(dc, 0, 256, e), 256);
  CHECK(e[0].peRed == 0 && e[0].peBlue == 0);
  CHECK(e[7].peRed == 0xc0 && e[7].peGreen == 0xc0);
  CHECK(e[8].peRed == 0xc0 && e[8].peGreen == 0xdc && e[8].peBlue == 0xc0);
  CHECK(e[9].peRed == 0xa6 && e[9].peGreen == 0xca && e[9].peBlue == 0xf0);
  CHECK(e[255].peRed == 255 && e[255].peGreen == 255 && e[255].peBlue == 255);
  CHECK(e[249].peRed == 255 && e[249].peGreen == 0);
  CHECK_EQ(reb::compat::SetSystemPaletteUse(dc, SYSPAL_NOSTATIC), SYSPAL_STATIC);
  CHECK_EQ(reb::compat::SetSystemPaletteUse(dc, SYSPAL_STATIC), SYSPAL_NOSTATIC);
  CHECK_EQ(reb::compat::SetSystemPaletteUse(dc, SYSPAL_STATIC), SYSPAL_STATIC);
  minwin_gdi::DestroyWindowDc(dc);
}

TEST(identity_palette_realizes_to_the_same_indices)
{
  RECT all = {0, 0, 640, 480};
  HDC dc = ScreenDc(all);
  uint32_t middle[236];
  for (int i = 0; i < 236; ++i) {
    middle[i] = 0x010203u * (i + 1) + 0x101010u;
  }
  HPALETTE pal = MakeIdentityPalette(dc, middle);
  HPALETTE old = reb::compat::SelectPalette(dc, pal, FALSE);
  int before = g_paletteNotifications;
  UINT changed = reb::compat::RealizePalette(dc);
  CHECK_EQ(changed, 236);
  CHECK_EQ(g_paletteNotifications, before + 1);
  PALETTEENTRY e[256];
  reb::compat::GetSystemPaletteEntries(dc, 0, 256, e);
  for (int i = 10; i < 246; ++i) {
    uint32_t c = middle[i - 10];
    CHECK(e[i].peRed == ((c >> 16) & 255) && e[i].peGreen == ((c >> 8) & 255) && e[i].peBlue == (c & 255));
  }
  uint32_t rgb[256], generation = 0;
  minwin_gdi::CurrentPalette(rgb, &generation);
  CHECK_EQ(rgb[100], middle[90]);
  CHECK_EQ(rgb[249], 0xff0000);
  CHECK_EQ(reb::compat::RealizePalette(dc), 0); // already realized: nothing changes
  CHECK_EQ(g_paletteNotifications, before + 1);
  reb::compat::SelectPalette(dc, old, FALSE);
  reb::compat::DeleteObject(pal);
  minwin_gdi::DestroyWindowDc(dc);
}

TEST(memory_dib_to_window_dc_is_an_index_copy_with_identity_palette)
{
  ClearScreen();
  RECT all = {0, 0, 640, 480};
  HDC screen = ScreenDc(all);
  uint32_t middle[236];
  Quad256 table;
  memset(&table, 0, sizeof table);
  for (int i = 0; i < 236; ++i) {
    middle[i] = 0x020301u * (i + 3) + 0x050505u;
  }
  HPALETTE pal = MakeIdentityPalette(screen, middle);
  reb::compat::SelectPalette(screen, pal, FALSE);
  reb::compat::RealizePalette(screen);
  uint32_t rgb[256], gen;
  minwin_gdi::CurrentPalette(rgb, &gen);
  for (int i = 0; i < 256; ++i) {
    SetQuad(table.q[i], rgb[i]);
  }
  uint8_t* bits;
  HBITMAP dib = MakeDib(16, 8, table, &bits);
  for (int i = 0; i < 16 * 8; ++i) {
    bits[i] = static_cast<uint8_t>(i * 2 + 1);
  }
  HDC mem = reb::compat::CreateCompatibleDC(screen);
  HGDIOBJ old = reb::compat::SelectObject(mem, dib);
  reb::compat::SelectPalette(mem, pal, FALSE);
  CHECK(reb::compat::BitBlt(screen, 100, 50, 16, 8, mem, 0, 0, SRCCOPY));
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 16; ++x) {
      CHECK_EQ(ScreenPixel(100 + x, 50 + 7 - y), bits[y * 16 + x]);
    }
  }
  reb::compat::SelectObject(mem, old);
  reb::compat::DeleteObject(dib);
  reb::compat::DeleteDC(mem);
  reb::compat::SelectPalette(screen, reb::compat::GetStockObject(15) ? static_cast<HPALETTE>(reb::compat::GetStockObject(15)) : nullptr, FALSE);
  reb::compat::DeleteObject(pal);
  minwin_gdi::DestroyWindowDc(screen);
}

TEST(window_dc_origin_and_clip_rects)
{
  ClearScreen();
  RECT visible[2] = {{110, 60, 140, 80}, {110, 80, 125, 100}}; // an L-shaped visible region
  minwin_gdi::WindowDcDesc d = {nullptr, 100, 50, visible, 2};
  HDC dc = minwin_gdi::CreateWindowDc(d);
  HBRUSH brush = reb::compat::CreateSolidBrush(RGB(255, 255, 255));
  RECT r = {0, 0, 100, 100}; // device coordinates relative to the origin (100,50)
  reb::compat::FillRect(dc, &r, brush);
  reb::compat::DeleteObject(brush);
  int painted = 0;
  for (int y = 0; y < 480; ++y) {
    for (int x = 0; x < 640; ++x) {
      if (ScreenPixel(x, y)) {
        ++painted;
        bool inside = (x >= 110 && x < 140 && y >= 60 && y < 80) || (x >= 110 && x < 125 && y >= 80 && y < 100);
        CHECK(inside);
      }
    }
  }
  CHECK_EQ(painted, 30 * 20 + 15 * 20);
  CHECK_EQ(ScreenPixel(110, 60), 255);
  minwin_gdi::DestroyWindowDc(dc);
}

TEST(xor_rubber_band_on_window_dc_restores_the_screen)
{
  ClearScreen();
  uint8_t* s = minwin_gdi::Screen().bits;
  for (int i = 0; i < 640 * 480; ++i) {
    s[i] = static_cast<uint8_t>(i * 7);
  }
  std::vector<uint8_t> before(s, s + 640 * 480);
  RECT all = {0, 0, 640, 480};
  HDC dc = ScreenDc(all);
  HGDIOBJ oldBrush = reb::compat::SelectObject(dc, reb::compat::GetStockObject(NULL_BRUSH));
  HGDIOBJ oldPen = reb::compat::SelectObject(dc, reb::compat::GetStockObject(WHITE_PEN));
  reb::compat::SetROP2(dc, R2_XORPEN);
  reb::compat::Rectangle(dc, 20, 30, 120, 90);
  CHECK(memcmp(s, before.data(), before.size()) != 0);
  CHECK_EQ(s[30 * 640 + 20], static_cast<uint8_t>(before[30 * 640 + 20] ^ 255));
  CHECK_EQ(s[40 * 640 + 50], before[40 * 640 + 50]); // interior untouched
  reb::compat::Rectangle(dc, 20, 30, 120, 90);
  CHECK(memcmp(s, before.data(), before.size()) == 0);
  reb::compat::SelectObject(dc, oldPen);
  reb::compat::SelectObject(dc, oldBrush);
  minwin_gdi::DestroyWindowDc(dc);
}

TEST(select_and_delete_object_rules)
{
  HDC a = reb::compat::CreateCompatibleDC(nullptr);
  HDC b = reb::compat::CreateCompatibleDC(nullptr);
  Quad256 table;
  memset(&table, 0, sizeof table);
  uint8_t* bits;
  HBITMAP bm = MakeDib(4, 4, table, &bits);
  HGDIOBJ def = reb::compat::SelectObject(a, bm);
  CHECK(def != nullptr);
  CHECK(reb::compat::SelectObject(b, bm) == nullptr); // already selected elsewhere
  CHECK(!reb::compat::DeleteObject(bm));              // still selected
  CHECK(reb::compat::SelectObject(a, def) == bm);
  CHECK(reb::compat::DeleteObject(bm));
  CHECK(!reb::compat::DeleteObject(bm)); // gone
  HPEN pen = reb::compat::CreatePen(PS_SOLID, 1, RGB(1, 2, 3));
  HGDIOBJ oldPen = reb::compat::SelectObject(a, pen);
  CHECK(oldPen == reb::compat::GetStockObject(BLACK_PEN));
  CHECK(!reb::compat::DeleteObject(pen));
  reb::compat::SelectObject(a, oldPen);
  CHECK(reb::compat::DeleteObject(pen));
  CHECK(reb::compat::DeleteObject(reb::compat::GetStockObject(BLACK_BRUSH))); // stock objects survive
  CHECK(reb::compat::GetStockObject(BLACK_BRUSH) != nullptr);
  CHECK(reb::compat::GetStockObject(NULL_BRUSH) != reb::compat::GetStockObject(BLACK_BRUSH));
  CHECK(reb::compat::SelectObject(a, nullptr) == nullptr);
  reb::compat::DeleteDC(a);
  reb::compat::DeleteDC(b);
}

TEST(dc_state_setters_return_previous_values)
{
  HDC dc = reb::compat::CreateCompatibleDC(nullptr);
  CHECK_EQ(reb::compat::SetTextColor(dc, RGB(1, 2, 3)), 0);
  CHECK_EQ(reb::compat::SetTextColor(dc, RGB(4, 5, 6)), RGB(1, 2, 3));
  CHECK_EQ(reb::compat::SetBkColor(dc, 0x123456), 0xffffff);
  CHECK_EQ(reb::compat::SetBkMode(dc, TRANSPARENT), OPAQUE);
  CHECK_EQ(reb::compat::SetBkMode(dc, OPAQUE), TRANSPARENT);
  CHECK_EQ(reb::compat::SetROP2(dc, R2_XORPEN), 13);
  CHECK_EQ(reb::compat::SetStretchBltMode(dc, 3), 1);
  CHECK_EQ(reb::compat::SetTextAlign(dc, 0), 0);
  POINT p = {-1, -1};
  reb::compat::MoveToEx(dc, 7, 9, nullptr);
  reb::compat::MoveToEx(dc, 1, 1, &p);
  CHECK(p.x == 7 && p.y == 9);
  POINT lp[2] = {{3, 4}, {5, 6}};
  CHECK(reb::compat::DPtoLP(dc, lp, 2));
  CHECK(lp[1].x == 5);
  reb::compat::DeleteDC(dc);
}

TEST(ddb_paths_createcompatiblebitmap_and_createdibitmap)
{
  RECT all = {0, 0, 640, 480};
  HDC screen = ScreenDc(all);
  HBITMAP ddb = reb::compat::CreateCompatibleBitmap(screen, 30, 20);
  CHECK(ddb != nullptr);
  BITMAP info;
  CHECK_EQ(reb::compat::GetObjectA(ddb, sizeof info, &info), sizeof info);
  CHECK_EQ(info.bmWidth, 30);
  CHECK_EQ(info.bmHeight, 20);
  CHECK_EQ(info.bmBitsPixel, 8);
  CHECK(info.bmBits == nullptr); // device-dependent: no visible bits
  HDC mem = reb::compat::CreateCompatibleDC(screen);
  reb::compat::SelectObject(mem, ddb);
  HBRUSH brush = reb::compat::CreateSolidBrush(RGB(255, 255, 255));
  RECT r = {2, 2, 10, 10};
  reb::compat::FillRect(mem, &r, brush);
  reb::compat::DeleteObject(brush);
  ClearScreen();
  reb::compat::BitBlt(screen, 50, 60, 30, 20, mem, 0, 0, SRCCOPY);
  CHECK_EQ(ScreenPixel(52, 62), 255);
  CHECK_EQ(ScreenPixel(51, 62), 0);

  // CreateDIBitmap with a colour table that is the system palette: indices copy straight through
  struct {
    BITMAPINFOHEADER header;
    RGBQUAD colors[256];
  } dibInfo;
  memset(&dibInfo, 0, sizeof dibInfo);
  uint32_t rgb[256], gen;
  minwin_gdi::CurrentPalette(rgb, &gen);
  dibInfo.header.biSize = sizeof(BITMAPINFOHEADER);
  dibInfo.header.biWidth = 4;
  dibInfo.header.biHeight = 2;
  dibInfo.header.biPlanes = 1;
  dibInfo.header.biBitCount = 8;
  dibInfo.header.biClrUsed = 256;
  for (int i = 0; i < 256; ++i) {
    SetQuad(dibInfo.colors[i], rgb[i]);
  }
  uint8_t pixels[8] = {255, 249, 0, 7, 1, 2, 3, 4}; // bottom row first
  HBITMAP converted = reb::compat::CreateDIBitmap(screen, &dibInfo.header, CBM_INIT, pixels, reinterpret_cast<BITMAPINFO*>(&dibInfo),
                                     DIB_RGB_COLORS);
  CHECK(converted != nullptr);
  reb::compat::SelectObject(mem, converted);
  reb::compat::BitBlt(screen, 0, 0, 4, 2, mem, 0, 0, SRCCOPY);
  CHECK_EQ(ScreenPixel(0, 0), 1); // top row is the second DIB row
  CHECK_EQ(ScreenPixel(3, 0), 4);
  CHECK_EQ(ScreenPixel(0, 1), 255);
  CHECK_EQ(ScreenPixel(3, 1), 7);
  HGDIOBJ def = reb::compat::GetStockObject(BLACK_PEN);
  (void)def;
  reb::compat::DeleteDC(mem);
  reb::compat::DeleteObject(ddb);
  reb::compat::DeleteObject(converted);
  minwin_gdi::DestroyWindowDc(screen);
}

TEST(palette_object_api)
{
  struct {
    WORD version, count;
    PALETTEENTRY entries[4];
  } lp = {0x300, 4, {{1, 2, 3, 0}, {4, 5, 6, 0}, {7, 8, 9, 0}, {10, 11, 12, 0}}};
  HPALETTE p = reb::compat::CreatePalette(reinterpret_cast<LOGPALETTE*>(&lp));
  PALETTEENTRY out[8];
  CHECK_EQ(reb::compat::GetPaletteEntries(p, 0, 0, nullptr), 4);
  CHECK_EQ(reb::compat::GetPaletteEntries(p, 1, 8, out), 3);
  CHECK(out[0].peRed == 4);
  CHECK(reb::compat::ResizePalette(p, 6));
  CHECK_EQ(reb::compat::GetPaletteEntries(p, 0, 0, nullptr), 6);
  CHECK_EQ(reb::compat::SetPaletteEntries(p, 4, 4, lp.entries), 2);
  WORD count = 0;
  CHECK_EQ(reb::compat::GetObjectA(p, sizeof count, &count), 2);
  CHECK_EQ(count, 6);
  reb::compat::DeleteObject(p);
}
