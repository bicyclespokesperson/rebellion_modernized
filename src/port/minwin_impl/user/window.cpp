// Window classes, the window tree, creation and destruction, geometry, visibility, z-order, hit testing.
#include <cstdlib>
#include <cstring>

#include <vector>

#include "port/minwin_impl/user/user_internal.h"

namespace minwin
{
namespace
{

// Top-level windows other than tooltips (see MarkLayoutChanged in input.cpp).
bool MovesPointerOnChange(const Window* w)
{
  return !(w->style & WS_CHILD) && w->proc != TooltipProc;
}

WindowClass* g_classes = nullptr;
ATOM g_nextAtom = 0xc000;
Window* g_topLevelHead = nullptr; // desktop children, topmost first
Window* g_topLevelTail = nullptr;
uint32_t g_serial = 0;

// HWNDs are small integers allocated like the Windows/Wine server does: a slot index (handle = 0x20 + 2 * index, in the
// low 16 bits) and a generation (high 16 bits) that grows each time the slot is reused; freed slots are reused last-in
// first-out. The game keeps windows in trees keyed by the handle value (CoolWindow::m_linkedWindows), so the order of
// the values decides the order children are shown and hidden in; Wine's order is recycled slots after fresh ones, with
// the generation as the major key.
constexpr unsigned kFirstHandle = 0x20;
constexpr unsigned kHandleSlots = 0x7fe0;
struct HandleSlot {
  Window* window;
  uint16_t generation;
  int nextFree;
};
HandleSlot g_handleSlots[kHandleSlots];
int g_firstFree = -1;
unsigned g_slotHighWater = 0;

uintptr_t AllocateHandle(Window* w)
{
  int index;
  if (g_firstFree >= 0) {
    index = g_firstFree;
    g_firstFree = g_handleSlots[index].nextFree;
  } else if (g_slotHighWater < kHandleSlots) {
    index = static_cast<int>(g_slotHighWater++);
  } else {
    return 0;
  }
  HandleSlot& slot = g_handleSlots[index];
  if (++slot.generation >= 0xffff) {
    slot.generation = 1;
  }
  slot.window = w;
  return (static_cast<uintptr_t>(slot.generation) << 16) | (static_cast<unsigned>(index) * 2 + kFirstHandle);
}

void ReleaseHandle(const Window* w)
{
  const auto low = static_cast<unsigned>(w->handle & 0xffff);
  if (low < kFirstHandle || (low & 1)) {
    return;
  }
  const unsigned index = (low - kFirstHandle) / 2;
  if (index >= kHandleSlots || g_handleSlots[index].window != w) {
    return;
  }
  g_handleSlots[index].window = nullptr;
  g_handleSlots[index].nextFree = g_firstFree;
  g_firstFree = static_cast<int>(index);
}

constexpr int kLiveSlots = 16384;
Window* g_live[kLiveSlots];
Window* const kTombstone = reinterpret_cast<Window*>(1);

size_t Hash(const Window* w)
{
  return (reinterpret_cast<uintptr_t>(w) >> 4) * 2654435761u % kLiveSlots;
}

void LiveInsert(Window* w)
{
  size_t i = Hash(w);
  while (g_live[i] && g_live[i] != kTombstone) {
    i = (i + 1) % kLiveSlots;
  }
  g_live[i] = w;
}

void LiveRemove(Window* w)
{
  ReleaseHandle(w);
  size_t i = Hash(w);
  for (int n = 0; n < kLiveSlots && g_live[i]; ++n, i = (i + 1) % kLiveSlots) {
    if (g_live[i] == w) {
      g_live[i] = kTombstone;
      return;
    }
  }
}

WindowClass* FindClass(const char* name)
{
  for (WindowClass* c = g_classes; c; c = c->next) {
    if (!_stricmp(c->name, name)) {
      return c;
    }
  }
  return nullptr;
}

WindowClass* FindClassByAtom(ATOM atom)
{
  for (WindowClass* c = g_classes; c; c = c->next) {
    if (c->atom == atom) {
      return c;
    }
  }
  return nullptr;
}

WindowClass* ClassFromArgument(LPCSTR name)
{
  if (reinterpret_cast<uintptr_t>(name) < 0x10000) {
    return FindClassByAtom(static_cast<ATOM>(reinterpret_cast<uintptr_t>(name)));
  }
  return FindClass(name);
}

void EnsureBuiltinClasses()
{
  static bool done = false;
  if (done) {
    return;
  }
  done = true;
  WNDCLASSA tooltip;
  memset(&tooltip, 0, sizeof tooltip);
  tooltip.style = CS_DBLCLKS;
  tooltip.lpfnWndProc = TooltipProc;
  tooltip.lpszClassName = "tooltips_class32";
  reb::compat::RegisterClassA(&tooltip);
  g_classes->builtin = true;
}

void NcInsets(DWORD style, int* left, int* top, int* right, int* bottom)
{
  int frame = 0;
  if (style & WS_BORDER) {
    frame += 1;
  }
  if (style & WS_DLGFRAME) {
    frame += 3;
  }
  if (style & kWsThickFrame) {
    frame += 4;
  }
  *left = *right = *bottom = frame;
  *top = frame + ((style & kWsCaption) == kWsCaption ? 19 : 0);
}

} // namespace

Window* FromHandle(HWND hwnd)
{
  const auto value = reinterpret_cast<uintptr_t>(hwnd);
  const auto low = static_cast<unsigned>(value & 0xffff);
  if (value > 0xffffffffu || low < kFirstHandle || (low & 1)) {
    return nullptr;
  }
  const unsigned index = (low - kFirstHandle) / 2;
  if (index >= kHandleSlots) {
    return nullptr;
  }
  Window* w = g_handleSlots[index].window;
  return w && w->handle == value ? w : nullptr;
}

bool IsLive(const Window* w)
{
  if (!w || w == kTombstone) {
    return false;
  }
  size_t i = Hash(w);
  for (int n = 0; n < kLiveSlots && g_live[i]; ++n, i = (i + 1) % kLiveSlots) {
    if (g_live[i] == w) {
      return true;
    }
  }
  return false;
}

Window* FirstTopLevel()
{
  return g_topLevelHead;
}
Window* TopLevelsHead()
{
  return g_topLevelHead;
}
Window* TopLevelsTail()
{
  return g_topLevelTail;
}

Window* TopLevelOf(Window* w)
{
  while (w && w->parent) {
    w = w->parent;
  }
  return w;
}

Window* RootOwner(Window* w)
{
  while (w && (w->parent || w->owner)) {
    w = w->parent ? w->parent : w->owner;
  }
  return w;
}

bool IsDescendant(const Window* ancestor, const Window* w)
{
  for (const Window* p = w ? w->parent : nullptr; p; p = p->parent) {
    if (p == ancestor) {
      return true;
    }
  }
  return false;
}

bool IsWindowVisibleTree(const Window* w)
{
  for (; w; w = w->parent) {
    if (!(w->style & WS_VISIBLE)) {
      return false;
    }
  }
  return true;
}

bool IsEnabledTree(const Window* w)
{
  for (; w; w = w->parent) {
    if (w->style & kWsDisabled) {
      return false;
    }
  }
  return true;
}

POINT WindowOriginOnScreen(const Window* w)
{
  POINT p = {0, 0};
  for (; w; w = w->parent) {
    p.x += w->x;
    p.y += w->y;
    if (w->parent) {
      p.x += w->parent->clientLeft;
      p.y += w->parent->clientTop;
    }
  }
  return p;
}

POINT ClientOriginOnScreen(const Window* w)
{
  POINT p = WindowOriginOnScreen(w);
  p.x += w->clientLeft;
  p.y += w->clientTop;
  return p;
}

RECT WindowRectOnScreen(const Window* w)
{
  const POINT o = WindowOriginOnScreen(w);
  return MakeRect(o.x, o.y, o.x + w->width, o.y + w->height);
}

RECT ClientRectOnScreen(const Window* w)
{
  const POINT o = ClientOriginOnScreen(w);
  return MakeRect(o.x, o.y, o.x + w->clientWidth, o.y + w->clientHeight);
}

void LinkAfter(Window* w, Window* parent, Window* after)
{
  // `after` is a sibling that ends up directly above w; null puts w on top.
  Window*& head = parent ? parent->firstChild : g_topLevelHead;
  Window*& tail = parent ? parent->lastChild : g_topLevelTail;
  w->parent = parent;
  if (!after) {
    w->prev = nullptr;
    w->next = head;
    if (head) {
      head->prev = w;
    }
    head = w;
    if (!tail) {
      tail = w;
    }
    return;
  }
  w->prev = after;
  w->next = after->next;
  if (after->next) {
    after->next->prev = w;
  } else {
    tail = w;
  }
  after->next = w;
}

void Unlink(Window* w)
{
  Window*& head = w->parent ? w->parent->firstChild : g_topLevelHead;
  Window*& tail = w->parent ? w->parent->lastChild : g_topLevelTail;
  if (w->prev) {
    w->prev->next = w->next;
  } else {
    head = w->next;
  }
  if (w->next) {
    w->next->prev = w->prev;
  } else {
    tail = w->prev;
  }
  w->prev = w->next = nullptr;
}

static void SubtractVisibleChildren(const Window* w, Region* region)
{
  for (const Window* c = w->firstChild; c; c = c->next) {
    if (c->style & WS_VISIBLE) {
      region->Subtract(WindowRectOnScreen(c));
    }
  }
}

// The part of w that is not covered by windows above it: ancestors' clip, higher siblings (always for top-level
// windows, for children only with WS_CLIPSIBLINGS), and visible children when clipChildren.
void ComputeVisibleRegion(const Window* w, bool clientOnly, bool clipChildren, bool clipSiblings, Region* out)
{
  out->Clear();
  if (!IsWindowVisibleTree(w)) {
    return;
  }
  out->Set(clientOnly ? ClientRectOnScreen(w) : WindowRectOnScreen(w));
  for (const Window* node = w; node; node = node->parent) {
    if (node->parent) {
      out->IntersectWith(ClientRectOnScreen(node->parent));
    }
    const bool clips = node == w ? clipSiblings || !node->parent : (node->style & WS_CLIPSIBLINGS) || !node->parent;
    if (clips) {
      for (const Window* above = node->prev; above; above = above->prev) {
        if (above->style & WS_VISIBLE) {
          out->Subtract(WindowRectOnScreen(above));
        }
      }
    }
    if (node != w && node->parent && !(node->parent->style & WS_CLIPCHILDREN)) {
      // a sibling subtree not clipped by the parent: nothing to remove for the ancestors' own children
    }
  }
  if (clipChildren) {
    SubtractVisibleChildren(w, out);
  }
}

Window* ChildAtPoint(Window* parent, POINT client, UINT flags)
{
  if (client.x < 0 || client.y < 0 || client.x >= parent->clientWidth || client.y >= parent->clientHeight) {
    return nullptr;
  }
  for (Window* c = parent->firstChild; c; c = c->next) {
    if ((flags & 1) && !(c->style & WS_VISIBLE)) {
      continue;
    }
    if ((flags & 2) && (c->style & kWsDisabled)) {
      continue;
    }
    if ((flags & 4) && (c->exStyle & kWsExTransparent)) {
      continue;
    }
    const int x = client.x - c->x - parent->clientLeft + parent->clientLeft;
    const int y = client.y - c->y;
    (void)x;
    (void)y;
    const RECT r = MakeRect(c->x, c->y, c->x + c->width, c->y + c->height);
    const RECT rc = MakeRect(r.left, r.top, r.right, r.bottom);
    if (client.x >= rc.left && client.x < rc.right && client.y >= rc.top && client.y < rc.bottom) {
      return c;
    }
  }
  return parent;
}

// Children's x,y are relative to the parent's client origin, so a point in the parent's client space tests directly.
static Window* Descend(Window* w, POINT clientPoint, bool skipDisabled, bool sendHitTest, int* hit)
{
  for (Window* c = w->firstChild; c; c = c->next) {
    if (!(c->style & WS_VISIBLE)) {
      continue;
    }
    if (skipDisabled && (c->style & kWsDisabled)) {
      continue;
    }
    if (clientPoint.x < c->x || clientPoint.y < c->y || clientPoint.x >= c->x + c->width
        || clientPoint.y >= c->y + c->height) {
      continue;
    }
    POINT inChild = {clientPoint.x - c->x - c->clientLeft, clientPoint.y - c->y - c->clientTop};
    if (Window* deeper = Descend(c, inChild, skipDisabled, sendHitTest, hit)) {
      return deeper;
    }
    if (sendHitTest) {
      POINT screen = {0, 0};
      const POINT o = ClientOriginOnScreen(c);
      screen.x = o.x + inChild.x;
      screen.y = o.y + inChild.y;
      const int code = static_cast<int>(Send(c, kWmNcHitTest, 0, MakeLParam(screen.x, screen.y)));
      if (code == kHtTransparent) {
        continue;
      }
      if (hit) {
        *hit = code;
      }
    } else if (hit) {
      *hit = kHtClient;
    }
    return c;
  }
  return nullptr;
}

Window* WindowAtPoint(POINT screen, bool skipDisabled, bool sendHitTest, int* hit)
{
  for (Window* top = g_topLevelHead; top; top = top->next) {
    if (!(top->style & WS_VISIBLE)) {
      continue;
    }
    if (skipDisabled && (top->style & kWsDisabled)) {
      continue;
    }
    if (screen.x < top->x || screen.y < top->y || screen.x >= top->x + top->width || screen.y >= top->y + top->height) {
      continue;
    }
    POINT inClient = {screen.x - top->x - top->clientLeft, screen.y - top->y - top->clientTop};
    if (Window* deeper = Descend(top, inClient, skipDisabled, sendHitTest, hit)) {
      return deeper;
    }
    if (sendHitTest) {
      const int code = static_cast<int>(Send(top, kWmNcHitTest, 0, MakeLParam(screen.x, screen.y)));
      if (code == kHtTransparent) {
        continue;
      }
      if (hit) {
        *hit = code;
      }
    } else if (hit) {
      *hit = kHtClient;
    }
    return top;
  }
  return nullptr;
}

void ForEachVisibleWindow(void (*fn)(Window*, void*), void* ctx)
{
  struct Walk {
    static void Visit(Window* w, void (*f)(Window*, void*), void* c)
    {
      if (!(w->style & WS_VISIBLE)) {
        return;
      }
      f(w, c);
      for (Window* child = w->lastChild; child; child = child->prev) {
        Visit(child, f, c);
      }
    }
  };
  for (Window* top = g_topLevelTail; top; top = top->prev) {
    Walk::Visit(top, fn, ctx);
  }
}

void RegisterBuiltinClasses()
{
  EnsureBuiltinClasses();
}

} // namespace minwin

