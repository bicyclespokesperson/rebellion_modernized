#include "ui/cool_window.h"
#include <algorithm>
#include <string>
#include <utility>

#include <cstring>

#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/strstream_io.h"
#include "ui/cool_app.h"
#include "ui/resource_dll.h"

// ---- Window object registry ---------------------------------------------------------------------------------

namespace
{

uint32_t WindowObjectId(const CoolWindow* window)
{
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(window));
}

// The original copies without checking the allocation (a failed allocation crashes in the copy).
char* DuplicateCString(const char* text)
{
  char* copy = static_cast<char*>(operator new(std::strlen(text) + 1));
  std::memcpy(copy, text, std::strlen(text) + 1);
  return copy;
}

constexpr UINT kToolInfoSize = 0x2c;
constexpr UINT kClassStyle = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;

TTTOOLINFOA MakeToolInfo(HWND owner, HINSTANCE instance, UINT flags, UINT_PTR toolId, LPSTR text)
{
  TTTOOLINFOA info;
  info.cbSize = kToolInfoSize;
  info.uFlags = flags;
  info.hwnd = owner;
  info.uId = toolId;
  info.hinst = instance;
  info.lpszText = text;
  return info;
}

} // namespace

int RegisterWindowObject(CoolWindow* window)
{
  const uint32_t id = WindowObjectId(window);
  return g_windowRegistry.emplace(id, window).second;
}

int IsWindowObjectRegistered(CoolWindow* window)
{
  return g_windowRegistry.find(WindowObjectId(window)) != g_windowRegistry.end();
}

int UnregisterWindowObject(CoolWindow* window)
{
  return g_windowRegistry.erase(WindowObjectId(window)) != 0;
}

CoolWindow* GetCoolWindow(HWND hwnd)
{
  CoolWindow* window = reinterpret_cast<CoolWindow*>(GetWindowLongPtr(hwnd, 0));
  if (window && !IsWindowObjectRegistered(window))
    window = nullptr;
  return window;
}

// ---- Helper containers ----------------------------------------------------------------------------------------

HwndNode::HwndNode(HWND hwnd) : m_hwnd(hwnd), m_window(nullptr)
{
}

HwndNode::~HwndNode() = default;

HwndTree::HwndTree() = default;

HwndTree::~HwndTree() = default;

int HwndTree::Insert(HwndNode* node)
{
  if (!node || std::any_of(m_nodes.begin(), m_nodes.end(),
                           [node](const auto& owned)
                           {
                             return owned.get() == node;
                           }))
    return false;
  auto position =
    std::find_if(m_nodes.begin(), m_nodes.end(),
                 [node](const auto& owned)
                 {
                   return reinterpret_cast<uintptr_t>(owned->m_hwnd) > reinterpret_cast<uintptr_t>(node->m_hwnd);
                 });
  position = m_nodes.emplace(position);
  position->reset(node);
  return true;
}

void HwndTree::Remove(HwndNode* node)
{
  const auto found = std::find_if(m_nodes.begin(), m_nodes.end(),
                                  [node](const auto& owned)
                                  {
                                    return owned.get() == node;
                                  });
  if (found != m_nodes.end())
  {
    found->release();
    m_nodes.erase(found);
  }
}

HwndNode* HwndTree::FindByHwnd(HWND hwnd)
{
  const auto found = std::find_if(m_nodes.begin(), m_nodes.end(),
                                  [hwnd](const auto& owned)
                                  {
                                    return owned->m_hwnd == hwnd;
                                  });
  return found == m_nodes.end() ? nullptr : (*found).get();
}

void HwndTree::RemoveAndDelete(HWND hwnd)
{
  HwndNode* node = FindByHwnd(hwnd);
  if (node)
  {
    Remove(node);
    delete node;
  }
}

PopupEntry::PopupEntry(HwndNode* node) : m_node(node)
{
}

PopupEntry::~PopupEntry()
{
  if (m_node)
    delete m_node;
}

ChildWindowTree::ChildWindowTree() = default;

ChildWindowTree::~ChildWindowTree()
{
  Clear();
}

int ChildWindowTree::Insert(CoolWindow* window)
{
  if (!window)
    return false;
  auto [position, inserted] = m_children.try_emplace(window->m_childId);
  if (!inserted)
    return false;
  position->second.reset(window);
  return true;
}

