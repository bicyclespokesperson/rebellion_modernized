#include "core/reb_abi.h"
#include "strategy_ui/game_options_window.h"

#include <cmath>
#include <cstdint>

#include "app/app_cdcheck_version.h"
#include "app/app_dialogs.h"
#include "app/rebapp_globals.h"
#include "app/saved_game_info_decl.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/reb_x87.h"
#include "core/vptr_store.h"
#include "gfx/dib_bitmap.h"
#include "multiplayer/multiplayer_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "tactical/comm2.h"
#include "tactical/tactical_facade.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_label.h"
#include "ui/cool_slider.h"
#include "ui/cool_string_field.h"
#include "ui/resource_dll.h"

extern const char g_textcommDllName[];             // 0x6a7edc "textcomm.dll"
extern const char g_commonDllName[];               // 0x6a7c98 "common.dll"
extern const char g_spaceString[];                 // 0x6a7f40 " "
extern const char g_gameOptionsRequestLoadText[];  // 0x6a7eec
extern const char g_versionText[];                 // 0x6a7f44 "1.01.00"
extern const char g_emptyText[];                   // 0x6b120c

namespace {

constexpr uint32_t kCommonSlot = 8;
constexpr uint32_t kTextSlot = 1;
constexpr uint32_t kBitmapKind = 2;
constexpr int kSlotCount = 6;
constexpr int kStateFrontEndOptions = 3;
constexpr int kStateOptionsScreen = 0xd;
constexpr int kBattleStateFirst = 0x14;
constexpr int kBattleStateLast = 0x17;
constexpr int kMinVolume = -0x2580;
constexpr int kSliderRange = 100;
constexpr int16_t kStringGroup = 1;
constexpr int kLabelFont = 7;
constexpr uint32_t kLabelOn = 0x200ff00;
constexpr uint32_t kLabelOff = 0x2008000;
constexpr uint32_t kLabelDisabled = 0x2505050;
constexpr uint32_t kOnOffDelta = 0x7f00;

constexpr uint32_t kMusicOption = 0x100;
constexpr uint32_t kOptionBits[5] = {0x10000000, 0x1000000, 0x100000, 0x10000, 0x2000000};

enum Command : uint32_t {
    kSaveFirst = 0x6f,
    kSaveLast = 0x74,
    kLoadFirst = 0x79,
    kLoadLast = 0x7e,
    kNameFirst = 0x83,
    kNameLast = 0x88,
    kMusicSlider = 0x8d,
    kVolumeSlider = 0x8e,
    kBackButton = 0x8f,
    kDoneButton = 0x90,
    kStartButton = 0x91,
    kMusicToggle = 0x99,
    kOptionToggleFirst = 0xa1,
    kOptionToggleLast = 0xa6,
};

enum Message : uint32_t {
    kMessageSaved = 0x474,
    kMessageSaveFailed = 0x475,
    kMessageNoGameRunning = 0x476,
    kMessageLoad = 0x477,
    kMessageSave = 0x478,
    kMessageCloseDialog = 0x405,
    kMessageCommandFromField = 0x407,
    kMessageDialogResult = 0x46a,
};

constexpr int kRowY[kSlotCount] = {0x51, 0x7b, 0xa5, 0xcf, 0xf9, 0x123};
constexpr int kIconY[kSlotCount] = {0x53, 0x7d, 0xa7, 0xd1, 0xfb, 0x125};
constexpr int kOptionRowY[5] = {0x137, 0x151, 0x16d, 0x188, 0x1a3};
constexpr int kOptionLabelY[5] = {0x139, 0x154, 0x16f, 0x18a, 0x1a5};
constexpr int kDialogWidth = 600;
constexpr int kDialogHeight = 0x78;
constexpr int kSaveHeaderIconX = 0x55;

bool IsBattleState(int state) { return state >= kBattleStateFirst && state <= kBattleStateLast; }
bool IsLobbyState(int state) { return state == 4 || state == 5; }

// volume (hundredths of decibel) of a slider position: floor(log10(position / 100) * 2000); 0 is silent.
int SliderPositionToVolume(int position) {
    if (position == 0) return kMinVolume;
    const double hundredth = 0.01;
    const double scale = 2000.0;
    return static_cast<int>(floor(reb_scaled_log10(position, hundredth, scale)));
}

// The inverse: floor(10 ^ (volume / 2000) * 100), through the CRT's x87 pow like the original.
int VolumeToSliderPosition(int volume) {
    if (volume < kMinVolume) return 0;
    const double ten = 10.0;
    const double hundredth = 0.01;
    const double twentieth = 0.05;
    const double hundred = 100.0;
    return static_cast<int>(floor(reb_scaled_pow(ten, volume, hundredth, twentieth, hundred)));
}

uint32_t LabelColor(uint32_t options, uint32_t bit) { return (options & bit ? kOnOffDelta : 0) + kLabelOff; }

ResourceStringId OnOffText(bool on) { return ResourceStringId{static_cast<int16_t>(on ? 0x1031 : 0x1032), kStringGroup}; }

template <class T>
void DeleteIfPresent(T* object) {
    if (object) delete object;
}

void DeleteLabel(CoolLabel* label) {
    if (label) {
        label->~CoolLabel();
        operator delete(label);
    }
}

CoolLabel* NewLabel(uint16_t stringId, int x, int y, int width, int height, COLORREF color, UINT drawFlags,
                    UINT layoutFlags) {
    return NewOrNull<CoolLabel>(stringId, x, y, width, height, color, kLabelFont, drawFlags, layoutFlags);
}

void DrawLabel(CoolLabel* label, HDC dc, bool dimmed) {
    if (!label) return;
    // BUG(original): the DC is passed where SetColor expects the window to invalidate.
    if (dimmed) label->SetColor(kLabelDisabled, reinterpret_cast<HWND>(dc));
    label->Draw(dc);
}

CoolStrobeButton* NewButton(HMODULE module, int x, int y, int width, int height, GameOptionsWindow* parent,
                            uint32_t childId, uint32_t normalId, uint32_t pressedId, DibBitmap* backdrop) {
    return NewOrNull<CoolStrobeButton>(module, x, y, width, height, parent, childId, normalId, pressedId, backdrop, 0,
                                       0);
}

void AddChild(GameOptionsWindow* window, CoolStrobeButton* button) {
    button->Create(0);
    window->m_children.Insert(button);
}

bool SlotUsable(int slot) {
    SavedGameInfo header = AppReadSaveHeader(slot, g_isNetworkGame);
    if (!g_isNetworkGame) return header.m_gameType == 1;
    return header.m_gameType == 2 && IsSlotAssigned(slot);
}

void DrawSaveIcon(GameOptionsWindow* window, const SavedGameInfo& info, int slotIndex) {
    uint32_t iconId;
    if (info.m_gameType == 2) {
        iconId = 0x2749;
    } else if (info.m_side == 1) {
        iconId = 0x2748;
    } else if (info.m_side == 2) {
        iconId = 0x2747;
    } else {
        return;
    }
    DibBitmap* background = window->GetBackground();
    if (!background) return;
    DibBitmap* icon = NewOrNull<DibBitmap>(CoolAppGetResourceModule(kCommonSlot), iconId, kBitmapKind);
    if (!icon) return;
    icon->DrawKeyed(background, kSaveHeaderIconX, kIconY[slotIndex]);
    icon->~DibBitmap();
    operator delete(icon);
}

void ShowSavedName(GameOptionsWindow* window, CoolStringField* field, const SavedGameInfo& info, int slotIndex) {
    String name;
    name.Assign(info.GetName());
    if (name.Length() == 0) {
        field->SetText(g_emptyText);
        return;
    }
    field->SetText(name.m_text);
    DrawSaveIcon(window, info, slotIndex);
}

}  // namespace

