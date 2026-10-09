// DirectDraw host: creation, cooperative level and display mode switching (0x614000-0x615300).
#include "gfx/directdraw_host.h"

#include <new>

#include "core/vptr_store.h"
#include "iostream/iostream.h"

extern "C" int __cdecl atexit(void(__cdecl* function)(void));

extern RECT g_windowedRect;
extern DisplayModeTree g_displayModes;
extern uint8_t g_displayModesDestroyedGuard;
extern IUnknown* g_ddSurfaces[4];  // only released: the strategic game never creates a surface
extern HWND g_ddWindow;
extern int g_displayWidth;
extern int g_displayHeight;
extern int g_displayBitsPerPixel;
extern int g_ddReportErrors;
extern int g_ddExclusive;
extern HMODULE g_ddErrorModule;

namespace {

constexpr HRESULT kDirectDrawUnsupported = 0x88760082;
constexpr DWORD kExclusiveFlags = DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE | DDSCL_ALLOWMODEX;
constexpr DWORD kEnumSurfacesExisting = DDENUMSURFACES_DOESEXIST;
constexpr LONG kStyleWithCaption = 0xc20000;
constexpr LONG kStyleBitsKept = 0x7f3dffff;
constexpr UINT kSetWindowPosResize = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;
constexpr UINT kSetWindowPosNotTopmost = SWP_NOSIZE | SWP_NOMOVE | SWP_NOACTIVATE;
constexpr UINT kSetWindowPosMove = SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE;

// The directory and file name are concatenated in a strstream, as the original did.
HMODULE LoadErrorTextLibrary(const char* directory, int ownsDirectory) {
    strstream stream;
    strstream_ctor(&stream, 1);
    ostream* out = stream.Out();
    if (directory) {
        ostream* next = ostream_operator_ls_cstr(out, directory);
        ostream_operator_ls_cstr(next, "\\");
    }
    ostream* next = ostream_operator_ls_cstr(out, "cdderr.dll");
    ostream_operator_ls_char(next, 0);

    ios* base = stream.Ios();
    HMODULE module = LoadLibraryA(strstreambuf_str(base->m_bp));
    strstreambuf_freeze(base->m_bp, 0);

    iostream_access::Clear(base, 0);
    if (directory && ownsDirectory) operator delete(const_cast<char*>(directory));
    strstream_dtor(base);
    ios_dtor(base);
    return module;
}
}  // namespace

DirectDrawHost::DirectDrawHost(const char* directory, int ownsDirectory) {
    g_windowedRect.left = 0;
    g_windowedRect.top = 0;
    g_windowedRect.right = 0;
    g_directDraw2 = nullptr;
    g_ddSurfaces[1] = nullptr;
    g_ddSurfaces[2] = nullptr;
    g_ddSurfaces[3] = nullptr;
    g_ddWindow = nullptr;
    g_ddSurfaces[0] = nullptr;
    g_displayWidth = 0;
    g_displayHeight = 0;
    g_displayBitsPerPixel = 0;
    g_ddReportErrors = 0;
    g_ddExclusive = 0;
    g_windowedRect.bottom = 0;
    g_ddErrorModule = LoadErrorTextLibrary(directory, ownsDirectory);
}

DirectDrawHost::~DirectDrawHost() {
    REB_KEEP_VPTR_STORE();
    ReleaseAll();
    FreeLibrary(g_ddErrorModule);
}

HRESULT DirectDrawHost::Initialize(HWND window, int skipAdapterEnumeration) {
    g_ddWindow = window;
    HRESULT result = kDirectDrawUnsupported;
    if (skipAdapterEnumeration == 0) result = DirectDrawEnumerateA(FindAdapterWith3dCallback, this);
    if (!g_directDraw2) {
        result = DirectDrawCreate(nullptr, &g_directDraw, nullptr);
        if (result == 0)
            result = g_directDraw->QueryInterface(IID_IDirectDraw2, reinterpret_cast<void**>(&g_directDraw2));
        else
            g_directDraw2 = nullptr;
        if (result != 0) g_directDraw2 = nullptr;
    }
    ReportDirectDrawError(result, nullptr);
    return result;
}

void DirectDrawHost::ReleaseAll() {
    static constexpr int kReleaseOrder[] = {2, 1, 0, 3};
    for (int index : kReleaseOrder) {
        if (g_ddSurfaces[index]) {
            g_ddSurfaces[index]->Release();
            g_ddSurfaces[index] = nullptr;
        }
    }
    if (g_directDraw2) {
        g_directDraw2->Release();
        g_directDraw2 = nullptr;
        g_directDraw->Release();
        g_directDraw = nullptr;
    }
}

HRESULT DirectDrawHost::SetExclusiveMode(DWORD extraFlags) {
    HRESULT result = E_FAIL;
    // The original tests g_directDraw2 but calls through g_directDraw.
    if (g_directDraw2) {
        result = g_directDraw->SetCooperativeLevel(g_ddWindow, extraFlags | kExclusiveFlags);
        if (result == 0) g_ddExclusive = 1;
    }
    ReportDirectDrawError(result, nullptr);
    return result;
}

