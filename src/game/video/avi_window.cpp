// VideoWindow: a child window that plays an AVI file through an MCI "avivideo" device. The game ships no .avi
// files (its movies are Smacker), so this path is dead in practice.
#include "video/video_manager.h"

extern const char* g_aviDeviceType; // "avivideo"

namespace
{
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

struct MciOpenParams { // MCI_DGV_OPEN_PARMS
  DWORD_PTR callback;
  MCIDEVICEID deviceId;
  LPCSTR deviceType;
  LPCSTR element;
  LPCSTR alias;
  DWORD style;
  HWND parent;
};

struct MciWindowParams { // MCI_DGV_WINDOW_PARMS
  DWORD_PTR callback;
  HWND window;
  UINT showCommand;
  LPCSTR text;
};
} // namespace

VideoWindow::VideoWindow(const char* path, HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                         int autoStart)
    : CoolWindow(instance, "Video Window", kStyleChildVisible, x, y, width, height, parent, nullptr,
                 static_cast<HBRUSH>(reb::compat::GetStockObject(BLACK_BRUSH)), nullptr, reb::compat::LoadCursorA(nullptr, IDC_ARROW), nullptr, 0,
                 0)
{
  m_path = StringFromText(path);
  m_device = 0;
  m_unk198 = 0;
  m_unk19c = 0;
  m_autoStart = autoStart;
}

VideoWindow::~VideoWindow() = default;

LRESULT VideoWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  HWND window = m_hwnd;
  LRESULT result = 0;
  MCI_GENERIC_PARMS realizeParams = {reinterpret_cast<DWORD_PTR>(window)};
  switch (message) {
    case WM_CREATE:
      if (!OpenDriver(m_path.c_str())) {
        return -1;
      }
      if (!OpenMovie(window, m_path.c_str(), &m_device)) {
        if (m_device != 0) {
          CloseMovie(window, m_device);
        }
        m_device = 0;
        CloseDevices(m_path.c_str());
        return -1;
      }
      reb::compat::mciSendCommandA(m_device, kMciRealize, kMciRealizeNormal | MCI_WAIT, reinterpret_cast<DWORD_PTR>(&realizeParams));
      if (!m_autoStart) {
        return result;
      }
      Play(0);
      return result;
    case WM_DESTROY:
      if (m_device != 0) {
        CloseMovie(window, m_device);
      }
      CloseDevices(m_path.c_str());
      return result;
    case WM_LBUTTONDOWN:
      if (!m_parent) {
        return result;
      }
      reb::compat::PostMessageA(m_parent->m_hwnd, kMovieMessageSkipped, 0, 0);
      return result;
    case MM_MCINOTIFY:
      if (wParam != kMciNotifySuccessful || !m_parent) {
        return result;
      }
      reb::compat::SendMessageA(m_parent->m_hwnd, kMovieMessageFinished, 0, 0);
      return result;
    case WM_ACTIVATE:
    case WM_PALETTECHANGED:
      if (wParam == reinterpret_cast<WPARAM>(window)) {
        return result;
      }
      [[fallthrough]];
    case WM_QUERYNEWPALETTE:
      if (m_device == 0) {
        return result;
      }
      reb::compat::mciSendCommandA(m_device, kMciRealize, message != WM_PALETTECHANGED ? kMciRealizeNormal : kMciRealizeBackground,
                      reinterpret_cast<DWORD_PTR>(&realizeParams));
      return result;
    default:
      return reb::compat::DefWindowProcA(window, message, wParam, lParam);
  }
}

int VideoWindow::OpenMovie(HWND window, const char* path, MCIDEVICEID* device)
{
  if (!path || !device) {
    return 0;
  }
  MciOpenParams open = {};
  open.element = path;
  open.style = WS_CHILD;
  open.parent = window;
  MCIERROR error = reb::compat::mciSendCommandA(0, MCI_OPEN, MCI_OPEN_ELEMENT | kMciOpenParent | kMciOpenStyle,
                                   reinterpret_cast<DWORD_PTR>(&open));
  if (error == 0) {
    *device = open.deviceId;
    MciWindowParams windowParams = {0, window, SW_SHOW, nullptr};
    error = reb::compat::mciSendCommandA(*device, kMciWindow, kMciWindowHandle | kMciWindowState,
                            reinterpret_cast<DWORD_PTR>(&windowParams));
    if (error == 0) {
      MCI_STATUS_PARMS status{};
      status.dwItem = kMciStatusItemRequest;
      error = reb::compat::mciSendCommandA(*device, MCI_STATUS, MCI_STATUS_ITEM, reinterpret_cast<DWORD_PTR>(&status));
    }
  }
  return error == 0;
}

int VideoWindow::Play(int fullScreen)
{
  if (m_device == 0) {
    return 0;
  }
  MCI_PLAY_PARMS play{};
  play.dwCallback = reinterpret_cast<DWORD_PTR>(m_hwnd);
  play.dwFrom = 0;
  play.dwTo = 0;
  const DWORD flags = fullScreen ? MCI_NOTIFY | kMciPlayFullScreen : MCI_NOTIFY;
  MCIERROR error = reb::compat::mciSendCommandA(m_device, MCI_PLAY, flags, reinterpret_cast<DWORD_PTR>(&play));
  return error == 0;
}

int VideoWindow::CloseMovie(HWND window, MCIDEVICEID device)
{
  MCI_GENERIC_PARMS close{};
  MCIERROR error = reb::compat::mciSendCommandA(device, MCI_CLOSE, 0, reinterpret_cast<DWORD_PTR>(&close));
  if (error == 0) {
    reb::compat::InvalidateRect(window, nullptr, TRUE);
    reb::compat::UpdateWindow(window);
  }
  return error == 0;
}

int VideoWindow::OpenDriver(const char* path)
{
  if (!path || !g_aviDeviceType) {
    return 0;
  }
  CloseDevices(path);
  MciOpenParams open = {};
  open.deviceType = g_aviDeviceType;
  MCIERROR error = reb::compat::mciSendCommandA(0, MCI_OPEN, MCI_OPEN_TYPE, reinterpret_cast<DWORD_PTR>(&open));
  return error == 0;
}

int VideoWindow::CloseDevices(const char* path)
{
  MCI_GENERIC_PARMS close{};
  MCIDEVICEID device = path ? reb::compat::mciGetDeviceIDA(path) : 0;
  if (device != 0) {
    reb::compat::mciSendCommandA(device, MCI_CLOSE, 0, reinterpret_cast<DWORD_PTR>(&close));
  }
  device = g_aviDeviceType ? reb::compat::mciGetDeviceIDA(g_aviDeviceType) : 0;
  MCIERROR error = device == 0 ? 0 : reb::compat::mciSendCommandA(device, MCI_CLOSE, 0, reinterpret_cast<DWORD_PTR>(&close));
  return error == 0;
}

int VideoWindow::GetMovieSize(int* width, int* height, HWND, const char* path)
{
  if (!width || !height || !path) {
    return 0;
  }
  PAVIFILE file;
  AVIFILEINFOA info;
  int found = 0;
  *width = 0;
  *height = 0;
  if (reb::compat::AVIFileOpenA(&file, path, 0, nullptr) == 0) {
    found = reb::compat::AVIFileInfoA(file, &info, sizeof(info)) == 0;
    if (found) {
      *width = static_cast<int>(info.dwWidth);
      *height = static_cast<int>(info.dwHeight);
    }
    reb::compat::AVIFileRelease(file);
  }
  return found;
}

const char* g_aviDeviceType = nullptr;
