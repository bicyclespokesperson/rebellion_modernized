// DirectDraw host: creation, cooperative level and display mode switching (0x614000-0x615300).
#include "gfx/directdraw_host.h"

#include <memory>
#include <new>
#include <string>

namespace
{

constexpr HRESULT kDirectDrawUnsupported = 0x88760082;
constexpr DWORD kExclusiveFlags = DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE | DDSCL_ALLOWMODEX;
constexpr DWORD kEnumSurfacesExisting = DDENUMSURFACES_DOESEXIST;
constexpr LONG kStyleWithCaption = 0xc20000;
constexpr LONG kStyleBitsKept = 0x7f3dffff;
constexpr UINT kSetWindowPosResize = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;
constexpr UINT kSetWindowPosNotTopmost = SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE;
constexpr UINT kSetWindowPosMove = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;

struct DeleteDirectory
{
  void operator()(const char* directory) const
  {
    ::operator delete(const_cast<char*>(directory));
  }
};

HMODULE LoadErrorTextLibrary(const char* directory, int ownsDirectory)
{
  std::unique_ptr<const char, DeleteDirectory> ownedDirectory(ownsDirectory ? directory : nullptr);
  std::string path;
  if (directory && *directory)
  {
    path = directory;
    path += "/";
  }
  path += "cdderr.dll";
  return LoadLibraryA(path.c_str());
}
} // namespace

DirectDrawHost::DirectDrawHost(const char* directory, int ownsDirectory)
: errorModule_(LoadErrorTextLibrary(directory, ownsDirectory))
{
}

DirectDrawHost::~DirectDrawHost()
{
  ReleaseAll();
  FreeLibrary(errorModule_);
}

HRESULT DirectDrawHost::Initialize(HWND window, int skipAdapterEnumeration)
{
  window_ = window;
  HRESULT result = kDirectDrawUnsupported;
  if (skipAdapterEnumeration == 0)
    result = DirectDrawEnumerateA(FindAdapterWith3dCallback, this);
  if (!directDraw2_)
  {
    result = DirectDrawCreate(nullptr, &directDraw_, nullptr);
    if (result == 0)
      result = directDraw_->QueryInterface(IID_IDirectDraw2, reinterpret_cast<void**>(&directDraw2_));
    else
      directDraw2_ = nullptr;
    if (result != 0)
      directDraw2_ = nullptr;
  }
  ReportError(result, nullptr);
  return result;
}

void DirectDrawHost::ReleaseAll()
{
  static constexpr int kReleaseOrder[] = {2, 1, 0, 3};
  for (int index : kReleaseOrder)
  {
    if (surfaces_[index])
    {
      surfaces_[index]->Release();
      surfaces_[index] = nullptr;
    }
  }
  if (directDraw2_)
  {
    directDraw2_->Release();
    directDraw2_ = nullptr;
  }
  if (directDraw_)
  {
    directDraw_->Release();
    directDraw_ = nullptr;
  }
}

HRESULT DirectDrawHost::SetExclusiveMode(DWORD extraFlags)
{
  HRESULT result = E_FAIL;
  // The original tests directDraw2_ but calls through directDraw_.
  if (directDraw2_)
  {
    result = directDraw_->SetCooperativeLevel(window_, extraFlags | kExclusiveFlags);
    if (result == 0)
      exclusive_ = 1;
  }
  ReportError(result, nullptr);
  return result;
}

HRESULT DirectDrawHost::SetNormalMode(DWORD extraFlags)
{
  HRESULT result = E_FAIL;
  if (directDraw2_)
  {
    result = directDraw2_->SetCooperativeLevel(window_, extraFlags | DDSCL_NORMAL);
    if (result == 0)
      exclusive_ = 0;
  }
  ReportError(result, nullptr);
  return result;
}

