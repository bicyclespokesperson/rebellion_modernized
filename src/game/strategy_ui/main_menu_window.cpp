#include "strategy_ui/main_menu_window.h"

#include <cstdint>

#include "app/rebapp_globals.h"
#include "audio/music.h"
#include "audio/sound_manager.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/utility_classes.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_label.h"
#include "ui/cool_timer.h"
#include "ui/resource_dll.h"

extern const char g_commonDllName[];  // 0x6a7c98 "common.dll"

namespace {

constexpr uint32_t kCommonSlot = 8;
constexpr uint32_t kStringSlot = 1;
constexpr uint32_t kBackdropId = 0x4e21;
constexpr uint32_t kBitmapKind = 10;
constexpr int kBackdropChildId = 4;
constexpr int kMovieQuit = 0x6e;
constexpr DWORD kButtonReleaseDelay = 100;
constexpr uint16_t kClickSoundDefault = 0x1f44;
constexpr uint16_t kClickSoundCycle = 8000;
constexpr uint16_t kClickSoundMultiplayer = 0x1f41;
constexpr uint16_t kClickSoundOptions = 0x1f42;

enum Command : uint32_t {
    kAllianceNewGame = 0x65,
    kEmpireNewGame = 0x66,
    kMultiplayer = 0x67,
    kLoadGame = 0x68,
    kOptions = 0x69,
    kCycleCommand = 0x6a,
    kModeCommand0 = 0x6e,
    kModeCommand1 = 0x6f,
    kModeCommand2 = 0x70,
    kToggleCommand = 0x71,
    kQuitCommand = 0x73,
};

constexpr int kStateNewGameSetup = 4;
constexpr int kStateLoadGame = 0xb;
constexpr int kStateOptions = 0xd;
constexpr int kStateStartGame = 7;

struct CycleRegion {
    int m_index;
    RECT m_area;
    uint32_t m_toolId;
    UINT m_tooltipId;
    uint32_t m_bitmapId;
};

constexpr CycleRegion kCycleRegions[3] = {
    {0, {0x122, 0x125, 0x13a, 0x13a}, 0x6b, 0x100f, 0x2721},
    {1, {0x146, 0x125, 0x15e, 0x13a}, 0x6c, 0x100e, 0x2722},
    {2, {0x16a, 0x125, 0x182, 0x13a}, 0x6d, 0x100d, 0x2723},
};

DibBitmap* LoadBitmap(HMODULE module, uint32_t id) { return NewOrNull<DibBitmap>(module, id, kBitmapKind); }

CoolFrameButton* NewFrameButton(HMODULE module, int x, int y, int width, int height, MainMenuWindow* parent,
                                uint32_t childId, uint32_t firstFrameId, uint32_t pressedId, uint16_t frameCount) {
    return NewOrNull<CoolFrameButton>(module, x, y, width, height, parent, childId, firstFrameId, pressedId,
                                      frameCount, parent->GetBackground(), 0, 0);
}

void SetUpButton(MainMenuWindow* parent, CoolStrobeButton* button, uint16_t clickSound, UINT tooltipId) {
    if (!button) return;
    button->Create(0);
    button->SetClickSound(clickSound);
    parent->AddToolFromResource(button, tooltipId);
}

void SetUpToggleButton(MainMenuWindow* parent, CoolStrobeButton* button, uint32_t checkedBitmapId, UINT tooltipId,
                       int checkedWhenModeIs) {
    if (!button) return;
    button->SetStateFlags(kButtonToggle);
    button->LoadStateBitmap(kButtonChecked, checkedBitmapId);
    button->Create(0);
    button->SetClickSound(kClickSoundDefault);
    parent->AddToolFromResource(button, tooltipId);
    if (checkedWhenModeIs >= 0 && parent->m_mode == checkedWhenModeIs) button->SetStateFlags(kButtonChecked);
}

void StepButton(CoolFrameButton* button) {
    if (button) button->StepFrame();
}

void HideIfPresent(CoolWindow* window) {
    if (window) window->Hide();
}

template <class T>
void DeleteIfPresent(T* object) {
    if (object) delete object;
}

void DeleteBitmap(DibBitmap* bitmap) {
    if (bitmap) {
        bitmap->~DibBitmap();
        operator delete(bitmap);
    }
}

void ClearCheckedIfSet(CoolStrobeButton* button) {
    if (button && button->HasStateFlags(kButtonChecked)) button->ClearStateFlags(kButtonChecked);
}

void SelectMode(MainMenuWindow* menu, CoolStrobeButton* selected, CoolStrobeButton* otherA, CoolStrobeButton* otherB,
                int mode) {
    if (!selected->HasStateFlags(kButtonChecked)) {
        selected->SetStateFlags(kButtonChecked);
    } else {
        ClearCheckedIfSet(otherA);
        ClearCheckedIfSet(otherB);
    }
    menu->m_mode = mode;
}

void ShowOptionHint(MainMenuWindow* menu, int16_t stringId) {
    const ResourceStringId id = {stringId, 1};
    menu->m_hintLabel->SetTextFromResource(&id);
    menu->m_hintLabel->Invalidate(menu->m_hwnd, TRUE);
}

void RealizeBackdropPalette(MainMenuWindow* menu) {
    if (!menu->GetBackground()) return;
    HDC dc = GetDC(menu->m_hwnd);
    HPALETTE palette = menu->GetBackground()->CreatePaletteFromColorTable();
    SetIdentityPalette(dc, palette);
    ReleaseDC(menu->m_hwnd, dc);
}

void StartMenuSound(HWND parentWindow) {
    AppSetVolume(AppGetVolume());
    SoundManager::Initialize(parentWindow, 1, 0);
}

int CycleValueOfPosition(uint32_t position) {
    switch (position & 3) {
    case 0: return 3;
    case 1: return 2;
    case 2: return 1;
    default: return 0;
    }
}

void PressCycleButton(CoolCycleButton* cycle) {
    SendMessage(cycle->m_hwnd, WM_LBUTTONDOWN, 0, 0);
    UpdateWindow(cycle->m_hwnd);
    Sleep(kButtonReleaseDelay);
}

}  // namespace


