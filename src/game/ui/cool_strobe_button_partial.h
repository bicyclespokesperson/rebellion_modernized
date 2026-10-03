#pragma once
#include <cstdint>

#include "core/layout.h"
#include "ui/cool_window.h"

class DibBitmap;

// PARTIAL declaration of CoolStrobeButton (ctor 0x602150, vtable 0x66df58), which belongs to the button module
// (0x602150-0x607ea0). Only what CoolTabControl uses is declared here; replace this header with the real one.
// State bits of m_state used by the tab control.
enum CoolStrobeButtonState : uint32_t {
    kStrobeSelected = 0x04,  // the button is drawn with its "selected" bitmap
    kStrobeTab = 0x08,       // set by CoolTabControl::AddTab
};

class CoolStrobeButton : public CoolWindow {
public:
    void SetStateFlags(uint32_t flags);                    // 0x6030c0: m_state |= flags, invalidates
    void ClearStateFlags(uint32_t flags);                  // 0x6030f0: m_state &= ~flags, invalidates
    bool HasStateFlags(uint32_t flags);                    // 0x603120
    void LoadStateBitmap(int state, uint16_t resourceId);  // 0x603150
    uint16_t GetStateResourceId(int state);                // 0x6034a0
    DibBitmap* GetStateBitmap(int state);                  // 0x603550

    uint8_t m_unk94[0x10];          // +0x94 state bitmaps (0x94..0xa0)
    DibBitmap* m_parentBackground;  // +0xa4 picture the button's background is cut from
    uint8_t m_unka8[0x1c];          // +0xa8
    uint32_t m_state;               // +0xc4 CoolStrobeButtonState
    uint8_t m_unkc8[0x44];          // +0xc8
    int m_backgroundX;              // +0x10c where the button sits in m_parentBackground
    int m_backgroundY;              // +0x110
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_OFFSET(CoolStrobeButton, m_parentBackground, 0xa4);
REB_ASSERT_OFFSET(CoolStrobeButton, m_state, 0xc4);
REB_ASSERT_OFFSET(CoolStrobeButton, m_backgroundX, 0x10c);
REB_ASSERT_OFFSET(CoolStrobeButton, m_backgroundY, 0x110);
#pragma clang diagnostic pop