HRESULT DirectDrawHost::SetDisplayMode(int width, int height, int bitsPerPixel, int resizeWindow)
{
  HRESULT result = E_FAIL;
  if (exclusive_ == 1)
  {
    if (directDraw2_)
      result = directDraw2_->SetDisplayMode(width, height, bitsPerPixel, 0, 0);
    if (result != 0)
    {
      ReportError(result, nullptr);
      return result;
    }
    bitsPerPixel_ = bitsPerPixel;
  }
  else if (resizeWindow == 1)
  {
    result = ResizeWindow(window_, width, height);
  }
  else
  {
    result = 0;
  }
  if (result == 0)
  {
    width_ = width;
    height_ = height;
  }
  ReportError(result, nullptr);
  return result;
}

int DirectDrawHost::ResizeWindow(HWND window, int width, int height)
{
  const LONG style = GetWindowLongA(window, GWL_STYLE);
  SetWindowLongA(window, GWL_STYLE, (style & kStyleBitsKept) | kStyleWithCaption);
  RECT frame;
  SetRect(&frame, 0, 0, width, height);
  const DWORD exStyle = GetWindowLongA(window, GWL_EXSTYLE);
  const BOOL hasMenu = GetMenu(window) != nullptr;
  const DWORD newStyle = GetWindowLongA(window, GWL_STYLE);
  AdjustWindowRectEx(&frame, newStyle, hasMenu, exStyle);
  SetWindowPos(window, nullptr, 0, 0, frame.right - frame.left, frame.bottom - frame.top, kSetWindowPosResize);
  SetWindowPos(window, HWND_NOTOPMOST, 0, 0, 0, 0, kSetWindowPosNotTopmost);

  RECT workArea;
  SystemParametersInfoA(SPI_GETWORKAREA, 0, &workArea, 0);
  GetWindowRect(window, &frame);
  if (frame.left < workArea.left)
    frame.left = workArea.left;
  if (frame.top < workArea.top)
    frame.top = workArea.top;
  SetWindowPos(window, nullptr, frame.left, frame.top, 0, 0, kSetWindowPosMove);
  GetWindowRect(window, &windowedRect_);
  return 0;
}

HRESULT DirectDrawHost::RestoreDisplayMode()
{
  return directDraw2_ ? directDraw2_->RestoreDisplayMode() : E_FAIL;
}

HRESULT DirectDrawHost::RestoreSurfaces()
{
  HRESULT result = E_FAIL;
  DDSURFACEDESC description;
  description.dwSize = sizeof(DDSURFACEDESC);
  description.dwFlags = 0;
  if (directDraw2_)
    result = directDraw2_->EnumSurfaces(kEnumSurfacesExisting, &description, nullptr, RestoreSurfaceCallback);
  return result;
}

HRESULT CALLBACK RestoreSurfaceCallback(LPDIRECTDRAWSURFACE surface, LPDDSURFACEDESC, void*)
{
  HRESULT result = E_FAIL;
  if (surface)
    result = surface->Restore();
  return result;
}

BOOL CALLBACK DirectDrawHost::FindAdapterWith3dCallback(GUID* driverGuid, char*, char*, void* context)
{
  BOOL keepSearching = TRUE;
  IDirectDraw* candidate = nullptr;
  if (!driverGuid || DirectDrawCreate(driverGuid, &candidate, nullptr) != 0)
    return keepSearching;

  DDCAPS_DX5 driverCaps = {};
  DDCAPS_DX5 helCaps = {};
  driverCaps.dwSize = sizeof(DDCAPS_DX5);
  helCaps.dwSize = sizeof(DDCAPS_DX5);
  if (candidate->GetCaps(reinterpret_cast<LPDDCAPS>(&driverCaps), reinterpret_cast<LPDDCAPS>(&helCaps)) == 0 &&
      (driverCaps.dwCaps & DDCAPS_3D))
  {
    keepSearching = FALSE;
    if (context)
    {
      auto& host = *static_cast<DirectDrawHost*>(context);
      host.directDraw_ = candidate;
      candidate->QueryInterface(IID_IDirectDraw2, reinterpret_cast<void**>(&host.directDraw2_));
    }
    return keepSearching;
  }
  candidate->Release();
  return keepSearching;
}
