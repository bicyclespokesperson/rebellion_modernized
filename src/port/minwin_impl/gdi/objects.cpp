// GDI objects: pens, brushes, bitmaps and DIB sections, palettes; SelectObject / DeleteObject / GetObject / GetStockObject.
#define MINWIN_IMPLEMENTATION 1
#include <stdlib.h>
#include <string.h>

#include <unordered_set>

#include "gdi_internal.h"

namespace mwgdi {

void (*g_fontCacheFree)(void*) = nullptr;

Font::~Font() {
    if (textCache && g_fontCacheFree) g_fontCacheFree(textCache);
}

Bitmap::~Bitmap() { free(bits); }

namespace {
std::unordered_set<const void*>& Live() {
    // Game globals delete their graphics objects after other static destructors have run.
    static auto* live = new std::unordered_set<const void*>;
    return *live;
}
}  // namespace

void Register(Object* o) { Live().insert(o); }
void Unregister(Object* o) { Live().erase(o); }

bool IsLive(const void* handle, ObjType type) {
    if (!handle || !Live().count(handle)) return false;
    return static_cast<const Object*>(handle)->type == type;
}

Palette* DefaultPalette() {
    static Palette* p = [] {
        Palette* d = new Palette;
        d->stock = true;
        for (int i = 0; i < 20; ++i) {
            uint32_t c = kStaticColors[i];
            d->entries.push_back({uint8_t(c >> 16), uint8_t(c >> 8), uint8_t(c), 0});
        }
        d->toPhysical.resize(20);
        for (int i = 0; i < 20; ++i) d->toPhysical[i] = static_cast<uint8_t>(i < 10 ? i : 236 + i);
        d->realized = true;
        Register(d);
        return d;
    }();
    return p;
}

namespace {

Brush* MakeStockBrush(COLORREF color, int style) {
    Brush* b = new Brush;
    b->stock = true;
    b->color = color;
    b->style = style;
    Register(b);
    return b;
}

Pen* MakeStockPen(COLORREF color, int style) {
    Pen* p = new Pen;
    p->stock = true;
    p->color = color;
    p->style = style;
    Register(p);
    return p;
}

}  // namespace

Brush* StockBrush(int which) {
    static Brush* brushes[6] = {};
    if (which < 0 || which > 5) return nullptr;
    if (!brushes[which]) {
        static const COLORREF colors[6] = {0x00ffffff, 0x00c0c0c0, 0x00808080, 0x00404040, 0x00000000, 0};
        brushes[which] = MakeStockBrush(colors[which], which == 5 ? 1 : 0);
    }
    return brushes[which];
}

Pen* StockPen(int which) {  // 6 WHITE_PEN, 7 BLACK_PEN, 8 NULL_PEN
    static Pen* pens[3] = {};
    if (which < 6 || which > 8) return nullptr;
    Pen*& p = pens[which - 6];
    if (!p) p = MakeStockPen(which == 6 ? 0x00ffffff : 0, which == 8 ? 5 : 0);
    return p;
}

}  // namespace mwgdi

using namespace mwgdi;