MainMenuWindow::MainMenuWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent)
    : GameWindowObject(instance, x, y, width, height, parent, nullptr, 0) {
    m_hintLabel = nullptr;
    m_strobeTimer = nullptr;
    m_modeButton0 = nullptr;
    m_modeButton1 = nullptr;
    m_modeButton2 = nullptr;
    m_multiplayerButton = nullptr;
    m_empireButton = nullptr;
    m_allianceButton = nullptr;
    m_quitButton = nullptr;
    m_toggleButton = nullptr;
    m_cycleButton = nullptr;
    m_loadButton = nullptr;
    m_optionsButton = nullptr;
    m_clickedRegion = nullptr;
    m_cycleImages[0] = nullptr;
    m_cycleImages[1] = nullptr;
    m_cycleImages[2] = nullptr;
    m_cycleValue = 1;
    m_gameType = 1;
    m_connectionKind = 1;
    m_mode = 0;
    m_leaving = 0;
    m_playingMovie = 0;
    char* dllLocation = GetDllLocation();
    LoadResourceModule(dllLocation, g_commonDllName, kCommonSlot);
    if (dllLocation) operator delete(dllLocation);
    g_isNetworkGame = 0;
    g_mainMenuWindow = this;
}

MainMenuWindow::~MainMenuWindow() {
    REB_KEEP_VPTR_STORE();
    if (m_strobeTimer) {
        m_strobeTimer->Pause();
        delete m_strobeTimer;
    }
    m_strobeTimer = nullptr;
    g_mainMenuWindow = nullptr;
    if (m_hintLabel) {
        m_hintLabel->~CoolLabel();
        operator delete(m_hintLabel);
    }
    DestroyControls();
    DeleteBitmap(m_cycleImages[0]);
    m_cycleImages[0] = nullptr;
    DeleteBitmap(m_cycleImages[1]);
    m_cycleImages[1] = nullptr;
    DeleteBitmap(m_cycleImages[2]);
    m_cycleImages[2] = nullptr;
    if (m_leaving == 0) SoundManager::Shutdown(0);
    m_regions.Clear();
}

