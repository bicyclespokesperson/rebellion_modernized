#include <cstddef>
#include <new>

#include "app/app_externals.h"
#include "app/app_strings.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "audio/sound_manager.h"
#include "core/layout.h"
#include "gfx/dib_bitmap.h"
#include "gfx/directdraw_host.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "tactical/comm2.h"
#include "tactical/tactical_game.h"
#include "ui/game_window_object.h"

namespace {

constexpr UINT kBlackness = 0x42;
constexpr int kShowNormal = 5;
constexpr uint32_t kTextCommSlot = 1;
constexpr uint32_t kCommonSlot = 8;
constexpr uint32_t kTextStraSlot = 2;
constexpr uint32_t kFrontEndBackdropId = 0x4e21;
constexpr int kBattleFlowTactical = 7;
constexpr int kBattleFlowStrategic = 0x16;
constexpr int kNetworkModeConnected = 7;
constexpr uint32_t kModalChildStrategic = 7;
constexpr uint32_t kModalChildStrategicAlt = 8;
constexpr int kMovieFirstIntro = 0x65;
constexpr int kMovieSecondIntro = 0x66;
constexpr int kMovieNewGameEmpire = 0x68;
constexpr int kMovieVictoryAlliance = 0x6a;
constexpr int kMovieVictoryEmpire = 0x6b;
constexpr int kMovieDefeatAlliance = 0x6c;
constexpr int kMovieDefeatEmpire = 0x6d;
constexpr int kQuitDialogMessage = 0x470;
constexpr int kQuitDialogText = 0x1202;
constexpr int kQuitDialogX = 500;
constexpr int kQuitDialogY = 0x50;

constexpr size_t kMovieWindowSize = 0xb8;
constexpr size_t kStrategicWindowSize = 0x49c;
constexpr size_t kTacticalWindowSize = 0x114;
constexpr size_t kFrontEndWindowSize = 0x108;
constexpr size_t kLobbyWindowSize = 0x194;
constexpr size_t kOptionsWindowSize = 0x11c;
constexpr size_t kDibBitmapSize = 0x28;

template <class Construct>
CoolWindow* ConstructWindow(size_t size, Construct construct) {
    void* memory = ::operator new(size);
    return memory ? static_cast<CoolWindow*>(construct(memory)) : nullptr;
}

void ShowCursorCounter(bool visible) {
    int count = ShowCursor(visible);
    if (visible) {
        while (count < 0) count = ShowCursor(TRUE);
    } else {
        while (count >= 0) count = ShowCursor(FALSE);
    }
}

void DeleteHeapString(char* text) {
    if (text) ::operator delete(text);
}

void ShowNewWindow(CoolWindow* window) {
    window->Create(0);
    window->Show(kShowNormal);
    SetFocus(window->m_hwnd);
}

}  // namespace

void RebellionApplication::ReleaseOverlayWindow() {
    if (!m_overlayWindow) return;
    m_overlayWindow->Hide();
    DestroyWindow(m_overlayWindow->m_hwnd);
    delete m_overlayWindow;
    m_overlayWindow = nullptr;
}

void RebellionApplication::DetachActiveWindow() {
    if (!m_activeWindow) return;
    m_activeWindow->Hide();
    CoolWindow* window = m_activeWindow;
    if (m_strategicWindow == window) m_strategicWindow = nullptr;
    if (m_tacticalWindow == window) m_tacticalWindow = nullptr;
    PostMessageA(m_hwnd, kMsgDestroyWindow, 0, reinterpret_cast<LPARAM>(window));
    m_activeWindow = nullptr;
}

void RebellionApplication::FillWindowBlack() {
    HDC dc = GetDC(m_hwnd);
    PatBlt(dc, 0, 0, m_clientWidth, m_clientHeight, kBlackness);
    ReleasePalette(dc);
    ReleaseDC(m_hwnd, dc);
}

void RebellionApplication::SetGameParams(int side, int gameType, int param8, int paramF0, int connectionKind,
                                         int networkMode) {
    m_side = side;
    m_param8 = param8;
    m_gameType = gameType;
    m_networkMode = networkMode;
    m_paramF0 = paramF0;
    m_param10 = 0;
    if (connectionKind == 1) {
        m_param10 = 0;
    } else if (connectionKind == 2) {
        m_param10 = 1;
    }
}