GameOptionsWindow::GameOptionsWindow(HINSTANCE instance, int x, int y, int width, int height, CoolWindow* parent,
                                     int state)
    : GameWindowObject(instance, x, y, width, height, parent, nullptr, 0) {
    m_volumeSlider = nullptr;
    m_musicSlider = nullptr;
    m_unknownE4 = 0;
    m_musicTitle = nullptr;
    for (CoolLabel*& label : m_optionTitles) label = nullptr;
    m_musicValue = nullptr;
    for (CoolLabel*& label : m_optionValues) label = nullptr;
    m_versionLabel = nullptr;
    m_state = state;
    m_battleFlowState = 0;
    const uint32_t options2 = AppGetDisplayOptions2();
    const uint32_t options = AppGetDisplayOptions();
    m_options = options2 | options;
    for (int i = 0; i < kSlotCount; ++i) {
        m_nameFields[i] = nullptr;
        m_slotLoadable[i] = 0;
    }
    m_battleFlowState = AppGetBattleFlowState();
    char* dllLocation = GetDllLocation();
    if (!CoolAppGetResourceModule(kTextSlot)) LoadResourceModule(dllLocation, g_textcommDllName, kTextSlot);
    LoadResourceModule(dllLocation, g_commonDllName, kCommonSlot);
    if (dllLocation) operator delete(dllLocation);
}

