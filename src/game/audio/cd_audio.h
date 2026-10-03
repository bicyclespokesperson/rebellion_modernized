#pragma once
#include <cstdint>

#include "core/layout.h"
#include "platform/win32.h"

// MCI CD-audio device (the original game's CD music). It is opened only when SoundManager::Initialize gets
// kSoundInitCdAudio, which no caller passes, and nothing ever sends it a play command; it is reconstructed for
// completeness.
class CdAudio {
public:
    CdAudio();
    ~CdAudio();

    // Opens the device and selects the track/minute/second/frame (TMSF) time format. Returns an MCI error, 0 if ok.
    // A null window does nothing.
    MCIERROR Open(HWND window);
    void Close();
    MCIERROR Stop();
    // MCI mode (MCI_MODE_PLAY = 0x20e, MCI_MODE_PAUSE = 0x211, MCI_MODE_NOT_READY = 0x20c when unavailable).
    DWORD GetMode();
    // MCI_STATUS item; MCI_MODE_NOT_READY if the device is closed or the query fails.
    DWORD GetStatus(DWORD item);

    HWND m_window;          // +0x00
    MCIDEVICEID m_device;   // +0x04
    int m_opened;           // +0x08 1 once the device is open and set up
    uint32_t m_unk0c;       // +0x0c cleared by Stop
    uint32_t m_unk10;       // +0x10 cleared by Stop
    uint32_t m_unk14;       // +0x14 cleared by Stop
};
REB_ASSERT_SIZE(CdAudio, 0x18);