void ChildWindowTree::Remove(CoolWindow* window)
{
  if (!window)
    return;
  const auto found = m_children.find(window->m_childId);
  if (found != m_children.end() && found->second.get() == window)
  {
    found->second.release();
    m_children.erase(found);
  }
}

void ChildWindowTree::Clear()
{
  decltype(m_children) detached;
  detached.swap(m_children);
}

CoolWindow* ChildWindowTree::FindById(uint32_t id)
{
  const auto found = m_children.find(id);
  return found == m_children.end() ? nullptr : found->second.get();
}

CoolWindow* ChildWindowTree::FindByHwnd(HWND hwnd)
{
  for (const auto& entry : Entries())
  {
    auto* node = entry.get();
    if (node->m_hwnd == hwnd)
      return node;
  }
  return nullptr;
}

// ---- CoolWindow: construction and destruction ---------------------------------------------------------------

CoolWindow::CoolWindow(HINSTANCE instance,
                       const char* className,
                       uint32_t style,
                       int x,
                       int y,
                       int width,
                       int height,
                       CoolWindow* parent,
                       const char* windowName,
                       HBRUSH background,
                       HICON icon,
                       HCURSOR cursor,
                       HMENU menu,
                       uint32_t unknown58,
                       uint32_t flags)
{
  int borderWidth = 0;
  int borderHeight = 0;
  if (g_defaultCursor && (flags & kCoolOwnCursor) == 0)
    cursor = g_defaultCursor;
  m_className = className ? DuplicateCString(className) : nullptr;
  m_windowName = windowName ? DuplicateCString(windowName) : nullptr;
  m_x = x;
  m_parent = parent;
  m_y = y;
  m_width = width;
  m_height = height;
  if (style & WS_BORDER)
  {
    borderWidth = GetSystemMetrics(SM_CXBORDER);
    borderHeight = GetSystemMetrics(SM_CYBORDER);
  }
  if (style & WS_DLGFRAME)
  {
    borderWidth += GetSystemMetrics(SM_CXDLGFRAME);
    borderHeight += GetSystemMetrics(SM_CYDLGFRAME);
    if (style & WS_BORDER)
      borderHeight += GetSystemMetrics(SM_CYCAPTION);
  }
  m_style = style;
  m_unknown58 = unknown58;
  m_background = background;
  m_icon = icon;
  m_cursor = cursor;
  m_clientWidth = width - borderWidth;
  m_clientHeight = height - borderHeight;
  m_hwnd = nullptr;
  m_unknown44 = 0;
  m_menu = menu;
  m_flags = flags;
  m_exStyle = 0;
  m_instance = instance;
  m_savedFont = nullptr;
  m_font = nullptr;
  if (int fontSlot = GetDefaultFontSlot())
    SetFont(fontSlot);
  m_tooltipText = nullptr;
  m_tooltipOwner = nullptr;
}

CoolWindow::~CoolWindow()
{
  UnregisterWindowObject(this);
  if (m_className)
    operator delete(m_className);
  if (m_windowName)
    operator delete(m_windowName);
  if (m_tooltipText)
    RemoveTool(this);

  EndPopup();
}

// ---- CoolWindow: window class and creation -------------------------------------------------------------------

ATOM CoolWindow::RegisterWindowClass(UINT classStyle)
{
  WNDCLASSA windowClass;
  windowClass.hInstance = m_instance;
  windowClass.hIcon = m_icon;
  windowClass.style = classStyle;
  windowClass.hCursor = m_cursor;
  windowClass.cbClsExtra = 0;
  windowClass.hbrBackground = m_background;
  windowClass.lpszMenuName = nullptr;
  windowClass.lpszClassName = m_className;
  windowClass.lpfnWndProc = CoolWindowProc;
  windowClass.cbWndExtra = 4;
  return RegisterClassA(&windowClass);
}

int CoolWindow::Create(int useExStyle)
{
  WNDCLASSA existingClass;
  if (!GetClassInfoA(m_instance, m_className, &existingClass))
    RegisterWindowClass(kClassStyle);
  HWND parentWindow = m_parent ? m_parent->m_hwnd : nullptr;
  HMENU menu = m_parent ? reinterpret_cast<HMENU>(static_cast<UINT_PTR>(m_childId)) : m_menu;
  CoolWindow* creationParameter = this;
  const DWORD extendedStyle = useExStyle ? m_exStyle : 0;
  m_hwnd = CreateWindowExA(extendedStyle, m_className, m_windowName, m_style, m_x, m_y, m_width, m_height, parentWindow,
                           menu, m_instance, &creationParameter);
  if (!m_hwnd)
    GetLastError();
  RegisterWithActivePopup();
  return m_hwnd != nullptr;
}