GameOptionsWindow::~GameOptionsWindow() {
    REB_KEEP_VPTR_STORE();
    for (CoolStringField* field : m_nameFields) DeleteIfPresent(field);
    DeleteIfPresent(m_musicSlider);
    DeleteIfPresent(m_volumeSlider);
    DeleteLabels();
    if (!IsBattleState(m_battleFlowState)) {
        AppEnableSpeedAfterDialog();
    } else {
        UiAckSync(UiGetHumanSide());
    }
}

LRESULT REB_STDCALL GameOptionsWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    HWND hwnd = m_hwnd;
    const bool inBattle = IsBattleState(m_battleFlowState);
    switch (message) {
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);
        Draw(dc);
        DrawOverlay(dc);
        EndPaint(hwnd, &paint);
        return 0;
    }
    case WM_CREATE:
        UiBeginSync(0);
        m_childId = 5;
        CreateBackdrop();
        CreateLabels();
        CreateControls();
        return 0;
    case WM_DRAWITEM: {
        const DRAWITEMSTRUCT* item = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (!item) return 0;
        CoolStrobeButton* button = static_cast<CoolStrobeButton*>(m_children.FindById(item->CtlID));
        if (!button) return 0;
        button->Draw(item->hDC, static_cast<char>(item->itemState & 1));
        return 0;
    }
    case WM_COMMAND: {
        const uint32_t id = wParam & 0xffff;
        if (id >= kSaveFirst && id <= kSaveLast) {
            m_selectedSlot = id - 0x6e;
            const int lobby = IsLobbyState(m_state) ? 1 : 0;
            bool noGameToReplace = false;
            if (AppGetSavedGameType(m_selectedSlot, lobby)) noGameToReplace = m_slotLoadable[m_selectedSlot - 1] == 0;
            if (noGameToReplace) {
                ShowAppDialog(kMessageSave, this, 0x121e, 0x121f, kDialogWidth, kDialogHeight);
                return 0;
            }
            SaveSelectedSlot();
            return 0;
        }
        if (id >= kLoadFirst && id <= kLoadLast) {
            m_selectedSlot = id - 0x78;
            bool anyLoadable = false;
            for (int i = 0; i < kSlotCount; ++i) {
                if (m_slotLoadable[i] == 1) {
                    anyLoadable = true;
                    break;
                }
            }
            if (!anyLoadable && m_battleFlowState != 0) {
                ShowAppDialog(kMessageLoad, this, 0x121c, 0x121d, kDialogWidth, kDialogHeight);
                return 0;
            }
            LoadSelectedSlot();
            return 0;
        }
        if (id >= kNameFirst && id <= kNameLast) {
            // BUG(original): the name field ids start at 0x83, so this is not the slot number the buttons store.
            m_selectedSlot = id - 0x78;
            return 0;
        }
        switch (id) {
        case kMusicSlider: {
            const int volume = static_cast<int>(lParam) == 0 ? kMinVolume : SliderPositionToVolume(static_cast<int>(lParam));
            if (inBattle) TacticalNotifySyncQuiet(static_cast<int>(lParam));
            AppSetMusicVolume(volume);
            return 0;
        }
        case kVolumeSlider:
            AppSetVolume(lParam == 0 ? kMinVolume : SliderPositionToVolume(static_cast<int>(lParam)));
            return 0;
        case kBackButton:
            AppSetState(kStateOptionsScreen);
            return 0;
        case kDoneButton:
            AppSetDisplayOptions(m_options);
            AppSetDisplayOptions2(m_options);
            g_resourceDlls.UnloadSlot(kCommonSlot);
            if (IsLobbyState(m_state)) {
                CommSendPlayerQuit();
                m_state = kStateFrontEndOptions;
            }
            AppSetState(m_state);
            return 0;
        case kStartButton: {
            AppSetDisplayOptions(m_options);
            AppSetDisplayOptions2(m_options);
            if (m_battleFlowState != 0) {
                bool anyLoadable = false;
                for (int i = 0; i < kSlotCount; ++i) {
                    if (m_slotLoadable[i] == 1) {
                        anyLoadable = true;
                        break;
                    }
                }
                if (!anyLoadable) {
                    ShowResultDialog(kMessageNoGameRunning);
                    return 0;
                }
            }
            AppSetState(kStateFrontEndOptions);
            return 0;
        }
        case kMusicToggle:
            OnOptionToggled(id, 0);
            return 0;
        case 0xa1:
        case 0xa2:
        case 0xa3:
        case 0xa4:
        case 0xa6:
            OnOptionToggled(id, 1);
            return 0;
        }
        return 0;
    }
    case kMessageCloseDialog:
        if (wParam == 0xd) DestroyAppDialog();
        return 0;
    case kMessageCommandFromField:
        PostMessageA(hwnd, WM_COMMAND, wParam - 0x14, 0);
        return 0;
    case kMessageDialogResult:
        switch (wParam) {
        case kMessageSaved:
            UpdateSlotButtons();
            InvalidateRect(m_hwnd, nullptr, FALSE);
            return 0;
        case kMessageNoGameRunning:
            AppSetState(kStateFrontEndOptions);
            return 0;
        case kMessageLoad:
            LoadSelectedSlot();
            return 0;
        case kMessageSave:
            SaveSelectedSlot();
            return 0;
        }
        return 0;
    default:
        return GameWindowObject::WindowProc(message, wParam, lParam);
    }
}

