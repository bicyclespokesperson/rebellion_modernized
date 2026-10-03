#pragma once
#include <cstdint>

#include "core/layout.h"
#include "ui/cool_window.h"

class DibBitmap;

// PARTIAL declaration of CoolStrobeButton (ctor 0x602150, size 0x114), which belongs to the button module
// (0x602150-0x607ea0, ui/cool_button.h on the batch-A branch). Only what CoolTabControl uses is declared, with
// batch A's names; drop this header and include ui/cool_button.h once both are merged.
enum CoolButtonState : uint32_t {
    kButtonChecked = 0x04,
    kButtonToggle = 0x08,  // a click flips kButtonChecked
};

class CoolStrobeButton : public CoolWindow {
public:
    void SetStateFlags(uint32_t bits);                     // 0x6030c0
    void ClearStateFlags(uint32_t bits);                   // 0x6030f0
    bool HasStateFlags(uint32_t bits);                     // 0x603120
    void LoadStateBitmap(int state, uint32_t resourceId);  // 0x603150
    int16_t GetStateResourceId(int state);                 // 0x6034a0
    DibBitmap* GetStateBitmap(int state);                  // 0x603550

    uint8_t m_unk94[0x10];  // +0x94
    DibBitmap* m_backdrop;  // +0xa4 not owned
    uint8_t m_unka8[0x1c];  // +0xa8
    uint32_t m_state;       // +0xc4 CoolButtonState
    uint8_t m_unkc8[0x44];  // +0xc8
    int m_backdropX;        // +0x10c where the button sits in the backdrop
    int m_backdropY;        // +0x110
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_OFFSET(CoolStrobeButton, m_backdrop, 0xa4);
REB_ASSERT_OFFSET(CoolStrobeButton, m_state, 0xc4);
REB_ASSERT_OFFSET(CoolStrobeButton, m_backdropX, 0x10c);
REB_ASSERT_OFFSET(CoolStrobeButton, m_backdropY, 0x110);
#pragma clang diagnostic pop