using namespace minwin;

// ---- classes
// ---------------------------------------------------------------------------------------------------------

ATOM REB_STDCALL reb::compat::RegisterClassA(const WNDCLASSA* cls)
{
  if (!cls || !cls->lpszClassName || reinterpret_cast<uintptr_t>(cls->lpszClassName) < 0x10000) {
    return 0;
  }
  if (FindClass(cls->lpszClassName)) {
    return 0;
  }
  auto* c = static_cast<WindowClass*>(Alloc(sizeof(WindowClass)));
  strncpy(c->name, cls->lpszClassName, sizeof c->name - 1);
  c->atom = g_nextAtom++;
  c->style = cls->style;
  c->proc = cls->lpfnWndProc;
  c->clsExtra = cls->cbClsExtra;
  c->wndExtra = cls->cbWndExtra;
  c->instance = cls->hInstance;
  c->cursor = cls->hCursor;
  c->background = cls->hbrBackground;
  c->next = g_classes;
  g_classes = c;
  return c->atom;
}

BOOL REB_STDCALL reb::compat::GetClassInfoA(HINSTANCE, LPCSTR name, LPWNDCLASSA out)
{
  EnsureBuiltinClasses();
  const WindowClass* c = ClassFromArgument(name);
  if (!c) {
    return FALSE;
  }
  memset(out, 0, sizeof *out);
  out->style = c->style;
  out->lpfnWndProc = c->proc;
  out->cbClsExtra = c->clsExtra;
  out->cbWndExtra = c->wndExtra;
  out->hInstance = c->instance;
  out->hCursor = c->cursor;
  out->hbrBackground = c->background;
  out->lpszClassName = c->name;
  return TRUE;
}