void GameOptionsWindow::CreateBackdrop() {
    HWND hwnd = m_hwnd;
    DibBitmap* background =
        NewOrNull<DibBitmap>(CoolAppGetResourceModule(kCommonSlot), 0x4e22u, kBitmapKind);
    HDC dc = GetDC(hwnd);
    HPALETTE palette = background->CreatePaletteFromColorTable();
    SetIdentityPalette(dc, palette);
    ReleaseDC(hwnd, dc);
    SetBackground(background, 0, 0);
}

void GameOptionsWindow::CreateControls() {
    HINSTANCE instance = m_instance;
    HMODULE module = CoolAppGetResourceModule(kCommonSlot);
    DibBitmap* backdrop = GetBackground();
    String name;
    const bool inBattle = IsBattleState(m_battleFlowState);
    const bool lobby = IsLobbyState(m_state);

    for (int i = 0; i < kSlotCount; ++i) {
        const int slot = i + 1;
        CoolStrobeButton* save = NewButton(module, 0x22, kRowY[i], 0x2a, 0x14, this, slot + 0x6e, 0x273e, 0x273f, backdrop);
        if (save) {
            save->LoadStateBitmap(kButtonDisabled, 0x2740);
            AddChild(this, save);
            AddToolFromResource(save, 0x1010);
            if (m_battleFlowState == 0 || inBattle || (g_isNetworkGame != 0 && AppGetUnknown138() == 0)) save->Disable();
        }

        CoolStringField* field = NewOrNull<CoolStringField>(instance, 0x77, kRowY[i], 0xa0, 0x14, this, backdrop, 0u);
        m_nameFields[i] = field;
        if (field) {
            field->m_textColor = 0x2ffffff;
            field->m_shadowColor = 0x2000000;
            field->m_childId = slot + 0x82;
            field->Create(0);
            SavedGameInfo info = AppReadSaveHeader(slot, lobby ? 1 : 0);
            name.Assign(info.GetName());
            if (name.Length() == 0) {
                field->SetText(g_emptyText);
            } else {
                field->SetText(name.m_text);
                DrawSaveIcon(this, info, i);
            }
            if (m_battleFlowState == 0 || inBattle || (g_isNetworkGame != 0 && AppGetUnknown138() == 0)) field->Disable();
        }

        CoolStrobeButton* load = NewButton(module, 0x11f, kRowY[i], 0x29, 0x14, this, slot + 0x78, 0x2741, 0x2742, backdrop);
        if (load) {
            load->LoadStateBitmap(kButtonDisabled, 0x2743);
            AddChild(this, load);
            if (inBattle || (g_isNetworkGame != 0 && AppGetUnknown138() == 0)) load->Disable();
            const bool usable = SlotUsable(slot);
            if (!usable) {
                load->Disable();
            } else if (AppGetSavedGameType(slot, lobby ? 1 : 0) == 0) {
                load->Disable();
            }
            AddToolFromResource(load, 0x1011);
        }
    }

    const int volumeControl = AppGetVolume();
    m_volumeSlider = NewOrNull<CoolSlider>(module, 0x188, 0xbe, this, kVolumeSlider, 2, backdrop, 0x2745u, 0x2746u,
                                           static_cast<uint32_t>(VolumeToSliderPosition(volumeControl)),
                                           static_cast<uint32_t>(kSliderRange));
    if (m_volumeSlider) {
        m_volumeSlider->Create(0);
        AddToolFromResource(m_volumeSlider, 0x1013);
    }
    const int musicVolume = AppGetMusicVolume();
    m_musicSlider = NewOrNull<CoolSlider>(module, 0x188, 0x82, this, kMusicSlider, 2, backdrop, 0x2744u, 0x2746u,
                                          static_cast<uint32_t>(VolumeToSliderPosition(musicVolume)),
                                          static_cast<uint32_t>(kSliderRange));
    if (m_musicSlider) {
        m_musicSlider->Create(0);
        AddToolFromResource(m_musicSlider, 0x1012);
    }

    CoolStrobeButton* start = NewButton(module, 0x4c, 0x17d, 0x2a, 0x2a, this, kStartButton, 0x2733, 0x2734, backdrop);
    if (start) {
        AddChild(this, start);
        AddToolFromResource(start, 0x1014);
    }

    uint32_t doneNormal;
    uint32_t donePressed;
    uint32_t doneDisabled;
    switch (m_state) {
    case 4:
    case 0xc:
        doneNormal = 0x272d;
        donePressed = 0x272e;
        doneDisabled = 0x272f;
        break;
    case 8:
        if (AppGetSide() != 1) {
            doneNormal = 0x2727;
            donePressed = 0x2728;
            doneDisabled = 0x2729;
            break;
        }
        [[fallthrough]];
    default:
        doneNormal = 0x2724;
        donePressed = 0x2725;
        doneDisabled = 0x2726;
        break;
    case 9:
        doneNormal = 0x272a;
        donePressed = 0x272b;
        doneDisabled = 0x272c;
        break;
    }
    CoolStrobeButton* done = NewButton(module, 0xa2, 0x17e, 0x2a, 0x2a, this, kDoneButton, doneNormal, donePressed, backdrop);
    if (done) {
        done->LoadStateBitmap(kButtonDisabled, doneDisabled);
        AddChild(this, done);
        AddToolFromResource(done, (m_state == 4 || m_state == 5) ? 0x1019 : 0x1015);
        if (m_battleFlowState == 0 && m_state != 4 && m_state != 5) done->Disable();
    }

    CoolStrobeButton* back = NewButton(module, 0xf8, 0x17d, 0x2a, 0x2a, this, kBackButton, 0x2736, 0x2737, backdrop);
    if (back) {
        AddChild(this, back);
        AddToolFromResource(back, 0x1018);
    }

    CoolStrobeButton* music = NewButton(module, 0x160, 0x4c, 0x13, 0x23, this, kMusicToggle, 0x2738, 0x2739, backdrop);
    if (music) {
        music->LoadStateBitmap(kButtonDisabled, 0x273a);
        music->LoadStateBitmap(kButtonChecked, 0x2739);
        music->SetStateFlags(kButtonToggle);
        AddChild(this, music);
        if (m_options & kMusicOption) music->SetStateFlags(kButtonChecked);
    }

    const uint32_t optionIds[5] = {0xa1, 0xa2, 0xa3, 0xa4, 0xa6};
    for (int i = 0; i < 5; ++i) {
        CoolStrobeButton* toggle = NewButton(module, 0x165, kOptionRowY[i], 0x23, 0x16, this, optionIds[i], 0x273b, 0x273c, backdrop);
        if (!toggle) continue;
        toggle->LoadStateBitmap(kButtonDisabled, 0x273d);
        toggle->LoadStateBitmap(kButtonChecked, 0x273c);
        toggle->SetStateFlags(kButtonToggle);
        AddChild(this, toggle);
        if (inBattle) {
            toggle->Disable();
        } else if (m_options & kOptionBits[i]) {
            toggle->SetStateFlags(kButtonChecked);
        }
    }
    UpdateSlotButtons();
}

