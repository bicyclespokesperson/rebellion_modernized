// The small USER32 pieces: system metrics, the caret, cursors, resource-backed loaders (strings, accelerators, bitmaps), rectangle
// helpers, wsprintf and MessageBox.
#include <SDL3/SDL.h>

#define MINWIN_IMPLEMENTATION 1
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "port/minwin_impl/user/input.h"
#include "port/minwin_impl/user/user_internal.h"

SDL_Cursor* CreateCursorFromResource(HINSTANCE, LPCSTR);

namespace minwin {

int ShowMessageBox(const char* text, const char* caption, unsigned type);
void SetSdlTitle(const char* text);

namespace {

struct CursorObject {
    SDL_Cursor* sdl;
    bool stock;
};

CursorObject* g_cursor = nullptr;
int g_showCursorCounter = 0;

struct Caret {
    Window* window;
    int width, height;
    int x, y;
    int hidden;
    unsigned created;
} g_caret = {nullptr, 0, 0, 0, 0, 0, 0};

CursorObject* NewCursor(SDL_Cursor* sdl, bool stock) {
    CursorObject* c = static_cast<CursorObject*>(Alloc(sizeof(CursorObject)));
    c->sdl = sdl;
    c->stock = stock;
    return c;
}

SDL_SystemCursor SystemCursorFor(unsigned id) {
    switch (id) {
        case 32513: return SDL_SYSTEM_CURSOR_TEXT;
        case 32514: return SDL_SYSTEM_CURSOR_WAIT;
        case 32515: return SDL_SYSTEM_CURSOR_CROSSHAIR;
        case 32642: return SDL_SYSTEM_CURSOR_NWSE_RESIZE;
        case 32643: return SDL_SYSTEM_CURSOR_NESW_RESIZE;
        case 32644: return SDL_SYSTEM_CURSOR_EW_RESIZE;
        case 32645: return SDL_SYSTEM_CURSOR_NS_RESIZE;
        case 32646: return SDL_SYSTEM_CURSOR_MOVE;
        case 32648: return SDL_SYSTEM_CURSOR_NOT_ALLOWED;
        case 32649: return SDL_SYSTEM_CURSOR_POINTER;
        default: return SDL_SYSTEM_CURSOR_DEFAULT;
    }
}

void ApplyCursor() {
    const bool visible = g_showCursorCounter >= 0 && g_cursor != nullptr;
    if (visible) {
        if (g_cursor->sdl) SDL_SetCursor(g_cursor->sdl);
        SDL_ShowCursor();
    } else {
        SDL_HideCursor();
    }
}

}  // namespace

int g_showCursorCount = 0;

void DrawCaretOverlay(uint32_t* pixels, int pitch) {
    if (!g_caret.window || g_caret.hidden > 0 || !IsLive(g_caret.window) || !IsWindowVisibleTree(g_caret.window)) return;
    const unsigned phase = (PlatformTicksMs() - g_caret.created) / 530;
    if (phase & 1) return;
    Region visible;
    ComputeVisibleRegion(g_caret.window, true, (g_caret.window->style & WS_CLIPCHILDREN) != 0, true, &visible);
    const POINT o = ClientOriginOnScreen(g_caret.window);
    const RECT caret = MakeRect(o.x + g_caret.x, o.y + g_caret.y, o.x + g_caret.x + g_caret.width, o.y + g_caret.y + g_caret.height);
    for (int i = 0; i < visible.Count(); ++i) {
        RECT r;
        if (!Intersect(&r, caret, visible.Rects()[i])) continue;
        for (int y = r.top; y < r.bottom; ++y)
            for (int x = r.left; x < r.right; ++x)
                if (x >= 0 && y >= 0 && x < minwin_gdi::kScreenWidth && y < minwin_gdi::kScreenHeight) pixels[y * pitch + x] ^= 0x00ffffffu;
    }
}

}  // namespace minwin

using namespace minwin;

namespace {

HRSRC FindResourceFor(HINSTANCE instance, LPCSTR name, int type) {
    return FindResourceA(instance ? instance : GetModuleHandleA(nullptr), name, MAKEINTRESOURCE(type));
}

}  // namespace

