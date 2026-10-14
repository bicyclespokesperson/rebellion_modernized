#define MINWIN_IMPLEMENTATION 1
#include "port/minwin/gdi32.h"
#include "port/minwin/user32.h"

#include <cstdlib>
#include <cstring>

#include "../config/config.h"
#include "../gdi_api.h"
#include "../tests/mw_test.h"
#include "font_files.h"

namespace {

struct Golden {
    int height, weight;
    int tmHeight, ascent, descent, internalLeading, externalLeading, avgWidth, maxWidth, tmWeight;
    int fox, hello, www, rebellion;
    int wrapWidth, wrapHeight;
};

const Golden kGolden[] = {
#include "text_golden.inc"
};

bool FontAvailable() {
    static int available = -1;
    if (available < 0) {
        mw::config::SetGlobalPath("mw_text_test.ini");
        mwtext::SetExtraFontDirectories({"third_party/liberation-fonts-ttf-2.1.5", "../../../third_party/liberation-fonts-ttf-2.1.5"});
        available = !mwtext::FindArial().regular.empty();
        if (!available) printf("  (skipped: no Arial or Liberation Sans found; run tools/fetch_deps.sh)\n");
    }
    return available != 0;
}

HFONT GameFont(int height, int weight) {
    LOGFONTA lf;
    memset(&lf, 0, sizeof lf);
    lf.lfHeight = height;
    lf.lfWeight = weight;
    lf.lfQuality = PROOF_QUALITY;
    strcpy(lf.lfFaceName, "Arial");
    return CreateFontIndirectA(&lf);
}

int Width(HDC dc, const char* s) {
    SIZE sz;
    GetTextExtentPoint32A(dc, s, static_cast<int>(strlen(s)), &sz);
    return static_cast<int>(sz.cx);
}

}  // namespace

TEST(game_fonts_match_wine_metrics_and_extents) {
    if (!FontAvailable()) return;
    HDC dc = CreateCompatibleDC(nullptr);
    for (const Golden& g : kGolden) {
        HFONT font = GameFont(g.height, g.weight);
        HGDIOBJ old = SelectObject(dc, font);
        TEXTMETRICA tm;
        CHECK(GetTextMetricsA(dc, &tm));
        CHECK_EQ(tm.tmHeight, g.tmHeight);
        CHECK_EQ(tm.tmAscent, g.ascent);
        CHECK_EQ(tm.tmDescent, g.descent);
        CHECK_EQ(tm.tmInternalLeading, g.internalLeading);
        CHECK_EQ(tm.tmExternalLeading, g.externalLeading);
        CHECK_EQ(tm.tmAveCharWidth, g.avgWidth);
        CHECK_EQ(tm.tmMaxCharWidth, g.maxWidth);
        CHECK_EQ(tm.tmWeight, g.tmWeight);
        CHECK_EQ(Width(dc, "The quick brown fox jumps over the lazy dog"), g.fox);
        CHECK_EQ(Width(dc, "Hello, World!"), g.hello);
        CHECK_EQ(Width(dc, "WWW iii 0123456789"), g.www);
        CHECK_EQ(Width(dc, "Rebellion"), g.rebellion);
        RECT r = {0, 0, 100, 0};
        int h = DrawTextA(dc, "The quick brown fox jumps over the lazy dog", -1, &r, DT_WORDBREAK | DT_CALCRECT);
        CHECK_EQ(r.right, g.wrapWidth);
        CHECK_EQ(r.bottom, g.wrapHeight);
        CHECK_EQ(h, g.wrapHeight);
        SelectObject(dc, old);
        DeleteObject(font);
    }
    DeleteDC(dc);
}