void MainMenuWindow::DestroyControls() {
    DeleteIfPresent(m_quitButton);
    DeleteIfPresent(m_modeButton2);
    DeleteIfPresent(m_modeButton1);
    DeleteIfPresent(m_modeButton0);
    DeleteIfPresent(m_toggleButton);
    DeleteIfPresent(m_cycleButton);
    m_cycleButton = nullptr;
    DeleteIfPresent(m_multiplayerButton);
    m_multiplayerButton = nullptr;
    DeleteIfPresent(m_allianceButton);
    m_allianceButton = nullptr;
    DeleteIfPresent(m_empireButton);
    m_empireButton = nullptr;
    DeleteIfPresent(m_loadButton);
    m_loadButton = nullptr;
    DeleteIfPresent(m_optionsButton);
    m_optionsButton = nullptr;
}

void MainMenuWindow::HideControls() {
    HideIfPresent(m_cycleButton);
    HideIfPresent(m_multiplayerButton);
    HideIfPresent(m_allianceButton);
    HideIfPresent(m_empireButton);
    HideIfPresent(m_loadButton);
    HideIfPresent(m_optionsButton);
    HideIfPresent(m_quitButton);
    HideIfPresent(m_modeButton2);
    HideIfPresent(m_modeButton1);
    HideIfPresent(m_modeButton0);
    HideIfPresent(m_toggleButton);
}

void MainMenuWindow::CreateControls() {
    HMODULE module = CoolAppGetResourceModule(kCommonSlot);
    m_cycleButton = NewOrNull<CoolCycleButton>(module, 0xf2, 0x10f, 0x2c, 0x2f, this, kCycleCommand, 0x2713, 0x271f,
                                               0x2712, 0x271e, 0x2711, GetBackground(), 0);
    m_multiplayerButton = NewFrameButton(module, 0x43, 0x17d, 0x33, 0x3d, this, kMultiplayer, 0x2b8f, 0x2715, 0x1e);
    m_allianceButton = NewFrameButton(module, 0x1b5, 0x133, 0x3e, 0x37, this, kAllianceNewGame, 0x2b17, 0x2717, 0xf);
    m_empireButton = NewFrameButton(module, 0x99, 0x134, 0x3e, 0x37, this, kEmpireNewGame, 0x2af9, 0x2719, 0xf);
    m_loadButton = NewFrameButton(module, 0x19b, 0xe8, 0x28, 0x25, this, kLoadGame, 0x2be9, 0x271d, 0xf);
    m_optionsButton = NewFrameButton(module, 0x218, 0x189, 0x3f, 0x40, this, kOptions, 0x2bad, 0x271b, 0x1e);
    m_modeButton0 = NewFrameButton(module, 0x3d, 0x29, 0x33, 0x24, this, kModeCommand0, 0x2b35, 0x2c09, 0x1e);
    m_modeButton1 = NewFrameButton(module, 0x7c, 0x28, 0x31, 0x24, this, kModeCommand1, 0x2b53, 0x2c0b, 0x1e);
    m_modeButton2 = NewFrameButton(module, 0xbb, 0x29, 0x2d, 0x24, this, kModeCommand2, 0x2b71, 0x2c0a, 0x1e);
    m_toggleButton = NewOrNull<CoolStrobeButton>(module, 0x131, 0x14d, 0x2a, 0x1e, this, kToggleCommand, 0x27ae,
                                                 0x27af, GetBackground(), 0, 0);
    m_quitButton = NewOrNull<CoolStrobeButton>(module, 0x1cb, 0xf2, 0x21, 0x1c, this, kQuitCommand, 0x2c07, 0x2c08,
                                               GetBackground(), 0, 0);

    SetUpButton(this, m_quitButton, kClickSoundDefault, 0x100b);
    SetUpButton(this, m_cycleButton, kClickSoundCycle, 0x1001);
    SetUpButton(this, m_allianceButton, kClickSoundDefault, 0x1002);
    SetUpButton(this, m_empireButton, kClickSoundDefault, 0x1003);
    SetUpButton(this, m_loadButton, kClickSoundDefault, 0x1005);
    SetUpButton(this, m_multiplayerButton, kClickSoundMultiplayer, 0x1004);
    SetUpButton(this, m_optionsButton, kClickSoundOptions, 0x1006);
    SetUpToggleButton(this, m_toggleButton, 0x27af, 0x100a, -1);
    SetUpToggleButton(this, m_modeButton0, 0x2c09, 0x1007, 0);
    SetUpToggleButton(this, m_modeButton1, 0x2c0b, 0x1008, 1);
    SetUpToggleButton(this, m_modeButton2, 0x2c0a, 0x1009, 2);

    for (int i = 0; i < 3; ++i) {
        const CycleRegion& region = kCycleRegions[i];
        m_cycleImages[i] = LoadBitmap(module, region.m_bitmapId);
        IndexedRectNode* node = NewOrNull<IndexedRectNode>();
        node->m_index = region.m_index;
        RECT area;
        SetRect(&area, region.m_area.left, region.m_area.top, region.m_area.right, region.m_area.bottom);
        node->m_left = area.left;
        node->m_top = area.top;
        node->m_right = area.right;
        node->m_bottom = area.bottom;
        m_regions.Insert(node);
        AddRectToolFromResource(area, region.m_toolId, region.m_tooltipId);
    }

    m_hintLabel = NewOrNull<CoolLabel>(static_cast<uint16_t>(0), 0x10f, 0x176, 0x6e, 0xe, 0x200ff00u, 10, 0x25u, 3u);
    if (m_hintLabel) {
        const ResourceStringId hint = {0x1060, 1};
        m_hintLabel->SetTextFromResource(&hint);
    }
    m_strobeTimer = NewOrNull<CoolStrobeTimer>(&MainMenuStrobeStep);
    if (m_strobeTimer) m_strobeTimer->Restart(0, 0);
}