void GameOptionsWindow::LoadSelectedSlot() {
    String name;
    if (m_state != 4 && m_state != 5) {
        CommTrace(g_gameOptionsRequestLoadText);
        CommSendPlayerQuit();
    }
    AppRequestLoad(m_selectedSlot);
    SendSaveCommand(2, m_selectedSlot, name);
}

void GameOptionsWindow::SaveSelectedSlot() {
    String name;
    name = m_nameFields[m_selectedSlot - 1]->m_text.m_text;
    AppRequestSave(m_selectedSlot, name);
    SendSaveCommand(1, m_selectedSlot, name);
}

void GameOptionsWindow::SendSaveCommand(int kind, int slot, String name) {
    bool send = false;
    if (kind == 1) {
        send = AppGetGameType() == 2;
    } else if (kind == 2 && IsLobbyState(m_state)) {
        send = true;
    }
    if (send) {
        const DWORD tick = GetTickCount();
        AppSetAutosaveTick(tick);
        AppSendSaveCommand(kind, slot, name, tick);
    }
}

void GameOptionsWindow::ShowResultDialog(uint32_t message) {
    switch (message) {
    case kMessageSaved:
        ShowAppNoticeDialog(message, this, 0, 0x1217, 0x1300, kDialogWidth, kDialogHeight);
        break;
    case kMessageSaveFailed:
        ShowAppNoticeDialog(message, this, 0, 0x1219, 0x1300, kDialogWidth, kDialogHeight);
        break;
    case kMessageNoGameRunning:
        ShowAppDialog(message, this, 0x121a, 0x121b, kDialogWidth, kDialogHeight);
        break;
    case kMessageLoad:
        ShowAppDialog(message, this, 0x121c, 0x121d, kDialogWidth, kDialogHeight);
        break;
    case kMessageSave:
        ShowAppDialog(message, this, 0x121e, 0x121f, kDialogWidth, kDialogHeight);
        break;
    }
}