TEST(textout_renders_into_the_dib_with_the_text_colour) {
    if (!FontAvailable()) return;
    struct {
        BITMAPINFOHEADER header;
        RGBQUAD colors[256];
    } info;
    memset(&info, 0, sizeof info);
    info.header.biSize = sizeof(BITMAPINFOHEADER);
    info.header.biWidth = 100;
    info.header.biHeight = 30;
    info.header.biPlanes = 1;
    info.header.biBitCount = 8;
    info.header.biClrUsed = 256;
    for (int i = 0; i < 256; ++i) info.colors[i].rgbRed = info.colors[i].rgbGreen = info.colors[i].rgbBlue = static_cast<BYTE>(i);
    HDC dc = CreateCompatibleDC(nullptr);
    void* bits = nullptr;
    HBITMAP bm = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBm = SelectObject(dc, bm);
    HFONT font = GameFont(16, 400);
    HGDIOBJ oldFont = SelectObject(dc, font);
    memset(bits, 77, 100 * 30);
    SetTextColor(dc, RGB(200, 200, 200));
    SetBkMode(dc, TRANSPARENT);
    CHECK(TextOutA(dc, 2, 3, "Hi", 2));
    const auto* p = static_cast<const unsigned char*>(bits);
    int text = 0, background = 0, other = 0;
    for (int i = 0; i < 100 * 30; ++i) (p[i] == 200 ? text : p[i] == 77 ? background : other)++;
    CHECK(text > 20);
    CHECK_EQ(other, 0);
    CHECK(background > 2900);
    SetBkMode(dc, OPAQUE);
    SetBkColor(dc, RGB(10, 10, 10));
    TextOutA(dc, 2, 3, "Hi", 2);
    int filled = 0;
    for (int i = 0; i < 100 * 30; ++i) filled += p[i] == 10;
    CHECK(filled > 100);
    SelectObject(dc, oldFont);
    SelectObject(dc, oldBm);
    DeleteObject(font);
    DeleteObject(bm);
    DeleteDC(dc);
}

