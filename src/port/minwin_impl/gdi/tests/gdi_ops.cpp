#include "gdi_ops.h"
#include "canvas.h"

#include <string.h>

#include "port/minwin/gdi32.h"
#include "port/minwin/user32.h"

namespace {

using namespace scenario;

bool Wanted(const char* filter, const char* name) { return !filter || strstr(name, filter); }

#define SCENARIO(NAME)                                                         \
    if (Wanted(filter, #NAME)) {                                               \
        Scenario_##NAME(out);                                                  \
    }

void FillRectWith(Canvas& c, COLORREF color, int l, int t, int r, int b) {
    HBRUSH brush = CreateSolidBrush(color);
    RECT rc = {l, t, r, b};
    FillRect(c.dc, &rc, brush);
    DeleteObject(brush);
}

void Scenario_fill_colors(std::vector<Dump>* out) {
    Canvas c(64, 48);
    c.Fill(5);
    const COLORREF colors[] = {RGB(0x12, 0x34, 0x56), RGB(0, 0xff, 0x80), RGB(0x13, 0x35, 0x57), RGB(200, 100, 50), RGB(0, 0, 0),
                               RGB(255, 255, 255), RGB(255, 0, 0), PALETTERGB(255, 8, 8), PALETTERGB(0x12, 0x34, 0x56),
                               0x01000000 | 22, 0x01000000 | 300, RGB(1, 1, 1), RGB(0xfe, 0xfe, 0xfe), RGB(128, 128, 128)};
    int n = sizeof colors / sizeof colors[0];
    for (int i = 0; i < n; ++i) FillRectWith(c, colors[i], (i % 7) * 9, (i / 7) * 10, (i % 7) * 9 + 8, (i / 7) * 10 + 9);
    FillRectWith(c, RGB(9, 9, 9), -5, 30, 4, 60);  // clipped at the left edge
    FillRectWith(c, RGB(9, 9, 9), 60, 40, 80, 50);  // clipped at the right/bottom edge
    FillRectWith(c, RGB(0, 0, 255), 30, 30, 30, 40);  // empty
    FillRectWith(c, RGB(0, 0, 255), 35, 40, 33, 30);  // inverted: empty
    out->push_back(c.Snapshot("fill_colors"));
}

void Scenario_patblt(std::vector<Dump>* out) {
    Canvas c(64, 48);
    c.Pattern(1);
    HBRUSH brush = CreateSolidBrush(RGB(0x00, 0xff, 0x80));
    HGDIOBJ old = SelectObject(c.dc, brush);
    const DWORD rops[] = {0xf00021 /*PATCOPY*/, 0x42 /*BLACKNESS*/, 0xff0062 /*WHITENESS*/, 0x5a0049 /*PATINVERT*/, 0x550009 /*DSTINVERT*/,
                          0xfa0089 /*PATPAINT*/, 0xa000c9 /*PATAND?*/, 0x0f0001 /*NOTPATCOPY*/};
    for (int i = 0; i < 8; ++i) PatBlt(c.dc, (i % 4) * 16 + 1, (i / 4) * 20 + 2, 14, 18, rops[i]);
    PatBlt(c.dc, 60, 44, 10, 10, 0xf00021);   // clipped
    PatBlt(c.dc, -3, -3, 6, 6, 0xf00021);     // clipped
    PatBlt(c.dc, 40, 30, -10, -5, 0xf00021);  // negative extents
    SelectObject(c.dc, old);
    DeleteObject(brush);
    out->push_back(c.Snapshot("patblt"));
}

void Scenario_bitblt_rops(std::vector<Dump>* out) {
    Canvas dst(96, 48);
    Canvas src(32, 24);
    src.Pattern(3);
    dst.Pattern(9);
    HBRUSH brush = CreateSolidBrush(RGB(0x00, 0xff, 0x80));
    HGDIOBJ old = SelectObject(dst.dc, brush);
    const DWORD rops[] = {0xcc0020 /*SRCCOPY*/, 0xee0086 /*SRCPAINT*/, 0x8800c6 /*SRCAND*/, 0x660046 /*SRCINVERT*/, 0x440328 /*SRCERASE*/,
                          0x330008 /*NOTSRCCOPY*/, 0x1100a6 /*NOTSRCERASE*/, 0xc000ca /*MERGECOPY*/, 0xbb0226 /*MERGEPAINT*/,
                          0xfb0a09 /*PATPAINT*/, 0x5a0049 /*PATINVERT*/, 0xf00021 /*PATCOPY*/};
    for (int i = 0; i < 12; ++i) BitBlt(dst.dc, (i % 6) * 16, (i / 6) * 24, 15, 23, src.dc, (i * 3) % 10, (i * 5) % 7, rops[i]);
    BitBlt(dst.dc, 90, 40, 20, 20, src.dc, 0, 0, 0xcc0020);    // clipped at dst
    BitBlt(dst.dc, 0, 0, 40, 40, src.dc, 20, 10, 0xcc0020);    // source rect extends past the source bitmap
    BitBlt(dst.dc, 50, 10, 10, 10, src.dc, -4, -4, 0xcc0020);  // source origin negative
    SelectObject(dst.dc, old);
    DeleteObject(brush);
    out->push_back(dst.Snapshot("bitblt_rops"));
}

void Scenario_bitblt_other_table(std::vector<Dump>* out) {
    Canvas dst(48, 32, 1);
    Canvas src(48, 32, 2);
    src.Pattern(5);
    dst.Fill(0);
    BitBlt(dst.dc, 0, 0, 48, 32, src.dc, 0, 0, 0xcc0020);
    out->push_back(dst.Snapshot("bitblt_other_table"));
}

void Scenario_bitblt_same_surface(std::vector<Dump>* out) {
    Canvas c(64, 48);
    c.Pattern(7);
    BitBlt(c.dc, 8, 6, 40, 30, c.dc, 0, 0, 0xcc0020);  // overlapping copy
    out->push_back(c.Snapshot("bitblt_same_surface"));
}

void Lines(Canvas& c, HDC dc, int pw, COLORREF color, int style, int rop2, std::vector<Dump>* out, const char* name, bool axisOnly = false) {
    HPEN pen = CreatePen(style, pw, color);
    HGDIOBJ old = SelectObject(dc, pen);
    SetROP2(dc, rop2);
    const int cx = 32, cy = 24;
    const int ring[][2] = {{60, 24}, {58, 36}, {52, 44}, {40, 47}, {32, 47}, {20, 46}, {8, 44}, {2, 36}, {0, 24}, {3, 14}, {10, 4},
                           {22, 1}, {32, 0}, {44, 2}, {55, 8}, {61, 16}, {63, 23}, {57, 30}, {49, 46}, {17, 40}};
    if (!axisOnly)
        for (const auto& p : ring) {
            MoveToEx(dc, cx, cy, nullptr);
            LineTo(dc, p[0], p[1]);
        }
    MoveToEx(dc, 5, 5, nullptr);
    LineTo(dc, 60, 5);
    LineTo(dc, 60, 40);
    LineTo(dc, 5, 40);
    LineTo(dc, 5, 6);
    POINT poly[] = {{10, 10}, {20, 12}, {30, 10}, {50, 30}, {50, 31}, {10, 31}};
    POINT axisPoly[] = {{10, 10}, {20, 10}, {20, 20}, {44, 20}, {44, 12}, {12, 12}};
    if (axisOnly) Polyline(dc, axisPoly, 6);
    else Polyline(dc, poly, 6);
    SelectObject(dc, old);
    DeleteObject(pen);
    out->push_back(c.Snapshot(name));
}

void Scenario_lines_solid(std::vector<Dump>* out) {
    Canvas c(64, 48);
    c.Fill(0);
    Lines(c, c.dc, 1, RGB(0x12, 0x34, 0x56), PS_SOLID, 13, out, "lines_solid");
}

void Scenario_lines_xor(std::vector<Dump>* out) {
    Canvas c(64, 48);
    c.Pattern(2);
    Lines(c, c.dc, 1, RGB(255, 255, 255), PS_SOLID, R2_XORPEN, out, "lines_xor");
}

void Scenario_lines_dot(std::vector<Dump>* out) {
    Canvas c(64, 48);
    c.Fill(3);
    SetBkColor(c.dc, RGB(0x00, 0xff, 0x80));
    Lines(c, c.dc, 1, RGB(0x12, 0x34, 0x56), PS_DOT, 13, out, "lines_dot", true);
}

void Scenario_lines_dot_transparent(std::vector<Dump>* out) {
    Canvas c(64, 48);
    c.Fill(3);
    SetBkMode(c.dc, TRANSPARENT);
    Lines(c, c.dc, 1, RGB(0x12, 0x34, 0x56), PS_DOT, 13, out, "lines_dot_transparent", true);
}

void Scenario_lines_random(std::vector<Dump>* out) {
    Canvas c(128, 96);
    c.Fill(0);
    g_lcg = 99;
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0x12, 0x34, 0x56));
    HGDIOBJ old = SelectObject(c.dc, pen);
    SetROP2(c.dc, R2_XORPEN);
    for (int i = 0; i < 400; ++i) {
        int x0 = Next() % 128, y0 = Next() % 96, x1 = Next() % 128, y1 = Next() % 96;
        if (i % 5 == 0) y1 = y0 + (x1 - x0) / 2;  // exact 2:1 slopes: tie cases
        if (i % 7 == 0) y1 = y0 + (x1 - x0);      // diagonals
        if (i % 11 == 0) y1 = y0 + (x1 - x0) * 3 / 2;
        MoveToEx(c.dc, x0, y0, nullptr);
        LineTo(c.dc, x1, y1);
    }
    SelectObject(c.dc, old);
    DeleteObject(pen);
    out->push_back(c.Snapshot("lines_random"));
}