void CoolWindow::Show(int command)
{
  if (m_hwnd)
    ShowWindow(m_hwnd, command);
}

void CoolWindow::Hide()
{
  Show(0);
}

// The next two are linker-folded with the same do-nothing functions of other classes.
int CoolWindow::OnRightButtonUp(int, int, CoolWindow*)
{
  return 0;
}

void CoolWindow::Virtual08(int, int, int)
{
}

void CoolWindow::Enable()
{
  if (!IsWindowEnabled(m_hwnd))
    EnableWindow(m_hwnd, TRUE);
}

void CoolWindow::Disable()
{
  if (IsWindowEnabled(m_hwnd))
    EnableWindow(m_hwnd, FALSE);
}

BOOL CoolWindow::IsEnabled()
{
  return IsWindowEnabled(m_hwnd);
}

void CoolWindow::CenterInParent()
{
  int x;
  int y;
  if (m_parent)
  {
    x = m_parent->m_clientWidth / 2 - m_width / 2;
    y = m_parent->m_clientHeight / 2 - m_height / 2;
  }
  else
  {
    x = GetSystemMetrics(SM_CXSCREEN) / 2 - m_width / 2;
    y = GetSystemMetrics(SM_CYSCREEN) / 2 - m_height / 2;
  }
  SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void CoolWindow::SetWindowTitle(const char* title)
{
  if (m_windowName)
    operator delete(m_windowName);
  m_windowName = title ? DuplicateCString(title) : nullptr;
  if (m_hwnd)
    SetWindowTextA(m_hwnd, m_windowName);
}

void CoolWindow::SetClassName(const char* className)
{
  if (m_className)
    operator delete(m_className);
  m_className = DuplicateCString(className);
}

void CoolWindow::DestroyNativeWindow()
{
  if (m_hwnd)
    DestroyWindow(m_hwnd);
}

void CoolWindow::SetFont(int fontSlot)
{
  m_font = CoolGetFont(fontSlot);
}

void CoolWindow::SelectCurrentFont(HDC dc)
{
  if (m_font)
    m_savedFont = SelectObject(dc, m_font);
  else
    m_savedFont = nullptr;
}

void CoolWindow::RestoreFont(HDC dc)
{
  if (m_savedFont)
    SelectObject(dc, m_savedFont);
}

void CoolWindow::SetTooltipText(const char* text)
{
  if (m_tooltipText)
    operator delete(m_tooltipText);
  if (!text)
  {
    m_tooltipText = nullptr;
    return;
  }
  m_tooltipText = static_cast<char*>(operator new(std::strlen(text) + 1));
  if (m_tooltipText)
    std::memcpy(m_tooltipText, text, std::strlen(text) + 1);
}

// ---- CoolWindow: default message handling ---------------------------------------------------------------------

LRESULT CoolWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == WM_SHOWWINDOW)
  {
    if (wParam)
    {
      for (const auto& entry : m_linkedWindows.Entries())
        static_cast<HwndNode*>(entry.get())->m_window->Show(SW_SHOW);
    }
    return 0;
  }
  if (message == WM_RBUTTONUP)
    return OnRightButtonUp(static_cast<short>(lParam), static_cast<short>(lParam >> 16), this);
  if (message == 0x405)
  {
    DestroyChildById(static_cast<uint32_t>(wParam));
    return 0;
  }
  return DefWindowProcA(m_hwnd, message, wParam, lParam);
}

int CoolWindow::DestroyChildById(uint32_t childId)
{
  CoolWindow* child = m_children.FindById(childId);
  if (child)
  {
    m_children.Remove(child);
    child->DestroyNativeWindow();
    delete child;
  }
  return child != nullptr;
}

// ---- Message dispatcher ----------------------------------------------------------------------------------------