TEST(offscreen_text_keeps_device_detail_and_raw_dib_writes_invalidate_it) {
    if (!FontAvailable()) return;
    struct {
        BITMAPINFOHEADER header;
        RGBQUAD colors[256];
    } info = {};
    info.header.biSize = sizeof(BITMAPINFOHEADER);
    info.header.biWidth = 80;
    info.header.biHeight = -30;
    info.header.biPlanes = 1;
    info.header.biBitCount = 8;
    info.header.biClrUsed = 256;
    for (int i = 0; i < 256; ++i)
        info.colors[i].rgbRed = info.colors[i].rgbGreen = info.colors[i].rgbBlue = static_cast<BYTE>(i);

    minwin_gdi::DeviceScreen(2);
    HDC memory = CreateCompatibleDC(nullptr);
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(memory, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBitmap = SelectObject(memory, bitmap);
    HFONT font = GameFont(16, 400);
    HGDIOBJ oldFont = SelectObject(memory, font);
    memset(bits, 255, 80 * 30);
    SetTextColor(memory, RGB(0, 0, 0));
    SetBkMode(memory, TRANSPARENT);
    SIZE before, after;
    CHECK(GetTextExtentPoint32A(memory, "Hi", 2, &before));
    CHECK(TextOutA(memory, 2, 2, "Hi", 2));
    CHECK(GetTextExtentPoint32A(memory, "Hi", 2, &after));
    CHECK_EQ(before.cx, after.cx);
    CHECK_EQ(before.cy, after.cy);

    RECT visible = {0, 0, 640, 480};
    minwin_gdi::WindowDcDesc desc = {nullptr, 0, 0, &visible, 1};
    HDC screenDc = minwin_gdi::CreateWindowDc(desc);
    CHECK(BitBlt(screenDc, 0, 0, 80, 30, memory, 0, 0, SRCCOPY));
    const minwin_gdi::ScreenSurface device = minwin_gdi::DeviceScreen(2);
    int detailedBlocks = 0;
    for (int y = 0; y < 30; ++y)
        for (int x = 0; x < 80; ++x) {
            const int xx = x * 2, yy = y * 2;
            const uint8_t a = device.bits[yy * device.pitch + xx];
            detailedBlocks += a != device.bits[yy * device.pitch + xx + 1] ||
                              a != device.bits[(yy + 1) * device.pitch + xx] ||
                              a != device.bits[(yy + 1) * device.pitch + xx + 1];
        }
    CHECK(detailedBlocks > 0);

    minwin_gdi::DeviceBitmapPixels imported;
    CHECK(minwin_gdi::CopyDeviceBitmapPixels(bitmap, imported));
    CHECK_EQ(imported.scale, 2);
    CHECK_EQ(imported.bits.size(), static_cast<size_t>(80 * 30 * 4));
    static_cast<uint8_t*>(bits)[25 * 80 + 70] = 0;
    for (int sy = 0; sy < 2; ++sy)
        for (int sx = 0; sx < 2; ++sx)
            imported.bits[(25 * 2 + sy) * 80 * 2 + 70 * 2 + sx] = 0;
    CHECK(minwin_gdi::SetDeviceBitmapPixels(bitmap, 2, imported.bits, 70, 25, 1, 1) != 0);
    CHECK(BitBlt(screenDc, 0, 0, 80, 30, memory, 0, 0, SRCCOPY));
    CHECK_EQ(device.bits[(25 * 2) * device.pitch + 70 * 2], 0);
    int preservedTextBlocks = 0;
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 60; ++x) {
            const int xx = x * 2, yy = y * 2;
            const uint8_t a = device.bits[yy * device.pitch + xx];
            preservedTextBlocks += a != device.bits[yy * device.pitch + xx + 1] ||
                                   a != device.bits[(yy + 1) * device.pitch + xx] ||
                                   a != device.bits[(yy + 1) * device.pitch + xx + 1];
        }
    CHECK(preservedTextBlocks > 0);

    minwin_gdi::DeviceScreen(4);
    CHECK(BitBlt(screenDc, 0, 0, 80, 30, memory, 0, 0, SRCCOPY));
    const minwin_gdi::ScreenSurface device4 = minwin_gdi::DeviceScreen(4);
    int detailedBlocks4 = 0;
    for (int y = 0; y < 30; ++y)
        for (int x = 0; x < 80; ++x) {
            const int xx = x * 4, yy = y * 4;
            const uint8_t a = device4.bits[yy * device4.pitch + xx];
            for (int sy = 0; sy < 4; ++sy)
                for (int sx = 0; sx < 4; ++sx)
                    detailedBlocks4 += a != device4.bits[(yy + sy) * device4.pitch + xx + sx];
        }
    CHECK(detailedBlocks4 > 0);
    for (int y = 0; y < 30 * 4; ++y)
        for (int x = 79 * 4; x < 80 * 4; ++x)
            CHECK_EQ(device4.bits[y * device4.pitch + x], 255);

    minwin_gdi::DeviceScreen(2);
    CHECK(BitBlt(screenDc, 0, 0, 80, 30, memory, 0, 0, SRCCOPY));
    const minwin_gdi::ScreenSurface device2Again = minwin_gdi::DeviceScreen(2);
    int detailedBlocks2Again = 0;
    for (int y = 0; y < 30; ++y)
        for (int x = 0; x < 80; ++x) {
            const int xx = x * 2, yy = y * 2;
            const uint8_t a = device2Again.bits[yy * device2Again.pitch + xx];
            detailedBlocks2Again += a != device2Again.bits[yy * device2Again.pitch + xx + 1] ||
                                    a != device2Again.bits[(yy + 1) * device2Again.pitch + xx] ||
                                    a != device2Again.bits[(yy + 1) * device2Again.pitch + xx + 1];
        }
    CHECK(detailedBlocks2Again > 0);
    for (int y = 0; y < 30 * 2; ++y)
        for (int x = 79 * 2; x < 80 * 2; ++x)
            CHECK_EQ(device2Again.bits[y * device2Again.pitch + x], 255);

    memset(bits, 255, 80 * 30);
    CHECK(BitBlt(screenDc, 0, 0, 80, 30, memory, 0, 0, SRCCOPY));
    const minwin_gdi::ScreenSurface device2Cleared = minwin_gdi::DeviceScreen(2);
    for (int y = 0; y < 60; ++y)
        for (int x = 0; x < 160; ++x)
            CHECK_EQ(device2Cleared.bits[y * device2Cleared.pitch + x], 255);

    HBRUSH black = CreateSolidBrush(RGB(0, 0, 0));
    RECT small = {70, 25, 71, 26};
    CHECK(FillRect(memory, &small, black));
    CHECK(BitBlt(screenDc, 0, 0, 80, 30, memory, 0, 0, SRCCOPY));
    CHECK_EQ(device2Cleared.bits[(25 * 2) * device2Cleared.pitch + 70 * 2], 0);
    for (int y = 0; y < 60; ++y)
        for (int x = 0; x < 160; ++x)
            if (x < 70 * 2 || x >= 71 * 2 || y < 25 * 2 || y >= 26 * 2)
                CHECK_EQ(device2Cleared.bits[y * device2Cleared.pitch + x], 255);
    DeleteObject(black);

    minwin_gdi::DestroyWindowDc(screenDc);
    SelectObject(memory, oldFont);
    SelectObject(memory, oldBitmap);
    DeleteObject(font);
    DeleteObject(bitmap);
    DeleteDC(memory);
    minwin_gdi::DeviceScreen(1);
}

