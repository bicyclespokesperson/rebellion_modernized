// THROWAWAY stand-in for the GDI component (src/port/minwin_impl/gdi) so that the window manager can be linked and exercised before
// that component lands. It implements gdi_api.h and the gdi32 entry points just far enough to see windows and their 8-bit DIB
// blits on screen: DIB sections, memory DCs, SRCCOPY BitBlt/SetDIBitsToDevice, PatBlt/FillRect, palettes with an identity
// RealizePalette. Text and lines are accepted and ignored. Selected with REB_GDI_STUB=ON; delete when the real component is merged.
#define MINWIN_IMPLEMENTATION 1
#include <stdlib.h>
#include <string.h>

#include "port/minwin_impl/gdi_api.h"

namespace {

enum Kind { kDc = 1, kBitmap, kBrush, kPen, kFont, kPalette, kOther };

struct Object {
    int kind;
};
struct Bitmap {
    int kind;
    int width, height, bpp, pitch;
    bool topDown;
    uint8_t* bits;
};
struct Brush {
    int kind;
    COLORREF color;
};
struct Palette {
    int kind;
    uint32_t rgb[256];
    int count;
};
struct Dc {
    int kind;
    bool memory;
    HWND window;
    int originX, originY;
    RECT* clip;
    int clipCount;
    Bitmap* bitmap;
    Palette* palette;
    Brush* brush;
};

uint8_t g_screen[minwin_gdi::kScreenWidth * minwin_gdi::kScreenHeight];
uint32_t g_systemPalette[256];
uint32_t g_generation = 1;
bool g_initialised = false;
bool g_noStatic = false;
Object g_stockOther = {kOther};
Palette g_defaultPalette = {kPalette, {0}, 0};
Brush g_stockBrushes[6] = {{kBrush, 0xffffff}, {kBrush, 0}, {kBrush, 0x808080}, {kBrush, 0}, {kBrush, 0}, {kBrush, 0}};

const uint32_t kStaticLow[10] = {0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0, 0xc0dcc0, 0xa6caf0};
const uint32_t kStaticHigh[10] = {0xfffbf0, 0xa0a0a4, 0x808080, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff};

void Init() {
    if (g_initialised) return;
    g_initialised = true;
    for (int i = 0; i < 10; ++i) {
        g_systemPalette[i] = kStaticLow[i];
        g_systemPalette[246 + i] = kStaticHigh[i];
    }
}

RECT Rect(int l, int t, int r, int b) {
    RECT x;
    x.left = l; x.top = t; x.right = r; x.bottom = b;
    return x;
}

bool IntersectRects(RECT* out, const RECT& a, const RECT& b) {
    out->left = a.left > b.left ? a.left : b.left;
    out->top = a.top > b.top ? a.top : b.top;
    out->right = a.right < b.right ? a.right : b.right;
    out->bottom = a.bottom < b.bottom ? a.bottom : b.bottom;
    return out->left < out->right && out->top < out->bottom;
}

uint8_t NearestIndex(COLORREF c) {
    const int r = c & 0xff, g = (c >> 8) & 0xff, b = (c >> 16) & 0xff;
    int best = 0, bestDistance = 1 << 30;
    for (int i = 0; i < 256; ++i) {
        const int pr = (g_systemPalette[i] >> 16) & 0xff, pg = (g_systemPalette[i] >> 8) & 0xff, pb = g_systemPalette[i] & 0xff;
        const int d = (pr - r) * (pr - r) + (pg - g) * (pg - g) + (pb - b) * (pb - b);
        if (d < bestDistance) { bestDistance = d; best = i; }
    }
    return static_cast<uint8_t>(best);
}

Dc* ToDc(HDC h) { return reinterpret_cast<Dc*>(h); }

struct Target {
    uint8_t* bits;
    int pitch, width, height;
    bool topDown;
};

bool TargetOf(const Dc* dc, Target* t) {
    if (dc->memory) {
        if (!dc->bitmap || !dc->bitmap->bits || dc->bitmap->bpp != 8) return false;
        t->bits = dc->bitmap->bits;
        t->pitch = dc->bitmap->pitch;
        t->width = dc->bitmap->width;
        t->height = dc->bitmap->height;
        t->topDown = dc->bitmap->topDown;
        return true;
    }
    t->bits = g_screen;
    t->pitch = minwin_gdi::kScreenWidth;
    t->width = minwin_gdi::kScreenWidth;
    t->height = minwin_gdi::kScreenHeight;
    t->topDown = true;
    return true;
}

uint8_t* RowOf(const Target& t, int y) { return t.bits + static_cast<size_t>(t.topDown ? y : t.height - 1 - y) * static_cast<size_t>(t.pitch); }

// Calls f(targetX, targetY, dcX, dcY) for each pixel of `dst` (DC coordinates) that the DC may draw to.
template <class F>
void ForEachPixel(const Dc* dc, const Target& t, RECT dst, F&& f) {
    if (dc->memory || !dc->clip) {
        RECT r;
        if (!IntersectRects(&r, dst, Rect(0, 0, t.width, t.height))) return;
        for (int y = r.top; y < r.bottom; ++y)
            for (int x = r.left; x < r.right; ++x) f(x, y, x, y);
        return;
    }
    const RECT screenRect = Rect(dst.left + dc->originX, dst.top + dc->originY, dst.right + dc->originX, dst.bottom + dc->originY);
    for (int i = 0; i < dc->clipCount; ++i) {
        RECT r;
        if (!IntersectRects(&r, screenRect, dc->clip[i])) continue;
        if (!IntersectRects(&r, r, Rect(0, 0, t.width, t.height))) continue;
        for (int y = r.top; y < r.bottom; ++y)
            for (int x = r.left; x < r.right; ++x) f(x, y, x - dc->originX, y - dc->originY);
    }
}

void Fill(Dc* dc, RECT r, uint8_t index) {
    Target t;
    if (!TargetOf(dc, &t)) return;
    ForEachPixel(dc, t, r, [&](int x, int y, int, int) { RowOf(t, y)[x] = index; });
}

int PitchFor(int width, int bpp) { return ((width * bpp + 31) / 32) * 4; }

Bitmap* NewBitmap(int width, int height, int bpp, bool topDown) {
    Bitmap* b = static_cast<Bitmap*>(calloc(1, sizeof(Bitmap)));
    b->kind = kBitmap;
    b->width = width;
    b->height = height;
    b->bpp = bpp;
    b->pitch = PitchFor(width, bpp);
    b->topDown = topDown;
    b->bits = static_cast<uint8_t*>(calloc(1, static_cast<size_t>(b->pitch) * static_cast<size_t>(height ? height : 1)));
    return b;
}

}  // namespace

