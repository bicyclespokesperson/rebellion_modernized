#pragma once
#include "core/reb_abi.h"
#include <cstdint>

#include "core/containers.h"
#include "platform/win32.h"

// DirectDraw as the game uses it (0x614000-0x6153b0): the object only switches the display to 640x480x8 while
// the game window is active; the strategic game creates no surfaces, palettes or clippers. All of its state is
// in globals (g_directDraw, g_directDraw2, ...), shared with the tactical code, which reads them.
//
// Every method that talks to DirectDraw reports a failure through ReportDirectDrawError, which shows a message
// box once the tactical window has switched reporting on (g_ddReportErrors).

class DirectDrawHost {
public:
    // `directory` (may be null; deleted afterwards when `ownsDirectory`) is the folder searched for the
    // DirectDraw error-text library "cdderr.dll", whose handle is kept in g_ddErrorModule.
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
};

// A display mode as ordered in g_displayModes (by area, then height, then depth). The tree is never populated.
class DisplayModeNode : public AvlNode {
public:
    int m_width;          // +0x18
    int m_height;         // +0x1c
    int m_bitsPerPixel;   // +0x20
};

class DisplayModeTree : public AvlTree {
public:
    DisplayModeTree();
    virtual ~DisplayModeTree();
    virtual int IsLess(AvlNode* a, AvlNode* b) override;
};

void ReportDirectDrawError(HRESULT result, HWND parent);

// Static initializer and atexit handler of g_displayModes.
void InitializeDirectDrawStatics();

HRESULT REB_STDCALL RestoreSurfaceCallback(LPDIRECTDRAWSURFACE surface, LPDDSURFACEDESC description, void* context);
BOOL REB_STDCALL FindAdapterWith3dCallback(GUID* driverGuid, char* description, char* name, void* context);

extern IDirectDraw* g_directDraw;
extern IDirectDraw2* g_directDraw2;