int REB_STDCALL reb::compat::GetClassNameA(HWND hwnd, LPSTR out, int max)
{
  const Window* w = FromHandle(hwnd);
  if (!w || max <= 0) {
    return 0;
  }
  strncpy(out, w->cls->name, static_cast<size_t>(max) - 1);
  out[max - 1] = 0;
  return static_cast<int>(strlen(out));
}

// ---- creation and destruction
// ----------------------------------------------------------------------------------------

HWND REB_STDCALL reb::compat::CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR title, DWORD style, int x, int y, int width,
                                 int height, HWND parentHandle, HMENU menu, HINSTANCE instance, LPVOID param)
{
  EnsureBuiltinClasses();
  EnsurePlatform();
  WindowClass* cls = ClassFromArgument(className);
  if (!cls) {
    return nullptr;
  }
  Window* parent = FromHandle(parentHandle);
  auto* w = static_cast<Window*>(Alloc(sizeof(Window)));
  w->cls = cls;
  w->proc = cls->proc;
  w->style = style;
  w->exStyle = exStyle;
  w->instance = instance;
  w->serial = ++g_serial;
  const bool isChild = (style & WS_CHILD) && parent;
  if (isChild) {
    w->id = reinterpret_cast<LONG_PTR>(menu);
  } else if (parent) {
    w->owner = RootOwner(parent);
  }
  if (x == CW_USEDEFAULT) {
    x = 0;
  }
  if (y == CW_USEDEFAULT) {
    y = 0;
  }
  if (width == CW_USEDEFAULT) {
    width = isChild ? 0 : kScreenWidthDefault;
  }
  if (height == CW_USEDEFAULT) {
    height = isChild ? 0 : kScreenHeightDefault;
  }
  w->x = x;
  w->y = y;
  w->width = width;
  w->height = height;
  w->extraBytes = (cls->wndExtra + static_cast<int>(sizeof(LONG_PTR)) - 1) / static_cast<int>(sizeof(LONG_PTR))
                  * static_cast<int>(sizeof(LONG_PTR));
  w->extra = static_cast<uint8_t*>(Alloc(static_cast<size_t>(w->extraBytes)));
  w->needSize = true;
  w->needNcPaint = true;
  w->text = DuplicateString(title);
  w->style &= ~static_cast<DWORD>(WS_VISIBLE);
  const DWORD wantVisible = style & WS_VISIBLE;

  // New windows go on top of their siblings; top-level windows below any topmost ones.
  Window* after = nullptr;
  if (!isChild && !(exStyle & kWsExTopmost)) {
    for (Window* t = FirstTopLevel(); t && (t->exStyle & kWsExTopmost); t = t->next) {
      after = t;
    }
  }
  LinkAfter(w, isChild ? parent : nullptr, after);
  LiveInsert(w);
  w->handle = AllocateHandle(w);
  if (!w->handle) {
    Unlink(w);
    LiveRemove(w);
    Free(w->text);
    Free(w->extra);
    Free(w);
    return nullptr;
  }

  CREATESTRUCTA cs;
  memset(&cs, 0, sizeof cs);
  cs.lpCreateParams = param;
  cs.hInstance = instance;
  cs.hMenu = menu;
  cs.hwndParent = parentHandle;
  cs.cy = height;
  cs.cx = width;
  cs.y = y;
  cs.x = x;
  cs.style = static_cast<LONG>(style);
  cs.lpszName = title;
  cs.lpszClass = cls->name;
  cs.dwExStyle = exStyle;
  w->style |= wantVisible ? 0 : 0;
  if (!Send(w, WM_NCCREATE, 0, reinterpret_cast<LPARAM>(&cs))) {
    if (IsLive(w)) {
      Send(w, WM_NCDESTROY, 0, 0);
      OnWindowDestroyed(w);
      Unlink(w);
      LiveRemove(w);
      Free(w->text);
      Free(w->extra);
      Free(w);
    }
    return nullptr;
  }
  RECT rect = MakeRect(x, y, x + width, y + height);
  Send(w, kWmNcCalcSize, FALSE, reinterpret_cast<LPARAM>(&rect));
  w->clientLeft = rect.left - x;
  w->clientTop = rect.top - y;
  w->clientWidth = rect.right - rect.left;
  w->clientHeight = rect.bottom - rect.top;
  if (Send(w, WM_CREATE, 0, reinterpret_cast<LPARAM>(&cs)) == -1) {
    if (IsLive(w)) {
      reb::compat::DestroyWindow(ToHandle(w));
    }
    return nullptr;
  }
  if (!IsLive(w)) {
    return nullptr;
  }
  if (w->needSize) {
    w->needSize = false;
    Send(w, WM_SIZE, kSizeRestored, MakeLParam(w->clientWidth, w->clientHeight));
    if (!IsLive(w)) {
      return nullptr;
    }
    Send(w, WM_MOVE, 0, MakeLParam(w->x + (isChild ? 0 : 0) + w->clientLeft, w->y + w->clientTop));
    if (!IsLive(w)) {
      return nullptr;
    }
  }
  if (isChild && !(exStyle & kWsExNoParentNotify)) {
    Send(parent, kWmParentNotify, MAKEWPARAM(WM_CREATE, static_cast<WORD>(w->id)),
         reinterpret_cast<LPARAM>(ToHandle(w)));
    if (!IsLive(w)) {
      return nullptr;
    }
  }
  if (wantVisible) {
    reb::compat::ShowWindow(ToHandle(w), kSwShow);
  }
  return IsLive(w) ? ToHandle(w) : nullptr;
}