extern "C" {

HGDIOBJ REB_STDCALL GetStockObject(int i) {
    if (i >= 0 && i <= 5) return StockBrush(i);
    if (i >= 6 && i <= 8) return StockPen(i);
    if (i == 15) return DefaultPalette();
    static Font* fonts[32] = {};
    if (i >= 10 && i < 32 && i != 15) {  // OEM_FIXED_FONT..DEFAULT_GUI_FONT: one shared ANSI font each
        if (!fonts[i]) {
            Font* f = new Font;
            f->stock = true;
            f->lf.lfHeight = (i == 10 || i == 11) ? 12 : 13;
            f->lf.lfWeight = (i == 13 || i == 10) ? 700 : 400;
            strcpy(f->lf.lfFaceName, (i == 10 || i == 11) ? "Courier New" : (i == 13 ? "System" : "MS Sans Serif"));
            Register(f);
            fonts[i] = f;
        }
        return fonts[i];
    }
    return nullptr;
}

HBRUSH REB_STDCALL CreateSolidBrush(COLORREF color) {
    Brush* b = new Brush;
    b->color = color;
    Register(b);
    return reinterpret_cast<HBRUSH>(b);
}

HBRUSH REB_STDCALL CreateBrushIndirect(const LOGBRUSH* plbrush) {
    if (!plbrush) return nullptr;
    Brush* b = new Brush;
    b->style = plbrush->lbStyle == 1 ? 1 : 0;  // BS_NULL; hatch and pattern brushes draw as solid
    b->color = plbrush->lbColor;
    Register(b);
    return reinterpret_cast<HBRUSH>(b);
}

HPEN REB_STDCALL CreatePen(int iStyle, int cWidth, COLORREF color) {
    Pen* p = new Pen;
    p->style = iStyle;
    p->width = cWidth;
    p->color = color;
    Register(p);
    return reinterpret_cast<HPEN>(p);
}

HGDIOBJ REB_STDCALL SelectObject(HDC hdc, HGDIOBJ h) {
    Dc* dc = DcOf(hdc);
    if (!dc || !h) return nullptr;
    if (IsLive(h, ObjType::Pen)) {
        Pen* old = dc->pen;
        dc->pen = static_cast<Pen*>(h);
        ++dc->pen->selectCount;
        --old->selectCount;
        return old;
    }
    if (IsLive(h, ObjType::Brush)) {
        Brush* old = dc->brush;
        dc->brush = static_cast<Brush*>(h);
        ++dc->brush->selectCount;
        --old->selectCount;
        return old;
    }
    if (IsLive(h, ObjType::Font)) {
        Font* old = dc->font;
        dc->font = static_cast<Font*>(h);
        ++dc->font->selectCount;
        --old->selectCount;
        return old;
    }
    if (IsLive(h, ObjType::Palette)) {
        Palette* old = dc->palette;
        dc->palette = static_cast<Palette*>(h);
        UpdateMapping(dc->palette);
        return old;
    }
    if (IsLive(h, ObjType::Bitmap)) {
        Bitmap* bm = static_cast<Bitmap*>(h);
        if (!dc->memory || (bm->selectedIn && bm->selectedIn != dc)) return nullptr;
        Bitmap* old = dc->bitmap;
        if (old) old->selectedIn = nullptr;
        dc->bitmap = bm;
        bm->selectedIn = dc;
        dc->surface = SurfaceOfDc(dc);
        ResetClipToSurface(dc);
        return old;
    }
    return nullptr;
}

BOOL REB_STDCALL DeleteObject(HGDIOBJ ho) {
    Object* o = static_cast<Object*>(ho);
    if (!ho || !IsLive(ho, o->type) || o->type == ObjType::Dc) return FALSE;
    if (o->stock) return TRUE;
    if (o->type == ObjType::Bitmap && static_cast<Bitmap*>(o)->selectedIn) return FALSE;
    if (o->selectCount > 0) return FALSE;
    Unregister(o);
    delete o;
    return TRUE;
}

int REB_STDCALL GetObjectA(HANDLE h, int c, LPVOID pv) {
    if (!h) return 0;
    if (IsLive(h, ObjType::Bitmap)) {
        Bitmap* b = static_cast<Bitmap*>(h);
        if (!pv) return sizeof(BITMAP);
        if (c < static_cast<int>(sizeof(BITMAP))) return 0;
        BITMAP* bm = static_cast<BITMAP*>(pv);
        bm->bmType = 0;
        bm->bmWidth = b->width;
        bm->bmHeight = b->height;
        bm->bmWidthBytes = b->stride;
        bm->bmPlanes = 1;
        bm->bmBitsPixel = 8;
        bm->bmBits = b->isDib ? b->bits : nullptr;
        return sizeof(BITMAP);
    }
    if (IsLive(h, ObjType::Palette)) {
        if (!pv || c < 2) return 0;
        *static_cast<uint16_t*>(pv) = static_cast<uint16_t>(static_cast<Palette*>(h)->entries.size());
        return 2;
    }
    if (IsLive(h, ObjType::Brush)) {
        if (!pv || c < static_cast<int>(sizeof(LOGBRUSH))) return 0;
        Brush* b = static_cast<Brush*>(h);
        LOGBRUSH* lb = static_cast<LOGBRUSH*>(pv);
        lb->lbStyle = b->style;
        lb->lbColor = b->color;
        lb->lbHatch = 0;
        return sizeof(LOGBRUSH);
    }
    if (IsLive(h, ObjType::Font)) {
        if (!pv || c < static_cast<int>(sizeof(LOGFONTA))) return 0;
        *static_cast<LOGFONTA*>(pv) = static_cast<Font*>(h)->lf;
        return sizeof(LOGFONTA);
    }
    return 0;
}

}  // extern "C"