// 100 independent lines, one per 32x32 tile, COPYPEN (to calibrate the line algorithm against the reference).
void Scenario_lines_tiles(std::vector<Dump>* out) {
    Canvas c(320, 320);
    c.Fill(0);
    g_lcg = 4242;
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0x12, 0x34, 0x56));
    HGDIOBJ old = SelectObject(c.dc, pen);
    for (int i = 0; i < 100; ++i) {
        int ox = (i % 10) * 32, oy = (i / 10) * 32;
        int x0 = Next() % 30 + 1, y0 = Next() % 30 + 1, x1 = Next() % 30 + 1, y1 = Next() % 30 + 1;
        if (i % 4 == 0) y1 = y0 + (x1 - x0) / 2;
        if (i % 4 == 1) x1 = x0 + (y1 - y0) / 2;
        if (y1 < 1) y1 = 1;
        if (y1 > 30) y1 = 30;
        if (x1 < 1) x1 = 1;
        if (x1 > 30) x1 = 30;
        MoveToEx(c.dc, ox + x0, oy + y0, nullptr);
        LineTo(c.dc, ox + x1, oy + y1);
    }
    SelectObject(c.dc, old);
    DeleteObject(pen);
    out->push_back(c.Snapshot("lines_tiles"));
}

void Scenario_rectangles(std::vector<Dump>* out) {
    Canvas c(80, 60);
    c.Fill(1);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(0x12, 0x34, 0x56));
    HBRUSH brush = CreateSolidBrush(RGB(0x00, 0xff, 0x80));
    HGDIOBJ oldPen = SelectObject(c.dc, pen);
    HGDIOBJ oldBrush = SelectObject(c.dc, brush);
    Rectangle(c.dc, 2, 2, 20, 14);   // pen + brush
    Rectangle(c.dc, 22, 2, 24, 14);  // 2 wide
    Rectangle(c.dc, 26, 2, 27, 14);  // 1 wide
    Rectangle(c.dc, 28, 2, 40, 3);   // 1 high
    Rectangle(c.dc, 42, 2, 44, 4);   // 2x2
    Rectangle(c.dc, 60, 14, 50, 4);  // reversed coordinates
    SelectObject(c.dc, GetStockObject(NULL_PEN));
    Rectangle(c.dc, 2, 20, 20, 32);  // brush only
    Rectangle(c.dc, 22, 20, 23, 32);
    Rectangle(c.dc, 26, 20, 28, 22);
    SelectObject(c.dc, pen);
    SelectObject(c.dc, GetStockObject(NULL_BRUSH));
    Rectangle(c.dc, 2, 38, 20, 50);  // pen only
    Rectangle(c.dc, 70, 50, 90, 70);  // clipped
    SelectObject(c.dc, GetStockObject(WHITE_PEN));
    SetROP2(c.dc, R2_XORPEN);
    Rectangle(c.dc, 24, 38, 44, 56);  // the rubber band
    Rectangle(c.dc, 24, 38, 44, 56);  // erased again
    Rectangle(c.dc, 30, 40, 50, 58);
    SelectObject(c.dc, oldPen);
    SelectObject(c.dc, oldBrush);
    DeleteObject(pen);
    DeleteObject(brush);
    out->push_back(c.Snapshot("rectangles"));
}