namespace minwin
{

static void SendDestroyMessages(Window* w)
{
  Send(w, WM_DESTROY, 0, 0);
  if (!IsLive(w)) {
    return;
  }
  // Creation order, not z-order: the children's WM_DESTROY follow the parent's, all before any WM_NCDESTROY.
  Window* order[4096];
  int count = 0;
  for (Window* c = w->lastChild; c && count < 4096; c = c->prev) {
    order[count++] = c;
  }
  for (int i = 0; i < count; ++i) {
    if (IsLive(order[i])) {
      SendDestroyMessages(order[i]);
    }
  }
}

static void FinishDestroy(Window* w)
{
  Window* order[4096];
  int count = 0;
  for (Window* c = w->lastChild; c && count < 4096; c = c->prev) {
    order[count++] = c;
  }
  for (int i = 0; i < count; ++i) {
    if (IsLive(order[i])) {
      FinishDestroy(order[i]);
    }
  }
  if (!IsLive(w)) {
    return;
  }
  Send(w, WM_NCDESTROY, 0, 0);
  OnWindowDestroyed(w);
  PurgeMessagesFor(w);
  PurgeTimersFor(w);
  Unlink(w);
  LiveRemove(w);
  Free(w->text);
  Free(w->extra);
  Free(w);
}

} // namespace minwin