namespace
{

CoolWindow* WindowObjectOf(HWND hwnd)
{
  return reinterpret_cast<CoolWindow*>(GetWindowLongPtr(hwnd, 0));
}

LRESULT Forward(CoolWindow* window, UINT message, WPARAM wParam, LPARAM lParam)
{
  return window->WindowProc(message, wParam, lParam);
}

LRESULT ForwardOrDefault(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CoolWindow* window = WindowObjectOf(hwnd);
  if (window)
    return Forward(window, message, wParam, lParam);
  return DefWindowProcA(hwnd, message, wParam, lParam);
}

LRESULT HandleSystemCommand(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  CoolWindow* window = WindowObjectOf(hwnd);
  if (!window)
    return DefWindowProcA(hwnd, WM_SYSCOMMAND, wParam, lParam);
  const UINT command = static_cast<UINT>(wParam) & 0xfff0;
  if (command < 0xf011)
  {
    if (command == SC_MOVE)
    {
      if (window->m_flags & kCoolNoMove)
        return 0;
      return Forward(window, WM_SYSCOMMAND, wParam, lParam);
    }
    if (command == SC_SIZE)
    {
      if (window->m_flags & kCoolNoSize)
        return 0;
      return Forward(window, WM_SYSCOMMAND, wParam, lParam);
    }
  }
  else
  {
    if (command == SC_MINIMIZE)
    {
      if (window->m_flags & kCoolNoMinimize)
        return 0;
      return Forward(window, WM_SYSCOMMAND, wParam, lParam);
    }
    if (command == SC_MAXIMIZE)
    {
      if (window->m_flags & kCoolNoMaximize)
        return 0;
    }
    else if (command == SC_RESTORE)
    {
      if ((window->m_flags & kCoolNoMinimize) && IsIconic(window->m_hwnd))
        return 0;
      if ((window->m_flags & kCoolNoMaximize) && IsZoomed(window->m_hwnd))
        return 0;
      return Forward(window, WM_SYSCOMMAND, wParam, lParam);
    }
  }
  return Forward(window, WM_SYSCOMMAND, wParam, lParam);
}

LRESULT HandleNonClientDoubleClick(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  CoolWindow* window = WindowObjectOf(hwnd);
  if (!window)
    return 0;
  if (wParam != HTCAPTION)
    return DefWindowProcA(hwnd, WM_NCLBUTTONDBLCLK, wParam, lParam);
  if ((window->m_flags & kCoolNoMaximize) && IsZoomed(window->m_hwnd))
    return 0;
  if ((window->m_flags & kCoolNoMinimize) && IsIconic(window->m_hwnd))
    return 0;
  return Forward(window, WM_NCLBUTTONDBLCLK, HTCAPTION, lParam);
}

LRESULT HandleCreate(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  const CREATESTRUCTA* createStruct = reinterpret_cast<const CREATESTRUCTA*>(lParam);
  CoolWindow* window = *static_cast<CoolWindow* const*>(createStruct->lpCreateParams);
  RegisterWindowObject(window);
  SetWindowLongPtr(hwnd, 0, reinterpret_cast<LONG_PTR>(window));
  if (!window)
    return DefWindowProcA(hwnd, WM_CREATE, wParam, lParam);
  HDC dc = GetDC(hwnd);
  PatBlt(dc, 0, 0, window->m_clientWidth, window->m_clientHeight, BLACKNESS);
  ReleaseDC(hwnd, dc);
  window->m_hwnd = hwnd;
  return Forward(window, WM_CREATE, wParam, lParam);
}

LRESULT HandleDestroy(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  CoolWindow* window = WindowObjectOf(hwnd);
  if (!window)
    return DefWindowProcA(hwnd, WM_DESTROY, wParam, lParam);
  const LRESULT result = Forward(window, WM_DESTROY, wParam, lParam);
  if (window->m_tooltipText)
    window->RemoveTool(window);
  return result;
}

LRESULT HandleMoveOrSize(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  CoolWindow* window = WindowObjectOf(hwnd);
  if (!window)
    return DefWindowProcA(hwnd, message, wParam, lParam);
  const uint32_t packed = static_cast<uint32_t>(lParam);
  if (message == WM_MOVE)
  {
    window->m_x = packed & 0xffff;
    window->m_y = packed >> 16;
  }
  else
  {
    window->m_clientWidth = packed & 0xffff;
    window->m_clientHeight = packed >> 16;
  }
  return Forward(window, message, wParam, lParam);
}

} // namespace