void Scenario_setdibits(std::vector<Dump>* out) {
    Canvas c(64, 48, 1);
    c.Fill(0);
    struct {
        BITMAPINFOHEADER header;
        RGBQUAD colors[256];
    } info;
    memset(&info, 0, sizeof info);
    Palette256 table = MakeTable(1);
    info.header.biSize = sizeof(BITMAPINFOHEADER);
    info.header.biWidth = 20;
    info.header.biHeight = 16;
    info.header.biPlanes = 1;
    info.header.biBitCount = 8;
    info.header.biClrUsed = 256;
    memcpy(info.colors, table.q, sizeof table.q);
    uint8_t pixels[20 * 16];
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 20; ++x) pixels[y * 20 + x] = static_cast<uint8_t>(10 + x * 3 + y * 11);
    const BITMAPINFO* bmi = reinterpret_cast<BITMAPINFO*>(&info);
    SetDIBitsToDevice(c.dc, 2, 2, 20, 16, 0, 0, 0, 16, pixels, bmi, DIB_RGB_COLORS);
    SetDIBitsToDevice(c.dc, 30, 2, 10, 8, 5, 3, 0, 16, pixels, bmi, DIB_RGB_COLORS);    // sub-rectangle
    SetDIBitsToDevice(c.dc, 50, 40, 20, 16, 0, 0, 0, 16, pixels, bmi, DIB_RGB_COLORS);  // clipped
    SetDIBitsToDevice(c.dc, -6, 20, 20, 16, 0, 0, 0, 16, pixels, bmi, DIB_RGB_COLORS);  // clipped left
    // a DIB with a different colour table, matched to the destination's
    Palette256 other = MakeTable(2);
    memcpy(info.colors, other.q, sizeof other.q);
    SetDIBitsToDevice(c.dc, 30, 20, 20, 16, 0, 0, 0, 16, pixels, bmi, DIB_RGB_COLORS);
    out->push_back(c.Snapshot("setdibits"));
}