LRESULT __stdcall MainMenuWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND hwnd = m_hwnd;
    switch (message) {
    case WM_COMMAND:
        OnCommand(wParam & 0xffff);
        return 0;
    case WM_LBUTTONUP: {
        m_clickedRegion = m_regions.FindContaining(lParam & 0xffff, static_cast<uint32_t>(lParam) >> 16);
        if (!m_clickedRegion) return 0;
        CoolCycleButton* cycle = m_cycleButton;
        if (!cycle) return 0;
        const int index = static_cast<IndexedRectNode*>(m_clickedRegion)->m_index;
        bool press = true;
        if (index == 0) {
            if ((cycle->m_position & 3) == 2) {
                press = false;
            } else if (cycle->m_direction != 0) {
                cycle->m_direction = 0;
            }
        } else if (index == 1) {
            if ((cycle->m_position & 3) == 1) press = false;
        } else if (index == 2) {
            if ((cycle->m_position & 3) == 0) {
                press = false;
            } else if (cycle->m_direction != 1) {
                cycle->m_direction = 1;
            }
        }
        if (!press) {
            m_clickedRegion = nullptr;
            return 0;
        }
        m_cycleButton->ClearStateFlags(kButtonPixelHit);
        cycle = m_cycleButton;
        if ((cycle->m_position & 3) == 1 || static_cast<IndexedRectNode*>(m_clickedRegion)->m_index == 1) {
            PressCycleButton(cycle);
        } else {
            PressCycleButton(cycle);
            SendMessage(m_cycleButton->m_hwnd, WM_LBUTTONUP, 0, 0);
            UpdateWindow(m_cycleButton->m_hwnd);
            PressCycleButton(m_cycleButton);
        }
        SendMessage(m_cycleButton->m_hwnd, WM_LBUTTONUP, 0, 0);
        UpdateWindow(m_cycleButton->m_hwnd);
        m_cycleButton->SetStateFlags(kButtonPixelHit);
        return 0;
    }
    case WM_CREATE: {
        g_isNetworkGame = 0;
        HMODULE module = CoolAppGetResourceModule(kCommonSlot);
        SetBackground(NewOrNull<DibBitmap>(module, kBackdropId, 2u), 0, 0);
        m_childId = kBackdropChildId;
        RealizeBackdropPalette(this);
        g_stringModule = CoolAppGetResourceModule(kStringSlot);
        StartMenuSound(m_parent->m_hwnd);
        InitMusic();
        StartMusicTrack(UiHumanNextAdvice());
        CreateControls();
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        Draw(dc);
        DibBitmap* image = nullptr;
        int x = 0;
        if (m_cycleValue == 1) {
            image = m_cycleImages[2];
            x = 0x122;
        } else if (m_cycleValue == 2) {
            image = m_cycleImages[1];
            x = 0x146;
        } else if (m_cycleValue == 3) {
            image = m_cycleImages[0];
            x = 0x16a;
        }
        if (image) {
            const int height = image->Height();
            const int imageWidth = image->Width();
            image->DrawToDc(dc, '@', SRCCOPY, x, 0x125, 0, 0, imageWidth, height);
        }
        if (m_hintLabel) m_hintLabel->Draw(dc);
        EndPaint(hwnd, &paint);
        return 0;
    }
    default:
        return GameWindowObject::WindowProc(message, wParam, lParam);
    }
}

