#pragma once
#include <cstdint>

#include "core/containers.h"
#include "core/layout.h"
#include "platform/win32.h"

// Base of every window in the game ("Cool" GUI toolkit, 0x5fef50-0x610b20). Only the part the movie windows
// need is declared here; the toolkit's own reconstruction owns the rest of the class.
//
// A CoolWindow is an AvlNode (it sits in the tree of its parent's children). Its HWND exists only after
// Create(); the window procedure is a virtual that takes `this` on the stack (__stdcall member).
class CoolWindow : public AvlNode {
public:
    CoolWindow(HINSTANCE instance, const char* className, uint32_t style, int x, int y, int width, int height,
               CoolWindow* parent, const char* windowName, HBRUSH background, int unk60, HCURSOR cursor, int unk68,
               int unk58, uint32_t flags);
    virtual ~CoolWindow();

    virtual void Show(int command);  // ShowWindow
    virtual void Hide();             // Show(SW_HIDE)
    virtual void Enable();
    virtual void Disable();
    virtual LRESULT __stdcall WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
    virtual void Virtual06(int value);
    virtual int Virtual07();
    virtual void Virtual08();

    // Registers the window class if needed and calls CreateWindowEx; true when the window exists.
    bool Create(int createFlag);

    HWND m_hwnd;                   // +0x18
    HINSTANCE m_instance;          // +0x1c
    CoolWindow* m_parent;          // +0x20
    uint8_t m_unk24[0x94 - 0x24];  // +0x24
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CoolWindow, 0x94);
REB_ASSERT_OFFSET(CoolWindow, m_hwnd, 0x18);
REB_ASSERT_OFFSET(CoolWindow, m_instance, 0x1c);
REB_ASSERT_OFFSET(CoolWindow, m_parent, 0x20);
#pragma clang diagnostic pop