namespace minwin_gdi {

ScreenSurface Screen() {
    Init();
    return {g_screen, kScreenWidth, kScreenHeight, kScreenWidth};
}

void CurrentPalette(uint32_t rgb[256], uint32_t* generation) {
    Init();
    memcpy(rgb, g_systemPalette, sizeof g_systemPalette);
    *generation = g_generation;
}

HDC CreateWindowDc(const WindowDcDesc& desc) {
    Init();
    Dc* dc = static_cast<Dc*>(calloc(1, sizeof(Dc)));
    dc->kind = kDc;
    dc->window = desc.window;
    dc->originX = desc.originX;
    dc->originY = desc.originY;
    dc->clipCount = desc.clipCount;
    dc->clip = static_cast<RECT*>(malloc(sizeof(RECT) * static_cast<size_t>(desc.clipCount ? desc.clipCount : 1)));
    if (desc.clipCount) memcpy(dc->clip, desc.clip, sizeof(RECT) * static_cast<size_t>(desc.clipCount));
    dc->palette = &g_defaultPalette;
    return reinterpret_cast<HDC>(dc);
}

void DestroyWindowDc(HDC handle) {
    Dc* dc = ToDc(handle);
    if (!dc) return;
    free(dc->clip);
    free(dc);
}

}  // namespace minwin_gdi