void RebellionApplication::SetState(int state) {
    m_previousState = m_state;
    m_state = state;
    for (;;) {
        switch (state) {
        case kStateIntroMovie:
        case kStateIntroMovie2:
        case kStateNewGameMovie:
            m_connected = 0;
            m_pauseRequested = 0;
            m_peersReady = 0;
            SetIdentityPaletteForDisplay();
            BeginMovieState();
            return;
        case kStateMainMenu:
            m_connected = 0;
            m_pauseRequested = 0;
            m_peersReady = 0;
            m_networkMode = 0;
            m_gameType = 0;
            CommClearLobbyLaunch();
            CommCleanUp();
            [[fallthrough]];
        case kStateLobbyHost:
        case kStateLobbyJoin:
        case kStateOptionsMenu:
        case kStateOptionsInGame:
            SetIdentityPaletteForDisplay();
            DoMainChildWindow(m_previousState);
            return;
        case kStateStrategic:
            m_connected = 1;
            SetIdentityPaletteForDisplay();
            ShowStrategicWindow(1);
            return;
        case kStateTactical:
            SetIdentityPaletteForDisplay();
            ShowTacticalWindow();
            return;
        case kStateBattleSwitch: {
            int battleFlowState = AppGetBattleFlowState();
            int next;
            if (battleFlowState == kBattleFlowTactical) {
                next = kStateTactical;
            } else if (battleFlowState == kBattleFlowStrategic) {
                next = kStateStrategic;
            } else {
                return;
            }
            DetachActiveWindow();
            DestroyStrategicWindow();
            DestroyTacticalWindow();
            m_previousState = m_state;
            m_state = next;
            state = next;
            continue;
        }
        case kStateConfirmQuit:
            SetCaptureWindow(nullptr);
            SetFocus(nullptr);
            ReleaseOverlayWindow();
            AppShowQuestionDialog(kQuitDialogMessage, 0, kQuitDialogText, kQuitDialogX, kQuitDialogY);
            m_state = m_previousState;
            return;
        case kStateQuit:
            WinHelpA(m_hwnd, nullptr, HELP_QUIT, 0);
            CommTrace(
                "RebellionApplication::QuitRebellion(): REBAPP_UI_PHASE_COCKPIT_SCREEN: About to call "
                "PlayerQuitSession()!\n");
            CommSendPlayerQuit();
            DestroyTacticalGame();
            ReleaseOverlayWindow();
            DestroyAllWindows();
            CommShutdown();
            FUN_00409370();
            SoundManager::Shutdown(0);
            g_directDrawHost.ReleaseAll();
            PostQuitMessage(0);
            return;
        case kStateVictoryMovieEmpire:
        case kStateVictoryMovieAlliance:
        case kStateDefeatMovieEmpire:
        case kStateDefeatMovieAlliance:
            SetIdentityPaletteForDisplay();
            DestroyAppDialog();
            DestroyStrategicWindow();
            DestroyTacticalWindow();
            CommCleanUp();
            FUN_00409370();
            BeginMovieState();
            return;
        default:
            m_state = kStateNone;
            return;
        }
    }
}

void RebellionApplication::PlayMovie(int movieId, int a, int b) {
    if (m_activeWindow) m_activeWindow->Hide();
    ReleaseOverlayWindow();
    FillWindowBlack();
    m_overlayWindow = ConstructWindow(kMovieWindowSize, [&](void* memory) {
        return FUN_004213e0(memory, movieId, b, m_instance, m_unknownA0, m_unknownA4, m_screenWidth, m_screenHeight,
                            this, a);
    });
    if (m_overlayWindow) {
        m_overlayWindow->Create(0);
        m_overlayWindow->Show(kShowNormal);
        SetFocus(m_overlayWindow->m_hwnd);
    }
}

