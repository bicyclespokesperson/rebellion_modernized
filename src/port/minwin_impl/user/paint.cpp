// Update regions (one rectangle per window: the union of what was invalidated), WM_PAINT scheduling, BeginPaint/EndPaint and
// the window DCs. Windows are drawn directly onto the screen surface by the GDI side, clipped to the visible area computed here.
#define MINWIN_IMPLEMENTATION 1
#include <string.h>

#include "port/minwin_impl/user/user_internal.h"

namespace minwin {
namespace {

RECT ClientBounds(const Window* w) { return MakeRect(0, 0, w->clientWidth, w->clientHeight); }

// The update rectangle clipped to what is actually visible (client coordinates); empty when covered or hidden.
RECT VisibleUpdateRect(const Window* w) {
    if (!w->hasUpdate || !IsWindowVisibleTree(w)) return MakeRect(0, 0, 0, 0);
    Region visible;
    ComputeVisibleRegion(w, true, (w->style & WS_CLIPCHILDREN) != 0, (w->style & WS_CLIPSIBLINGS) != 0, &visible);
    visible.IntersectWith(Offset(w->update, ClientOriginOnScreen(w).x, ClientOriginOnScreen(w).y));
    if (visible.Empty()) return MakeRect(0, 0, 0, 0);
    const POINT o = ClientOriginOnScreen(w);
    return Offset(visible.Bounds(), -o.x, -o.y);
}

struct ExposeContext {
    RECT area;
    Window* except;
    bool eraseNow;
    Window* touched[512];
    int touchedCount;
};

void ExposeVisitor(Window* w, void* raw) {
    ExposeContext* ctx = static_cast<ExposeContext*>(raw);
    if (w == ctx->except || IsDescendant(ctx->except, w)) return;
    Region visible;
    ComputeVisibleRegion(w, true, (w->style & WS_CLIPCHILDREN) != 0, (w->style & WS_CLIPSIBLINGS) != 0, &visible);
    visible.IntersectWith(ctx->area);
    if (visible.Empty()) return;
    const POINT o = ClientOriginOnScreen(w);
    const RECT bounds = Offset(visible.Bounds(), -o.x, -o.y);
    InvalidateWindowRect(w, &bounds, true);
    if (ctx->eraseNow && ctx->touchedCount < 512) ctx->touched[ctx->touchedCount++] = w;
}

}  // namespace

void InvalidateWindowRect(Window* w, const RECT* rect, bool erase) {
    RECT r = ClientBounds(w);
    if (rect && !Intersect(&r, r, *rect)) return;
    if (IsEmpty(r)) return;
    w->update = w->hasUpdate ? Union(w->update, r) : r;
    w->hasUpdate = true;
    if (erase) w->needErase = true;
}

void ValidateWindow(Window* w) {
    w->hasUpdate = false;
    w->needErase = false;
}

void ExposeArea(const RECT& screenRect, Window* except, bool eraseNow) {
    if (IsEmpty(screenRect)) return;
    ExposeContext* ctx = static_cast<ExposeContext*>(Alloc(sizeof(ExposeContext)));
    ctx->area = screenRect;
    ctx->except = except;
    ctx->eraseNow = eraseNow;
    ForEachVisibleWindow(ExposeVisitor, ctx);
    for (int i = 0; i < ctx->touchedCount; ++i)
        if (IsLive(ctx->touched[i])) EraseWindowNow(ctx->touched[i]);
    Free(ctx);
}

HDC AcquireWindowDc(Window* w, bool clientOnly, bool clipChildren) {
    Region visible;
    ComputeVisibleRegion(w, clientOnly, clipChildren, (w->style & WS_CLIPSIBLINGS) != 0 || !w->parent, &visible);
    minwin_gdi::WindowDcDesc desc;
    const POINT origin = clientOnly ? ClientOriginOnScreen(w) : WindowOriginOnScreen(w);
    desc.window = ToHandle(w);
    desc.originX = origin.x;
    desc.originY = origin.y;
    desc.clip = visible.Rects();
    desc.clipCount = visible.Count();
    return minwin_gdi::CreateWindowDc(desc);
}

static HDC AcquireClippedDc(Window* w, const RECT& clientRect) {
    Region visible;
    ComputeVisibleRegion(w, true, (w->style & WS_CLIPCHILDREN) != 0, (w->style & WS_CLIPSIBLINGS) != 0 || !w->parent, &visible);
    const POINT o = ClientOriginOnScreen(w);
    visible.IntersectWith(Offset(clientRect, o.x, o.y));
    minwin_gdi::WindowDcDesc desc;
    desc.window = ToHandle(w);
    desc.originX = o.x;
    desc.originY = o.y;
    desc.clip = visible.Rects();
    desc.clipCount = visible.Count();
    return minwin_gdi::CreateWindowDc(desc);
}

void EraseWindowNow(Window* w) {
    if (!(w->style & WS_VISIBLE) || !IsWindowVisibleTree(w)) return;
    if (w->needNcPaint) {
        w->needNcPaint = false;
        Send(w, kWmNcPaint, 1, 0);
        if (!IsLive(w)) return;
    }
    if (!w->hasUpdate || !w->needErase) return;
    const RECT area = VisibleUpdateRect(w);
    if (IsEmpty(area)) return;
    HDC dc = AcquireClippedDc(w, area);
    const LRESULT erased = Send(w, WM_ERASEBKGND, reinterpret_cast<WPARAM>(dc), 0);
    if (IsLive(w)) {
        if (erased) w->needErase = false;
    }
    minwin_gdi::DestroyWindowDc(dc);
}

Window* FindWindowNeedingPaint() {
    struct Search {
        static Window* Visit(Window* w) {
            if (!(w->style & WS_VISIBLE)) return nullptr;
            if (w->hasUpdate) {
                if (!IsEmpty(VisibleUpdateRect(w))) return w;
                w->hasUpdate = false;
                w->needErase = false;
            }
            for (Window* c = w->lastChild; c; c = c->prev)
                if (Window* hit = Visit(c)) return hit;
            return nullptr;
        }
    };
    for (Window* t = TopLevelsTail(); t; t = t->prev)
        if (Window* hit = Search::Visit(t)) return hit;
    return nullptr;
}

}  // namespace minwin