void MainMenuWindow::OnCommand(uint32_t commandId) {
    switch (commandId) {
    case kAllianceNewGame:
    case kEmpireNewGame: {
        HideControls();
        RandomGenerator random;
        const int side = commandId == kAllianceNewGame ? 1 : 2;
        const int modeOffset = commandId == kAllianceNewGame ? 1 : 4;
        AppSetGameParams(side, m_gameType, m_cycleValue, random.Seed(0), m_connectionKind, m_mode + modeOffset);
        UiLoadSideSpriteDlls(side, 1);
        SoundManager::Shutdown(0);
        m_leaving = 1;
        g_resourceDlls.UnloadSlot(kStringSlot);
        g_resourceDlls.UnloadSlot(kCommonSlot);
        AppSetState(kStateStartGame);
        return;
    }
    case kMultiplayer:
        m_gameType = 2;
        AppSetState(kStateNewGameSetup);
        return;
    case kLoadGame:
        HideControls();
        AppSetState(kStateLoadGame);
        return;
    case kOptions:
        AppSetState(kStateOptions);
        return;
    case kCycleCommand: {
        RECT area;
        SetRect(&area, 0x11f, 0x122, 0x184, 0x13b);
        if (m_cycleButton) m_cycleValue = CycleValueOfPosition(m_cycleButton->m_position);
        InvalidateRect(m_hwnd, &area, TRUE);
        return;
    }
    case kModeCommand0:
        SelectMode(this, m_modeButton0, m_modeButton1, m_modeButton2, 0);
        return;
    case kModeCommand1:
        SelectMode(this, m_modeButton1, m_modeButton0, m_modeButton2, 1);
        return;
    case kModeCommand2:
        SelectMode(this, m_modeButton2, m_modeButton1, m_modeButton0, 2);
        return;
    case kToggleCommand:
        if (!m_toggleButton->HasStateFlags(kButtonChecked)) {
            m_connectionKind = 1;
            if (m_hintLabel) ShowOptionHint(this, 0x1060);
        } else {
            m_connectionKind = 2;
            if (m_hintLabel) ShowOptionHint(this, 0x1061);
        }
        return;
    case kQuitCommand:
        SoundManager::Shutdown(0);
        m_leaving = 1;
        m_playingMovie = 1;
        Hide();
        AppPlayMovie(kMovieQuit, 0, reinterpret_cast<int>(&MainMenuMovieDone));
        return;
    }
}

void MainMenuStrobeStep() {
    MainMenuWindow* menu = g_mainMenuWindow;
    if (!menu || menu->m_playingMovie) return;
    StepButton(menu->m_empireButton);
    StepButton(menu->m_allianceButton);
    StepButton(menu->m_loadButton);
    StepButton(menu->m_modeButton0);
    StepButton(menu->m_modeButton1);
    StepButton(menu->m_modeButton2);
    StepButton(menu->m_multiplayerButton);
    StepButton(menu->m_optionsButton);
}

void MainMenuMovieDone() {
    MainMenuWindow* menu = g_mainMenuWindow;
    if (!menu) return;
    RealizeBackdropPalette(menu);
    InvalidateRect(menu->m_hwnd, nullptr, FALSE);
    menu->Show(SW_SHOW);
    menu->m_playingMovie = 0;
    HWND parentWindow = menu->m_parent->m_hwnd;
    StartMenuSound(parentWindow);
    menu->m_leaving = 0;
    InitMusic();
    StartMusicTrack(UiHumanNextAdvice());
}