HRESULT DirectDrawHost::SetNormalMode(DWORD extraFlags) {
    HRESULT result = E_FAIL;
    if (g_directDraw2) {
        result = g_directDraw2->SetCooperativeLevel(g_ddWindow, extraFlags | DDSCL_NORMAL);
        if (result == 0) g_ddExclusive = 0;
    }
    ReportDirectDrawError(result, nullptr);
    return result;
}

HRESULT DirectDrawHost::SetDisplayMode(int width, int height, int bitsPerPixel, int resizeWindow) {
    HRESULT result = E_FAIL;
    if (g_ddExclusive == 1) {
        if (g_directDraw2) result = g_directDraw2->SetDisplayMode(width, height, bitsPerPixel, 0, 0);
        if (result != 0) {
            ReportDirectDrawError(result, nullptr);
            return result;
        }
        g_displayBitsPerPixel = bitsPerPixel;
    } else if (resizeWindow == 1) {
        result = ResizeWindow(g_ddWindow, width, height);
    } else {
        result = 0;
    }
    if (result == 0) {
        g_displayWidth = width;
        g_displayHeight = height;
    }
    ReportDirectDrawError(result, nullptr);
    return result;
}

int DirectDrawHost::ResizeWindow(HWND window, int width, int height) {
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
    if (frame.left < workArea.left) frame.left = workArea.left;
    if (frame.top < workArea.top) frame.top = workArea.top;
    SetWindowPos(window, nullptr, frame.left, frame.top, 0, 0, kSetWindowPosMove);
    GetWindowRect(window, &g_windowedRect);
    return 0;
}

// BUG(original): always returns E_FAIL, even when the display mode was restored.
HRESULT DirectDrawHost::RestoreDisplayMode() {
    if (g_directDraw2) g_directDraw2->RestoreDisplayMode();
    return E_FAIL;
}

HRESULT DirectDrawHost::RestoreSurfaces() {
    HRESULT result = E_FAIL;
    DDSURFACEDESC description;
    description.dwSize = sizeof(DDSURFACEDESC);
    description.dwFlags = 0;
    if (g_directDraw2)
        result = g_directDraw2->EnumSurfaces(kEnumSurfacesExisting, &description, nullptr, RestoreSurfaceCallback);
    return result;
}

HRESULT __stdcall RestoreSurfaceCallback(LPDIRECTDRAWSURFACE surface, LPDDSURFACEDESC, void*) {
    HRESULT result = E_FAIL;
    if (surface) result = surface->Restore();
    return result;
}

BOOL __stdcall FindAdapterWith3dCallback(GUID* driverGuid, char*, char*, void* context) {
    BOOL keepSearching = TRUE;
    IDirectDraw* candidate = nullptr;
    if (!driverGuid || DirectDrawCreate(driverGuid, &candidate, nullptr) != 0) return keepSearching;

    DDCAPS_DX5 driverCaps = {};
    DDCAPS_DX5 helCaps = {};
    driverCaps.dwSize = sizeof(DDCAPS_DX5);
    helCaps.dwSize = sizeof(DDCAPS_DX5);
    if (candidate->GetCaps(reinterpret_cast<LPDDCAPS>(&driverCaps), reinterpret_cast<LPDDCAPS>(&helCaps)) == 0 &&
        (driverCaps.dwCaps & DDCAPS_3D)) {
        keepSearching = FALSE;
        if (context) {
            g_directDraw = candidate;
            candidate->QueryInterface(IID_IDirectDraw2, reinterpret_cast<void**>(&g_directDraw2));
        }
        return keepSearching;
    }
    candidate->Release();
    return keepSearching;
}

DisplayModeTree::DisplayModeTree() {}

DisplayModeTree::~DisplayModeTree() {
    AvlTree::Clear();
    REB_KEEP_VPTR_STORE();
}

int DisplayModeTree::IsLess(AvlNode* a, AvlNode* b) {
    auto* left = static_cast<DisplayModeNode*>(a);
    auto* right = static_cast<DisplayModeNode*>(b);
    const int leftArea = left->m_height * left->m_width;
    const int rightArea = right->m_height * right->m_width;
    // BUG(original): the depth comparison is not limited to equal area and height, so the ordering is
    // inconsistent for unequal modes.
    if (leftArea < rightArea || (leftArea == rightArea && left->m_height < right->m_height) ||
        left->m_bitsPerPixel < right->m_bitsPerPixel)
        return 1;
    return 0;
}

void ConstructDisplayModes() { new (&g_displayModes) DisplayModeTree(); }

void RegisterDisplayModesDestructor() { atexit(DestroyDisplayModes); }

void InitializeDirectDrawStatics() {
    ConstructDisplayModes();
    RegisterDisplayModesDestructor();
}

void DestroyDisplayModes() {
    if (!(g_displayModesDestroyedGuard & 1)) {
        g_displayModesDestroyedGuard |= 1;
        g_displayModes.~DisplayModeTree();
    }
}