LRESULT CALLBACK CoolWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message != WM_CREATE && message != WM_NCCREATE && message != WM_MDICREATE)
  {
    CoolWindow* window = WindowObjectOf(hwnd);
    if (window && !IsWindowObjectRegistered(window))
      return DefWindowProcA(hwnd, message, wParam, lParam);
  }
  if (message > WM_ACTIVATEAPP)
  {
    if (message == WM_NCDESTROY)
    {
      CoolWindow* window = WindowObjectOf(hwnd);
      if (!window)
        return DefWindowProcA(hwnd, WM_NCDESTROY, wParam, lParam);
      return Forward(window, WM_NCDESTROY, wParam, lParam);
    }
    if (message == WM_NCLBUTTONDBLCLK)
      return HandleNonClientDoubleClick(hwnd, wParam, lParam);
    if (message == WM_SYSCOMMAND)
      return HandleSystemCommand(hwnd, wParam, lParam);
    return ForwardOrDefault(hwnd, message, wParam, lParam);
  }
  if (message == WM_ACTIVATEAPP)
  {
    if (!g_app)
      return DefWindowProcA(hwnd, WM_ACTIVATEAPP, wParam, lParam);
    return Forward(g_app, WM_ACTIVATEAPP, wParam, lParam);
  }
  switch (message)
  {
    case WM_CREATE:
      return HandleCreate(hwnd, wParam, lParam);
    case WM_DESTROY:
      return HandleDestroy(hwnd, wParam, lParam);
    case WM_MOVE:
    case WM_SIZE:
      return HandleMoveOrSize(hwnd, message, wParam, lParam);
    default:
      return ForwardOrDefault(hwnd, message, wParam, lParam);
  }
}

// ---- Tooltips -----------------------------------------------------------------------------------------------------

void CoolWindow::CreateTooltipControl(LPARAM delayMs)
{
  if (g_tooltipWindow)
    return;
  g_tooltipWindow = CreateWindowExA(0, g_tooltipClassName, nullptr, TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
                                    CW_USEDEFAULT, CW_USEDEFAULT, m_hwnd, nullptr, m_instance, nullptr);
  if (g_tooltipWindow)
  {
    SendMessageA(g_tooltipWindow, TTM_ACTIVATE, 1, 0);
    SendMessageA(g_tooltipWindow, TTM_SETDELAYTIME, 0, delayMs);
    g_tooltipHook = SetWindowsHookExA(WH_GETMESSAGE, TooltipHookProc, nullptr, GetCurrentThreadId());
  }
}

void CoolWindow::AddTool(CoolWindow* tool, const char* text)
{
  if (!tool)
    return;
  tool->SetTooltipText(text);
  tool->m_tooltipOwner = m_hwnd;
  TTTOOLINFOA info =
    MakeToolInfo(m_hwnd, m_instance, TTF_IDISHWND, reinterpret_cast<UINT_PTR>(tool->m_hwnd), tool->m_tooltipText);
  if (g_tooltipWindow)
    SendMessageA(g_tooltipWindow, TTM_ADDTOOLA, 0, reinterpret_cast<LPARAM>(&info));
}

void CoolWindow::AddToolFromResource(CoolWindow* tool, UINT stringId)
{
  char text[0x100];
  LoadResourceString(stringId, text, sizeof(text));
  AddTool(tool, text);
}

void CoolWindow::AddToolFromString(CoolWindow* tool, const ResourceStringId* stringId)
{
  std::string text = LoadStringResource(stringId);
  if (!text.empty())
    AddTool(tool, text.c_str());
}

void CoolWindow::AddRectTool(RECT area, UINT toolId, const char* text)
{
  TTTOOLINFOA info = MakeToolInfo(m_hwnd, m_instance, 0, toolId, const_cast<LPSTR>(text));
  CopyRect(&info.rect, &area);
  if (g_tooltipWindow)
    SendMessageA(g_tooltipWindow, TTM_ADDTOOLA, 0, reinterpret_cast<LPARAM>(&info));
}

void CoolWindow::AddRectToolFromResource(RECT area, UINT toolId, UINT stringId)
{
  char text[0x100];
  LoadResourceString(stringId, text, sizeof(text));
  AddRectTool(area, toolId, text);
}

void CoolWindow::AddRectToolFromString(RECT area, UINT toolId, const ResourceStringId* stringId)
{
  std::string text = LoadStringResource(stringId);
  if (!text.empty())
    AddRectTool(area, toolId, text.c_str());
}

void CoolWindow::UpdateToolFromString(CoolWindow* tool, const ResourceStringId* stringId)
{
  std::string text = LoadStringResource(stringId);
  if (!text.empty())
    UpdateToolText(tool, text.c_str());
}