void Scenario_setdibits_scan(std::vector<Dump>* out) {
    Canvas c(64, 48, 1);
    c.Fill(0);
    struct {
        BITMAPINFOHEADER header;
        RGBQUAD colors[256];
    } info;
    memset(&info, 0, sizeof info);
    Palette256 table = MakeTable(1);
    info.header.biSize = sizeof(BITMAPINFOHEADER);
    info.header.biWidth = 20;
    info.header.biHeight = 16;
    info.header.biPlanes = 1;
    info.header.biBitCount = 8;
    info.header.biClrUsed = 256;
    memcpy(info.colors, table.q, sizeof table.q);
    uint8_t pixels[20 * 16];
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 20; ++x) pixels[y * 20 + x] = static_cast<uint8_t>(10 + x * 3 + y * 11);
    const BITMAPINFO* bmi = reinterpret_cast<BITMAPINFO*>(&info);
    // only scan lines 4..11 of the 16 are supplied (memory holds those 8 lines)
    int r1 = SetDIBitsToDevice(c.dc, 2, 2, 20, 16, 0, 0, 4, 8, pixels + 4 * 20, bmi, DIB_RGB_COLORS);
    int r2 = SetDIBitsToDevice(c.dc, 30, 2, 20, 6, 0, 2, 0, 16, pixels, bmi, DIB_RGB_COLORS);
    c.Set(0, 0, static_cast<uint8_t>(r1));
    c.Set(1, 0, static_cast<uint8_t>(r2));
    out->push_back(c.Snapshot("setdibits_scan"));
}

}  // namespace

void RunGdiScenarios(const char* filter, std::vector<Dump>* out) {
    SCENARIO(fill_colors)
    SCENARIO(patblt)
    SCENARIO(bitblt_rops)
    SCENARIO(bitblt_other_table)
    SCENARIO(bitblt_same_surface)
    SCENARIO(lines_solid)
    SCENARIO(lines_xor)
    SCENARIO(lines_random)
    SCENARIO(lines_tiles)
    SCENARIO(lines_dot)
    SCENARIO(lines_dot_transparent)
    SCENARIO(rectangles)
    SCENARIO(setdibits)
    SCENARIO(setdibits_scan)
}