void GameOptionsWindow::OnSaveResult(int saved, int slot) {
    if (saved) {
        ShowAppNoticeDialog(kMessageSaved, this, 0, 0x1217, 0x1300, kDialogWidth, kDialogHeight);
        m_slotLoadable[slot - 1] = 1;
        UpdateSlotButtons();
        InvalidateRect(m_hwnd, nullptr, FALSE);
        return;
    }
    ShowAppNoticeDialog(kMessageSaveFailed, this, 0, 0x1219, 0x1300, kDialogWidth, kDialogHeight);
}

void GameOptionsWindow::OnOptionToggled(uint32_t buttonId, int unused) {
    CoolStrobeButton* button = static_cast<CoolStrobeButton*>(m_children.FindById(buttonId));
    const bool inBattle = IsBattleState(m_battleFlowState);
    CoolLabel* title = nullptr;
    CoolLabel* value = nullptr;
    uint32_t bit = 0;
    switch (buttonId) {
    case kMusicToggle:
        title = m_musicTitle;
        value = m_musicValue;
        bit = kMusicOption;
        if (inBattle) {
            TacticalNotifySync(~m_options & kMusicOption);
        } else {
            AppSetMusicSwitch(~m_options & kMusicOption);
        }
        break;
    case 0xa1: title = m_optionTitles[0]; value = m_optionValues[0]; bit = kOptionBits[0]; break;
    case 0xa2: title = m_optionTitles[1]; value = m_optionValues[1]; bit = kOptionBits[1]; break;
    case 0xa3: title = m_optionTitles[2]; value = m_optionValues[2]; bit = kOptionBits[2]; break;
    case 0xa4: title = m_optionTitles[3]; value = m_optionValues[3]; bit = kOptionBits[3]; break;
    case 0xa6: title = m_optionTitles[4]; value = m_optionValues[4]; bit = kOptionBits[4]; break;
    default: break;
    }
    if (!button) return;
    const bool checked = button->HasStateFlags(kButtonChecked);
    if (checked) {
        m_options |= bit;
    } else {
        m_options &= ~bit;
    }
    if (title) {
        title->SetColor(checked ? kLabelOn : kLabelOff, nullptr);
        title->Invalidate(m_hwnd, TRUE);
    }
    if (!value) return;
    const ResourceStringId text = OnOffText(checked);
    value->SetColor(checked ? kLabelOn : kLabelOff, nullptr);
    value->SetTextFromResource(&text);
    value->Invalidate(m_hwnd, TRUE);
}