void CoolWindow::UpdateToolText(CoolWindow* tool, const char* text)
{
  if (!tool)
    return;
  tool->SetTooltipText(text);
  TTTOOLINFOA info =
    MakeToolInfo(m_hwnd, m_instance, TTF_IDISHWND, reinterpret_cast<UINT_PTR>(tool->m_hwnd), tool->m_tooltipText);
  if (g_tooltipWindow)
    SendMessageA(g_tooltipWindow, TTM_UPDATETIPTEXTA, 0, reinterpret_cast<LPARAM>(&info));
}

void CoolWindow::RemoveTool(CoolWindow* tool)
{
  TTTOOLINFOA info =
    MakeToolInfo(tool->m_tooltipOwner, m_instance, TTF_IDISHWND, reinterpret_cast<UINT_PTR>(tool->m_hwnd), nullptr);
  SetRectEmpty(&info.rect);
  if (g_tooltipWindow)
    SendMessageA(g_tooltipWindow, TTM_DELTOOLA, 0, reinterpret_cast<LPARAM>(&info));
  if (tool->m_tooltipText)
  {
    operator delete(tool->m_tooltipText);
    tool->m_tooltipText = nullptr;
  }
}

void CoolWindow::RemoveRectTool(UINT toolId)
{
  TTTOOLINFOA info = MakeToolInfo(m_hwnd, m_instance, 0, toolId, nullptr);
  SetRectEmpty(&info.rect);
  if (g_tooltipWindow)
    SendMessageA(g_tooltipWindow, TTM_DELTOOLA, 0, reinterpret_cast<LPARAM>(&info));
}

void ShutdownTooltips()
{
  if (g_tooltipHook)
  {
    UnhookWindowsHookEx(g_tooltipHook);
    g_tooltipHook = nullptr;
  }
  g_tooltipWindow = nullptr;
}

LRESULT CALLBACK TooltipHookProc(int code, WPARAM wParam, LPARAM lParam)
{
  MSG relayed;
  const MSG* message = reinterpret_cast<const MSG*>(lParam);
  if (message && code >= 0 && message->message >= WM_MOUSEMOVE && message->message <= WM_RBUTTONUP)
  {
    relayed.message = message->message;
    relayed.hwnd = message->hwnd;
    relayed.wParam = message->wParam;
    relayed.lParam = message->lParam;
    if (g_tooltipWindow)
      SendMessageA(g_tooltipWindow, TTM_RELAYEVENT, 0, reinterpret_cast<LPARAM>(&relayed));
  }
  return CallNextHookEx(g_tooltipHook, code, wParam, lParam);
}

// ---- Popup mode ------------------------------------------------------------------------------------------------------

LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam)
{
  bool swallow = false;
  PopupEntry* top = g_popupStack.empty() ? nullptr : g_popupStack.back().get();
  if (code >= 0 && code != HC_NOREMOVE && top && top->m_node)
  {
    CoolWindow* popup = top->m_node->m_window;
    if (popup)
    {
      const MOUSEHOOKSTRUCT* mouse = reinterpret_cast<const MOUSEHOOKSTRUCT*>(lParam);
      if (!popup->m_popupMembers.FindByHwnd(mouse->hwnd))
        swallow = true;
    }
  }
  if (!swallow)
    return CallNextHookEx(g_mouseHook, code, wParam, lParam);
  return 1;
}

int CoolWindow::BeginPopup()
{
  if (g_popupStack.empty())
    g_mouseHook = SetWindowsHookExA(WH_MOUSE, MouseHookProc, m_instance, GetCurrentThreadId());
  HwndNode* node = NewOrNull<HwndNode>(m_hwnd);
  if (!node)
  {
    if (g_popupStack.empty() && g_mouseHook)
    {
      UnhookWindowsHookEx(g_mouseHook);
      g_mouseHook = nullptr;
    }
    return false;
  }
  node->m_window = this;
  std::unique_ptr<PopupEntry> entryOwner(NewOrNull<PopupEntry>(node));
  HwndNode* member = NewOrNull<HwndNode>(m_hwnd);
  if (!entryOwner || !member)
  {
    if (!entryOwner)
      delete node;
    delete member;
    if (g_popupStack.empty() && g_mouseHook)
    {
      UnhookWindowsHookEx(g_mouseHook);
      g_mouseHook = nullptr;
    }
    return false;
  }
  PopupEntry* entry = entryOwner.get();
  g_popupStack.emplace_back(std::move(entryOwner));
  if (!m_popupMembers.Insert(member))
  {
    delete member;
    auto stackEntry = std::find_if(g_popupStack.begin(), g_popupStack.end(),
                                   [entry](const auto& candidate)
                                   {
                                     return candidate.get() == entry;
                                   });
    if (stackEntry != g_popupStack.end())
      g_popupStack.erase(stackEntry);
    if (g_popupStack.empty() && g_mouseHook)
    {
      UnhookWindowsHookEx(g_mouseHook);
      g_mouseHook = nullptr;
    }
    return false;
  }
  ReleaseAllCaptures();
  return true;
}

