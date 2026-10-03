// VideoWindow: a child window that plays an AVI file through an MCI "avivideo" device. The game ships no .avi
// files (its movies are Smacker), so this path is dead in practice.
#include <cstring>

#include "core/vptr_store.h"
#include "video/video_manager.h"

extern const char g_aviDeviceType[];  // "avivideo"

namespace {
constexpr DWORD kStyleChildVisible = WS_CHILD | WS_VISIBLE;

constexpr UINT kMciWindow = 0x841;
constexpr UINT kMciRealize = 0x840;
constexpr DWORD kMciOpenParent = 0x20000;
constexpr DWORD kMciOpenStyle = 0x10000;
constexpr DWORD kMciWindowHandle = 0x10000;
constexpr DWORD kMciWindowState = 0x40000;
constexpr DWORD kMciRealizeNormal = 0x10000;
constexpr DWORD kMciRealizeBackground = 0x20000;
constexpr DWORD kMciPlayFullScreen = 0x10000;
constexpr DWORD kMciStatusItemRequest = 0x4001;
constexpr WPARAM kMciNotifySuccessful = 1;

struct MciOpenParams {  // MCI_DGV_OPEN_PARMS
    DWORD_PTR callback;
    MCIDEVICEID deviceId;
    LPCSTR deviceType;
    LPCSTR element;
    LPCSTR alias;
    DWORD style;
    HWND parent;
};

struct MciWindowParams {  // MCI_DGV_WINDOW_PARMS
    DWORD_PTR callback;
    HWND window;
    UINT showCommand;
    LPCSTR text;
};
}  // namespace

VideoWindow::VideoWindow(const char* path, HINSTANCE instance, int x, int y, int width, int height,
                         CoolWindow* parent, int autoStart)
    : CoolWindow(instance, "Video Window", kStyleChildVisible, x, y, width, height, parent, nullptr,
                 static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)), 0, LoadCursorA(nullptr, IDC_ARROW), 0, 0, 0) {
    // BUG(original): the path is copied into the fixed 0x100-byte buffer without a length check.
    std::memcpy(m_path, path, std::strlen(path) + 1);
    m_unk198 = 0;
    m_unk19c = 0;
    m_autoStart = autoStart;
}

VideoWindow::~VideoWindow() { REB_KEEP_VPTR_STORE(); }

LRESULT __stdcall VideoWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND window = m_hwnd;
    LRESULT result = 0;
    // BUG(original): the MCI_REALIZE calls below pass the address of this function's own wParam/lParam slots as
    // their parameter block.
    DWORD_PTR realizeParams[4] = {static_cast<DWORD_PTR>(wParam), static_cast<DWORD_PTR>(lParam), 0, 0};
    switch (message) {
        case WM_CREATE:
            if (!OpenDriver(m_path)) return result;
            if (!OpenMovie(window, m_path, &m_device)) return result;
            mciSendCommandA(m_device, kMciRealize, kMciRealizeNormal | MCI_WAIT,
                            reinterpret_cast<DWORD_PTR>(realizeParams));
            if (!m_autoStart) return result;
            Play(0);
            return result;
        case WM_DESTROY:
            CloseMovie(window, m_device);
            CloseDevices(m_path);
            return result;
        case WM_LBUTTONDOWN:
            if (!m_parent) return result;
            PostMessageA(m_parent->m_hwnd, kMovieMessageSkipped, 0, 0);
            return result;
        case MM_MCINOTIFY:
            if (wParam != kMciNotifySuccessful) return result;
            // BUG(original): unlike WM_LBUTTONDOWN, this does not check for a missing parent window.
            SendMessageA(m_parent->m_hwnd, kMovieMessageFinished, 0, 0);
            return result;
        case WM_ACTIVATE:
        case WM_PALETTECHANGED:
            if (wParam == reinterpret_cast<WPARAM>(window)) return result;
            [[fallthrough]];
        case WM_QUERYNEWPALETTE:
            mciSendCommandA(m_device, kMciRealize,
                            message != WM_PALETTECHANGED ? kMciRealizeNormal : kMciRealizeBackground,
                            reinterpret_cast<DWORD_PTR>(realizeParams));
            return result;
        default:
            return DefWindowProcA(window, message, wParam, lParam);
    }
}

