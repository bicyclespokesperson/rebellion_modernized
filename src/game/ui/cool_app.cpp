#include "ui/cool_app.h"

#include <cstring>
#include <string.h>

#include "core/vptr_store.h"
#include "ui/cool_timer.h"
#include "ui/resource_dll.h"

CoolApp::CoolApp(HINSTANCE instance, const char* className, uint32_t style, int x, int y, int width, int height,
                 const char* windowName, HBRUSH background, HICON icon, HCURSOR cursor, HMENU menu,
                 uint32_t unknown58, uint32_t flags)
    : CoolWindow(instance, className, style, x, y, width, height, nullptr, windowName, background, icon, cursor, menu,
                 unknown58, flags) {
    BeginTimerPeriod(10);
    m_unknown9c = 0;
    m_appName = static_cast<char*>(operator new(std::strlen(className) + 1));
    std::memcpy(m_appName, className, std::strlen(className) + 1);
    m_unknownA0 = 0;
    m_unknownA4 = 0;
    m_screenWidth = 0;
    m_screenHeight = 0;
    m_unknown98 = 0;
    m_defaultFontSlot = 0;
    g_app = this;
}

CoolApp::~CoolApp() {
    if (g_palette) DeleteObject(g_palette);
    if (m_appName) operator delete(m_appName);
    g_app = nullptr;
    EndTimerPeriod();
    REB_KEEP_VPTR_STORE();
}

void CoolApp::SetScreenSize(int width, int height) {
    m_screenWidth = width;
    m_screenHeight = height;
}

int CoolApp::FilterMessage(MSG* message) {
    if (message->message != WM_KEYDOWN || message->wParam != VK_RETURN) return 1;
    GetClassNameA(message->hwnd, g_classNameBuffer, 0xff);
    if (_stricmp(g_classNameBuffer, "edit") != 0) return 1;

    HWND parentWindow = GetParent(message->hwnd);
    CoolWindow* parent = GetCoolWindow(parentWindow);
    CoolWindow* child = nullptr;
    if (parent) child = parent->m_children.FindByHwnd(message->hwnd);
    std::memset(g_classNameBuffer, 0, 0xff);
    message->message = 0x407;
    message->hwnd = parentWindow;
    message->lParam = reinterpret_cast<LPARAM>(child);
    message->wParam = child ? child->m_childId : 0;
    return 1;
}

HMODULE __stdcall CoolGetResourceModule(uint32_t slot) { return FindResourceModule(slot); }

HMODULE __stdcall CoolRegisterResourceDll(const char* basePath, const char* fileName, uint32_t slot) {
    return LoadResourceModule(basePath, fileName, slot);
}

void __stdcall CoolUnloadResourceSlot(uint32_t slot) { g_resourceDlls.UnloadSlot(slot); }

HMODULE CoolAppGetResourceModule(uint32_t slot) {
    HMODULE module = nullptr;
    if (g_app) module = CoolGetResourceModule(slot);
    return module;
}

void CoolAppRegisterResourceDll(const char* basePath, const char* fileName, uint32_t slot) {
    if (g_app) CoolRegisterResourceDll(basePath, fileName, slot);
}

void CoolAppUnloadResourceSlot(uint32_t slot) {
    if (g_app) CoolUnloadResourceSlot(slot);
}

int GetDefaultFontSlot() {
    int slot = 0;
    if (g_app) slot = g_app->m_defaultFontSlot;
    return slot;
}