PopupEntry* FindPopupEntry(HWND hwnd)
{
  for (auto entry = g_popupStack.rbegin(); entry != g_popupStack.rend(); ++entry)
  {
    if ((*entry)->m_node && (*entry)->m_node->m_hwnd == hwnd)
      return (*entry).get();
  }
  return nullptr;
}

int CoolWindow::EndPopup()
{
  PopupEntry* entry = FindPopupEntry(m_hwnd);
  if (!entry)
    return false;
  if (entry->m_node && entry->m_node->m_window)
    entry->m_node->m_window->m_popupMembers.RemoveAndDelete(m_hwnd);
  auto stackEntry = std::find_if(g_popupStack.begin(), g_popupStack.end(),
                                 [entry](const auto& candidate)
                                 {
                                   return candidate.get() == entry;
                                 });
  if (stackEntry != g_popupStack.end())
    g_popupStack.erase(stackEntry);
  if (g_popupStack.empty())
  {
    UnhookWindowsHookEx(g_mouseHook);
    g_mouseHook = nullptr;
  }
  return true;
}

void CoolWindow::RegisterWithActivePopup()
{
  PopupEntry* top = g_popupStack.empty() ? nullptr : g_popupStack.back().get();
  if (!top || !top->m_node || !top->m_node->m_window)
    return;
  HwndTree& members = top->m_node->m_window->m_popupMembers;
  if (members.FindByHwnd(m_hwnd))
    return;
  members.Insert(NewOrNull<HwndNode>(m_hwnd));
}

HWND CurrentPopupHwnd()
{
  PopupEntry* top = g_popupStack.empty() ? nullptr : g_popupStack.back().get();
  if (top && top->m_node)
    return top->m_node->m_hwnd;
  return nullptr;
}

int DismissPopup()
{
  HWND target = GetCapture();
  if (!target)
  {
    target = CurrentPopupHwnd();
    if (!target)
      return true;
  }
  SendMessageA(target, 0x40d, 0, 0);
  return false;
}

void CoolWindow::AddLinkedWindow(CoolWindow* window)
{
  if (!window || !window->m_hwnd)
    return;
  HwndNode* node = NewOrNull<HwndNode>(window->m_hwnd);
  if (!node)
    return;
  node->m_window = window;
  m_linkedWindows.Insert(node);
}

void CoolWindow::RemoveLinkedWindow(CoolWindow* window)
{
  if (window)
    m_linkedWindows.RemoveAndDelete(window->m_hwnd);
}

int ReleaseAllCaptures()
{
  ReleaseCapture();
  return 0;
}

HWND SetCaptureWindow(HWND hwnd)
{
  return SetCapture(hwnd);
}

// ---- Drawing helpers and palette -----------------------------------------------------------------------------------

void LoadResourceString(UINT id, LPSTR buffer, int size)
{
  LoadStringA(g_stringModule, id, buffer, size);
}

void DrawShadowedText(HDC dc,
                      LPCSTR text,
                      LPRECT rect,
                      COLORREF color,
                      COLORREF shadow,
                      UINT format,
                      int offset,
                      int length)
{
  RECT shadowRect;
  shadowRect.left = rect->left + offset;
  shadowRect.right = rect->right + offset;
  shadowRect.bottom = rect->bottom + offset;
  shadowRect.top = rect->top + offset;
  SetTextColor(dc, shadow);
  SetBkMode(dc, TRANSPARENT);
  DrawTextA(dc, text, length, &shadowRect, format);
  SetTextColor(dc, color);
  DrawTextA(dc, text, length, rect, format);
}

