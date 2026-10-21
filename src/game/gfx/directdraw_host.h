#pragma once
#include <cstdint>

#include "core/entry_identity.h"
#include "platform/win32.h"

// Owns the display interfaces and mode state used by the application.

class DirectDrawHost
{
  public:
  // `directory` (may be null; deleted afterwards when `ownsDirectory`) is the folder searched for the
  // DirectDraw error-text library "cdderr.dll".
  DirectDrawHost(const char* directory, int ownsDirectory);
  virtual ~DirectDrawHost();

  // Creates the DirectDraw objects (IDirectDraw, then IDirectDraw2) and remembers `window`. With
  // `skipAdapterEnumeration` 0 it first looks for a 3D-capable secondary adapter (never used).
  HRESULT Initialize(HWND window, int skipAdapterEnumeration);
  void ReleaseAll();
  // SetCooperativeLevel with DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE | DDSCL_ALLOWMODEX (plus `extraFlags`).
  HRESULT SetExclusiveMode(DWORD extraFlags);
  // SetCooperativeLevel with DDSCL_NORMAL (plus `extraFlags`).
  HRESULT SetNormalMode(DWORD extraFlags);
  // In exclusive mode switches the display; otherwise, when `resizeWindow` is 1, sizes the game window instead.
  HRESULT SetDisplayMode(int width, int height, int bitsPerPixel, int resizeWindow);
  // Windowed mode (-W): gives the window a frame and a client area of the given size.
  int ResizeWindow(HWND window, int width, int height);
  HRESULT RestoreDisplayMode();
  HRESULT RestoreSurfaces();
  void SetErrorReporting(bool enabled)
  {
    reportErrors_ = enabled;
  }

  private:
  void ReportError(HRESULT result, HWND parent);
  static BOOL FindAdapterWith3dCallback(GUID* driverGuid, char* description, char* name, void* context);

  IDirectDraw* directDraw_ = nullptr;
  IDirectDraw2* directDraw2_ = nullptr;
  IUnknown* surfaces_[4]{};
  HWND window_ = nullptr;
  RECT windowedRect_{};
  int width_ = 0;
  int height_ = 0;
  int bitsPerPixel_ = 0;
  bool exclusive_ = false;
  bool reportErrors_ = false;
  HMODULE errorModule_ = nullptr;
};

HRESULT RestoreSurfaceCallback(LPDIRECTDRAWSURFACE surface, LPDDSURFACEDESC description, void* context);
