// Shared by the GDI and text operation scenarios: an 8-bit DIB section selected into a memory DC, with a deterministic
// colour table (statics at both ends like the game's identity palette).
#pragma once
#include <string.h>

#include <string>
#include <vector>

#include "gdi_ops.h"
#include "port/minwin/gdi32.h"
#include "port/minwin/user32.h"

namespace scenario {

struct Palette256 {
    RGBQUAD q[256];
};

inline uint32_t g_lcg = 12345;
inline uint32_t Next() {
    g_lcg = g_lcg * 1664525u + 1013904223u;
    return g_lcg >> 8;
}

const uint32_t kStatics[20] = {0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0, 0xc0dcc0, 0xa6caf0,
                               0xfffbf0, 0xa0a0a4, 0x808080, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff};

inline void SetQuad(RGBQUAD& q, uint32_t rgb) {
    q.rgbRed = (rgb >> 16) & 255;
    q.rgbGreen = (rgb >> 8) & 255;
    q.rgbBlue = rgb & 255;
    q.rgbReserved = 0;
}

// Identity-style table: statics at both ends, known colours at 20-25, pseudo-random in between.
inline Palette256 MakeTable(uint32_t seed) {
    g_lcg = seed;
    Palette256 p;
    memset(&p, 0, sizeof p);
    for (int i = 0; i < 10; ++i) {
        SetQuad(p.q[i], kStatics[i]);
        SetQuad(p.q[246 + i], kStatics[10 + i]);
    }
    for (int i = 10; i < 246; ++i) SetQuad(p.q[i], Next() & 0xffffff);
    SetQuad(p.q[20], 0x123456);
    SetQuad(p.q[21], 0x00ff80);
    SetQuad(p.q[22], 0xff8000);
    SetQuad(p.q[23], 0x0a141e);
    SetQuad(p.q[24], 0xfefefe);
    SetQuad(p.q[25], 0x010101);
    return p;
}

struct Canvas {
    HDC dc = nullptr;
    HBITMAP bitmap = nullptr, old = nullptr;
    uint8_t* bits = nullptr;
    int w = 0, h = 0, stride = 0;
    Palette256 table;

    Canvas(int width, int height, uint32_t tableSeed = 1) : w(width), h(height) {
        table = MakeTable(tableSeed);
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
        dc = CreateCompatibleDC(nullptr);
        void* p = nullptr;
        bitmap = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&info), DIB_RGB_COLORS, &p, nullptr, 0);
        bits = static_cast<uint8_t*>(p);
        stride = (w + 3) & ~3;
        old = static_cast<HBITMAP>(SelectObject(dc, bitmap));
    }
    ~Canvas() {
        SelectObject(dc, old);
        DeleteObject(bitmap);
        DeleteDC(dc);
    }
    void Fill(uint8_t v) { memset(bits, v, static_cast<size_t>(stride) * h); }
    void Set(int x, int y, uint8_t v) { bits[(h - 1 - y) * stride + x] = v; }
    // a deterministic busy pattern
    void Pattern(int seed) {
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) Set(x, y, static_cast<uint8_t>(((x * 7 + y * 13 + seed) ^ (x * y)) & 255));
    }
    Dump Snapshot(const char* name) const {
        Dump d;
        d.name = name;
        d.width = w;
        d.height = h;
        d.pixels.resize(static_cast<size_t>(w) * h);
        for (int y = 0; y < h; ++y) memcpy(&d.pixels[static_cast<size_t>(y) * w], bits + (h - 1 - y) * stride, w);
        return d;
    }
};


}  // namespace scenario
