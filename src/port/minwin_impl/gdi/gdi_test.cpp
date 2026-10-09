#define MINWIN_IMPLEMENTATION 1
#include "port/minwin/gdi32.h"
#include "port/minwin/user32.h"

#include <string.h>

#include <vector>

#include "../gdi_api.h"
#include "../tests/mw_test.h"

extern int g_paletteNotifications;

namespace {

struct Quad256 {
    RGBQUAD q[256];
};

void SetQuad(RGBQUAD& q, uint32_t rgb) {
    q.rgbRed = (rgb >> 16) & 255;
    q.rgbGreen = (rgb >> 8) & 255;
    q.rgbBlue = rgb & 255;
    q.rgbReserved = 0;
}

HBITMAP MakeDib(int w, int h, const Quad256& table, uint8_t** bits) {
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
    HBITMAP bm = CreateDIBSection(nullptr, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS, &p, nullptr, 0);
    *bits = static_cast<uint8_t*>(p);
    return bm;
}

// The palette code of the game (CoolWindow::SetPalette): copy the system statics to both ends, NOCOLLAPSE the rest.
HPALETTE MakeIdentityPalette(HDC dc, const uint32_t middle[236]) {
    struct {
        WORD version, count;
        PALETTEENTRY entries[256];
    } lp;
    memset(&lp, 0, sizeof lp);
    PALETTEENTRY sys[256];
    SetSystemPaletteUse(dc, SYSPAL_NOSTATIC);
    SetSystemPaletteUse(dc, SYSPAL_STATIC);
    GetSystemPaletteEntries(dc, 0, 256, sys);
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
    HPALETTE p = CreatePalette(reinterpret_cast<LOGPALETTE*>(&lp));
    ResizePalette(p, 256);
    SetPaletteEntries(p, 0, 256, lp.entries);
    return p;
}

HDC ScreenDc(const RECT& visible) {
    minwin_gdi::WindowDcDesc d = {nullptr, 0, 0, &visible, 1};
    return minwin_gdi::CreateWindowDc(d);
}

uint8_t ScreenPixel(int x, int y) { return minwin_gdi::Screen().bits[y * minwin_gdi::Screen().pitch + x]; }

void ClearScreen() { memset(minwin_gdi::Screen().bits, 0, 640 * 480); }

}  // namespace

TEST(device_caps_describe_a_palette_device) {
    HDC dc = CreateCompatibleDC(nullptr);
    CHECK_EQ(GetDeviceCaps(dc, HORZRES), 640);
    CHECK_EQ(GetDeviceCaps(dc, VERTRES), 480);
    CHECK_EQ(GetDeviceCaps(dc, BITSPIXEL), 8);
    CHECK_EQ(GetDeviceCaps(dc, PLANES), 1);
    CHECK_EQ(GetDeviceCaps(dc, NUMCOLORS), 20);
    CHECK_EQ(GetDeviceCaps(dc, SIZEPALETTE), 256);
    CHECK((GetDeviceCaps(dc, RASTERCAPS) & RC_PALETTE) != 0);
    DeleteDC(dc);
    HDC info = CreateICA("DISPLAY", nullptr, nullptr, nullptr);
    CHECK_EQ(GetDeviceCaps(info, HORZRES), 640);
    DeleteDC(info);
}

TEST(system_palette_static_colours_and_use) {
    RECT all = {0, 0, 640, 480};
    HDC dc = ScreenDc(all);
    PALETTEENTRY e[256];
    CHECK_EQ(GetSystemPaletteEntries(dc, 0, 256, e), 256);
    CHECK(e[0].peRed == 0 && e[0].peBlue == 0);
    CHECK(e[7].peRed == 0xc0 && e[7].peGreen == 0xc0);
    CHECK(e[8].peRed == 0xc0 && e[8].peGreen == 0xdc && e[8].peBlue == 0xc0);
    CHECK(e[9].peRed == 0xa6 && e[9].peGreen == 0xca && e[9].peBlue == 0xf0);
    CHECK(e[255].peRed == 255 && e[255].peGreen == 255 && e[255].peBlue == 255);
    CHECK(e[249].peRed == 255 && e[249].peGreen == 0);
    CHECK_EQ(SetSystemPaletteUse(dc, SYSPAL_NOSTATIC), SYSPAL_STATIC);
    CHECK_EQ(SetSystemPaletteUse(dc, SYSPAL_STATIC), SYSPAL_NOSTATIC);
    CHECK_EQ(SetSystemPaletteUse(dc, SYSPAL_STATIC), SYSPAL_STATIC);
    minwin_gdi::DestroyWindowDc(dc);
}

