// minwin_impl/user: the window manager, message queue, timers, hooks and input of minwin's USER32 on SDL3.
// Single-threaded by design (docs/phase3_plan.md): every function here runs on the thread that pumps messages. The only
// concession to the game's other threads is a lock around the few entry points they call (see Lock()).
#pragma once
#include <stdint.h>

#include "port/minwin/minwin.h"
#include "port/minwin_impl/gdi_api.h"

namespace minwin {

// ---- constants the game does not use but the window manager sends or implements ---------------------------------------
enum : unsigned {
    kWmNull = 0x00, kWmEnable = 0x0a, kWmSetText = 0x0c, kWmGetText = 0x0d, kWmGetTextLength = 0x0e,
    kWmSetCursor = 0x20, kWmGetMinMaxInfo = 0x24, kWmNotifyFormat = 0x55, kWmWindowPosChanging = 0x46,
    kWmWindowPosChanged = 0x47, kWmNcCalcSize = 0x83, kWmNcHitTest = 0x84, kWmNcPaint = 0x85, kWmNcActivate = 0x86,
    kWmGetDlgCode = 0x87, kWmStyleChanging = 0x7c, kWmStyleChanged = 0x7d, kWmGetIcon = 0x7f, kWmChildActivate = 0x22,
    kWmNcMouseMove = 0xa0, kWmNcLButtonDown = 0xa1, kWmNcLButtonUp = 0xa2, kWmNcRButtonDown = 0xa4,
    kWmNcRButtonUp = 0xa5, kWmNcRButtonDblClk = 0xa6, kWmNcMButtonDown = 0xa7, kWmMouseFirst = 0x200,
    kWmMouseLast = 0x209, kWmMButtonDown = 0x207, kWmMButtonUp = 0x208, kWmMButtonDblClk = 0x209, kWmRButtonDblClk = 0x206,
    kWmParentNotify = 0x210, kWmImeSetContext = 0x281, kWmImeNotify = 0x282, kWmPaletteIsChanging = 0x310,
    kWmSysChar = 0x106, kWmMouseWheel = 0x20a, kWmCancelMode = 0x1f, kWmGetObject = 0x3d, kWmShowWindowMsg = 0x18,
    kWmSyncPaint = 0x88
};
enum : int {
    kHtError = -2, kHtTransparent = -1, kHtNowhere = 0, kHtClient = 1, kHtCaption = 2, kHtBorder = 18
};
enum : unsigned { kSwpNoSize = 1, kSwpNoMove = 2, kSwpNoZOrder = 4, kSwpNoRedraw = 8, kSwpNoActivate = 0x10,
    kSwpFrameChanged = 0x20, kSwpShowWindow = 0x40, kSwpHideWindow = 0x80, kSwpNoCopyBits = 0x100,
    kSwpNoOwnerZOrder = 0x200, kSwpNoSendChanging = 0x400, kSwpNoClientSize = 0x800, kSwpNoClientMove = 0x1000 };
enum : DWORD {
    kWsMaximizeBox = 0x10000, kWsMinimizeBox = 0x20000, kWsThickFrame = 0x40000, kWsSysMenu = 0x80000,
    kWsHScroll = 0x100000, kWsVScroll = 0x200000, kWsCaption = 0xc00000, kWsDisabled = 0x8000000,
    kWsMinimize = 0x20000000, kWsMaximize = 0x1000000, kWsExNoParentNotify = 4, kWsExTransparent = 0x20,
    kWsExToolWindow = 0x80, kWsExNoActivate = 0x08000000, kWsExTopmost = 8
};
enum : int { kGwlWndProc = -4, kGwlHInstance = -6, kGwlHwndParent = -8, kGwlId = -12, kGwlStyle = -16, kGwlExStyle = -20,
    kGwlUserData = -21 };
enum : int { kSwHide = 0, kSwShowNormal = 1, kSwShowMinimized = 2, kSwShowMaximized = 3, kSwShowNoActivate = 4, kSwShow = 5,
    kSwMinimize = 6, kSwShowMinNoActive = 7, kSwShowNA = 8, kSwRestore = 9, kSwShowDefault = 10, kSwForceMinimize = 11 };
enum : int { kSizeRestored = 0, kSizeMinimized = 1, kSizeMaximized = 2 };
enum : unsigned { kHcAction = 0, kHcNoRemove = 3 };
constexpr int kScreenWidthDefault = 640, kScreenHeightDefault = 480;

struct WindowPos {  // WINDOWPOS
    HWND hwnd;
    HWND hwndInsertAfter;
    int x, y, cx, cy;
    UINT flags;
};
struct NcCalcSizeParams {  // NCCALCSIZE_PARAMS
    RECT rgrc[3];
    WindowPos* lppos;
};
struct StyleStruct {  // STYLESTRUCT
    DWORD styleOld;
    DWORD styleNew;
};
struct MinMaxInfo {  // MINMAXINFO
    POINT ptReserved, ptMaxSize, ptMaxPosition, ptMinTrackSize, ptMaxTrackSize;
};
struct AccelEntry {  // ACCEL as stored in an RT_ACCELERATOR resource
    uint8_t flags;
    uint16_t key;
    uint16_t command;
};

// ---- rectangles and regions (rect lists: everything the game draws is rectangular) ------------------------------------
inline int Width(const RECT& r) { return r.right - r.left; }
inline int Height(const RECT& r) { return r.bottom - r.top; }
inline bool IsEmpty(const RECT& r) { return r.left >= r.right || r.top >= r.bottom; }
inline RECT MakeRect(int l, int t, int r, int b) { RECT x; x.left = l; x.top = t; x.right = r; x.bottom = b; return x; }
inline bool Intersect(RECT* out, const RECT& a, const RECT& b) {
    RECT r = MakeRect(a.left > b.left ? a.left : b.left, a.top > b.top ? a.top : b.top,
                      a.right < b.right ? a.right : b.right, a.bottom < b.bottom ? a.bottom : b.bottom);
    if (IsEmpty(r)) { *out = MakeRect(0, 0, 0, 0); return false; }
    *out = r;
    return true;
}
inline RECT Offset(const RECT& r, int dx, int dy) { return MakeRect(r.left + dx, r.top + dy, r.right + dx, r.bottom + dy); }
inline RECT Union(const RECT& a, const RECT& b) {
    if (IsEmpty(a)) return b;
    if (IsEmpty(b)) return a;
    return MakeRect(a.left < b.left ? a.left : b.left, a.top < b.top ? a.top : b.top,
                    a.right > b.right ? a.right : b.right, a.bottom > b.bottom ? a.bottom : b.bottom);
}

// A set of non-overlapping rectangles.
class Region {
public:
    Region() = default;
    ~Region();
    Region(const Region&) = delete;
    Region& operator=(const Region&) = delete;
    void Clear() { count_ = 0; }
    void Set(const RECT& r);
    void CopyFrom(const Region& other);
    void Subtract(const RECT& r);
    void IntersectWith(const RECT& r);
    bool Empty() const { return count_ == 0; }
    int Count() const { return count_; }
    const RECT* Rects() const { return rects_; }
    RECT Bounds() const;
    bool Contains(int x, int y) const;
private:
    void Reserve(int n);
    RECT* rects_ = nullptr;
    int count_ = 0;
    int capacity_ = 0;
};

// ---- classes and windows ---------------------------------------------------------------------------------------------
struct WindowClass {
    char name[80];
    ATOM atom;
    UINT style;
    WNDPROC proc;
    int clsExtra;
    int wndExtra;
    HINSTANCE instance;
    HCURSOR cursor;
    HBRUSH background;
    bool builtin;
    WindowClass* next;
};

struct Window {
    Window* parent;       // null: a top-level window (child of the desktop)
    Window* owner;
    Window* firstChild;   // z-order: first is topmost
    Window* lastChild;
    Window* prev;         // sibling above
    Window* next;         // sibling below
    WindowClass* cls;
    WNDPROC proc;
    DWORD style;
    DWORD exStyle;
    LONG_PTR id;
    HINSTANCE instance;
    LONG_PTR userData;
    int x, y, width, height;  // window rectangle, relative to the parent's client origin (screen coordinates for top-level)
    int clientLeft, clientTop, clientWidth, clientHeight;  // client rectangle relative to the window origin
    char* text;
    uint8_t* extra;
    int extraBytes;
    RECT update;          // client coordinates; valid when hasUpdate
    bool hasUpdate;
    bool needErase;
    bool needNcPaint;
    uint32_t serial;      // creation order
    bool needSize;        // WM_SIZE/WM_MOVE not yet sent after creation
    bool destroying;
    bool iconic;
    bool enabledFlag() const { return !(style & kWsDisabled); }
    int paintLock;        // BeginPaint nesting
    int caretHidden;
    int dispatchDepth;
};

struct Hook {
    int type;
    HOOKPROC proc;
    HINSTANCE module;
    DWORD thread;
    Hook* next;           // next older hook of the same type
    bool dead;
};

// ---- window manager (window.cpp) --------------------------------------------------------------------------------------
Window* FromHandle(HWND hwnd);                        // null unless a live window
inline HWND ToHandle(const Window* w) { return reinterpret_cast<HWND>(const_cast<Window*>(w)); }
bool IsLive(const Window* w);
Window* TopLevelOf(Window* w);
Window* RootOwner(Window* w);
bool IsDescendant(const Window* ancestor, const Window* w);  // strict
bool IsWindowVisibleTree(const Window* w);                    // WS_VISIBLE on the window and every ancestor
bool IsEnabledTree(const Window* w);
POINT ClientOriginOnScreen(const Window* w);
POINT WindowOriginOnScreen(const Window* w);
RECT ClientRectOnScreen(const Window* w);
RECT WindowRectOnScreen(const Window* w);
void ComputeVisibleRegion(const Window* w, bool clientOnly, bool clipChildren, bool clipSiblings, Region* out);  // screen coordinates
Window* FirstTopLevel();
Window* TopLevelsHead();
Window* TopLevelsTail();
Window* WindowAtPoint(POINT screen, bool skipDisabled, bool sendHitTest, int* hitTest);  // deepest window under a screen point
Window* ChildAtPoint(Window* parent, POINT clientPoint, UINT flags);
void LinkAfter(Window* w, Window* parent, Window* after);  // after == null: topmost
void Unlink(Window* w);
void ForEachVisibleWindow(void (*fn)(Window*, void*), void* ctx);  // top-down, parents before children

// Activation, focus, capture (focus.cpp)
Window* ActiveWindow();
Window* FocusWindow();
Window* CaptureWindow();
bool AppActive();
void ActivateTopLevel(Window* w, bool mouse);
void ActivateApp(bool active);
void SetFocusWindow(Window* w, bool notifyOnly = false);
void ReleaseCaptureSilently();
void SetCaptureWindow(Window* w);
void OnWindowDestroyed(Window* w);   // drops focus/active/capture references
void OnWindowHidden(Window* w);

// ---- messages (message.cpp) -------------------------------------------------------------------------------------------
LRESULT Send(Window* w, UINT msg, WPARAM wp, LPARAM lp);
bool Post(Window* w, UINT msg, WPARAM wp, LPARAM lp);
LRESULT DefaultWindowProc(Window* w, UINT msg, WPARAM wp, LPARAM lp);
void PumpPlatform();                           // SDL events -> input queue, timers, presentation
bool NextMessage(MSG* out, Window* filter, UINT min, UINT max, bool remove, bool wait);
void DispatchTimersAndMmTimers();
unsigned GetTicksMs();
void PurgeMessagesFor(Window* w);
void PurgeTimersFor(Window* w);
int NestingDepth();
bool IsOwnerThread();
void ProcessCrossThreadRequests();
LRESULT SendFromOtherThread(Window* w, UINT msg, WPARAM wp, LPARAM lp);
void TraceCreateNote(const char* what);

// Hooks
LRESULT CallHooks(int type, int code, WPARAM wp, LPARAM lp);
bool HasHook(int type);

// ---- painting (paint.cpp) ---------------------------------------------------------------------------------------------
void InvalidateWindowRect(Window* w, const RECT* clientRect, bool erase);  // null rect: the whole client area
void ValidateWindow(Window* w);
void EraseWindowNow(Window* w);                // WM_NCPAINT + WM_ERASEBKGND for the invalid part, like SetWindowPos does
Window* FindWindowNeedingPaint();
void ExposeArea(const RECT& screenRect, Window* except, bool eraseNow);  // invalidate the visible windows under an uncovered area
void SendPaintMessageFor(Window* w);           // GetMessage's WM_PAINT delivery preparation (WM_NCPAINT); returns after queuing
HDC AcquireWindowDc(Window* w, bool clientOnly, bool clipChildren);
void ReleaseWindowDc(HDC dc);
void DrawCaretOverlay(uint32_t* pixels, int pitchPixels);

// ---- input (input.cpp) ------------------------------------------------------------------------------------------------
void InitInput();
POINT CursorScreenPosition();
void SetCursorScreenPosition(POINT p);
void UpdateCursorForWindow(Window* underMouse, int hitTest, UINT mouseMessage);
extern int g_showCursorCount;

// ---- platform (platform.cpp) -----------------------------------------------------------------------------------------
bool EnsurePlatform();                         // SDL init + window; false if the platform could not be brought up
void PresentScreen();                          // convert the screen surface through the palette and show it
void ShutdownPlatform();
bool OutlinesEnabled();

// ---- misc ---------------------------------------------------------------------------------------------------------------
void* Alloc(size_t n);                         // zeroed
void Free(void* p);
char* DuplicateString(const char* s);
inline LPARAM MakeLParam(int lo, int hi) { return static_cast<LPARAM>((static_cast<uint32_t>(lo) & 0xffff) | (static_cast<uint32_t>(hi) << 16)); }

}  // namespace minwin