int VideoWindow::OpenMovie(HWND window, const char* path, MCIDEVICEID* device) {
    MciOpenParams open = {};
    open.element = path;
    open.style = WS_CHILD;
    open.parent = window;
    MCIERROR error = mciSendCommandA(0, MCI_OPEN, MCI_OPEN_ELEMENT | kMciOpenParent | kMciOpenStyle,
                                     reinterpret_cast<DWORD_PTR>(&open));
    if (error == 0) {
        *device = open.deviceId;
        MciWindowParams windowParams = {0, window, SW_SHOW, nullptr};
        error = mciSendCommandA(*device, kMciWindow, kMciWindowHandle | kMciWindowState,
                                reinterpret_cast<DWORD_PTR>(&windowParams));
        if (error == 0) {
            MCI_STATUS_PARMS status;
            status.dwItem = kMciStatusItemRequest;
            error = mciSendCommandA(*device, MCI_STATUS, MCI_STATUS_ITEM, reinterpret_cast<DWORD_PTR>(&status));
        }
    }
    return error == 0;
}

int VideoWindow::Play(int fullScreen) {
    MCI_PLAY_PARMS play;
    play.dwCallback = reinterpret_cast<DWORD_PTR>(m_hwnd) & 0xffff;
    play.dwFrom = 0;
    play.dwTo = 0;
    const DWORD flags = fullScreen ? MCI_NOTIFY | kMciPlayFullScreen : MCI_NOTIFY;
    MCIERROR error = mciSendCommandA(m_device, MCI_PLAY, flags, reinterpret_cast<DWORD_PTR>(&play));
    return error == 0;
}

int VideoWindow::CloseMovie(HWND window, MCIDEVICEID device) {
    // BUG(original): the parameter block passed to MCI_CLOSE is the address of this function's own `device`
    // argument slot.
    MCI_GENERIC_PARMS close = {device};
    MCIERROR error = mciSendCommandA(device, MCI_CLOSE, 0, reinterpret_cast<DWORD_PTR>(&close));
    if (error == 0) {
        InvalidateRect(window, nullptr, TRUE);
        UpdateWindow(window);
    }
    return error == 0;
}

int VideoWindow::OpenDriver(const char* path) {
    CloseDevices(path);
    MciOpenParams open = {};
    open.deviceType = g_aviDeviceType;
    MCIERROR error = mciSendCommandA(0, MCI_OPEN, MCI_OPEN_TYPE, reinterpret_cast<DWORD_PTR>(&open));
    return error == 0;
}

int VideoWindow::CloseDevices(const char* path) {
    // BUG(original): both MCI_CLOSE calls pass the address of this function's own `path` argument slot as the
    // parameter block.
    MCI_GENERIC_PARMS close = {reinterpret_cast<DWORD_PTR>(path)};
    MCIDEVICEID device = mciGetDeviceIDA(path);
    if (device != 0) mciSendCommandA(device, MCI_CLOSE, 0, reinterpret_cast<DWORD_PTR>(&close));
    device = mciGetDeviceIDA(g_aviDeviceType);
    MCIERROR error = mciSendCommandA(device, MCI_CLOSE, 0, reinterpret_cast<DWORD_PTR>(&close));
    return error == 0;
}

int VideoWindow::GetMovieSize(int* width, int* height, HWND, const char* path) {
    PAVIFILE file;
    AVIFILEINFOA info;
    int found = 0;
    *width = 0;
    *height = 0;
    if (AVIFileOpenA(&file, path, 0, nullptr) == 0) {
        found = AVIFileInfoA(file, &info, sizeof(info)) == 0;
        if (found) {
            *width = static_cast<int>(info.dwWidth);
            *height = static_cast<int>(info.dwHeight);
        }
        AVIFileRelease(file);
    }
    return found;
}
