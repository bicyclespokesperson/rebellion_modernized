// SmackerWindow: a child window that plays a Smacker (.smk) movie from a dedicated thread.
#include <cmath>
#include <cstring>

#include "audio/sound_manager.h"
#include "core/vptr_store.h"
#include "video/video_manager.h"

// BUG(original): the playback thread procedure is cdecl, not stdcall as CreateThread expects.
#pragma clang diagnostic ignored "-Wcast-calling-convention"

// SoundManager (0x610b20-0x611e90) entry points the window uses to leave the primary buffer volume as it found it.
int FUN_00610cf0();                                // primary buffer volume

namespace {
constexpr int kFirstSkipKey = VK_ESCAPE;
constexpr int kSecondSkipKey = VK_LBUTTON;
constexpr int kKeyDownInSignExtendedWord = 0x10000000;
constexpr DWORD kStyleChildVisible = WS_CHILD | WS_VISIBLE;
constexpr UINT kSetWindowPosFlags = SWP_NOMOVE | SWP_SHOWWINDOW;
constexpr int kBlackBrush = BLACK_BRUSH;
constexpr int kThreadPriorityAboveNormal = THREAD_PRIORITY_ABOVE_NORMAL;
constexpr int kVolumeFloor = -0x2580;
constexpr double kHundredthsToDecibels = 0.01;
constexpr double kDecibelsToLog10Amplitude = 0.05;
constexpr double kSmackFullVolume = 32768.0;
constexpr uint32_t kSmackPanCentre = 0x8000;

// The CRT's x87 pow: ST(1) = base, ST(0) = exponent, result in ST(0).
double CallCiPow(double base, double exponent) {
    double result;
    __asm__ volatile("call _CIpow" : "=t"(result) : "0"(exponent), "u"(base) : "st(1)", "eax", "ecx", "edx", "cc", "memory");
    return result;
}

uint32_t LowWord(uint32_t value) { return static_cast<uint16_t>(value); }
}  // namespace

SmackerWindow::SmackerWindow(const char* path, HINSTANCE instance, int x, int y, int width, int height,
                             CoolWindow* parent, int autoStart, uint32_t smackFlags)
    : CoolWindow(instance, "Smacker Window", kStyleChildVisible, x, y, width, height, parent, nullptr,
                 static_cast<HBRUSH>(GetStockObject(kBlackBrush)), 0, LoadCursorA(nullptr, IDC_ARROW), 0, 0, 0) {
    // BUG(original): the path is copied into the fixed 0x100-byte buffer without a length check.
    std::memcpy(m_path, path, std::strlen(path) + 1);
    m_unk194 = 0;
    m_unk198 = 0;
    m_scale = 1;
    m_smackBuffer = nullptr;
    m_smack = nullptr;
    m_loop = 0;
    m_threadId = 0;
    m_thread = nullptr;
    m_autoStart = autoStart;
    m_smackFlags = smackFlags;
    m_volumeTouched = 0;
    m_savedVolume = 0;
    if (!SoundManager::GetDirectSound()) SoundManager::Initialize(m_hwnd, 1, 0);
    m_savedVolume = FUN_00610cf0();
    SoundManager::Shutdown(0);
}

SmackerWindow::~SmackerWindow() {
    REB_KEEP_VPTR_STORE();
    StopPlayback();
    if (m_volumeTouched) {
        if (!SoundManager::GetDirectSound()) SoundManager::Initialize(m_hwnd, 1, 0);
        SoundManager::SetMasterVolume(m_savedVolume);
        SoundManager::Shutdown(0);
    }
}

LRESULT __stdcall SmackerWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND window = m_hwnd;
    switch (message) {
        case WM_CREATE:
            if (!OpenMovie()) return 0;
            if (!m_autoStart) return 0;
            StartPlayback(0);
            return 0;
        case WM_DESTROY:
            StopPlayback();
            return 0;
        case WM_PAINT: {
            if (!GetUpdateRect(window, nullptr, FALSE)) return 0;
            PAINTSTRUCT paint;
            HDC dc = BeginPaint(window, &paint);
            if (m_smackBuffer)
                SmackBufferBlit(m_smackBuffer, dc, 0, 0, 0, 0, static_cast<uint16_t>(m_smack->m_width),
                                static_cast<uint16_t>(m_smack->m_height));
            EndPaint(window, &paint);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_ACTIVATE:
        case WM_PALETTECHANGED:
            if (wParam == reinterpret_cast<WPARAM>(window)) return 0;
            [[fallthrough]];
        case WM_QUERYNEWPALETTE:
            if (!m_smackBuffer) return 0;
            return static_cast<uint16_t>(SmackBufferSetPalette(m_smackBuffer));
        default:
            return DefWindowProcA(window, message, wParam, lParam);
    }
}