void GameOptionsWindow::CreateLabels() {
    g_stringModule = CoolAppGetResourceModule(kTextSlot);
    DrawStringResource(0x1028, RECT{0x3a, 0x25, 299, 0x3a}, kLabelFont, kLabelOn, 0x25);
    DrawStringResource(0x1026, RECT{0x16a, 0x25, 0x25b, 0x3a}, kLabelFont, kLabelOn, 0x25);
    DrawStringResource(0x1027, RECT{0x16b, 0x111, 0x25a, 0x126}, kLabelFont, kLabelOn, 0x25);

    m_musicTitle = NewLabel(0x1025, 0x178, 0x4f, 0xb7, 0x21, (m_options & kMusicOption ? kOnOffDelta : 0) + kLabelOff,
                            0x24, 3);
    static constexpr uint16_t kTitleStrings[5] = {0x101f, 0x1020, 0x1021, 0x1022, 0x1024};
    for (int i = 0; i < 5; ++i) {
        m_optionTitles[i] = NewLabel(kTitleStrings[i], 0x18a, kOptionLabelY[i], 0xb7, 0x10,
                                     (m_options & kOptionBits[i] ? kOnOffDelta : 0) + kLabelOff, 0x10, 3);
    }
    {
        const bool on = (m_options & kMusicOption) != 0;
        m_musicValue = NewLabel(on ? 0x1031 : 0x1032, 0x245, 0x4f, 0x1f, 0x21, (on ? kOnOffDelta : 0) + kLabelOff, 0x24, 3);
    }
    for (int i = 0; i < 5; ++i) {
        const bool on = (m_options & kOptionBits[i]) != 0;
        m_optionValues[i] = NewLabel(on ? 0x1031 : 0x1032, 0x241, kOptionLabelY[i], 0x1f, 0x10,
                                     (on ? kOnOffDelta : 0) + kLabelOff, 0x10, 3);
    }
    m_versionLabel = NewOrNull<CoolLabel>(static_cast<uint16_t>(0), 0x19, 0x1b5, 0x136, 0x12, 0x2000000u, 10, 0x25u, 3u);
    if (m_versionLabel) {
        const ResourceStringId title = {0x100c, kStringGroup};
        String text(&title);
        text += String(g_versionText);
        String language(GetLanguageName());
        text += String(g_spaceString);
        text += language;
        m_versionLabel->SetText(text.m_text);
    }
}

void GameOptionsWindow::DrawOverlay(HDC dc) {
    const bool inBattle = IsBattleState(m_battleFlowState);
    if (m_musicTitle) m_musicTitle->Draw(dc);
    for (CoolLabel* title : m_optionTitles) DrawLabel(title, dc, inBattle);
    if (m_musicValue) m_musicValue->Draw(dc);
    for (CoolLabel* value : m_optionValues) DrawLabel(value, dc, inBattle);
    if (m_versionLabel) m_versionLabel->Draw(dc);
}

void GameOptionsWindow::DeleteLabels() {
    DeleteLabel(m_musicTitle);
    for (CoolLabel* title : m_optionTitles) DeleteLabel(title);
    DeleteLabel(m_musicValue);
    for (CoolLabel* value : m_optionValues) DeleteLabel(value);
    DeleteLabel(m_versionLabel);
}

void GameOptionsWindow::UpdateSlotButtons() {
    const int lobby = IsLobbyState(m_state) ? 1 : 0;
    for (int slot = 1; slot <= kSlotCount; ++slot) {
        CoolStrobeButton* load = static_cast<CoolStrobeButton*>(m_children.FindById(slot + 0x78));
        if (load) {
            const bool usable = SlotUsable(slot);
            if (!usable) {
                load->Disable();
            } else {
                const int type = AppGetSavedGameType(slot, lobby);
                if (!g_isNetworkGame) {
                    if (type == 0) load->Disable(); else load->Enable();
                } else if (lobby == 0 || type == 0) {
                    load->Disable();
                } else {
                    load->Enable();
                }
            }
        }
        CoolStringField* field = m_nameFields[slot - 1];
        if (field) {
            SavedGameInfo info = AppReadSaveHeader(slot, lobby);
            ShowSavedName(this, field, info, slot - 1);
        }
    }
}

String SavedGameInfo::GetName() const { return m_name; }