TEST(drawtext_flag_semantics) {
    if (!FontAvailable()) return;
    HDC dc = CreateCompatibleDC(nullptr);
    HFONT font = GameFont(14, 400);
    SelectObject(dc, font);
    TEXTMETRICA tm;
    GetTextMetricsA(dc, &tm);
    RECT r = {10, 20, 10, 20};
    int h = DrawTextA(dc, "Line one\nLine two\r\nLine three", -1, &r, DT_CALCRECT);
    CHECK_EQ(h, 3 * tm.tmHeight);
    CHECK_EQ(r.bottom, 20 + 3 * tm.tmHeight);
    RECT s = {0, 0, 0, 0};
    DrawTextA(dc, "Line one\nLine two", -1, &s, DT_CALCRECT | DT_SINGLELINE);
    CHECK_EQ(s.bottom, tm.tmHeight);
    RECT amp = {0, 0, 0, 0}, plain = {0, 0, 0, 0};
    DrawTextA(dc, "&Save", -1, &amp, DT_CALCRECT);
    DrawTextA(dc, "&Save", -1, &plain, DT_CALCRECT | DT_NOPREFIX);
    CHECK_EQ(amp.right, Width(dc, "Save"));
    CHECK_EQ(plain.right, Width(dc, "&Save"));
    CHECK_EQ(DrawTextA(dc, "", -1, &amp, 0), 0);
    DeleteDC(dc);
    DeleteObject(font);
}

TEST(font_search_prefers_config_then_system_then_liberation) {
    mw::config::SetGlobalPath("mw_text_font_search.ini");
    mw::config::Global().Set("font_path", "/definitely/not/here/arial.ttf");
    mwtext::SetExtraFontDirectories({});
    mwtext::FontFiles none = mwtext::FindArial();
    CHECK(none.source != "config");
    if (FontAvailable()) {
        mwtext::SetExtraFontDirectories({"third_party/liberation-fonts-ttf-2.1.5", "../../../third_party/liberation-fonts-ttf-2.1.5"});
        mwtext::FontFiles f = mwtext::FindArial();
        CHECK(!f.regular.empty());
        mw::config::Global().Set("font_path", f.regular);
        mwtext::FontFiles configured = mwtext::FindArial();
        CHECK_STR(configured.source, "config");
        CHECK_STR(configured.regular, f.regular);
    }
}
