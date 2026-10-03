#pragma once
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"

// Base of the "Cool" multimedia-timer objects (0x601f40, ui_framework). Declared here only as far as the sound
// code needs it: SoundCallbackTimer derives from it and never starts the timer (period 0).
class MmTimer {
public:
    MmTimer(UINT period, UINT resolution, UINT minimumDelay);  // 0x601f40
    virtual ~MmTimer();                                         // 0x601ff0 (deleting destructor 0x601fd0)
    virtual void Tick() = 0;                                    // pure virtual (0x618b80)

    void Stop();  // 0x602020: kills the timeSetEvent timer

    uint8_t m_unk04[0x34];  // +0x04: timer id, state flags, periods, CRITICAL_SECTION at +0x20
};
REB_ASSERT_SIZE(MmTimer, 0x38);
