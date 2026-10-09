#define MINWIN_IMPLEMENTATION 1
#include "port/minwin/gdi32.h"
#include "port/minwin/user32.h"

#include <stdlib.h>
#include <string.h>

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
    const unsigned char* p = static_cast<const unsigned char*>(bits);
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
