// DirectDraw for the one thing the game uses it for: switching to 640x480x8 (docs/platform_layer.md section 2). No surface is ever
// created; the picture goes out through the window manager's screen surface. What is kept from the real thing is what the game can
// observe: the objects and return codes of IDirectDraw / IDirectDraw2, and the effects on the window that DirectDraw's cooperative
// level and mode switch have (the same window-procedure messages as the real ddraw produced under Wine, see docs/phase3_plan.md P1).
#define MINWIN_IMPLEMENTATION 1
#include "port/minwin/ddraw.h"
#include "port/minwin/user32.h"

#include <string.h>

#if !defined(MW_IIDS_IN_GAME_DATA)  // the standalone executable's data image already defines it
extern "C" {
const GUID IID_IDirectDraw2 = {0xb3a6f3e0, 0x2b43, 0x11cf, {0xa2, 0xde, 0x00, 0xaa, 0x00, 0xb9, 0x33, 0x56}};
}
#endif

namespace {

const GUID kIidUnknown = {0x00000000, 0x0000, 0x0000, {0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
const GUID kIidDirectDraw = {0x6c14db80, 0xa733, 0x11ce, {0xa5, 0x21, 0x00, 0x20, 0xaf, 0x0b, 0xe5, 0x60}};

constexpr HRESULT kOk = 0;
constexpr HRESULT kErrInvalidParams = static_cast<HRESULT>(0x80070057);
constexpr HRESULT kErrNoInterface = static_cast<HRESULT>(0x80004002);
constexpr HRESULT kErrUnsupported = static_cast<HRESULT>(0x80004001);
constexpr HRESULT kErrNoCooperativeLevelSet = static_cast<HRESULT>(0x887600d4);
constexpr HRESULT kErrNoAggregation = static_cast<HRESULT>(0x80040110);

constexpr DWORD kCooperativeFullscreen = 0x1, kCooperativeNormal = 0x8, kCooperativeExclusive = 0x10;
constexpr LONG kWsPopup = static_cast<LONG>(0x80000000u), kWsExTopmost = 8;
constexpr LONG kFrameStyleBits = 0xc00000 | 0x80000 | 0x40000 | 0x20000 | 0x10000;  // caption, system menu, sizing frame, min/max boxes
constexpr int kGwlStyle = -16, kGwlExStyle = -20;
constexpr UINT kWmDisplayChange = 0x7e;
constexpr UINT kSwpNoRedraw = 8;
HWND const kHwndTopmost = reinterpret_cast<HWND>(static_cast<intptr_t>(-1));

bool SameGuid(const GUID& a, const GUID& b) { return memcmp(&a, &b, sizeof(GUID)) == 0; }

struct DisplayDevice {
    int references = 1;
    HWND window = nullptr;
    DWORD level = 0;
    bool modeSwitched = false;
};

// With the exclusive full-screen cooperative level the real ddraw takes the window out of its frame, makes it topmost and
// covers the display (here: 640x480), through three window-position changes.
void CoverDisplay(HWND window) {
    const LONG style = GetWindowLongA(window, kGwlStyle);
    SetWindowLongA(window, kGwlStyle, (style & ~kFrameStyleBits) | kWsPopup);
    SetWindowLongA(window, kGwlExStyle, GetWindowLongA(window, kGwlExStyle) | kWsExTopmost);
    SetWindowPos(window, kHwndTopmost, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_NOACTIVATE);
    SetWindowPos(window, kHwndTopmost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    SetWindowPos(window, kHwndTopmost, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE | kSwpNoRedraw);
}

HRESULT SetLevel(DisplayDevice* device, HWND window, DWORD level) {
    if (!(level & (kCooperativeNormal | kCooperativeExclusive))) return kErrInvalidParams;
    if ((level & kCooperativeExclusive) && (!(level & kCooperativeFullscreen) || !window)) return kErrInvalidParams;
    device->window = window;
    device->level = level;
    if (level & kCooperativeExclusive) CoverDisplay(window);
    return kOk;
}

HRESULT SwitchMode(DisplayDevice* device, DWORD width, DWORD height, DWORD bitsPerPixel) {
    if (!(device->level & kCooperativeExclusive)) return kErrNoCooperativeLevelSet;
    if (width != 640 || height != 480 || bitsPerPixel != 8) return kErrUnsupported;
    device->modeSwitched = true;
    if (device->window) SendMessageA(device->window, kWmDisplayChange, bitsPerPixel, static_cast<LPARAM>(width | (height << 16)));
    return kOk;
}

HRESULT RestoreMode(DisplayDevice* device) {
    device->modeSwitched = false;
    return kOk;
}

class DrawImpl final : public IDirectDraw {
public:
    explicit DrawImpl(DisplayDevice* device) : m_device(device) {}
    HRESULT REB_STDCALL QueryInterface(const IID& riid, LPVOID* ppv) override;
    ULONG REB_STDCALL AddRef() override { return static_cast<ULONG>(++m_device->references); }
    ULONG REB_STDCALL Release() override;
    HRESULT REB_STDCALL Compact() override { return kOk; }
    HRESULT REB_STDCALL CreateClipper(DWORD, LPDIRECTDRAWCLIPPER*, IUnknown*) override { return kErrUnsupported; }
    HRESULT REB_STDCALL CreatePalette(DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE*, IUnknown*) override { return kErrUnsupported; }
    HRESULT REB_STDCALL CreateSurface(LPDDSURFACEDESC, LPDIRECTDRAWSURFACE*, IUnknown*) override { return kErrUnsupported; }
    HRESULT REB_STDCALL DuplicateSurface(LPDIRECTDRAWSURFACE, LPDIRECTDRAWSURFACE*) override { return kErrUnsupported; }
    HRESULT REB_STDCALL EnumDisplayModes(DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMMODESCALLBACK) override { return kOk; }
    HRESULT REB_STDCALL EnumSurfaces(DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMSURFACESCALLBACK) override { return kOk; }
    HRESULT REB_STDCALL FlipToGDISurface() override { return kErrUnsupported; }
    HRESULT REB_STDCALL GetCaps(LPDDCAPS, LPDDCAPS) override { return kOk; }
    HRESULT REB_STDCALL GetDisplayMode(LPDDSURFACEDESC) override { return kErrUnsupported; }
    HRESULT REB_STDCALL GetFourCCCodes(LPDWORD count, LPDWORD) override {
        if (count) *count = 0;
        return kOk;
    }
    HRESULT REB_STDCALL GetGDISurface(LPDIRECTDRAWSURFACE*) override { return kErrUnsupported; }
    HRESULT REB_STDCALL GetMonitorFrequency(LPDWORD) override { return kErrUnsupported; }
    HRESULT REB_STDCALL GetScanLine(LPDWORD) override { return kErrUnsupported; }
    HRESULT REB_STDCALL GetVerticalBlankStatus(LPBOOL) override { return kErrUnsupported; }
    HRESULT REB_STDCALL Initialize(GUID*) override { return kOk; }
    HRESULT REB_STDCALL RestoreDisplayMode() override { return RestoreMode(m_device); }
    HRESULT REB_STDCALL SetCooperativeLevel(HWND window, DWORD level) override { return SetLevel(m_device, window, level); }
    HRESULT REB_STDCALL SetDisplayMode(DWORD width, DWORD height, DWORD bitsPerPixel) override {
        return SwitchMode(m_device, width, height, bitsPerPixel);
    }
    HRESULT REB_STDCALL WaitForVerticalBlank(DWORD, HANDLE) override { return kOk; }

private:
    DisplayDevice* m_device;
};

class Draw2Impl final : public IDirectDraw2 {
public:
    explicit Draw2Impl(DisplayDevice* device) : m_device(device) {}
    HRESULT REB_STDCALL QueryInterface(const IID& riid, LPVOID* ppv) override;
    ULONG REB_STDCALL AddRef() override { return static_cast<ULONG>(++m_device->references); }
    ULONG REB_STDCALL Release() override;
    HRESULT REB_STDCALL Compact() override { return kOk; }
    HRESULT REB_STDCALL CreateClipper(DWORD, LPDIRECTDRAWCLIPPER*, IUnknown*) override { return kErrUnsupported; }
    HRESULT REB_STDCALL CreatePalette(DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE*, IUnknown*) override { return kErrUnsupported; }
    HRESULT REB_STDCALL CreateSurface(LPDDSURFACEDESC, LPDIRECTDRAWSURFACE*, IUnknown*) override { return kErrUnsupported; }
    HRESULT REB_STDCALL DuplicateSurface(LPDIRECTDRAWSURFACE, LPDIRECTDRAWSURFACE*) override { return kErrUnsupported; }
    HRESULT REB_STDCALL EnumDisplayModes(DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMMODESCALLBACK) override { return kOk; }
    HRESULT REB_STDCALL EnumSurfaces(DWORD, LPDDSURFACEDESC, LPVOID, LPDDENUMSURFACESCALLBACK) override { return kOk; }
    HRESULT REB_STDCALL FlipToGDISurface() override { return kErrUnsupported; }
    HRESULT REB_STDCALL GetCaps(LPDDCAPS, LPDDCAPS) override { return kOk; }
    HRESULT REB_STDCALL GetDisplayMode(LPDDSURFACEDESC) override { return kErrUnsupported; }
    HRESULT REB_STDCALL GetFourCCCodes(LPDWORD count, LPDWORD) override {
        if (count) *count = 0;
        return kOk;
    }
    HRESULT REB_STDCALL GetGDISurface(LPDIRECTDRAWSURFACE*) override { return kErrUnsupported; }
    HRESULT REB_STDCALL GetMonitorFrequency(LPDWORD) override { return kErrUnsupported; }
    HRESULT REB_STDCALL GetScanLine(LPDWORD) override { return kErrUnsupported; }
    HRESULT REB_STDCALL GetVerticalBlankStatus(LPBOOL) override { return kErrUnsupported; }
    HRESULT REB_STDCALL Initialize(GUID*) override { return kOk; }
    HRESULT REB_STDCALL RestoreDisplayMode() override { return RestoreMode(m_device); }
    HRESULT REB_STDCALL SetCooperativeLevel(HWND window, DWORD level) override { return SetLevel(m_device, window, level); }
    HRESULT REB_STDCALL SetDisplayMode(DWORD width, DWORD height, DWORD bitsPerPixel, DWORD, DWORD) override {
        return SwitchMode(m_device, width, height, bitsPerPixel);
    }
    HRESULT REB_STDCALL WaitForVerticalBlank(DWORD, HANDLE) override { return kOk; }
    HRESULT REB_STDCALL GetAvailableVidMem(LPDDSCAPS, LPDWORD total, LPDWORD freeBytes) override {
        if (total) *total = 0;
        if (freeBytes) *freeBytes = 0;
        return kOk;
    }

private:
    DisplayDevice* m_device;
};

// The device lives as long as any of its interface views; one reference count serves all of them.
struct Views {
    DisplayDevice device;
    DrawImpl draw{&device};
    Draw2Impl draw2{&device};
};

Views* ViewsOf(DisplayDevice* device) { return reinterpret_cast<Views*>(device); }  // `device` is the first member

ULONG ReleaseDevice(DisplayDevice* device) {
    const int remaining = --device->references;
    if (remaining == 0) delete ViewsOf(device);
    return static_cast<ULONG>(remaining);
}

HRESULT DrawImpl::QueryInterface(const IID& riid, LPVOID* ppv) {
    if (!ppv) return kErrInvalidParams;
    *ppv = nullptr;
    Views* views = ViewsOf(m_device);
    if (SameGuid(riid, kIidUnknown) || SameGuid(riid, kIidDirectDraw)) *ppv = static_cast<IDirectDraw*>(&views->draw);
    else if (SameGuid(riid, IID_IDirectDraw2)) *ppv = static_cast<IDirectDraw2*>(&views->draw2);
    else return kErrNoInterface;
    ++m_device->references;
    return kOk;
}

ULONG DrawImpl::Release() { return ReleaseDevice(m_device); }

HRESULT Draw2Impl::QueryInterface(const IID& riid, LPVOID* ppv) {
    if (!ppv) return kErrInvalidParams;
    *ppv = nullptr;
    Views* views = ViewsOf(m_device);
    if (SameGuid(riid, kIidUnknown) || SameGuid(riid, IID_IDirectDraw2)) *ppv = static_cast<IDirectDraw2*>(&views->draw2);
    else if (SameGuid(riid, kIidDirectDraw)) *ppv = static_cast<IDirectDraw*>(&views->draw);
    else return kErrNoInterface;
    ++m_device->references;
    return kOk;
}

ULONG Draw2Impl::Release() { return ReleaseDevice(m_device); }

}  // namespace

extern "C" {

// One primary display, reported with a null GUID as DirectDraw does.
HRESULT REB_STDCALL DirectDrawEnumerateA(LPDDENUMCALLBACKA callback, LPVOID context) {
    if (!callback) return kErrInvalidParams;
    char name[] = "display";
    char description[] = "display";
    callback(nullptr, description, name, context);
    return kOk;
}

HRESULT REB_STDCALL DirectDrawCreate(GUID*, LPDIRECTDRAW* result, IUnknown* outer) {
    if (!result) return kErrInvalidParams;
    *result = nullptr;
    if (outer) return kErrNoAggregation;
    *result = &(new Views)->draw;
    return kOk;
}

}  // extern "C"
