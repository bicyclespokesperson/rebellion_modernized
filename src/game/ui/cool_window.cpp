#include "ui/cool_window.h"

#include <cstring>

#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/strstream_io.h"
#include "core/vptr_store.h"
#include "ui/cool_app.h"
#include "ui/resource_dll.h"

// ---- Window object registry ---------------------------------------------------------------------------------

namespace {

uint32_t WindowObjectId(const CoolWindow* window) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(window)); }

// The original copies without checking the allocation (a failed allocation crashes in the copy).
char* DuplicateCString(const char* text) {
    char* copy = static_cast<char*>(operator new(std::strlen(text) + 1));
    std::memcpy(copy, text, std::strlen(text) + 1);
    return copy;
}

constexpr UINT kToolInfoSize = 0x2c;
constexpr UINT kClassStyle = CS_VREDRAW | CS_HREDRAW | CS_DBLCLKS;

TTTOOLINFOA MakeToolInfo(HWND owner, HINSTANCE instance, UINT flags, UINT_PTR toolId, LPSTR text) {
    TTTOOLINFOA info;
    info.cbSize = kToolInfoSize;
    info.uFlags = flags;
    info.hwnd = owner;
    info.uId = toolId;
    info.hinst = instance;
    info.lpszText = text;
    return info;
}

}  // namespace

int RegisterWindowObject(CoolWindow* window) {
    const uint32_t id = WindowObjectId(window);
    if (g_windowRegistry.FindById(id)) return false;
    IdNode* node = NewOrNull<IdNode>(id);
    if (!node) return false;
    return g_windowRegistry.AddWithId(node) == id;
}

int IsWindowObjectRegistered(CoolWindow* window) { return g_windowRegistry.FindById(WindowObjectId(window)) != nullptr; }

int UnregisterWindowObject(CoolWindow* window) {
    IdNode* node = g_windowRegistry.DetachById(WindowObjectId(window));
    if (node) delete node;
    return node != nullptr;
}

CoolWindow* GetCoolWindow(HWND hwnd) {
    CoolWindow* window = reinterpret_cast<CoolWindow*>(GetWindowLongPtr(hwnd, 0));
    if (window && !IsWindowObjectRegistered(window)) window = nullptr;
    return window;
}

// ---- Helper containers ----------------------------------------------------------------------------------------

HwndNode::HwndNode(HWND hwnd) : m_hwnd(hwnd), m_window(nullptr) {}

HwndNode::~HwndNode() { REB_KEEP_VPTR_STORE(); }

HwndTree::HwndTree() {}

HwndTree::~HwndTree() { REB_KEEP_VPTR_STORE(); }

// Linker-folded with IdAvlTree::IsLess (0x5f5560), which compares the dword at +0x18.
int HwndTree::IsLess(AvlNode* a, AvlNode* b) {
    return static_cast<HwndNode*>(a)->m_hwnd < static_cast<HwndNode*>(b)->m_hwnd;
}

HwndNode* HwndTree::FindByHwnd(HWND hwnd) {
    HwndNode* node = static_cast<HwndNode*>(m_root);
    while (node) {
        if (node->m_hwnd < hwnd)
            node = static_cast<HwndNode*>(node->m_right);
        else if (hwnd < node->m_hwnd)
            node = static_cast<HwndNode*>(node->m_left);
        else
            return node;
    }
    return nullptr;
}

void HwndTree::RemoveAndDelete(HWND hwnd) {
    HwndNode* node = FindByHwnd(hwnd);
    if (node) {
        Remove(node);
        delete node;
    }
}

PopupEntry::PopupEntry(HwndNode* node) : m_node(node) {}

PopupEntry::~PopupEntry() {
    if (m_node) delete m_node;
    REB_KEEP_VPTR_STORE();
}

ChildWindowTree::ChildWindowTree() {}

ChildWindowTree::~ChildWindowTree() { REB_KEEP_VPTR_STORE(); }

int ChildWindowTree::IsLess(AvlNode* a, AvlNode* b) {
    return static_cast<CoolWindow*>(a)->m_childId < static_cast<CoolWindow*>(b)->m_childId;
}

