#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "platform/win32.h"
#include "ui/cool_window.h"

// The application's main window: the base class of RebellionApplication (0x413000). There is one, g_app; it also
// receives WM_ACTIVATEAPP, which Windows sends to the first window of the application.
class CoolApp : public CoolWindow {
public:
    CoolApp(HINSTANCE instance, const char* className, uint32_t style, int x, int y, int width, int height,
            const char* windowName, HBRUSH background, HICON icon, HCURSOR cursor, HMENU menu, uint32_t unknown58,
            uint32_t flags);  // 0x5fb850: sets g_app, starts the 10 ms timer period
    virtual ~CoolApp();       // 0x5fb980 (scalar deleting destructor 0x5fb960)

    // Slot 9: called by the main loop before TranslateMessage. Turns Return in a standard "edit" control into a
    // 0x407 message for the control's parent (wParam = child id, lParam = child). Always returns 1.
    virtual int FilterMessage(MSG* message);  // 0x5fba70

    void SetScreenSize(int width, int height);  // 0x5fba00

    char* m_appName;           // +0x94 heap copy of the class name
    uintptr_t m_unknown98;     // +0x98 the accelerator table handle (HACCEL) of the main window
    int m_unknown9c;           // +0x9c
    int m_unknownA0;           // +0xa0
    int m_unknownA4;           // +0xa4
    int m_screenWidth;         // +0xa8
    int m_screenHeight;        // +0xac
    int m_defaultFontSlot;     // +0xb0 font every new CoolWindow starts with (0: none)
};

// Cool's resource-DLL API (stdcall wrappers kept by the application).
HMODULE REB_STDCALL CoolGetResourceModule(uint32_t slot);                                           // 0x5fba20
HMODULE REB_STDCALL CoolRegisterResourceDll(const char* basePath, const char* fileName, uint32_t slot);  // 0x5fba30
void REB_STDCALL CoolUnloadResourceSlot(uint32_t slot);                                            // 0x5fba50

HMODULE CoolAppGetResourceModule(uint32_t slot);   // 0x6037f0: 0 without an application
void CoolAppRegisterResourceDll(const char* basePath, const char* fileName, uint32_t slot);  // 0x603810
void CoolAppUnloadResourceSlot(uint32_t slot);   // 0x603830
int GetDefaultFontSlot();                  // 0x603850: g_app default font slot, 0 without an application

extern CoolApp* g_app;                  // 0x6be3b8
extern char g_classNameBuffer[0x100];   // 0x6be3c0 scratch for GetClassNameA

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CoolApp, 0xb4);
REB_ASSERT_OFFSET(CoolApp, m_appName, 0x94);
REB_ASSERT_OFFSET(CoolApp, m_screenWidth, 0xa8);
REB_ASSERT_OFFSET(CoolApp, m_defaultFontSlot, 0xb0);
#pragma clang diagnostic pop
