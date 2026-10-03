#pragma once
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"
#include "ui/cool_window.h"

// Minimal stand-in for the batch-A CoolStrobeButton (ui/cool_button.h, 0x602150-0x604570), reduced to what the
// scroll bars and ButtonRepeatTimer use; names are the owner's. Drop it for the owner's header at merge.

class DibBitmap;

// The "CoolStrobeButton" window class (ctor 0x602150, vtable 0x66df58): a bitmap button.
class CoolStrobeButton : public CoolWindow {
public:
    virtual void Draw(HDC dc, char state);  // slot 9, 0x602d30
    virtual int IsHot();                    // slot 10, 0x452fb0
    virtual void OnRepeatTimer();           // slot 11, 0x6036a0: posts the button's WM_COMMAND again while held

    uint8_t m_unknown94[0x10];  // +0x94
    DibBitmap* m_backdrop;      // +0xa4 not owned
    uint8_t m_unknownA8[0x64];  // +0xa8
    int m_backdropX;            // +0x10c where the button sits in the backdrop
    int m_backdropY;            // +0x110
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(CoolStrobeButton, 0x114);
REB_ASSERT_OFFSET(CoolStrobeButton, m_backdrop, 0xa4);
REB_ASSERT_OFFSET(CoolStrobeButton, m_backdropX, 0x10c);
#pragma clang diagnostic pop
