#include "audio/cd_audio.h"

namespace {

constexpr DWORD kModeNotReady = 0x20c;
constexpr DWORD kModePlay = 0x20e;
constexpr DWORD kModePause = 0x211;
constexpr DWORD kStatusModeItem = 4;  // MCI_STATUS_MODE

}  // namespace

CdAudio::CdAudio() : m_window(nullptr), m_device(0), m_opened(0), m_unk0c(0), m_unk10(0), m_unk14(0) {}

CdAudio::~CdAudio() {
    if (m_device == 0) return;
    if (m_opened == 1) {
        if (GetMode() == kModePlay || GetMode() == kModePause) Stop();
        Close();
    }
}

MCIERROR CdAudio::Open(HWND window) {
    MCIERROR error = 0;
    if (window) {
        m_window = window;
        MCI_OPEN_PARMS open = {};
        open.lpstrDeviceType = MAKEINTRESOURCEA(MCI_DEVTYPE_CD_AUDIO);
        error = mciSendCommandA(0, MCI_OPEN, MCI_OPEN_TYPE | MCI_OPEN_TYPE_ID, reinterpret_cast<DWORD_PTR>(&open));
        if (error == 0) {
            m_device = open.wDeviceID;
            MCI_SET_PARMS set = {};
            set.dwTimeFormat = MCI_FORMAT_TMSF;
            error = mciSendCommandA(open.wDeviceID, MCI_SET, MCI_SET_TIME_FORMAT, reinterpret_cast<DWORD_PTR>(&set));
            if (error == 0) {
                m_opened = 1;
                return 0;
            }
            Close();
        }
    }
    return error;
}

void CdAudio::Close() {
    if (m_device != 0 && m_opened == 1) {
        mciSendCommandA(m_device, MCI_CLOSE, 0, 0);
        m_device = 0;
        m_opened = 0;
    }
}

MCIERROR CdAudio::Stop() {
    MCIERROR error = 0;
    if (m_device != 0 && m_opened != 0) {
        if (GetMode() == kModePause || GetMode() == kModePlay) {
            MCI_GENERIC_PARMS generic = {};
            error = mciSendCommandA(m_device, MCI_STOP, 0, reinterpret_cast<DWORD_PTR>(&generic));
            m_unk10 = 0;
            m_unk14 = 0;
            m_unk0c = 0;
        }
    }
    return error;
}

DWORD CdAudio::GetMode() { return GetStatus(kStatusModeItem); }

DWORD CdAudio::GetStatus(DWORD item) {
    DWORD result = kModeNotReady;
    if (m_device != 0 && m_opened == 1) {
        MCI_STATUS_PARMS status = {};
        status.dwItem = item;
        if (mciSendCommandA(m_device, MCI_STATUS, MCI_STATUS_ITEM, reinterpret_cast<DWORD_PTR>(&status)) == 0)
            result = status.dwReturn;
        else
            result = kModeNotReady;
    }
    return result;
}