using namespace minwin;

extern "C" {

BOOL REB_STDCALL InvalidateRect(HWND hwnd, const RECT* rect, BOOL erase) {
    if (!hwnd) {
        for (Window* t = FirstTopLevel(); t; t = t->next) InvalidateWindowRect(t, nullptr, erase != 0);
        return TRUE;
    }
    Window* w = FromHandle(hwnd);
    if (!w) return FALSE;
    InvalidateWindowRect(w, rect, erase != 0);
    return TRUE;
}

BOOL REB_STDCALL UpdateWindow(HWND hwnd) {
    Window* w = FromHandle(hwnd);
    if (!w) return FALSE;
    if (w->hasUpdate && IsWindowVisibleTree(w) && !IsEmpty(VisibleUpdateRect(w))) Send(w, WM_PAINT, 0, 0);
    return TRUE;
}

BOOL REB_STDCALL GetUpdateRect(HWND hwnd, LPRECT rect, BOOL erase) {
    Window* w = FromHandle(hwnd);
    if (!w) return FALSE;
    const RECT r = VisibleUpdateRect(w);
    if (rect) *rect = r;
    if (erase && !IsEmpty(r)) EraseWindowNow(w);
    return !IsEmpty(r);
}

HDC REB_STDCALL BeginPaint(HWND hwnd, LPPAINTSTRUCT ps) {
    Window* w = FromHandle(hwnd);
    if (!w) return nullptr;
    memset(ps, 0, sizeof *ps);
    RECT area = VisibleUpdateRect(w);
    const bool wantsErase = w->needErase;
    ps->rcPaint = area;
    // The update region is validated up front, as Windows does; the erase uses the old one.
    HDC dc = AcquireClippedDc(w, area);
    ps->hdc = dc;
    ps->fErase = FALSE;
    w->hasUpdate = false;
    if (wantsErase) {
        w->needErase = false;
        const LRESULT erased = Send(w, WM_ERASEBKGND, reinterpret_cast<WPARAM>(dc), 0);
        ps->fErase = erased ? FALSE : TRUE;
    }
    ++w->paintLock;
    return dc;
}

BOOL REB_STDCALL EndPaint(HWND hwnd, const PAINTSTRUCT* ps) {
    Window* w = FromHandle(hwnd);
    if (w && w->paintLock > 0) --w->paintLock;
    if (ps && ps->hdc) minwin_gdi::DestroyWindowDc(ps->hdc);
    return TRUE;
}

HDC REB_STDCALL GetDC(HWND hwnd) {
    if (!hwnd) {
        RECT screen = MakeRect(0, 0, minwin_gdi::kScreenWidth, minwin_gdi::kScreenHeight);
        minwin_gdi::WindowDcDesc desc;
        desc.window = nullptr;
        desc.originX = 0;
        desc.originY = 0;
        desc.clip = &screen;
        desc.clipCount = 1;
        return minwin_gdi::CreateWindowDc(desc);
    }
    Window* w = FromHandle(hwnd);
    if (!w) return nullptr;
    return AcquireWindowDc(w, true, (w->style & WS_CLIPCHILDREN) != 0);
}

HDC REB_STDCALL GetWindowDC(HWND hwnd) {
    Window* w = FromHandle(hwnd);
    if (!w) return nullptr;
    return AcquireWindowDc(w, false, (w->style & WS_CLIPCHILDREN) != 0);
}

int REB_STDCALL ReleaseDC(HWND, HDC dc) {
    if (!dc) return 0;
    minwin_gdi::DestroyWindowDc(dc);
    return 1;
}

}  // extern "C"