void RebellionApplication::BeginMovieState() {
    DetachActiveWindow();
    ShowCursorCounter(false);
    ReleaseOverlayWindow();
    FillWindowBlack();
    int movieId;
    switch (m_state) {
    case kStateIntroMovie:
        movieId = kMovieFirstIntro;
        break;
    case kStateIntroMovie2:
        movieId = kMovieSecondIntro;
        break;
    case kStateNewGameMovie: {
        m_unknown130 = 1;
        m_unknown134 = 0;
        char* dllLocation = GetDllLocation();
        CoolRegisterResourceDll(dllLocation, g_textstraDllName, kTextStraSlot);
        DeleteHeapString(dllLocation);
        FUN_00409280();
        GameSetup setup;
        FUN_0051c740(&setup);
        setup.m_unknown08 = m_param10;
        setup.m_unknown04 = m_param8;
        setup.m_unknown10 = m_paramF0;
        setup.m_networkMode = m_networkMode;
        g_isNetworkGame = m_networkMode == kNetworkModeConnected;
        AppStartGame(&setup, m_gameType, m_side);
        PlayMovie((m_side != 1) + kMovieNewGameEmpire, 0, 0);
        FUN_0051c780(&setup);
        return;
    }
    case kStateVictoryMovieEmpire:
        movieId = kMovieVictoryEmpire;
        break;
    case kStateVictoryMovieAlliance:
        movieId = kMovieVictoryAlliance;
        break;
    case kStateDefeatMovieEmpire:
        movieId = kMovieDefeatEmpire;
        break;
    case kStateDefeatMovieAlliance:
        movieId = kMovieDefeatAlliance;
        break;
    default:
        return;
    }
    PlayMovie(movieId, 0, 0);
}

void RebellionApplication::DoMainChildWindow(int previousState) {
    ReleaseOverlayWindow();
    if (m_strategicWindow) {
        if (m_activeWindow == m_strategicWindow) m_activeWindow = nullptr;
        m_strategicWindow->Hide();
    }
    if (m_tacticalWindow) {
        if (m_activeWindow == m_tacticalWindow) m_activeWindow = nullptr;
        m_tacticalWindow->Hide();
    }
    DetachActiveWindow();
    FillWindowBlack();

    char* dllLocation = GetDllLocation();
    if (!FindResourceModule(kTextCommSlot)) LoadResourceModule(dllLocation, g_textcommDllName, kTextCommSlot);
    if (!FindResourceModule(kCommonSlot)) LoadResourceModule(dllLocation, g_commonDllName, kCommonSlot);
    HMODULE commonModule = FindResourceModule(kCommonSlot);
    DeleteHeapString(dllLocation);

    int x = m_unknownA0;
    int y = m_unknownA4;
    int width = m_screenWidth;
    int height = m_screenHeight;
    switch (m_state) {
    case kStateMainMenu: {
        ShowCursorCounter(true);
        CommTrace(
            "RebellionApplication::DoMainChildWindow(): REBAPP_UI_PHASE_COCKPIT_SCREEN: About to call "
            "PlayerQuitSession()!\n");
        CommSendPlayerQuit();
        DestroyStrategicWindow();
        DestroyTacticalWindow();
        CommCleanUp();
        FUN_00409370();
        UiSetPausePending(0);
        DibBitmap* backdrop = nullptr;
        if (void* memory = ::operator new(kDibBitmapSize)) {
            backdrop = new (memory) DibBitmap(commonModule, kFrontEndBackdropId, 2u);
        }
        if (backdrop) {
            HDC dc = GetDC(m_hwnd);
            SetIdentityPalette(dc, backdrop->CreatePaletteFromColorTable());
            ReleaseDC(m_hwnd, dc);
            backdrop->~DibBitmap();
            ::operator delete(backdrop);
        }
        m_activeWindow = ConstructWindow(kFrontEndWindowSize, [&](void* memory) {
            return FUN_00404c30(memory, m_instance, x, y, width, height, this);
        });
        break;
    }
    case kStateLobbyHost:
    case kStateLobbyJoin:
        m_activeWindow = ConstructWindow(kLobbyWindowSize, [&](void* memory) {
            return FUN_0040d7a0(memory, m_instance, x, y, width, height, this, 1, 8, 0x2774, 0xe, 0, 0, 1, 1);
        });
        m_networkMode = kNetworkModeConnected;
        break;
    case kStateOptionsMenu:
    case kStateOptionsInGame:
        m_activeWindow = ConstructWindow(kOptionsWindowSize, [&](void* memory) {
            return FUN_00406950(memory, m_instance, x, y, width, height, this, previousState);
        });
        break;
    default:
        break;
    }
    if (m_activeWindow) {
        m_activeWindow->Create(0);
        m_activeWindow->Show(kShowNormal);
        SetFocus(m_activeWindow->m_hwnd);
    }
}