TEST(identity_palette_realizes_to_the_same_indices) {
    RECT all = {0, 0, 640, 480};
    HDC dc = ScreenDc(all);
    uint32_t middle[236];
    for (int i = 0; i < 236; ++i) middle[i] = 0x010203u * (i + 1) + 0x101010u;
    HPALETTE pal = MakeIdentityPalette(dc, middle);
    HPALETTE old = SelectPalette(dc, pal, FALSE);
    int before = g_paletteNotifications;
    UINT changed = RealizePalette(dc);
    CHECK_EQ(changed, 236);
    CHECK_EQ(g_paletteNotifications, before + 1);
    PALETTEENTRY e[256];
    GetSystemPaletteEntries(dc, 0, 256, e);
    for (int i = 10; i < 246; ++i) {
        uint32_t c = middle[i - 10];
        CHECK(e[i].peRed == ((c >> 16) & 255) && e[i].peGreen == ((c >> 8) & 255) && e[i].peBlue == (c & 255));
    }
    uint32_t rgb[256], generation = 0;
    minwin_gdi::CurrentPalette(rgb, &generation);
    CHECK_EQ(rgb[100], middle[90]);
    CHECK_EQ(rgb[249], 0xff0000);
    CHECK_EQ(RealizePalette(dc), 0);  // already realized: nothing changes
    CHECK_EQ(g_paletteNotifications, before + 1);
    SelectPalette(dc, old, FALSE);
    DeleteObject(pal);
    minwin_gdi::DestroyWindowDc(dc);
}

TEST(memory_dib_to_window_dc_is_an_index_copy_with_identity_palette) {
    ClearScreen();
    RECT all = {0, 0, 640, 480};
    HDC screen = ScreenDc(all);
    uint32_t middle[236];
    Quad256 table;
    memset(&table, 0, sizeof table);
    for (int i = 0; i < 236; ++i) middle[i] = 0x020301u * (i + 3) + 0x050505u;
    HPALETTE pal = MakeIdentityPalette(screen, middle);
    SelectPalette(screen, pal, FALSE);
    RealizePalette(screen);
    uint32_t rgb[256], gen;
    minwin_gdi::CurrentPalette(rgb, &gen);
    for (int i = 0; i < 256; ++i) SetQuad(table.q[i], rgb[i]);
    uint8_t* bits;
    HBITMAP dib = MakeDib(16, 8, table, &bits);
    for (int i = 0; i < 16 * 8; ++i) bits[i] = static_cast<uint8_t>(i * 2 + 1);
    HDC mem = CreateCompatibleDC(screen);
    HGDIOBJ old = SelectObject(mem, dib);
    SelectPalette(mem, pal, FALSE);
    CHECK(BitBlt(screen, 100, 50, 16, 8, mem, 0, 0, SRCCOPY));
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 16; ++x) CHECK_EQ(ScreenPixel(100 + x, 50 + 7 - y), bits[y * 16 + x]);
    SelectObject(mem, old);
    DeleteObject(dib);
    DeleteDC(mem);
    SelectPalette(screen, GetStockObject(15) ? static_cast<HPALETTE>(GetStockObject(15)) : nullptr, FALSE);
    DeleteObject(pal);
    minwin_gdi::DestroyWindowDc(screen);
}