CoolWindow* ChildWindowTree::FindById(uint32_t id) {
    CoolWindow* node = static_cast<CoolWindow*>(m_root);
    while (node) {
        if (id < node->m_childId)
            node = static_cast<CoolWindow*>(node->m_left);
        else if (node->m_childId < id)
            node = static_cast<CoolWindow*>(node->m_right);
        else
            return node;
    }
    return nullptr;
}

CoolWindow* ChildWindowTree::FindByHwnd(HWND hwnd) {
    for (CoolWindow* node = static_cast<CoolWindow*>(First()); node; node = static_cast<CoolWindow*>(node->m_successor)) {
        if (node->m_hwnd == hwnd) return node;
    }
    return nullptr;
}

IdAvlTreeElement::IdAvlTreeElement() {}

IdAvlTreeElement::~IdAvlTreeElement() { REB_KEEP_VPTR_STORE(); }

// ---- CoolWindow: construction and destruction ---------------------------------------------------------------

CoolWindow::CoolWindow(HINSTANCE instance, const char* className, uint32_t style, int x, int y, int width, int height,
                       CoolWindow* parent, const char* windowName, HBRUSH background, HICON icon, HCURSOR cursor,
                       HMENU menu, uint32_t unknown58, uint32_t flags) {
    int borderWidth = 0;
    int borderHeight = 0;
    if (g_defaultCursor && (flags & kCoolOwnCursor) == 0) cursor = g_defaultCursor;
    m_className = className ? DuplicateCString(className) : nullptr;
    m_windowName = windowName ? DuplicateCString(windowName) : nullptr;
    m_x = x;
    m_parent = parent;
    m_y = y;
    m_width = width;
    m_height = height;
    if (style & WS_BORDER) {
        borderWidth = GetSystemMetrics(SM_CXBORDER);
        borderHeight = GetSystemMetrics(SM_CYBORDER);
    }
    if (style & WS_DLGFRAME) {
        borderWidth += GetSystemMetrics(SM_CXDLGFRAME);
        borderHeight += GetSystemMetrics(SM_CYDLGFRAME);
        if (style & WS_BORDER) borderHeight += GetSystemMetrics(SM_CYCAPTION);
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
    if (int fontSlot = GetDefaultFontSlot()) SetFont(fontSlot);
    m_tooltipText = nullptr;
    m_tooltipOwner = nullptr;
}

CoolWindow::~CoolWindow() {
    UnregisterWindowObject(this);
    if (m_className) operator delete(m_className);
    if (m_windowName) operator delete(m_windowName);
    if (m_tooltipText) RemoveTool(this);

    HwndNode* previous = nullptr;
    for (HwndNode* node = g_captureStack; node;) {
        HwndNode* next = static_cast<HwndNode*>(node->m_successor);
        if (node->m_hwnd == m_hwnd) {
            if (previous)
                previous->m_successor = next;
            else
                g_captureStack = next;
            delete node;
        } else {
            previous = node;
        }
        node = next;
    }
    if (g_captureStack && g_captureStack->m_hwnd != GetCapture()) SetCapture(g_captureStack->m_hwnd);
    EndPopup();
    REB_KEEP_VPTR_STORE();
}

// ---- CoolWindow: window class and creation -------------------------------------------------------------------

ATOM CoolWindow::RegisterWindowClass(UINT classStyle) {
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

bool CoolWindow::Create(int useExStyle) {
    WNDCLASSA existingClass;
    if (!GetClassInfoA(m_instance, m_className, &existingClass)) RegisterWindowClass(kClassStyle);
    HWND parentWindow = m_parent ? m_parent->m_hwnd : nullptr;
    HMENU menu = m_parent ? reinterpret_cast<HMENU>(static_cast<UINT_PTR>(m_childId)) : m_menu;
    CoolWindow* creationParameter = this;
    // BUG(original): the extended style is used whichever value useExStyle has.
    (void)useExStyle;
    m_hwnd = CreateWindowExA(m_exStyle, m_className, m_windowName, m_style, m_x, m_y, m_width, m_height, parentWindow,
                             menu, m_instance, &creationParameter);
    if (!m_hwnd) GetLastError();
    RegisterWithActivePopup();
    return m_hwnd != nullptr;
}

void CoolWindow::Show(int command) {
    if (m_hwnd) ShowWindow(m_hwnd, command);
}

void CoolWindow::Hide() { Show(0); }

// The next two are linker-folded with the same do-nothing functions of other classes.
int CoolWindow::OnRightButtonUp(int, int, CoolWindow*) { return 0; }

void CoolWindow::Virtual08(int, int, int) {}

void CoolWindow::Enable() {
    if (!IsWindowEnabled(m_hwnd)) EnableWindow(m_hwnd, TRUE);
}

void CoolWindow::Disable() {
    if (IsWindowEnabled(m_hwnd)) EnableWindow(m_hwnd, FALSE);
}

BOOL CoolWindow::IsEnabled() { return IsWindowEnabled(m_hwnd); }

void CoolWindow::CenterInParent() {
    int x;
    int y;
    if (m_parent) {
        x = m_parent->m_clientWidth / 2 - m_width / 2;
        y = m_parent->m_clientHeight / 2 - m_height / 2;
    } else {
        x = GetSystemMetrics(SM_CXSCREEN) / 2 - m_width / 2;
        y = GetSystemMetrics(SM_CYSCREEN) / 2 - m_height / 2;
    }
    SetWindowPos(m_hwnd, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

void CoolWindow::SetWindowTitle(const char* title) {
    if (m_windowName) operator delete(m_windowName);
    m_windowName = title ? DuplicateCString(title) : nullptr;
    if (m_hwnd) SetWindowTextA(m_hwnd, m_windowName);
}

void CoolWindow::SetClassName(const char* className) {
    if (m_className) operator delete(m_className);
    m_className = DuplicateCString(className);
}

void CoolWindow::DestroyNativeWindow() {
    if (m_hwnd) DestroyWindow(m_hwnd);
}

void CoolWindow::SetFont(int fontSlot) { m_font = CoolGetFont(fontSlot); }

void CoolWindow::SelectCurrentFont(HDC dc) {
    if (m_font)
        m_savedFont = SelectObject(dc, m_font);
    else
        m_savedFont = nullptr;
}

void CoolWindow::RestoreFont(HDC dc) {
    if (m_savedFont) SelectObject(dc, m_savedFont);
}

void CoolWindow::SetTooltipText(const char* text) {
    if (m_tooltipText) operator delete(m_tooltipText);
    if (!text) {
        m_tooltipText = nullptr;
        return;
    }
    m_tooltipText = static_cast<char*>(operator new(std::strlen(text) + 1));
    if (m_tooltipText) std::memcpy(m_tooltipText, text, std::strlen(text) + 1);
}

// ---- CoolWindow: default message handling ---------------------------------------------------------------------

LRESULT __stdcall CoolWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_SHOWWINDOW) {
        if (wParam) {
            for (AvlNode* node = m_linkedWindows.First(); node; node = node->m_successor)
                static_cast<HwndNode*>(node)->m_window->Show(SW_SHOW);
        }
        return 0;
    }
    if (message == WM_RBUTTONUP) return OnRightButtonUp(static_cast<short>(lParam), static_cast<short>(lParam >> 16), this);
    if (message == 0x405) {
        DestroyChildById(static_cast<uint32_t>(wParam));
        return 0;
    }
    return DefWindowProcA(m_hwnd, message, wParam, lParam);
}

int CoolWindow::DestroyChildById(uint32_t childId) {
    CoolWindow* child = m_children.FindById(childId);
    if (child) {
        m_children.Remove(child);
        child->DestroyNativeWindow();
        delete child;
    }
    return child != nullptr;
}

// ---- Message dispatcher ----------------------------------------------------------------------------------------

namespace {

CoolWindow* WindowObjectOf(HWND hwnd) { return reinterpret_cast<CoolWindow*>(GetWindowLongPtr(hwnd, 0)); }

LRESULT Forward(CoolWindow* window, UINT message, WPARAM wParam, LPARAM lParam) {
    return window->WindowProc(message, wParam, lParam);
}

LRESULT ForwardOrDefault(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    CoolWindow* window = WindowObjectOf(hwnd);
    if (window) return Forward(window, message, wParam, lParam);
    return DefWindowProcA(hwnd, message, wParam, lParam);
}

LRESULT HandleSystemCommand(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    CoolWindow* window = WindowObjectOf(hwnd);
    if (!window) return DefWindowProcA(hwnd, WM_SYSCOMMAND, wParam, lParam);
    const UINT command = static_cast<UINT>(wParam) & 0xfff0;
    if (command < 0xf011) {
        if (command == SC_MOVE) {
            if (window->m_flags & kCoolNoMove) return 0;
            return Forward(window, WM_SYSCOMMAND, wParam, lParam);
        }
        if (command == SC_SIZE) {
            if (window->m_flags & kCoolNoSize) return 0;
            return Forward(window, WM_SYSCOMMAND, wParam, lParam);
        }
    } else {
        if (command == SC_MINIMIZE) {
            if (window->m_flags & kCoolNoMinimize) return 0;
            return Forward(window, WM_SYSCOMMAND, wParam, lParam);
        }
        if (command == SC_MAXIMIZE) {
            if (window->m_flags & kCoolNoMaximize) return 0;
        } else if (command == SC_RESTORE) {
            if ((window->m_flags & kCoolNoMinimize) && IsIconic(window->m_hwnd)) return 0;
            if ((window->m_flags & kCoolNoMaximize) && IsZoomed(window->m_hwnd)) return 0;
            return Forward(window, WM_SYSCOMMAND, wParam, lParam);
        }
    }
    return Forward(window, WM_SYSCOMMAND, wParam, lParam);
}

LRESULT HandleNonClientDoubleClick(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    CoolWindow* window = WindowObjectOf(hwnd);
    if (!window) return 0;
    if (wParam != HTCAPTION) return DefWindowProcA(hwnd, WM_NCLBUTTONDBLCLK, wParam, lParam);
    if ((window->m_flags & kCoolNoMaximize) && IsZoomed(window->m_hwnd)) return 0;
    if ((window->m_flags & kCoolNoMinimize) && IsIconic(window->m_hwnd)) return 0;
    return Forward(window, WM_NCLBUTTONDBLCLK, HTCAPTION, lParam);
}

LRESULT HandleCreate(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    const CREATESTRUCTA* createStruct = reinterpret_cast<const CREATESTRUCTA*>(lParam);
    CoolWindow* window = *static_cast<CoolWindow* const*>(createStruct->lpCreateParams);
    RegisterWindowObject(window);
    SetWindowLongPtr(hwnd, 0, reinterpret_cast<LONG_PTR>(window));
    if (!window) return DefWindowProcA(hwnd, WM_CREATE, wParam, lParam);
    HDC dc = GetDC(hwnd);
    PatBlt(dc, 0, 0, window->m_clientWidth, window->m_clientHeight, BLACKNESS);
    ReleaseDC(hwnd, dc);
    window->m_hwnd = hwnd;
    return Forward(window, WM_CREATE, wParam, lParam);
}

LRESULT HandleDestroy(HWND hwnd, WPARAM wParam, LPARAM lParam) {
    CoolWindow* window = WindowObjectOf(hwnd);
    if (!window) return DefWindowProcA(hwnd, WM_DESTROY, wParam, lParam);
    const LRESULT result = Forward(window, WM_DESTROY, wParam, lParam);
    if (window->m_tooltipText) window->RemoveTool(window);
    if (g_captureStack && g_captureStack->m_hwnd == GetCapture() && g_captureStack->m_hwnd == window->m_hwnd)
        ReleaseCapture();
    return result;
}

LRESULT HandleMoveOrSize(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    CoolWindow* window = WindowObjectOf(hwnd);
    if (!window) return DefWindowProcA(hwnd, message, wParam, lParam);
    const uint32_t packed = static_cast<uint32_t>(lParam);
    if (message == WM_MOVE) {
        window->m_x = packed & 0xffff;
        window->m_y = packed >> 16;
    } else {
        window->m_clientWidth = packed & 0xffff;
        window->m_clientHeight = packed >> 16;
    }
    return Forward(window, message, wParam, lParam);
}

}  // namespace

LRESULT CALLBACK CoolWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message != WM_CREATE && message != WM_NCCREATE && message != WM_MDICREATE) {
        CoolWindow* window = WindowObjectOf(hwnd);
        if (window && !IsWindowObjectRegistered(window)) return DefWindowProcA(hwnd, message, wParam, lParam);
    }
    if (message > WM_ACTIVATEAPP) {
        if (message == WM_NCDESTROY) {
            CoolWindow* window = WindowObjectOf(hwnd);
            if (!window) return DefWindowProcA(hwnd, WM_NCDESTROY, wParam, lParam);
            return Forward(window, WM_NCDESTROY, wParam, lParam);
        }
        if (message == WM_NCLBUTTONDBLCLK) return HandleNonClientDoubleClick(hwnd, wParam, lParam);
        if (message == WM_SYSCOMMAND) return HandleSystemCommand(hwnd, wParam, lParam);
        return ForwardOrDefault(hwnd, message, wParam, lParam);
    }
    if (message == WM_ACTIVATEAPP) {
        if (!g_app) return DefWindowProcA(hwnd, WM_ACTIVATEAPP, wParam, lParam);
        return Forward(g_app, WM_ACTIVATEAPP, wParam, lParam);
    }
    switch (message) {
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

void CoolWindow::CreateTooltipControl(LPARAM delayMs) {
    if (g_tooltipWindow) return;
    g_tooltipWindow = CreateWindowExA(0, g_tooltipClassName, nullptr, TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT,
                                      CW_USEDEFAULT, CW_USEDEFAULT, m_hwnd, nullptr, m_instance, nullptr);
    if (g_tooltipWindow) {
        SendMessageA(g_tooltipWindow, TTM_ACTIVATE, 1, 0);
        SendMessageA(g_tooltipWindow, TTM_SETDELAYTIME, 0, delayMs);
        g_tooltipHook = SetWindowsHookExA(WH_GETMESSAGE, TooltipHookProc, nullptr, GetCurrentThreadId());
    }
}

void CoolWindow::AddTool(CoolWindow* tool, const char* text) {
    if (!tool) return;
    tool->SetTooltipText(text);
    tool->m_tooltipOwner = m_hwnd;
    TTTOOLINFOA info = MakeToolInfo(m_hwnd, m_instance, TTF_IDISHWND, reinterpret_cast<UINT_PTR>(tool->m_hwnd),
                                    tool->m_tooltipText);
    if (g_tooltipWindow) SendMessageA(g_tooltipWindow, TTM_ADDTOOLA, 0, reinterpret_cast<LPARAM>(&info));
}

void CoolWindow::AddToolFromResource(CoolWindow* tool, UINT stringId) {
    char text[0x100];
    LoadResourceString(stringId, text, sizeof(text));
    AddTool(tool, text);
}

void CoolWindow::AddToolFromString(CoolWindow* tool, const ResourceStringId* stringId) {
    String text(stringId);
    if (text.m_text) AddTool(tool, text.m_text);
}

void CoolWindow::AddRectTool(RECT area, UINT toolId, const char* text) {
    TTTOOLINFOA info = MakeToolInfo(m_hwnd, m_instance, 0, toolId, const_cast<LPSTR>(text));
    CopyRect(&info.rect, &area);
    if (g_tooltipWindow) SendMessageA(g_tooltipWindow, TTM_ADDTOOLA, 0, reinterpret_cast<LPARAM>(&info));
}

void CoolWindow::AddRectToolFromResource(RECT area, UINT toolId, UINT stringId) {
    char text[0x100];
    LoadResourceString(stringId, text, sizeof(text));
    AddRectTool(area, toolId, text);
}

void CoolWindow::AddRectToolFromString(RECT area, UINT toolId, const ResourceStringId* stringId) {
    String text(stringId);
    if (text.m_text) AddRectTool(area, toolId, text.m_text);
}

void CoolWindow::UpdateToolFromString(CoolWindow* tool, const ResourceStringId* stringId) {
    String text(stringId);
    if (text.m_text) UpdateToolText(tool, text.m_text);
}

void CoolWindow::UpdateToolText(CoolWindow* tool, const char* text) {
    if (!tool) return;
    tool->SetTooltipText(text);
    TTTOOLINFOA info = MakeToolInfo(m_hwnd, m_instance, TTF_IDISHWND, reinterpret_cast<UINT_PTR>(tool->m_hwnd),
                                    tool->m_tooltipText);
    if (g_tooltipWindow) SendMessageA(g_tooltipWindow, TTM_UPDATETIPTEXTA, 0, reinterpret_cast<LPARAM>(&info));
}

void CoolWindow::RemoveTool(CoolWindow* tool) {
    TTTOOLINFOA info = MakeToolInfo(tool->m_tooltipOwner, m_instance, TTF_IDISHWND,
                                    reinterpret_cast<UINT_PTR>(tool->m_hwnd), nullptr);
    SetRectEmpty(&info.rect);
    if (g_tooltipWindow) SendMessageA(g_tooltipWindow, TTM_DELTOOLA, 0, reinterpret_cast<LPARAM>(&info));
    if (tool->m_tooltipText) {
        operator delete(tool->m_tooltipText);
        tool->m_tooltipText = nullptr;
    }
}

void CoolWindow::RemoveRectTool(UINT toolId) {
    TTTOOLINFOA info = MakeToolInfo(m_hwnd, m_instance, 0, toolId, nullptr);
    SetRectEmpty(&info.rect);
    if (g_tooltipWindow) SendMessageA(g_tooltipWindow, TTM_DELTOOLA, 0, reinterpret_cast<LPARAM>(&info));
}

void ShutdownTooltips() {
    if (g_tooltipHook) {
        UnhookWindowsHookEx(g_tooltipHook);
        g_tooltipHook = nullptr;
    }
    g_tooltipWindow = nullptr;
}

LRESULT CALLBACK TooltipHookProc(int code, WPARAM wParam, LPARAM lParam) {
    MSG relayed;
    const MSG* message = reinterpret_cast<const MSG*>(lParam);
    if (message && code >= 0 && message->message >= WM_MOUSEMOVE && message->message <= WM_RBUTTONUP) {
        relayed.message = message->message;
        relayed.hwnd = message->hwnd;
        relayed.wParam = message->wParam;
        relayed.lParam = message->lParam;
        if (g_tooltipWindow) SendMessageA(g_tooltipWindow, TTM_RELAYEVENT, 0, reinterpret_cast<LPARAM>(&relayed));
    }
    return CallNextHookEx(g_tooltipHook, code, wParam, lParam);
}

// ---- Popup mode ------------------------------------------------------------------------------------------------------

LRESULT CALLBACK MouseHookProc(int code, WPARAM wParam, LPARAM lParam) {
    bool swallow = false;
    PopupEntry* top = static_cast<PopupEntry*>(g_popupStack.m_top);
    if (code >= 0 && code != HC_NOREMOVE && top && top->m_node) {
        CoolWindow* popup = top->m_node->m_window;
        if (popup) {
            const MOUSEHOOKSTRUCT* mouse = reinterpret_cast<const MOUSEHOOKSTRUCT*>(lParam);
            if (!popup->m_popupMembers.FindByHwnd(mouse->hwnd)) swallow = true;
        }
    }
    if (!swallow) return CallNextHookEx(g_mouseHook, code, wParam, lParam);
    return 1;
}

bool CoolWindow::BeginPopup() {
    if (!g_popupStack.m_top) g_mouseHook = SetWindowsHookExA(WH_MOUSE, MouseHookProc, m_instance, GetCurrentThreadId());
    HwndNode* node = NewOrNull<HwndNode>(m_hwnd);
    // BUG(original): a failed allocation is dereferenced here.
    node->m_window = this;
    PopupEntry* entry = NewOrNull<PopupEntry>(node);
    g_popupStack.Push(entry);
    HwndNode* member = NewOrNull<HwndNode>(m_hwnd);
    m_popupMembers.Insert(member);
    ReleaseAllCaptures();
    return true;
}

PopupEntry* __stdcall FindPopupEntry(HWND hwnd) {
    PopupEntry* entry = static_cast<PopupEntry*>(g_popupStack.m_top);
    if (!entry) return nullptr;
    while (!entry->m_node || entry->m_node->m_hwnd != hwnd) {
        entry = static_cast<PopupEntry*>(entry->m_next);
        if (!entry) return nullptr;
    }
    return entry;
}

bool CoolWindow::EndPopup() {
    PopupEntry* entry = FindPopupEntry(m_hwnd);
    if (!entry) return false;
    if (entry->m_node && entry->m_node->m_window) entry->m_node->m_window->m_popupMembers.RemoveAndDelete(m_hwnd);
    g_popupStack.RemoveAndDelete(entry);
    if (!g_popupStack.m_top) {
        UnhookWindowsHookEx(g_mouseHook);
        g_mouseHook = nullptr;
    }
    return true;
}

void CoolWindow::RegisterWithActivePopup() {
    PopupEntry* top = static_cast<PopupEntry*>(g_popupStack.m_top);
    if (!top || !top->m_node || !top->m_node->m_window) return;
    HwndTree& members = top->m_node->m_window->m_popupMembers;
    if (members.FindByHwnd(m_hwnd)) return;
    members.Insert(NewOrNull<HwndNode>(m_hwnd));
}

HWND CurrentPopupHwnd() {
    PopupEntry* top = static_cast<PopupEntry*>(g_popupStack.m_top);
    if (top && top->m_node) return top->m_node->m_hwnd;
    return nullptr;
}

bool DismissPopup() {
    HWND target = GetCapture();
    if (!target) {
        target = CurrentPopupHwnd();
        if (!target) return true;
    }
    SendMessageA(target, 0x40d, 0, 0);
    return false;
}

void CoolWindow::AddLinkedWindow(CoolWindow* window) {
    if (!window || !window->m_hwnd) return;
    HwndNode* node = NewOrNull<HwndNode>(window->m_hwnd);
    if (!node) return;
    node->m_window = window;
    m_linkedWindows.Insert(node);
}

void CoolWindow::RemoveLinkedWindow(CoolWindow* window) {
    if (window) m_linkedWindows.RemoveAndDelete(window->m_hwnd);
}

int ReleaseAllCaptures() {
    int hadCaptures = 0;
    if (g_captureStack) {
        hadCaptures = 1;
        do {
            HwndNode* next = static_cast<HwndNode*>(g_captureStack->m_successor);
            if (g_captureStack) delete g_captureStack;
            g_captureStack = next;
        } while (g_captureStack);
        g_captureStack = nullptr;
    }
    ReleaseCapture();
    return hadCaptures;
}

void __stdcall SetCaptureWindow(HWND hwnd) { SetCapture(hwnd); }

// ---- Drawing helpers and palette -----------------------------------------------------------------------------------

void LoadResourceString(UINT id, LPSTR buffer, int size) { LoadStringA(g_stringModule, id, buffer, size); }

void __stdcall DrawShadowedText(HDC dc, LPCSTR text, LPRECT rect, COLORREF color, COLORREF shadow, UINT format,
                                int offset, int length) {
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

UINT __stdcall SetIdentityPalette(HDC dc, HPALETTE palette) {
    struct {
        WORD version;
        WORD entryCount;
        PALETTEENTRY entries[256];
    } logPalette;
    PALETTEENTRY systemEntries[256];

    UINT realized = 0xffffffff;
    const int staticHalf = GetDeviceCaps(dc, NUMCOLORS) / 2;
    int highStart = 0x100 - staticHalf;
    if (g_savedPalette && palette != g_palette) ReleasePalette(dc);
    if ((GetDeviceCaps(dc, RASTERCAPS) & RC_PALETTE) == 0) {
        g_palette = palette;
        return 0;
    }
    if (palette) {
        SetSystemPaletteUse(dc, SYSPAL_NOSTATIC);
        SetSystemPaletteUse(dc, SYSPAL_STATIC);
        GetPaletteEntries(palette, 0, 0x100, logPalette.entries);
        GetSystemPaletteEntries(dc, 0, 0x100, systemEntries);
        for (int i = 0; i < staticHalf; ++i) {
            logPalette.entries[i] = systemEntries[i];
            logPalette.entries[i].peFlags = 0;
        }
        for (int i = highStart; i < 0x100; ++i) {
            logPalette.entries[i] = systemEntries[i];
            logPalette.entries[i].peFlags = 0;
        }
        for (int i = staticHalf; i < highStart; ++i) logPalette.entries[i].peFlags = PC_NOCOLLAPSE;
        logPalette.version = 0x300;
        logPalette.entryCount = 1;
        HPALETTE identity = CreatePalette(reinterpret_cast<LOGPALETTE*>(&logPalette));
        if (identity) {
            if (ResizePalette(identity, 0x100)) {
                SetPaletteEntries(identity, 0, 0x100, logPalette.entries);
                DeleteObject(palette);
                palette = identity;
            }
        }
        g_palette = palette;
        if (g_savedPalette) DeleteObject(g_savedPalette);
        g_savedPalette = nullptr;
        g_savedPalette = SelectPalette(dc, g_palette, FALSE);
        realized = RealizePalette(dc);
        g_displayPalette = palette;
    }
    return realized;
}

UINT CoolWindow::RealizeCurrentPalette() {
    HWND hwnd = m_hwnd;
    HDC dc = GetDC(hwnd);
    SelectPalette(dc, g_palette, FALSE);
    UINT realized = RealizePalette(dc);
    ReleaseDC(hwnd, dc);
    return realized;
}

void __stdcall ReleasePalette(HDC dc) {
    if (g_savedPalette) {
        SelectPalette(dc, g_savedPalette, FALSE);
        RealizePalette(dc);
        g_savedPalette = nullptr;
    }
    if (g_palette) {
        DeleteObject(g_palette);
        g_palette = nullptr;
    }
}

// ---- Resource DLL slots -----------------------------------------------------------------------------------------

HMODULE FindResourceModule(uint32_t slot) {
    ResourceDll* dll = static_cast<ResourceDll*>(g_resourceDlls.FindById(slot));
    return dll ? dll->m_module : nullptr;
}

uint32_t FindSlotOfModule(HMODULE module) {
    uint32_t slot = 0;
    for (ResourceDll* dll = static_cast<ResourceDll*>(g_resourceDlls.m_head); dll && slot == 0;
         dll = static_cast<ResourceDll*>(dll->m_next)) {
        if (module == dll->m_module) slot = dll->m_id;
    }
    return slot;
}

HMODULE LoadResourceModule(const char* basePath, const char* fileName, uint32_t slot) {
    HMODULE module = FindResourceModule(slot);
    if (module) return module;

    StrStream path;
    strstream_ctor(&path, 1);
    if (basePath) ostream_operator_ls_cstr(ostream_operator_ls_cstr(path.Out(), basePath), "\\");
    ostream_operator_ls_char(ostream_operator_ls_cstr(path.Out(), fileName), 0);

    ResourceDll* dll = NewOrNull<ResourceDll>(fileName);
    // BUG(original): a failed allocation is dereferenced here.
    if (!dll->m_module) {
        if (dll) delete dll;
        dll = NewOrNull<ResourceDll>(strstreambuf_str(StreambufOf(&path)));
        strstreambuf_freeze(StreambufOf(&path), 0);
        ClearIosState(&path);
    }
    if (dll) {
        dll->m_id = slot;
        g_resourceDlls.AddWithId(dll);
        module = dll->m_module;
    }
    strstream_dtor(path.Ios());
    ios_dtor(path.Ios());
    return module;
}