BOOL REB_STDCALL reb::compat::DestroyWindow(HWND handle)
{
  Window* w = FromHandle(handle);
  if (!w || w->destroying) {
    return FALSE;
  }
  w->destroying = true;
  const bool isChild = (w->style & WS_CHILD) && w->parent;
  const bool movesPointer = MovesPointerOnChange(w);
  Window* parent = w->parent;
  // Owned windows go first.
  for (Window* t = FirstTopLevel(); t;) {
    Window* next = t->next;
    if (t->owner == w) {
      reb::compat::DestroyWindow(ToHandle(t));
    }
    t = next;
    if (t && !IsLive(t)) {
      t = FirstTopLevel();
    }
  }
  if (isChild && !(w->exStyle & kWsExNoParentNotify) && parent && !parent->destroying) {
    Send(parent, kWmParentNotify, MAKEWPARAM(WM_DESTROY, static_cast<WORD>(w->id)),
         reinterpret_cast<LPARAM>(ToHandle(w)));
  }
  if (!IsLive(w)) {
    return TRUE;
  }
  if (w->style & WS_VISIBLE) {
    if (IsWindowVisibleTree(w)) {
      reb::compat::ShowWindow(handle, kSwHide);
    } else {
      Send(w, kWmShowWindowMsg, 0, 0);
      if (IsLive(w)) {
        w->style &= ~static_cast<DWORD>(WS_VISIBLE);
      }
    }
  }
  if (!IsLive(w)) {
    return TRUE;
  }
  OnWindowHidden(w);
  SendDestroyMessages(w);
  if (!IsLive(w)) {
    return TRUE;
  }
  FinishDestroy(w);
  if (movesPointer) {
    MarkLayoutChanged();
  }
  return TRUE;
}

BOOL REB_STDCALL reb::compat::IsWindow(HWND h)
{
  return FromHandle(h) != nullptr;
}

BOOL REB_STDCALL reb::compat::IsChild(HWND ancestor, HWND h)
{
  const Window* w = FromHandle(h);
  const Window* a = FromHandle(ancestor);
  return a && w && IsDescendant(a, w);
}

BOOL REB_STDCALL reb::compat::IsWindowVisible(HWND h)
{
  const Window* w = FromHandle(h);
  return w && IsWindowVisibleTree(w);
}

BOOL REB_STDCALL reb::compat::IsIconic(HWND h)
{
  const Window* w = FromHandle(h);
  return w && w->iconic;
}

BOOL REB_STDCALL reb::compat::IsZoomed(HWND h)
{
  const Window* w = FromHandle(h);
  return w && (w->style & kWsMaximize);
}

BOOL REB_STDCALL reb::compat::IsWindowEnabled(HWND h)
{
  const Window* w = FromHandle(h);
  return w && !(w->style & kWsDisabled);
}

BOOL REB_STDCALL reb::compat::EnableWindow(HWND h, BOOL enable)
{
  Window* w = FromHandle(h);
  if (!w) {
    return FALSE;
  }
  const bool wasDisabled = (w->style & kWsDisabled) != 0;
  if (enable && wasDisabled) {
    w->style &= ~static_cast<DWORD>(kWsDisabled);
    Send(w, kWmEnable, TRUE, 0);
  } else if (!enable && !wasDisabled) {
    if (CaptureWindow() == w || (CaptureWindow() && IsDescendant(w, CaptureWindow()))) {
      reb::compat::ReleaseCapture();
    }
    Send(w, kWmCancelMode, 0, 0);
    if (!IsLive(w)) {
      return wasDisabled;
    }
    w->style |= kWsDisabled;
    Send(w, kWmEnable, FALSE, 0);
    if (FocusWindow() == w || (FocusWindow() && IsDescendant(w, FocusWindow()))) {
      reb::compat::SetFocus(nullptr);
    }
  }
  return wasDisabled;
}

// ---- geometry
// ---------------------------------------------------------------------------------------------------------

BOOL REB_STDCALL reb::compat::GetClientRect(HWND h, LPRECT r)
{
  const Window* w = FromHandle(h);
  if (!w) {
    return FALSE;
  }
  *r = MakeRect(0, 0, w->clientWidth, w->clientHeight);
  return TRUE;
}

BOOL REB_STDCALL reb::compat::GetWindowRect(HWND h, LPRECT r)
{
  const Window* w = FromHandle(h);
  if (!w) {
    return FALSE;
  }
  *r = WindowRectOnScreen(w);
  return TRUE;
}

BOOL REB_STDCALL reb::compat::AdjustWindowRectEx(LPRECT r, DWORD style, BOOL, DWORD)
{
  int l, t, rr, b;
  NcInsets(style, &l, &t, &rr, &b);
  r->left -= l;
  r->top -= t;
  r->right += rr;
  r->bottom += b;
  return TRUE;
}

BOOL REB_STDCALL reb::compat::ClientToScreen(HWND h, LPPOINT p)
{
  const Window* w = FromHandle(h);
  if (!w) {
    return FALSE;
  }
  const POINT o = ClientOriginOnScreen(w);
  p->x += o.x;
  p->y += o.y;
  return TRUE;
}

BOOL REB_STDCALL reb::compat::ScreenToClient(HWND h, LPPOINT p)
{
  const Window* w = FromHandle(h);
  if (!w) {
    return FALSE;
  }
  const POINT o = ClientOriginOnScreen(w);
  p->x -= o.x;
  p->y -= o.y;
  return TRUE;
}

int REB_STDCALL reb::compat::MapWindowPoints(HWND from, HWND to, LPPOINT points, UINT count)
{
  const Window* a = FromHandle(from);
  const Window* b = FromHandle(to);
  const POINT oa = a ? ClientOriginOnScreen(a) : POINT{0, 0};
  const POINT ob = b ? ClientOriginOnScreen(b) : POINT{0, 0};
  for (UINT i = 0; i < count; ++i) {
    points[i].x += oa.x - ob.x;
    points[i].y += oa.y - ob.y;
  }
  return MAKELONG(static_cast<WORD>(oa.x - ob.x), static_cast<WORD>(oa.y - ob.y));
}