extern "C" {

// ---- metrics, rectangles, small stubs ---------------------------------------------------------------------------------------

int REB_STDCALL GetSystemMetrics(int index) {
    switch (index) {
        case SM_CXSCREEN: case SM_CXFULLSCREEN: return minwin_gdi::kScreenWidth;
        case SM_CYSCREEN: case SM_CYFULLSCREEN: return minwin_gdi::kScreenHeight;
        case SM_CYCAPTION: return 19;
        case SM_CXBORDER: case SM_CYBORDER: return 1;
        case SM_CXDLGFRAME: case SM_CYDLGFRAME: return 3;
        case SM_MOUSEPRESENT: return 1;
        default: return 0;
    }
}

BOOL REB_STDCALL SystemParametersInfoA(UINT action, UINT, PVOID param, UINT) {
    if (action == SPI_GETWORKAREA && param) {
        *static_cast<RECT*>(param) = MakeRect(0, 0, minwin_gdi::kScreenWidth, minwin_gdi::kScreenHeight);
        return TRUE;
    }
    return FALSE;
}

HMENU REB_STDCALL GetMenu(HWND) { return nullptr; }
BOOL REB_STDCALL MessageBeep(UINT) { return TRUE; }
BOOL REB_STDCALL WinHelpA(HWND, LPCSTR, UINT, ULONG_PTR) { return TRUE; }
void REB_STDCALL InitCommonControls(void) {}

int REB_STDCALL MessageBoxA(HWND, LPCSTR text, LPCSTR caption, UINT type) { return ShowMessageBox(text, caption, type); }
int REB_STDCALL MessageBoxExA(HWND, LPCSTR text, LPCSTR caption, UINT type, WORD) { return ShowMessageBox(text, caption, type); }

BOOL REB_STDCALL SetRect(LPRECT r, int l, int t, int rr, int b) { *r = MakeRect(l, t, rr, b); return TRUE; }
BOOL REB_STDCALL SetRectEmpty(LPRECT r) { *r = MakeRect(0, 0, 0, 0); return TRUE; }
BOOL REB_STDCALL CopyRect(LPRECT dst, const RECT* src) { *dst = *src; return TRUE; }
BOOL REB_STDCALL IntersectRect(LPRECT dst, const RECT* a, const RECT* b) { return Intersect(dst, *a, *b); }
BOOL REB_STDCALL UnionRect(LPRECT dst, const RECT* a, const RECT* b) {
    *dst = Union(*a, *b);
    return !IsEmpty(*dst);
}
BOOL REB_STDCALL IsRectEmpty(const RECT* r) { return IsEmpty(*r); }
BOOL REB_STDCALL PtInRect(const RECT* r, POINT p) { return p.x >= r->left && p.x < r->right && p.y >= r->top && p.y < r->bottom; }

int REB_CDECL wsprintfA(LPSTR out, LPCSTR format, ...) {
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(out, 1024, format, args);
    va_end(args);
    return n < 0 ? 0 : (n > 1023 ? 1023 : n);
}

int REB_STDCALL wvsprintfA(LPSTR out, LPCSTR format, va_list args) {
    const int n = vsnprintf(out, 1024, format, args);
    return n < 0 ? 0 : (n > 1023 ? 1023 : n);
}

// ---- cursor ---------------------------------------------------------------------------------------------------------------

HCURSOR REB_STDCALL LoadCursorA(HINSTANCE instance, LPCSTR name) {
    EnsurePlatform();
    const uintptr_t id = reinterpret_cast<uintptr_t>(name);
    if (!instance && id < 0x10000)
        return reinterpret_cast<HCURSOR>(NewCursor(SDL_CreateSystemCursor(SystemCursorFor(static_cast<unsigned>(id))), true));
    return reinterpret_cast<HCURSOR>(NewCursor(CreateCursorFromResource(instance, name), false));
}

HICON REB_STDCALL LoadIconA(HINSTANCE, LPCSTR) {
    static int dummy;
    return reinterpret_cast<HICON>(&dummy);
}

HCURSOR REB_STDCALL SetCursor(HCURSOR handle) {
    CursorObject* old = g_cursor;
    g_cursor = reinterpret_cast<CursorObject*>(handle);
    ApplyCursor();
    return reinterpret_cast<HCURSOR>(old);
}

int REB_STDCALL ShowCursor(BOOL show) {
    g_showCursorCounter += show ? 1 : -1;
    ApplyCursor();
    return g_showCursorCounter;
}

// ---- caret ----------------------------------------------------------------------------------------------------------------

BOOL REB_STDCALL CreateCaret(HWND hwnd, HBITMAP, int width, int height) {
    Window* w = FromHandle(hwnd);
    if (!w) return FALSE;
    g_caret.window = w;
    g_caret.width = width > 0 ? width : 1;
    g_caret.height = height > 0 ? height : 16;
    g_caret.x = g_caret.y = 0;
    g_caret.hidden = 1;
    g_caret.created = PlatformTicksMs();
    return TRUE;
}

BOOL REB_STDCALL DestroyCaret(void) {
    g_caret.window = nullptr;
    return TRUE;
}

BOOL REB_STDCALL HideCaret(HWND hwnd) {
    if (hwnd && FromHandle(hwnd) != g_caret.window) return FALSE;
    if (!g_caret.window) return FALSE;
    ++g_caret.hidden;
    return TRUE;
}

BOOL REB_STDCALL ShowCaret(HWND hwnd) {
    if (hwnd && FromHandle(hwnd) != g_caret.window) return FALSE;
    if (!g_caret.window) return FALSE;
    if (g_caret.hidden > 0) --g_caret.hidden;
    g_caret.created = PlatformTicksMs();
    return TRUE;
}

BOOL REB_STDCALL SetCaretPos(int x, int y) {
    if (!g_caret.window) return FALSE;
    g_caret.x = x;
    g_caret.y = y;
    g_caret.created = PlatformTicksMs();
    return TRUE;
}

BOOL REB_STDCALL GetCaretPos(LPPOINT p) {
    if (!g_caret.window) return FALSE;
    p->x = g_caret.x;
    p->y = g_caret.y;
    return TRUE;
}

// ---- resources ------------------------------------------------------------------------------------------------------------

int REB_STDCALL LoadStringA(HINSTANCE instance, UINT id, LPSTR buffer, int max) {
    HRSRC block = FindResourceFor(instance, MAKEINTRESOURCE((id >> 4) + 1), 6);
    if (!block) return 0;
    const uint8_t* data = static_cast<const uint8_t*>(LockResource(LoadResource(instance ? instance : GetModuleHandleA(nullptr), block)));
    if (!data) return 0;
    for (unsigned i = 0; i < (id & 15); ++i) {
        uint16_t length;
        memcpy(&length, data, sizeof length);
        data += sizeof length + length * sizeof(uint16_t);
    }
    uint16_t length;
    memcpy(&length, data, sizeof length);
    data += sizeof length;
    if (max <= 0) return 0;
    int copied = 0;
    for (; copied < static_cast<int>(length) && copied < max - 1; ++copied) {
        uint16_t unit;
        memcpy(&unit, data + copied * sizeof(uint16_t), sizeof unit);
        buffer[copied] = unit < 256 ? static_cast<char>(unit) : '?';
    }
    buffer[copied] = 0;
    return copied;
}

HACCEL REB_STDCALL LoadAcceleratorsA(HINSTANCE instance, LPCSTR name) {
    HRSRC block = FindResourceFor(instance, name, 9);
    if (!block) return nullptr;
    const uint8_t* data = static_cast<const uint8_t*>(LockResource(LoadResource(instance ? instance : GetModuleHandleA(nullptr), block)));
    if (!data) return nullptr;
    int count = 0;
    for (const uint8_t* p = data;; p += 8) {
        ++count;
        if (p[0] & 0x80) break;
    }
    AcceleratorTable* table = static_cast<AcceleratorTable*>(Alloc(sizeof(AcceleratorTable) + sizeof(AccelEntry) * static_cast<size_t>(count)));
    table->count = count;
    for (int i = 0; i < count; ++i) {
        const uint8_t* p = data + i * 8;
        uint16_t key, command;
        memcpy(&key, p + 2, sizeof key);
        memcpy(&command, p + 4, sizeof command);
        table->entries[i].flags = p[0] & 0x7f;
        table->entries[i].key = key;
        table->entries[i].command = command;
    }
    return reinterpret_cast<HACCEL>(table);
}

HBITMAP REB_STDCALL LoadBitmapA(HINSTANCE instance, LPCSTR name) {
    HRSRC block = FindResourceFor(instance, name, 2);
    if (!block) return nullptr;
    const uint8_t* data = static_cast<const uint8_t*>(LockResource(LoadResource(instance ? instance : GetModuleHandleA(nullptr), block)));
    if (!data) return nullptr;
    const BITMAPINFOHEADER* header = reinterpret_cast<const BITMAPINFOHEADER*>(data);
    unsigned colors = header->biClrUsed;
    if (!colors && header->biBitCount <= 8) colors = 1u << header->biBitCount;
    const uint8_t* bits = data + header->biSize + colors * sizeof(RGBQUAD);
    HDC dc = GetDC(nullptr);
    HBITMAP bitmap = CreateDIBitmap(dc, header, CBM_INIT, bits, reinterpret_cast<const BITMAPINFO*>(data), DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    return bitmap;
}

}  // extern "C"

namespace minwin_user {

void NotifyPaletteChanged(HWND realizer) {
    for (Window* t = FirstTopLevel(); t;) {
        Window* next = t->next;
        Send(t, kWmPaletteIsChanging, reinterpret_cast<WPARAM>(realizer), 0);
        t = next;
    }
}

}  // namespace minwin_user

SDL_Cursor* CreateCursorFromResource(HINSTANCE, LPCSTR) { return SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT); }