UINT SetIdentityPalette(HDC dc, HPALETTE palette)
{
  struct
  {
    WORD version;
    WORD entryCount;
    PALETTEENTRY entries[256];
  } logPalette;
  PALETTEENTRY systemEntries[256];

  UINT realized = 0xffffffff;
  const int staticHalf = GetDeviceCaps(dc, NUMCOLORS) / 2;
  int highStart = 0x100 - staticHalf;
  if (g_savedPalette && palette != g_palette)
    ReleasePalette(dc);
  if ((GetDeviceCaps(dc, RASTERCAPS) & RC_PALETTE) == 0)
  {
    g_palette = palette;
    return 0;
  }
  if (palette)
  {
    SetSystemPaletteUse(dc, SYSPAL_NOSTATIC);
    SetSystemPaletteUse(dc, SYSPAL_STATIC);
    GetPaletteEntries(palette, 0, 0x100, logPalette.entries);
    GetSystemPaletteEntries(dc, 0, 0x100, systemEntries);
    for (int i = 0; i < staticHalf; ++i)
    {
      logPalette.entries[i] = systemEntries[i];
      logPalette.entries[i].peFlags = 0;
    }
    for (int i = highStart; i < 0x100; ++i)
    {
      logPalette.entries[i] = systemEntries[i];
      logPalette.entries[i].peFlags = 0;
    }
    for (int i = staticHalf; i < highStart; ++i)
      logPalette.entries[i].peFlags = PC_NOCOLLAPSE;
    logPalette.version = 0x300;
    logPalette.entryCount = 1;
    HPALETTE identity = CreatePalette(reinterpret_cast<LOGPALETTE*>(&logPalette));
    if (identity)
    {
      if (ResizePalette(identity, 0x100))
      {
        SetPaletteEntries(identity, 0, 0x100, logPalette.entries);
        DeleteObject(palette);
        palette = identity;
      }
    }
    g_palette = palette;
    if (g_savedPalette)
      DeleteObject(g_savedPalette);
    g_savedPalette = nullptr;
    g_savedPalette = SelectPalette(dc, g_palette, FALSE);
    realized = RealizePalette(dc);
    g_displayPalette = palette;
  }
  return realized;
}

UINT CoolWindow::RealizeCurrentPalette()
{
  HWND hwnd = m_hwnd;
  HDC dc = GetDC(hwnd);
  SelectPalette(dc, g_palette, FALSE);
  UINT realized = RealizePalette(dc);
  ReleaseDC(hwnd, dc);
  return realized;
}

void ReleasePalette(HDC dc)
{
  if (g_savedPalette)
  {
    SelectPalette(dc, g_savedPalette, FALSE);
    RealizePalette(dc);
    g_savedPalette = nullptr;
  }
  if (g_palette)
  {
    DeleteObject(g_palette);
    g_palette = nullptr;
  }
}

// ---- Resource DLL slots -----------------------------------------------------------------------------------------

HMODULE FindResourceModule(uint32_t slot)
{
  auto* resources = CurrentUiResources();
  if (!resources)
    return nullptr;
  ResourceDll* dll = static_cast<ResourceDll*>(resources->libraries.FindById(slot));
  return dll ? dll->m_module : nullptr;
}

uint32_t FindSlotOfModule(HMODULE module)
{
  auto* resources = CurrentUiResources();
  if (!resources)
    return 0;
  uint32_t slot = 0;
  for (const auto& entry : resources->libraries.Entries())
  {
    if (slot != 0)
      break;
    auto* dll = static_cast<ResourceDll*>(entry.get());
    if (module == dll->m_module)
      slot = dll->m_id;
  }
  return slot;
}

HMODULE LoadResourceModule(const char* basePath, const char* fileName, uint32_t slot)
{
  auto* resources = CurrentUiResources();
  if (!resources)
    return nullptr;
  HMODULE module = FindResourceModule(slot);
  if (module)
    return module;

  StrStream path;
  strstream_ctor(&path, 1);
  if (basePath)
    ostream_operator_ls_cstr(ostream_operator_ls_cstr(path.Out(), basePath), "\\");
  ostream_operator_ls_char(ostream_operator_ls_cstr(path.Out(), fileName), 0);

  ResourceDll* dll = NewOrNull<ResourceDll>(fileName);
  if (!dll || !dll->m_module)
  {
    delete dll;
    dll = NewOrNull<ResourceDll>(strstreambuf_str(StreambufOf(&path)));
    strstreambuf_freeze(StreambufOf(&path), 0);
    ClearIosState(&path);
  }
  if (dll)
  {
    dll->m_id = slot;
    try
    {
      if (resources->libraries.AddWithId(dll) != 0)
        module = dll->m_module;
      else
        delete dll;
    }
    catch (...)
    {
      delete dll;
      throw;
    }
  }
  strstream_dtor(path.Ios());
  ios_dtor(path.Ios());
  return module;
}