HWND REB_STDCALL reb::compat::WindowFromPoint(POINT p)
{
  return ToHandle(WindowAtPoint(p, true, true, nullptr));
}

HWND REB_STDCALL reb::compat::ChildWindowFromPointEx(HWND h, POINT p, UINT flags)
{
  Window* w = FromHandle(h);
  if (!w) {
    return nullptr;
  }
  return ToHandle(ChildAtPoint(w, p, flags));
}

HWND REB_STDCALL reb::compat::GetParent(HWND h)
{
  const Window* w = FromHandle(h);
  if (!w) {
    return nullptr;
  }
  if (w->parent) {
    return ToHandle(w->parent);
  }
  return ToHandle(w->owner);
}

HWND REB_STDCALL reb::compat::SetParent(HWND childHandle, HWND parentHandle)
{
  Window* w = FromHandle(childHandle);
  Window* parent = FromHandle(parentHandle);
  if (!w) {
    return nullptr;
  }
  Window* old = w->parent;
  if (old == parent) {
    return ToHandle(old);
  }
  const bool wasVisible = IsWindowVisibleTree(w);
  if (wasVisible) {
    ExposeArea(WindowRectOnScreen(w), w, true);
  }
  Unlink(w);
  LinkAfter(w, parent, nullptr);
  if (IsWindowVisibleTree(w)) {
    InvalidateWindowRect(w, nullptr, true);
  }
  return ToHandle(old);
}

HWND REB_STDCALL reb::compat::GetTopWindow(HWND h)
{
  if (!h) {
    return ToHandle(FirstTopLevel());
  }
  const Window* w = FromHandle(h);
  return w ? ToHandle(w->firstChild) : nullptr;
}

HWND REB_STDCALL reb::compat::GetWindow(HWND h, UINT cmd)
{
  const Window* w = FromHandle(h);
  if (!w) {
    return nullptr;
  }
  const Window* first = w->parent ? w->parent->firstChild : FirstTopLevel();
  switch (cmd) {
    case 0:
      return ToHandle(first);
    case 1:
      {
        const Window* c = w;
        while (c->next) {
          c = c->next;
        }
        return ToHandle(c);
      }
    case 2:
      return ToHandle(w->next);
    case 3:
      return ToHandle(w->prev);
    case 4:
      return ToHandle(w->owner);
    case 5:
      return ToHandle(w->firstChild);
    default:
      return nullptr;
  }
}