TEST(window_dc_origin_and_clip_rects) {
    ClearScreen();
    RECT visible[2] = {{110, 60, 140, 80}, {110, 80, 125, 100}};  // an L-shaped visible region
    minwin_gdi::WindowDcDesc d = {nullptr, 100, 50, visible, 2};
    HDC dc = minwin_gdi::CreateWindowDc(d);
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    RECT r = {0, 0, 100, 100};  // device coordinates relative to the origin (100,50)
    FillRect(dc, &r, brush);
    DeleteObject(brush);
    int painted = 0;
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 640; ++x)
            if (ScreenPixel(x, y)) {
                ++painted;
                bool inside = (x >= 110 && x < 140 && y >= 60 && y < 80) || (x >= 110 && x < 125 && y >= 80 && y < 100);
                CHECK(inside);
            }
    CHECK_EQ(painted, 30 * 20 + 15 * 20);
    CHECK_EQ(ScreenPixel(110, 60), 255);
    minwin_gdi::DestroyWindowDc(dc);
}

TEST(xor_rubber_band_on_window_dc_restores_the_screen) {
    ClearScreen();
    uint8_t* s = minwin_gdi::Screen().bits;
    for (int i = 0; i < 640 * 480; ++i) s[i] = static_cast<uint8_t>(i * 7);
    std::vector<uint8_t> before(s, s + 640 * 480);
    RECT all = {0, 0, 640, 480};
    HDC dc = ScreenDc(all);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    HGDIOBJ oldPen = SelectObject(dc, GetStockObject(WHITE_PEN));
    SetROP2(dc, R2_XORPEN);
    Rectangle(dc, 20, 30, 120, 90);
    CHECK(memcmp(s, before.data(), before.size()) != 0);
    CHECK_EQ(s[30 * 640 + 20], static_cast<uint8_t>(before[30 * 640 + 20] ^ 255));
    CHECK_EQ(s[40 * 640 + 50], before[40 * 640 + 50]);  // interior untouched
    Rectangle(dc, 20, 30, 120, 90);
    CHECK(memcmp(s, before.data(), before.size()) == 0);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    minwin_gdi::DestroyWindowDc(dc);
}

TEST(select_and_delete_object_rules) {
    HDC a = CreateCompatibleDC(nullptr);
    HDC b = CreateCompatibleDC(nullptr);
    Quad256 table;
    memset(&table, 0, sizeof table);
    uint8_t* bits;
    HBITMAP bm = MakeDib(4, 4, table, &bits);
    HGDIOBJ def = SelectObject(a, bm);
    CHECK(def != nullptr);
    CHECK(SelectObject(b, bm) == nullptr);  // already selected elsewhere
    CHECK(!DeleteObject(bm));               // still selected
    CHECK(SelectObject(a, def) == bm);
    CHECK(DeleteObject(bm));
    CHECK(!DeleteObject(bm));  // gone
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(1, 2, 3));
    HGDIOBJ oldPen = SelectObject(a, pen);
    CHECK(oldPen == GetStockObject(BLACK_PEN));
    CHECK(!DeleteObject(pen));
    SelectObject(a, oldPen);
    CHECK(DeleteObject(pen));
    CHECK(DeleteObject(GetStockObject(BLACK_BRUSH)));  // stock objects survive
    CHECK(GetStockObject(BLACK_BRUSH) != nullptr);
    CHECK(GetStockObject(NULL_BRUSH) != GetStockObject(BLACK_BRUSH));
    CHECK(SelectObject(a, nullptr) == nullptr);
    DeleteDC(a);
    DeleteDC(b);
}