extern "C" {

HDC REB_STDCALL CreateCompatibleDC(HDC) {
    Dc* dc = static_cast<Dc*>(calloc(1, sizeof(Dc)));
    dc->kind = kDc;
    dc->memory = true;
    dc->palette = &g_defaultPalette;
    return reinterpret_cast<HDC>(dc);
}

HDC REB_STDCALL CreateICA(LPCSTR, LPCSTR, LPCSTR, const DEVMODEA*) {
    RECT screen = Rect(0, 0, minwin_gdi::kScreenWidth, minwin_gdi::kScreenHeight);
    minwin_gdi::WindowDcDesc desc = {nullptr, 0, 0, &screen, 1};
    return minwin_gdi::CreateWindowDc(desc);
}

BOOL REB_STDCALL DeleteDC(HDC dc) {
    minwin_gdi::DestroyWindowDc(dc);
    return TRUE;
}

int REB_STDCALL GetDeviceCaps(HDC, int index) {
    switch (index) {
        case HORZRES: return minwin_gdi::kScreenWidth;
        case VERTRES: return minwin_gdi::kScreenHeight;
        case BITSPIXEL: return 8;
        case PLANES: return 1;
        case NUMCOLORS: return 20;
        case RASTERCAPS: return RC_PALETTE;
        case SIZEPALETTE: return 256;
        case 0x6a: return 20;  // NUMRESERVED
        default: return 0;
    }
}

HBITMAP REB_STDCALL CreateDIBSection(HDC, const BITMAPINFO* info, UINT, void** bits, HANDLE, DWORD) {
    const int height = info->bmiHeader.biHeight;
    Bitmap* b = NewBitmap(info->bmiHeader.biWidth, height < 0 ? -height : height, info->bmiHeader.biBitCount, height < 0);
    if (bits) *bits = b->bits;
    return reinterpret_cast<HBITMAP>(b);
}

HBITMAP REB_STDCALL CreateCompatibleBitmap(HDC, int cx, int cy) { return reinterpret_cast<HBITMAP>(NewBitmap(cx, cy, 8, true)); }

HBITMAP REB_STDCALL CreateDIBitmap(HDC, const BITMAPINFOHEADER* header, DWORD init, const void* bits, const BITMAPINFO*, UINT) {
    const int height = header->biHeight;
    Bitmap* b = NewBitmap(header->biWidth, height < 0 ? -height : height, header->biBitCount, height < 0);
    if ((init & CBM_INIT) && bits) memcpy(b->bits, bits, static_cast<size_t>(b->pitch) * static_cast<size_t>(b->height));
    return reinterpret_cast<HBITMAP>(b);
}

int REB_STDCALL GetObjectA(HANDLE handle, int, LPVOID out) {
    Object* o = static_cast<Object*>(handle);
    if (!o || o->kind != kBitmap || !out) return 0;
    const Bitmap* b = reinterpret_cast<const Bitmap*>(o);
    BITMAP* info = static_cast<BITMAP*>(out);
    memset(info, 0, sizeof *info);
    info->bmWidth = b->width;
    info->bmHeight = b->height;
    info->bmWidthBytes = b->pitch;
    info->bmPlanes = 1;
    info->bmBitsPixel = static_cast<WORD>(b->bpp);
    info->bmBits = b->bits;
    return static_cast<int>(sizeof *info);
}

BOOL REB_STDCALL DeleteObject(HGDIOBJ handle) {
    Object* o = static_cast<Object*>(handle);
    if (!o || o == &g_stockOther || o == reinterpret_cast<Object*>(&g_defaultPalette)) return TRUE;
    if (o >= reinterpret_cast<Object*>(&g_stockBrushes[0]) && o < reinterpret_cast<Object*>(&g_stockBrushes[6])) return TRUE;
    if (o->kind == kBitmap) free(reinterpret_cast<Bitmap*>(o)->bits);
    free(o);
    return TRUE;
}

HGDIOBJ REB_STDCALL SelectObject(HDC handle, HGDIOBJ object) {
    Dc* dc = ToDc(handle);
    Object* o = static_cast<Object*>(object);
    if (!dc || !o) return nullptr;
    if (o->kind == kBitmap) {
        Bitmap* old = dc->bitmap;
        dc->bitmap = reinterpret_cast<Bitmap*>(o);
        return old ? reinterpret_cast<HGDIOBJ>(old) : &g_stockOther;
    }
    if (o->kind == kBrush) {
        Brush* old = dc->brush;
        dc->brush = reinterpret_cast<Brush*>(o);
        return old ? reinterpret_cast<HGDIOBJ>(old) : reinterpret_cast<HGDIOBJ>(&g_stockBrushes[0]);
    }
    return &g_stockOther;
}

HGDIOBJ REB_STDCALL GetStockObject(int index) {
    if (index >= 0 && index <= 5) return reinterpret_cast<HGDIOBJ>(&g_stockBrushes[index]);
    return &g_stockOther;
}

HBRUSH REB_STDCALL CreateSolidBrush(COLORREF color) {
    Brush* b = static_cast<Brush*>(calloc(1, sizeof(Brush)));
    b->kind = kBrush;
    b->color = color;
    return reinterpret_cast<HBRUSH>(b);
}

HBRUSH REB_STDCALL CreateBrushIndirect(const LOGBRUSH* lb) { return CreateSolidBrush(lb->lbColor); }

HPEN REB_STDCALL CreatePen(int, int, COLORREF) {
    Object* o = static_cast<Object*>(calloc(1, sizeof(Object)));
    o->kind = kPen;
    return reinterpret_cast<HPEN>(o);
}

HFONT REB_STDCALL CreateFontIndirectA(const LOGFONTA*) {
    Object* o = static_cast<Object*>(calloc(1, sizeof(Object)));
    o->kind = kFont;
    return reinterpret_cast<HFONT>(o);
}

HPALETTE REB_STDCALL CreatePalette(const LOGPALETTE* logical) {
    Palette* p = static_cast<Palette*>(calloc(1, sizeof(Palette)));
    p->kind = kPalette;
    p->count = logical->palNumEntries > 256 ? 256 : logical->palNumEntries;
    for (int i = 0; i < p->count; ++i)
        p->rgb[i] = (static_cast<uint32_t>(logical->palPalEntry[i].peRed) << 16) | (static_cast<uint32_t>(logical->palPalEntry[i].peGreen) << 8) |
                    logical->palPalEntry[i].peBlue;
    return reinterpret_cast<HPALETTE>(p);
}

UINT REB_STDCALL GetPaletteEntries(HPALETTE handle, UINT start, UINT count, LPPALETTEENTRY out) {
    const Palette* p = reinterpret_cast<const Palette*>(handle);
    if (!p) return 0;
    UINT n = 0;
    for (; n < count && static_cast<int>(start + n) < p->count; ++n) {
        const uint32_t c = p->rgb[start + n];
        out[n].peRed = static_cast<BYTE>(c >> 16);
        out[n].peGreen = static_cast<BYTE>(c >> 8);
        out[n].peBlue = static_cast<BYTE>(c);
        out[n].peFlags = 0;
    }
    return n;
}

UINT REB_STDCALL SetPaletteEntries(HPALETTE handle, UINT start, UINT count, const PALETTEENTRY* in) {
    Palette* p = reinterpret_cast<Palette*>(handle);
    if (!p) return 0;
    UINT n = 0;
    for (; n < count && start + n < 256; ++n)
        p->rgb[start + n] = (static_cast<uint32_t>(in[n].peRed) << 16) | (static_cast<uint32_t>(in[n].peGreen) << 8) | in[n].peBlue;
    if (static_cast<int>(start + n) > p->count) p->count = static_cast<int>(start + n);
    return n;
}

BOOL REB_STDCALL ResizePalette(HPALETTE handle, UINT n) {
    Palette* p = reinterpret_cast<Palette*>(handle);
    if (!p) return FALSE;
    p->count = n > 256 ? 256 : static_cast<int>(n);
    return TRUE;
}

HPALETTE REB_STDCALL SelectPalette(HDC handle, HPALETTE palette, BOOL) {
    Dc* dc = ToDc(handle);
    if (!dc) return nullptr;
    Palette* old = dc->palette;
    if (palette) dc->palette = reinterpret_cast<Palette*>(palette);
    return reinterpret_cast<HPALETTE>(old);
}

UINT REB_STDCALL SetSystemPaletteUse(HDC, UINT use) {
    const UINT old = g_noStatic ? SYSPAL_NOSTATIC : SYSPAL_STATIC;
    g_noStatic = use == SYSPAL_NOSTATIC;
    return old;
}

UINT REB_STDCALL RealizePalette(HDC handle) {
    Dc* dc = ToDc(handle);
    if (!dc || !dc->palette) return 0;
    static bool first = true;
    bool changed = false;
    const bool notify = first;
    first = false;
    for (int i = 0; i < dc->palette->count; ++i) {
        if (!g_noStatic && (i < 10 || i >= 246)) continue;
        if (g_systemPalette[i] != dc->palette->rgb[i]) {
            g_systemPalette[i] = dc->palette->rgb[i];
            changed = true;
        }
    }
    if (changed) ++g_generation;
    if (notify) minwin_user::NotifyPaletteChanged(dc->window);
    return static_cast<UINT>(dc->palette->count);
}

UINT REB_STDCALL GetSystemPaletteEntries(HDC, UINT start, UINT count, LPPALETTEENTRY out) {
    Init();
    UINT n = 0;
    for (; n < count && start + n < 256; ++n) {
        const uint32_t c = g_systemPalette[start + n];
        out[n].peRed = static_cast<BYTE>(c >> 16);
        out[n].peGreen = static_cast<BYTE>(c >> 8);
        out[n].peBlue = static_cast<BYTE>(c);
        out[n].peFlags = 0;
    }
    return n;
}

BOOL REB_STDCALL BitBlt(HDC dstHandle, int x, int y, int cx, int cy, HDC srcHandle, int sx, int sy, DWORD rop) {
    Dc* dst = ToDc(dstHandle);
    Dc* src = ToDc(srcHandle);
    if (!dst) return FALSE;
    Target to;
    if (!TargetOf(dst, &to)) return TRUE;
    if (rop == SRCCOPY && src) {
        Target from;
        if (!TargetOf(src, &from)) return TRUE;
        ForEachPixel(dst, to, Rect(x, y, x + cx, y + cy), [&](int tx, int ty, int dx, int dy) {
            const int px = sx + dx - x, py = sy + dy - y;
            if (px < 0 || py < 0 || px >= from.width || py >= from.height) return;
            RowOf(to, ty)[tx] = RowOf(from, py)[px];
        });
    } else if (rop == BLACKNESS) {
        Fill(dst, Rect(x, y, x + cx, y + cy), 0);
    }
    return TRUE;
}

BOOL REB_STDCALL PatBlt(HDC handle, int x, int y, int w, int h, DWORD rop) {
    Dc* dc = ToDc(handle);
    if (!dc) return FALSE;
    if (rop == BLACKNESS) Fill(dc, Rect(x, y, x + w, y + h), 0);
    else if (rop == PATCOPY && dc->brush) Fill(dc, Rect(x, y, x + w, y + h), NearestIndex(dc->brush->color));
    return TRUE;
}

int REB_STDCALL SetDIBitsToDevice(HDC handle, int xDest, int yDest, DWORD w, DWORD h, int xSrc, int ySrc, UINT, UINT, const void* bits,
                                  const BITMAPINFO* info, UINT) {
    Dc* dc = ToDc(handle);
    Target to;
    if (!dc || !bits || !TargetOf(dc, &to) || info->bmiHeader.biBitCount != 8) return 0;
    const int srcHeight = info->bmiHeader.biHeight < 0 ? -info->bmiHeader.biHeight : info->bmiHeader.biHeight;
    const bool srcTopDown = info->bmiHeader.biHeight < 0;
    const int pitch = PitchFor(info->bmiHeader.biWidth, 8);
    ForEachPixel(dc, to, Rect(xDest, yDest, xDest + static_cast<int>(w), yDest + static_cast<int>(h)), [&](int tx, int ty, int dx, int dy) {
        const int column = xSrc + dx - xDest;
        const int fromBottom = ySrc + (static_cast<int>(h) - 1 - (dy - yDest));
        const int row = srcTopDown ? srcHeight - 1 - fromBottom : srcHeight - 1 - fromBottom;
        if (column < 0 || column >= info->bmiHeader.biWidth || row < 0 || row >= srcHeight) return;
        RowOf(to, ty)[tx] = static_cast<const uint8_t*>(bits)[static_cast<size_t>(row) * static_cast<size_t>(pitch) + static_cast<size_t>(column)];
    });
    return static_cast<int>(h);
}

int REB_STDCALL FillRect(HDC handle, const RECT* rect, HBRUSH brush) {
    Dc* dc = ToDc(handle);
    if (!dc || !rect) return 0;
    const Brush* b = reinterpret_cast<const Brush*>(brush);
    Fill(dc, *rect, b && b->kind == kBrush ? NearestIndex(b->color) : 0);
    return 1;
}

int REB_STDCALL DrawTextA(HDC, LPCSTR, int count, LPRECT rect, UINT format) {
    if (format & DT_CALCRECT) {
        const int n = count < 0 ? 0 : count;
        rect->right = rect->left + n * 7;
        rect->bottom = rect->top + 14;
    }
    return 14;
}

BOOL REB_STDCALL GetTextExtentPoint32A(HDC, LPCSTR, int count, LPSIZE size) {
    size->cx = count * 7;
    size->cy = 14;
    return TRUE;
}

BOOL REB_STDCALL GetTextMetricsA(HDC, LPTEXTMETRICA tm) {
    memset(tm, 0, sizeof *tm);
    tm->tmHeight = 14;
    tm->tmAscent = 11;
    tm->tmDescent = 3;
    tm->tmAveCharWidth = 7;
    tm->tmMaxCharWidth = 14;
    return TRUE;
}

BOOL REB_STDCALL TextOutA(HDC, int, int, LPCSTR, int) { return TRUE; }
BOOL REB_STDCALL LineTo(HDC, int, int) { return TRUE; }
BOOL REB_STDCALL MoveToEx(HDC, int, int, LPPOINT) { return TRUE; }
BOOL REB_STDCALL Polyline(HDC, const POINT*, int) { return TRUE; }
BOOL REB_STDCALL Rectangle(HDC, int, int, int, int) { return TRUE; }
BOOL REB_STDCALL DPtoLP(HDC, LPPOINT, int) { return TRUE; }
COLORREF REB_STDCALL SetBkColor(HDC, COLORREF) { return 0; }
int REB_STDCALL SetBkMode(HDC, int) { return OPAQUE; }
int REB_STDCALL SetROP2(HDC, int) { return 13; }
int REB_STDCALL SetStretchBltMode(HDC, int) { return 1; }
COLORREF REB_STDCALL SetTextColor(HDC, COLORREF) { return 0; }
UINT REB_STDCALL SetTextAlign(HDC, UINT) { return 0; }

}  // extern "C"