void RebellionApplication::ShowStrategicWindow(int unused) {
    ReleaseOverlayWindow();
    DetachActiveWindow();
    FillWindowBlack();
    ShowCursorCounter(true);
    if (!m_strategicWindow) {
        int x = m_unknownA0;
        int y = m_unknownA4;
        int width = m_screenWidth;
        int height = m_screenHeight;
        if (m_side != 0) {
            int firstGame = m_previousState == kStateBattleSwitch || m_previousState == kStateTactical;
            m_strategicWindow = ConstructWindow(kStrategicWindowSize, [&](void* memory) {
                return FUN_00421c70(memory, m_instance, x, y, width, height, this, m_side, firstGame);
            });
        }
        if (m_strategicWindow) ShowNewWindow(m_strategicWindow);
    } else {
        HDC dc = GetDC(m_hwnd);
        DibBitmap* background = static_cast<GameWindowObject*>(m_strategicWindow)->GetBackground();
        SetIdentityPalette(dc, background->CreatePaletteFromColorTable());
        m_strategicWindow->Show(kShowNormal);
        ReleaseDC(m_hwnd, dc);
    }
    m_activeWindow = m_strategicWindow;
}

void RebellionApplication::DestroyStrategicWindow() {
    if (!m_strategicWindow) return;
    if (m_activeWindow == m_strategicWindow) m_activeWindow = nullptr;
    BOOL destroyed = DestroyWindow(m_strategicWindow->m_hwnd);
    if (destroyed && m_strategicWindow) delete m_strategicWindow;
    m_strategicWindow = nullptr;
}

void RebellionApplication::ShowTacticalWindow() {
    ReleaseOverlayWindow();
    DetachActiveWindow();
    FillWindowBlack();
    if (!m_tacticalWindow) {
        int x = m_unknownA0;
        int y = m_unknownA4;
        int width = m_screenWidth;
        int height = m_screenHeight;
        if (m_side != 0) {
            m_tacticalWindow = ConstructWindow(kTacticalWindowSize, [&](void* memory) {
                return FUN_0059a8d0(memory, m_instance, x, y, width, height, this, &g_directDrawHost);
            });
        }
        if (m_tacticalWindow) ShowNewWindow(m_tacticalWindow);
    } else {
        HDC dc = GetDC(m_hwnd);
        DibBitmap* background = static_cast<GameWindowObject*>(m_tacticalWindow)->GetBackground();
        SetIdentityPalette(dc, background->CreatePaletteFromColorTable());
        m_tacticalWindow->Show(kShowNormal);
        ReleaseDC(m_hwnd, dc);
    }
    m_activeWindow = m_tacticalWindow;
}

void RebellionApplication::DestroyTacticalWindow() {
    if (!m_tacticalWindow) return;
    if (m_activeWindow == m_tacticalWindow) m_activeWindow = nullptr;
    DestroyWindow(m_tacticalWindow->m_hwnd);
    delete m_tacticalWindow;
    m_tacticalWindow = nullptr;
}

void RebellionApplication::DestroyAllWindows() {
    DetachActiveWindow();
    DestroyStrategicWindow();
    DestroyTacticalWindow();
}

void RebellionApplication::ForwardToStrategicWindow() {
    CoolWindow* window = m_activeWindow;
    if (!window) return;
    if (window->m_childId == kModalChildStrategic || window->m_childId == kModalChildStrategicAlt) {
        static_cast<GalaxyWindow*>(window)->InitializeGameDisplay();
    }
}