BOOL REB_STDCALL reb::compat::BringWindowToTop(HWND h)
{
  return reb::compat::SetWindowPos(h, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

// ---- window words
// ------------------------------------------------------------------------------------------------------

static LONG_PTR GetLongPtr(HWND h, int index)
{
  const Window* w = FromHandle(h);
  if (!w) {
    return 0;
  }
  switch (index) {
    case kGwlWndProc:
      return reinterpret_cast<LONG_PTR>(w->proc);
    case kGwlHInstance:
      return reinterpret_cast<LONG_PTR>(w->instance);
    case kGwlHwndParent:
      return reinterpret_cast<LONG_PTR>(reb::compat::GetParent(h));
    case kGwlId:
      return w->id;
    case kGwlStyle:
      return static_cast<LONG_PTR>(static_cast<LONG>(w->style));
    case kGwlExStyle:
      return static_cast<LONG_PTR>(static_cast<LONG>(w->exStyle));
    case kGwlUserData:
      return w->userData;
    default:
      break;
  }
  if (index < 0 || index + static_cast<int>(sizeof(LONG_PTR)) > w->extraBytes) {
    return 0;
  }
  LONG_PTR v;
  memcpy(&v, w->extra + index, sizeof v);
  return v;
}

static LONG_PTR SetLongPtr(HWND h, int index, LONG_PTR value)
{
  Window* w = FromHandle(h);
  if (!w) {
    return 0;
  }
  const LONG_PTR old = GetLongPtr(h, index);
  switch (index) {
    case kGwlWndProc:
      w->proc = reinterpret_cast<WNDPROC>(value);
      return old;
    case kGwlId:
      w->id = value;
      return old;
    case kGwlUserData:
      w->userData = value;
      return old;
    case kGwlHInstance:
      w->instance = reinterpret_cast<HINSTANCE>(value);
      return old;
    case kGwlStyle:
    case kGwlExStyle:
      {
        StyleStruct change = {static_cast<DWORD>(old), static_cast<DWORD>(value)};
        Send(w, kWmStyleChanging, static_cast<WPARAM>(static_cast<LONG_PTR>(index)), reinterpret_cast<LPARAM>(&change));
        if (!IsLive(w)) {
          return old;
        }
        if (index == kGwlStyle) {
          w->style = change.styleNew;
        } else {
          w->exStyle = change.styleNew;
        }
        Send(w, kWmStyleChanged, static_cast<WPARAM>(static_cast<LONG_PTR>(index)), reinterpret_cast<LPARAM>(&change));
        return old;
      }
    default:
      break;
  }
  if (index < 0 || index + static_cast<int>(sizeof(LONG_PTR)) > w->extraBytes) {
    return 0;
  }
  memcpy(w->extra + index, &value, sizeof value);
  return old;
}

LONG REB_STDCALL reb::compat::GetWindowLongA(HWND h, int index)
{
  return static_cast<LONG>(GetLongPtr(h, index));
}
LONG REB_STDCALL reb::compat::SetWindowLongA(HWND h, int index, LONG value)
{
  return static_cast<LONG>(SetLongPtr(h, index, value));
}
LONG_PTR REB_STDCALL reb::compat::GetWindowLongPtrA(HWND h, int index)
{
  return GetLongPtr(h, index);
}
LONG_PTR REB_STDCALL reb::compat::SetWindowLongPtrA(HWND h, int index, LONG_PTR value)
{
  return SetLongPtr(h, index, value);
}

BOOL REB_STDCALL reb::compat::SetWindowTextA(HWND h, LPCSTR text)
{
  Window* w = FromHandle(h);
  if (!w) {
    return FALSE;
  }
  Send(w, kWmSetText, 0, reinterpret_cast<LPARAM>(text));
  return TRUE;
}

// ---- show, move, z-order
// -----------------------------------------------------------------------------------------------

namespace minwin
{

static Window* ResolveInsertAfter(Window* w, HWND insertAfter)
{
  // Returns the sibling that must end up directly above w (null: topmost), or w itself for "no change".
  Window* sibling = FromHandle(insertAfter);
  const auto special = reinterpret_cast<uintptr_t>(insertAfter);
  if (!sibling) {
    if (special == 0 || special == static_cast<uintptr_t>(-1)) { // HWND_TOP, HWND_TOPMOST
      Window* above = nullptr;
      if (!w->parent && special == 0) {
        for (Window* t = FirstTopLevel(); t && (t->exStyle & kWsExTopmost) && !(w->exStyle & kWsExTopmost);
             t = t->next) {
          if (t != w) {
            above = t;
          }
        }
      }
      return above;
    }
    if (special == 1) { // HWND_BOTTOM
      Window* last = w->parent ? w->parent->lastChild : nullptr;
      if (!w->parent) {
        for (Window* t = FirstTopLevel(); t; t = t->next) {
          last = t;
        }
      }
      return last == w ? w->prev : last;
    }
    if (special == static_cast<uintptr_t>(-2)) { // HWND_NOTOPMOST
      Window* above = nullptr;
      for (Window* t = FirstTopLevel(); t && (t->exStyle & kWsExTopmost); t = t->next) {
        above = t;
      }
      return above;
    }
  }
  return sibling == w ? w->prev : sibling;
}

static void EraseTree(Window* w)
{
  if (!(w->style & WS_VISIBLE)) {
    return;
  }
  EraseWindowNow(w);
  for (Window* c = w->lastChild; c; c = c->prev) {
    EraseTree(c);
  }
}

static void InvalidateTree(Window* w)
{
  if (!(w->style & WS_VISIBLE)) {
    return;
  }
  InvalidateWindowRect(w, nullptr, true);
  for (Window* c = w->lastChild; c; c = c->prev) {
    InvalidateTree(c);
  }
}

static void PaintTreeNow(HWND handle)
{
  Window* w = FromHandle(handle);
  if (!w || !IsWindowVisibleTree(w)) {
    return;
  }
  reb::compat::UpdateWindow(handle);
  w = FromHandle(handle);
  if (!w) {
    return;
  }
  std::vector<HWND> children;
  for (Window* child = w->lastChild; child; child = child->prev) {
    children.push_back(ToHandle(child));
  }
  for (HWND childHandle : children) {
    if (Window* child = FromHandle(childHandle); child && child->parent == w) {
      PaintTreeNow(childHandle);
    }
  }
}

// SetWindowPos for one window; all the other positioning calls are expressed through it.
BOOL ApplyWindowPos(Window* w, HWND insertAfter, int x, int y, int cx, int cy, UINT flags)
{
  WindowPos pos = {ToHandle(w), insertAfter, x, y, cx, cy, flags};
  if (!(flags & kSwpNoSendChanging)) {
    Send(w, kWmWindowPosChanging, 0, reinterpret_cast<LPARAM>(&pos));
    if (!IsLive(w)) {
      return FALSE;
    }
  }
  flags = pos.flags;
  const bool wasVisibleTree = IsWindowVisibleTree(w);
  const RECT oldScreen = WindowRectOnScreen(w);
  const int oldClientW = w->clientWidth, oldClientH = w->clientHeight;
  const POINT oldClientOrigin = {w->x + w->clientLeft, w->y + w->clientTop};
  const bool oldWasVisible = (w->style & WS_VISIBLE) != 0;
  const int oldW = w->width, oldH = w->height, oldX = w->x, oldY = w->y;

  if (!(flags & kSwpNoMove)) {
    w->x = pos.x;
    w->y = pos.y;
  }
  if (!(flags & kSwpNoSize)) {
    w->width = pos.cx;
    w->height = pos.cy;
  }
  const bool moved = w->x != oldX || w->y != oldY;
  const bool sized = w->width != oldW || w->height != oldH;
  if (flags & kSwpShowWindow) {
    w->style |= WS_VISIBLE;
  }
  if (flags & kSwpHideWindow) {
    w->style &= ~static_cast<DWORD>(WS_VISIBLE);
  }

  bool zChanged = false;
  if (!(flags & kSwpNoZOrder) && !(flags & kSwpHideWindow && false)) {
    Window* above = ResolveInsertAfter(w, pos.hwndInsertAfter);
    if (above != w->prev || (!above && w->prev)) {
      Window* parent = w->parent;
      Unlink(w);
      LinkAfter(w, parent, above);
      zChanged = true;
    }
    const auto special = reinterpret_cast<uintptr_t>(pos.hwndInsertAfter);
    if (special == static_cast<uintptr_t>(-1)) {
      w->exStyle |= kWsExTopmost;
    } else if (special == static_cast<uintptr_t>(-2)) {
      w->exStyle &= ~static_cast<DWORD>(kWsExTopmost);
    }
  }

  if (sized || (flags & kSwpFrameChanged)) {
    NcCalcSizeParams params;
    memset(&params, 0, sizeof params);
    params.rgrc[0] = MakeRect(w->x, w->y, w->x + w->width, w->y + w->height);
    params.rgrc[1] = MakeRect(oldX, oldY, oldX + oldW, oldY + oldH);
    params.rgrc[2] = MakeRect(oldX + w->clientLeft, oldY + w->clientTop, oldX + w->clientLeft + oldClientW,
                              oldY + w->clientTop + oldClientH);
    params.lppos = &pos;
    Send(w, kWmNcCalcSize, TRUE, reinterpret_cast<LPARAM>(&params));
    if (!IsLive(w)) {
      return FALSE;
    }
    w->clientLeft = params.rgrc[0].left - w->x;
    w->clientTop = params.rgrc[0].top - w->y;
    w->clientWidth = params.rgrc[0].right - params.rgrc[0].left;
    w->clientHeight = params.rgrc[0].bottom - params.rgrc[0].top;
    w->needNcPaint = true;
  }

  if ((w->style & WS_CHILD) && w->parent && !(flags & (kSwpNoActivate | kSwpHideWindow))) {
    Send(w, kWmChildActivate, 0, 0);
    if (!IsLive(w)) {
      return FALSE;
    }
  }

  const bool nowVisibleTree = IsWindowVisibleTree(w);
  const bool becameVisible = nowVisibleTree && !wasVisibleTree;
  const bool becameHidden = wasVisibleTree && !nowVisibleTree;
  const bool visibleChange
      = (flags & (kSwpShowWindow | kSwpHideWindow)) && (oldWasVisible != ((w->style & WS_VISIBLE) != 0));
  (void)visibleChange;
  if (!(flags & kSwpNoRedraw)) {
    if (becameHidden) {
      ExposeArea(oldScreen, w, true);
    } else if (nowVisibleTree) {
      if (becameVisible) {
        InvalidateTree(w);
        w->needNcPaint = true;
      } else if (moved || sized || zChanged) {
        if (wasVisibleTree) {
          ExposeArea(oldScreen, w, moved && !sized);
        }
        if (!IsLive(w)) {
          return TRUE;
        }
        InvalidateTree(w);
      }
      EraseTree(w);
      if (!IsLive(w)) {
        return TRUE;
      }
    }
  }

  if ((flags & kSwpShowWindow) && !w->parent && !(flags & kSwpNoActivate) && !(w->exStyle & kWsExNoActivate)) {
    ActivateTopLevel(w, false);
    if (!IsLive(w)) {
      return TRUE;
    }
  }

  UINT notified = flags;
  const POINT newClientOrigin = {w->x + w->clientLeft, w->y + w->clientTop};
  if (newClientOrigin.x == oldClientOrigin.x && newClientOrigin.y == oldClientOrigin.y) {
    notified |= kSwpNoClientMove;
  }
  if (w->clientWidth == oldClientW && w->clientHeight == oldClientH) {
    notified |= kSwpNoClientSize;
  }
  pos.flags = notified;
  pos.x = w->x;
  pos.y = w->y;
  pos.cx = w->width;
  pos.cy = w->height;
  Send(w, kWmWindowPosChanged, 0, reinterpret_cast<LPARAM>(&pos));
  if (!IsLive(w)) {
    return TRUE;
  }

  if (wasVisibleTree && nowVisibleTree && moved && !sized && !(flags & kSwpNoRedraw)) {
    PaintTreeNow(ToHandle(w));
    if (!IsLive(w)) {
      return TRUE;
    }
  }

  if (MovesPointerOnChange(w) && (becameHidden || becameVisible || moved || sized || zChanged)) {
    MarkLayoutChanged();
  }
  if (becameHidden || (!nowVisibleTree && (moved || sized))) {
    OnWindowHidden(w);
  }
  return TRUE;
}

} // namespace minwin

BOOL REB_STDCALL reb::compat::SetWindowPos(HWND h, HWND after, int x, int y, int cx, int cy, UINT flags)
{
  Window* w = FromHandle(h);
  if (!w) {
    return FALSE;
  }
  return ApplyWindowPos(w, after, x, y, cx, cy, flags);
}

BOOL REB_STDCALL reb::compat::MoveWindow(HWND h, int x, int y, int cx, int cy, BOOL repaint)
{
  Window* w = FromHandle(h);
  if (!w) {
    return FALSE;
  }
  return ApplyWindowPos(w, nullptr, x, y, cx, cy, kSwpNoZOrder | kSwpNoActivate | (repaint ? 0 : kSwpNoRedraw));
}

BOOL REB_STDCALL reb::compat::ShowWindow(HWND h, int command)
{
  Window* w = FromHandle(h);
  if (!w) {
    return FALSE;
  }
  const bool wasVisible = (w->style & WS_VISIBLE) != 0;
  UINT flags = kSwpNoSize | kSwpNoMove | kSwpNoZOrder | (w->parent ? kSwpNoActivate : 0u);
  bool show = true;
  switch (command) {
    case kSwHide:
      if (!wasVisible) {
        return FALSE;
      }
      show = false;
      flags |= kSwpHideWindow | kSwpNoActivate;
      break;
    case kSwShowNoActivate:
    case kSwShowNA:
    case kSwShowMinNoActive:
      flags |= kSwpShowWindow | kSwpNoActivate;
      break;
    case kSwMinimize:
    case kSwShowMinimized:
    case kSwForceMinimize:
      if (!w->iconic) {
        w->iconic = true;
        Send(w, kWmShowWindowMsg, 0, 0);
        Send(w, WM_SIZE, kSizeMinimized, 0);
      }
      return wasVisible;
    default:
      flags |= kSwpShowWindow;
      if (w->iconic) {
        w->iconic = false;
        Send(w, WM_SIZE, kSizeRestored, MakeLParam(w->clientWidth, w->clientHeight));
      }
      break;
  }
  if (show && wasVisible) {
    return TRUE;
  }
  Send(w, kWmShowWindowMsg, show ? 1 : 0, 0);
  if (IsLive(w) && w->parent && !IsWindowVisibleTree(w->parent)) {
    // A child of a hidden parent only changes its own visibility flag; nothing is shown or exposed yet.
    if (show) {
      w->style |= WS_VISIBLE;
    } else {
      w->style &= ~static_cast<DWORD>(WS_VISIBLE);
    }
    return wasVisible;
  }
  if (!IsLive(w)) {
    return wasVisible;
  }
  ApplyWindowPos(w, nullptr, 0, 0, 0, 0, flags);
  return wasVisible;
}