int SmackerWindow::StartPlayback(int loop) {
    SmackBuf* buffer = m_smackBuffer;
    if (buffer && m_smack) {
        Smack* smack = m_smack;
        m_loop = loop;
        SmackToBuffer(smack, (buffer->m_width - smack->m_width) >> 1, (buffer->m_height - smack->m_height) >> 1,
                      buffer->m_width, buffer->m_height, buffer->m_buffer, buffer->m_reversed);
        if (m_thread) {
            TerminateThread(m_thread, 0);
            m_thread = nullptr;
        }
        m_thread = CreateThread(nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(&PlaybackThreadProc), this, 0,
                                &m_threadId);
        if (m_thread) SetThreadPriority(m_thread, kThreadPriorityAboveNormal);
    }
    return 0;
}

int SmackerWindow::PrepareDestroy(HWND) {
    StopPlayback();
    return 1;
}

int SmackerWindow::OpenMovie() {
    int opened = 0;
    SmackSoundUseDirectSound(nullptr);
    m_smack = SmackOpen(m_path, m_smackFlags, kSmackAutoExtraBuffer);
    if (m_smack) {
        // The original multiplies 16-bit halves of the movie size by the zoom; the upper halves it passes on are
        // whatever the registers held.
        const uint32_t width = LowWord(m_smack->m_width);
        const uint32_t height = LowWord(m_smack->m_height);
        const uint32_t scale = LowWord(m_scale);
        m_smackBuffer = SmackBufferOpen(m_hwnd, kSmackBufferBlitType, width, height, width * scale, height * scale);
        SetWindowPos(m_hwnd, nullptr, 0, 0, static_cast<int>(LowWord(m_smackBuffer->m_width) * m_scale),
                     static_cast<int>(LowWord(m_smackBuffer->m_height) * m_scale), kSetWindowPosFlags);
        if (m_smackBuffer) SmackBufferSetPalette(m_smackBuffer);
        opened = 1;
    }
    return opened;
}

void SmackerWindow::StopPlayback() {
    if (m_thread) {
        TerminateThread(m_thread, 0);
        m_thread = nullptr;
        m_threadId = 0;
    }
    if (m_smackBuffer) {
        SmackBufferClose(m_smackBuffer);
        m_smackBuffer = nullptr;
    }
    if (m_smack) {
        SmackClose(m_smack);
        m_smack = nullptr;
    }
}

int SmackerWindow::GetMovieSize(int* width, int* height, HWND, const char* path) {
    *width = 0;
    *height = 0;
    Smack* smack = SmackOpen(path, kSmackAllAudioTracks, kSmackAutoExtraBuffer);
    if (!smack) return 0;
    *width = static_cast<int>(smack->m_width);
    *height = static_cast<int>(smack->m_height);
    SmackClose(smack);
    return 1;
}

int SmackerWindow::AdvanceFrame() {
    HWND window = m_hwnd;
    int finished = 0;
    if (SmackWait(m_smack) != 0) {
        const int firstKey = static_cast<int>(GetAsyncKeyState(kFirstSkipKey));
        const int secondKey = static_cast<int>(GetAsyncKeyState(kSecondSkipKey));
        if ((secondKey | firstKey) & kKeyDownInSignExtendedWord) {
            finished = 1;
            SendMessageA(m_parent->m_hwnd, kMovieMessageSkipped, 0, 0);
        }
        return finished;
    }

    HDC dc = GetDC(window);
    Smack* smack = m_smack;
    if (smack->m_newPalette) {
        // BUG(original): the palette type argument keeps the upper half of m_newPalette.
        SmackBufferNewPalette(m_smackBuffer, smack->m_palette,
                              (smack->m_newPalette & 0xffff0000u) | LowWord(smack->m_paletteType));
    }
    SmackDoFrame(m_smack);
    while (SmackToBufferRect(m_smack, LowWord(m_smackBuffer->m_surfaceType))) {
        smack = m_smack;
        if (static_cast<uint16_t>(SmackBufferBlit(m_smackBuffer, dc, 0, 0, LowWord(smack->m_lastRectX),
                                                  LowWord(smack->m_lastRectY), LowWord(smack->m_lastRectWidth),
                                                  LowWord(smack->m_lastRectHeight))) != 0)
            break;
    }
    ReleaseDC(window, dc);

    smack = m_smack;
    if (smack->m_frameNumber == smack->m_frameCount - 1) {
        finished = 1;
        if (m_loop != 0) {
            SmackGoto(smack, 1);
            finished = 0;
        }
        SendMessageA(m_parent->m_hwnd, kMovieMessageFinished, 0, 0);
        return finished;
    }
    SmackNextFrame(smack);
    return finished;
}

uint32_t SmackerWindow::PlaybackThreadProc(SmackerWindow* window) {
    if (window) {
        while (window->AdvanceFrame() == 0) {
        }
    }
    return 0;
}

void SmackerWindow::SetVolume(int volume) {
    const int scaled = (volume * 3) / 4;
    int smackVolume = 0;
    if (scaled >= kVolumeFloor) {
        const double amplitude = CallCiPow(10.0, scaled * kHundredthsToDecibels * kDecibelsToLog10Amplitude);
        smackVolume = static_cast<int>(floor(amplitude * kSmackFullVolume));
    }
    if (m_smack) SmackVolumePan(m_smack, kSmackAllAudioTracks, smackVolume, kSmackPanCentre);
    if (!m_volumeTouched) m_volumeTouched = 1;
}