TEST(dc_state_setters_return_previous_values) {
    HDC dc = CreateCompatibleDC(nullptr);
    CHECK_EQ(SetTextColor(dc, RGB(1, 2, 3)), 0);
    CHECK_EQ(SetTextColor(dc, RGB(4, 5, 6)), RGB(1, 2, 3));
    CHECK_EQ(SetBkColor(dc, 0x123456), 0xffffff);
    CHECK_EQ(SetBkMode(dc, TRANSPARENT), OPAQUE);
    CHECK_EQ(SetBkMode(dc, OPAQUE), TRANSPARENT);
    CHECK_EQ(SetROP2(dc, R2_XORPEN), 13);
    CHECK_EQ(SetStretchBltMode(dc, 3), 1);
    CHECK_EQ(SetTextAlign(dc, 0), 0);
    POINT p = {-1, -1};
    MoveToEx(dc, 7, 9, nullptr);
    MoveToEx(dc, 1, 1, &p);
    CHECK(p.x == 7 && p.y == 9);
    POINT lp[2] = {{3, 4}, {5, 6}};
    CHECK(DPtoLP(dc, lp, 2));
    CHECK(lp[1].x == 5);
    DeleteDC(dc);
}

TEST(ddb_paths_createcompatiblebitmap_and_createdibitmap) {
    RECT all = {0, 0, 640, 480};
    HDC screen = ScreenDc(all);
    HBITMAP ddb = CreateCompatibleBitmap(screen, 30, 20);
    CHECK(ddb != nullptr);
    BITMAP info;
    CHECK_EQ(GetObjectA(ddb, sizeof info, &info), sizeof info);
    CHECK_EQ(info.bmWidth, 30);
    CHECK_EQ(info.bmHeight, 20);
    CHECK_EQ(info.bmBitsPixel, 8);
    CHECK(info.bmBits == nullptr);  // device-dependent: no visible bits
    HDC mem = CreateCompatibleDC(screen);
    SelectObject(mem, ddb);
    HBRUSH brush = CreateSolidBrush(RGB(255, 255, 255));
    RECT r = {2, 2, 10, 10};
    FillRect(mem, &r, brush);
    DeleteObject(brush);
    ClearScreen();
    BitBlt(screen, 50, 60, 30, 20, mem, 0, 0, SRCCOPY);
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
    for (int i = 0; i < 256; ++i) SetQuad(dibInfo.colors[i], rgb[i]);
    uint8_t pixels[8] = {255, 249, 0, 7, 1, 2, 3, 4};  // bottom row first
    HBITMAP converted = CreateDIBitmap(screen, &dibInfo.header, CBM_INIT, pixels, reinterpret_cast<BITMAPINFO*>(&dibInfo), DIB_RGB_COLORS);
    CHECK(converted != nullptr);
    SelectObject(mem, converted);
    BitBlt(screen, 0, 0, 4, 2, mem, 0, 0, SRCCOPY);
    CHECK_EQ(ScreenPixel(0, 0), 1);  // top row is the second DIB row
    CHECK_EQ(ScreenPixel(3, 0), 4);
    CHECK_EQ(ScreenPixel(0, 1), 255);
    CHECK_EQ(ScreenPixel(3, 1), 7);
    HGDIOBJ def = GetStockObject(BLACK_PEN);
    (void)def;
    DeleteDC(mem);
    DeleteObject(ddb);
    DeleteObject(converted);
    minwin_gdi::DestroyWindowDc(screen);
}

TEST(palette_object_api) {
    struct {
        WORD version, count;
        PALETTEENTRY entries[4];
    } lp = {0x300, 4, {{1, 2, 3, 0}, {4, 5, 6, 0}, {7, 8, 9, 0}, {10, 11, 12, 0}}};
    HPALETTE p = CreatePalette(reinterpret_cast<LOGPALETTE*>(&lp));
    PALETTEENTRY out[8];
    CHECK_EQ(GetPaletteEntries(p, 0, 0, nullptr), 4);
    CHECK_EQ(GetPaletteEntries(p, 1, 8, out), 3);
    CHECK(out[0].peRed == 4);
    CHECK(ResizePalette(p, 6));
    CHECK_EQ(GetPaletteEntries(p, 0, 0, nullptr), 6);
    CHECK_EQ(SetPaletteEntries(p, 4, 4, lp.entries), 2);
    WORD count = 0;
    CHECK_EQ(GetObjectA(p, sizeof count, &count), 2);
    CHECK_EQ(count, 6);
    DeleteObject(p);
}
