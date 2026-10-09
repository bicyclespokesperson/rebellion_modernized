// Device contexts, the screen and its system palette, logical palettes (select / realize / animate), device caps.
#define MINWIN_IMPLEMENTATION 1
#include <stdlib.h>
#include <string.h>

#include "../gdi_api.h"
#include "gdi_internal.h"

namespace mwgdi {

// The 20 static colours Windows reserves: indices 0-9 and 246-255 (0x00RRGGBB).
const uint32_t kStaticColors[20] = {
    0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0, 0xc0dcc0, 0xa6caf0,
    0xfffbf0, 0xa0a0a4, 0x808080, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,
};

SystemPalette& System() {
    static SystemPalette* p = [] {
        SystemPalette* s = new SystemPalette;
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

uint32_t RgbOf(COLORREF c) { return PackRgb(c & 0xff, (c >> 8) & 0xff, (c >> 16) & 0xff); }

int NearestIndex(const uint32_t* table, int count, uint32_t rgb) {
    for (int i = 0; i < count; ++i)
        if ((table[i] & 0xffffff) == rgb) return i;
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

void UpdateMapping(Palette* p) {
    if (p->realized && p->toPhysical.size() == p->entries.size()) return;
    p->toPhysical.resize(p->entries.size());
    SystemPalette& sys = System();
    for (size_t i = 0; i < p->entries.size(); ++i)
        p->toPhysical[i] = static_cast<uint8_t>(NearestIndex(sys.rgb, 256, PackRgb(p->entries[i].peRed, p->entries[i].peGreen, p->entries[i].peBlue)));
}

namespace {

constexpr int kPcReserved = 1, kPcExplicit = 2, kPcNoCollapse = 4;

bool IsStaticSlot(const SystemPalette& s, int i) { return s.staticColors && (i < 10 || i >= 246); }
bool IsFixedSlot(const SystemPalette& s, int i) { return IsStaticSlot(s, i) || i == 0 || i == 255; }

void SetSlot(SystemPalette& s, int i, uint32_t rgb, int& changed) {
    if (s.rgb[i] != rgb) {
        s.rgb[i] = rgb;
        ++changed;
    }
}

}  // namespace

// Foreground realization of the DC's palette (the model: one active palette owns every non-static slot).
int RealizePaletteIn(Dc* dc) {
    Palette* pal = dc->palette;
    SystemPalette& sys = System();
    if (pal == DefaultPalette()) return 0;
    int n = static_cast<int>(pal->entries.size());
    pal->toPhysical.assign(n, 0);
    if (dc->memory) {
        pal->realized = false;
        UpdateMapping(pal);
        return 0;
    }
    uint8_t used[256] = {};
    for (int i = 0; i < 256; ++i) used[i] = IsFixedSlot(sys, i) ? 1 : 0;
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
            for (int k = 0; k < 256 && slot < 0; ++k)
                if ((used[k] || owned[k]) && sys.rgb[k] == rgb) slot = k;
        }
        if (slot < 0) {
            for (int k = 0; k < 256; ++k)
                if (!used[k] && !owned[k]) {
                    slot = k;
                    owned[k] = true;
                    SetSlot(sys, k, rgb, changed);
                    break;
                }
        }
        if (slot < 0) slot = NearestIndex(sys.rgb, 256, rgb);
        pal->toPhysical[i] = static_cast<uint8_t>(slot);
    }
    for (int k = 0; k < 256; ++k)
        if (!used[k] && !owned[k] && sys.rgb[k] != 0) {  // released by the previous palette
            sys.rgb[k] = 0;
            ++changed;
        }
    pal->realized = true;
    if (changed) ++sys.generation;
    return changed;
}

Dc* DcOf(HDC hdc) { return As<Dc>(hdc, ObjType::Dc); }

const uint32_t* TableOf(const Dc* dc) { return dc->surface.table ? dc->surface.table : System().rgb; }

Surface SurfaceOfDc(const Dc* dc) {
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

void ResetClipToSurface(Dc* dc) {
    dc->clip.clear();
    if (dc->surface.bits) dc->clip.push_back({0, 0, dc->surface.width, dc->surface.height});
}

namespace {

Dc* NewDc() {
    Dc* dc = new Dc;
    dc->pen = StockPen(7);
    dc->brush = StockBrush(0);
    dc->font = static_cast<Font*>(GetStockObject(13));
    dc->palette = DefaultPalette();
    ++dc->pen->selectCount;
    ++dc->brush->selectCount;
    ++dc->font->selectCount;
    Register(dc);
    return dc;
}

Bitmap* MonoDefaultBitmap() {
    Bitmap* b = new Bitmap;
    b->width = b->height = 1;
    b->stride = 4;
    b->bits = static_cast<uint8_t*>(calloc(1, 4));
    b->stock = true;
    Register(b);
    return b;
}

}  // namespace

}  // namespace mwgdi

using namespace mwgdi;

namespace minwin_gdi {

ScreenSurface Screen() {
    static uint8_t* bits = static_cast<uint8_t*>(calloc(kScreenWidth * kScreenHeight, 1));
    return {bits, kScreenWidth, kScreenHeight, kScreenWidth};
}

void CurrentPalette(uint32_t rgb[256], uint32_t* generation) {
    SystemPalette& s = System();
    memcpy(rgb, s.rgb, sizeof s.rgb);
    if (generation) *generation = s.generation;
}

HDC CreateWindowDc(const WindowDcDesc& desc) {
    Dc* dc = NewDc();
    dc->window = desc.window;
    dc->originX = desc.originX;
    dc->originY = desc.originY;
    dc->surface = SurfaceOfDc(dc);
    for (int i = 0; i < desc.clipCount; ++i) dc->clip.push_back(desc.clip[i]);
    return reinterpret_cast<HDC>(dc);
}

void DestroyWindowDc(HDC hdc) {
    Dc* dc = DcOf(hdc);
    if (!dc) return;
    --dc->pen->selectCount;
    --dc->brush->selectCount;
    --dc->font->selectCount;
    Unregister(dc);
    delete dc;
}

}  // namespace minwin_gdi

extern "C" {

HDC REB_STDCALL CreateCompatibleDC(HDC hdc) {
    (void)hdc;
    static Bitmap* defaultBitmap = MonoDefaultBitmap();
    Dc* dc = NewDc();
    dc->memory = true;
    dc->bitmap = defaultBitmap;
    dc->surface = SurfaceOfDc(dc);
    ResetClipToSurface(dc);
    return reinterpret_cast<HDC>(dc);
}

HDC REB_STDCALL CreateICA(LPCSTR, LPCSTR, LPCSTR, const DEVMODEA*) {
    Dc* dc = NewDc();
    dc->info = true;
    return reinterpret_cast<HDC>(dc);
}

BOOL REB_STDCALL DeleteDC(HDC hdc) {
    Dc* dc = DcOf(hdc);
    if (!dc) return FALSE;
    if (dc->bitmap) dc->bitmap->selectedIn = nullptr;
    --dc->pen->selectCount;
    --dc->brush->selectCount;
    --dc->font->selectCount;
    Unregister(dc);
    delete dc;
    return TRUE;
}

COLORREF REB_STDCALL SetTextColor(HDC hdc, COLORREF color) {
    Dc* dc = DcOf(hdc);
    if (!dc) return 0xffffffff;
    COLORREF old = dc->textColor;
    dc->textColor = color;
    return old;
}

COLORREF REB_STDCALL SetBkColor(HDC hdc, COLORREF color) {
    Dc* dc = DcOf(hdc);
    if (!dc) return 0xffffffff;
    COLORREF old = dc->bkColor;
    dc->bkColor = color;
    return old;
}

int REB_STDCALL SetBkMode(HDC hdc, int mode) {
    Dc* dc = DcOf(hdc);
    if (!dc || (mode != 1 && mode != 2)) return 0;
    int old = dc->bkMode;
    dc->bkMode = mode;
    return old;
}

int REB_STDCALL SetROP2(HDC hdc, int rop2) {
    Dc* dc = DcOf(hdc);
    if (!dc || rop2 < 1 || rop2 > 16) return 0;
    int old = dc->rop2;
    dc->rop2 = rop2;
    return old;
}

int REB_STDCALL SetStretchBltMode(HDC hdc, int mode) {
    Dc* dc = DcOf(hdc);
    if (!dc) return 0;
    int old = dc->stretchMode;
    dc->stretchMode = mode;
    return old;
}

UINT REB_STDCALL SetTextAlign(HDC hdc, UINT align) {
    Dc* dc = DcOf(hdc);
    if (!dc) return 0xffffffffu;
    UINT old = dc->textAlign;
    dc->textAlign = align;
    return old;
}

BOOL REB_STDCALL DPtoLP(HDC hdc, LPPOINT, int) { return DcOf(hdc) ? TRUE : FALSE; }

int REB_STDCALL GetDeviceCaps(HDC hdc, int index) {
    if (!DcOf(hdc)) return 0;
    switch (index) {
        case 0: return 0x0300;             // DRIVERVERSION
        case 2: return 1;                  // TECHNOLOGY: DT_RASDISPLAY
        case 4: return 169;                // HORZSIZE (mm)
        case 6: return 127;                // VERTSIZE
        case 8: return 640;                // HORZRES
        case 10: return 480;               // VERTRES
        case 12: return 8;                 // BITSPIXEL
        case 14: return 1;                 // PLANES
        case 16: return -1;                // NUMBRUSHES
        case 18: return -1;                // NUMPENS
        case 22: return 0;                 // NUMFONTS
        case 24: return 20;                // NUMCOLORS: the static colours of a palettised device
        case 26: return 0;                 // PDEVICESIZE
        case 28: return 0x1ff;             // CURVECAPS
        case 30: return 0x1ff;             // LINECAPS
        case 32: return 0xff;              // POLYGONALCAPS
        case 34: return 0x1ff;             // TEXTCAPS
        case 36: return 0;                 // CLIPCAPS
        case 38: return 0x7e99 | 0x100;    // RASTERCAPS: bitblt, bitmap64, di bitmap, palette, dibtodev, stretchblt, stretchdib...
        case 40: return 36;                // ASPECTX
        case 42: return 36;                // ASPECTY
        case 44: return 51;                // ASPECTXY
        case 88: return 96;                // LOGPIXELSX
        case 90: return 96;                // LOGPIXELSY
        case 104: return 256;              // SIZEPALETTE
        case 106: return 20;               // NUMRESERVED
        case 108: return 18;               // COLORRES
        default: return 0;
    }
}

UINT REB_STDCALL GetSystemPaletteEntries(HDC hdc, UINT iStart, UINT cEntries, LPPALETTEENTRY out) {
    if (!DcOf(hdc) || iStart >= 256) return 0;
    SystemPalette& s = System();
    UINT n = cEntries;
    if (iStart + n > 256) n = 256 - iStart;
    if (!out) return 256;
    for (UINT i = 0; i < n; ++i) {
        uint32_t c = s.rgb[iStart + i];
        out[i] = {uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c), 0};
    }
    return n;
}

UINT REB_STDCALL SetSystemPaletteUse(HDC hdc, UINT use) {
    Dc* dc = DcOf(hdc);
    if (!dc || (use != SYSPAL_STATIC && use != SYSPAL_NOSTATIC)) return 0;
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

HPALETTE REB_STDCALL CreatePalette(const LOGPALETTE* plpal) {
    if (!plpal) return nullptr;
    Palette* p = new Palette;
    p->entries.assign(plpal->palPalEntry, plpal->palPalEntry + plpal->palNumEntries);
    UpdateMapping(p);
    Register(p);
    return reinterpret_cast<HPALETTE>(p);
}

BOOL REB_STDCALL ResizePalette(HPALETTE hpal, UINT n) {
    Palette* p = As<Palette>(hpal, ObjType::Palette);
    if (!p || n == 0 || n > 1024) return FALSE;
    p->entries.resize(n);
    p->realized = false;
    UpdateMapping(p);
    return TRUE;
}

UINT REB_STDCALL SetPaletteEntries(HPALETTE hpal, UINT iStart, UINT cEntries, const PALETTEENTRY* e) {
    Palette* p = As<Palette>(hpal, ObjType::Palette);
    if (!p || p->stock || iStart >= p->entries.size() || !e) return 0;
    UINT n = cEntries;
    if (iStart + n > p->entries.size()) n = static_cast<UINT>(p->entries.size()) - iStart;
    for (UINT i = 0; i < n; ++i) p->entries[iStart + i] = e[i];
    p->realized = false;
    UpdateMapping(p);
    return n;
}

UINT REB_STDCALL GetPaletteEntries(HPALETTE hpal, UINT iStart, UINT cEntries, LPPALETTEENTRY out) {
    Palette* p = As<Palette>(hpal, ObjType::Palette);
    if (!p) return 0;
    if (!out) return static_cast<UINT>(p->entries.size());
    if (iStart >= p->entries.size()) return 0;
    UINT n = cEntries;
    if (iStart + n > p->entries.size()) n = static_cast<UINT>(p->entries.size()) - iStart;
    for (UINT i = 0; i < n; ++i) out[i] = p->entries[iStart + i];
    return n;
}

HPALETTE REB_STDCALL SelectPalette(HDC hdc, HPALETTE hPal, BOOL) {
    Dc* dc = DcOf(hdc);
    Palette* p = As<Palette>(hPal, ObjType::Palette);
    if (!dc || !p) return nullptr;
    Palette* old = dc->palette;
    dc->palette = p;
    UpdateMapping(p);
    return reinterpret_cast<HPALETTE>(old);
}

UINT REB_STDCALL RealizePalette(HDC hdc) {
    Dc* dc = DcOf(hdc);
    if (!dc) return 0xffffffffu;
    int changed = RealizePaletteIn(dc);
    if (changed && !dc->memory) minwin_user::NotifyPaletteChanged(dc->window);
    return static_cast<UINT>(changed);
}

}  // extern "C"
