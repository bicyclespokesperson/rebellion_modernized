// MultiPlayerWindow, part 2: the controls and what each page shows and hides.
#include <cstring>

#include "app/app_control_cmds.h"
#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/utility_classes.h"
#include "gfx/dib_bitmap.h"
#include "multiplayer/comm_mgr.h"
#include "multiplayer/message_codec.h"
#include "app/saved_game_info_decl.h"
#include "multiplayer/multiplayer_window.h"
#include "orders/order_base.h"
#include "tactical/comm2.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_string_field.h"
#include "ui/text_scroll_field.h"

extern const RECT g_rectProviderList;  // 0x658668
extern const RECT g_rectSessionList;   // 0x658778
extern uint32_t g_lobbyProviderItemWidth;  // 0x6b1244
extern int g_flowRangeSpan;                // 0x6b1240 width of the session list items (computed by 0x40d780)

namespace {

constexpr uint32_t kListOptions = kDragListSingleSelect | kDragListNoDrag;
constexpr int kListItemHeight = 0x14;
constexpr UINT kListTextFormat = 0x24;
constexpr COLORREF kSelectedTextColor = 0x20000ff;
constexpr COLORREF kTextColor = 0x200ff00;
constexpr uint16_t kScrollBarId = 0x27a5;
constexpr int kFieldFont = 4;
constexpr uint32_t kFieldUnknownEc = 0x20000ff;
constexpr uint16_t kNameMaxLength = 0x14;
constexpr uint16_t kChatMaxLength = 0x46;
constexpr uint32_t kSessionTimerMs = 5000;
constexpr uint32_t kNetworkTimerMs = 500;
constexpr int kNoticeProviderFailed = 0x484;
constexpr int kShowCommand = 5;
constexpr int kStateMainMenu = 3;
constexpr int kStateLobbyLaunched = 0xe;
constexpr int kNotCreated = 0x11;
constexpr int kSlotsInTable = 5;
constexpr char kCancelTrace[] = "MultiPlayerWindow::DoCancelMultiPlayer(): About to call PlayerQuitSession()!\n";
constexpr char kSetupTrace[] =
    "MultiPlayerWindow::DoCommand(): MP_BCKGND_GAME_SETUP : About to call PlayerQuitSession()!\n";
constexpr char kStatusMarker[] = "\x17" "4";
constexpr char kLineEnd[] = "\n";
constexpr char kSideA[] = "\x17" "10";
constexpr char kSideB[] = "\x17" "11";
constexpr char kSize1[] = "\x17" "20";
constexpr char kSize2[] = "\x17" "21";
constexpr char kSize3[] = "\x17" "22";
constexpr char kType1[] = "\x17" "30";
constexpr char kType2[] = "\x17" "31";

struct ButtonSpec {
    int x;
    int y;
    uint32_t normalId;
    uint32_t pressedId;
    uint32_t childId;
    UINT tooltipId;
    uint16_t labelStringId;
    int fontSlot;
    COLORREF color;
    COLORREF pressedColor;
    int repeatRate;
    int pixelHit;
};

constexpr ButtonSpec kButtons[] = {
    {0x1b5, 0x1ba, 0x2797, 0x2798, 0x67, 0x1102, 0, 0, 0, 0, 0, 0},
    {0x122, 0x1ba, 0x2794, 0x2795, 0x65, 0x1101, 0, 0, 0, 0, 0, 0},
    {0x8d, 0x1ba, 0x2791, 0x2792, 100, 0x1100, 0, 0, 0, 0, 0, 0},
    {0x122, 0x1ba, 0x279a, 0x279b, 0x66, 0x1103, 0, 0, 0, 0, 0, 0},
    {0x1f6, 199, 0x279d, 0x279e, 0x6f, 0x1104, 0, 0, 0, 0, 0, 0},
    {0x8b, 0x139, 0x27a3, 0x27a4, 0x68, 0, 0x1109, 0, 0x200ff00, 0x20000ff, 0, 0},
    {0x15a, 0x139, 0x27a3, 0x27a4, 0x69, 0, 0x1108, 0, 0x200ff00, 0x20000ff, 0, 0},
    {0x1b8, 0x47, 0x2785, 0x2786, 0x6a, 0x1161, 0, 0, 0, 0, 0, 0},
    {0x185, 0x47, 0x2782, 0x2783, 0x6b, 0x1160, 0, 0, 0, 0, 0, 0},
    {0x185, 0x85, 0x2788, 0x2789, 0x6c, 0x100f, 0, 0, 0, 0, 0, 0},
    {0x1b8, 0x85, 0x278b, 0x278c, 0x6d, 0x100e, 0, 0, 0, 0, 0, 0},
    {0x1eb, 0x85, 0x278e, 0x278f, 0x6e, 0x100d, 0, 0, 0, 0, 0, 0},
    {0x8e, 0xcd, 0x27a0, 0x27a1, 0x70, 0, 0, 0, 0, 0, 0, 0},
    {0x13e, 0xcd, 0x27a0, 0x27a1, 0x71, 0, 0, 0, 0, 0, 0, 0},
};

constexpr uint32_t kButtonChildIds[] = {100, 0x65, 0x66, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71};

CoolStrobeButton* MultiPlayerWindow::*const kButtonMembers[] = {
    &MultiPlayerWindow::m_backButton,     &MultiPlayerWindow::m_nextButton,     &MultiPlayerWindow::m_startButton,
    &MultiPlayerWindow::m_joinModeButton, &MultiPlayerWindow::m_hostModeButton, &MultiPlayerWindow::m_sideButtonA,
    &MultiPlayerWindow::m_sideButtonB,    &MultiPlayerWindow::m_sizeButton1,    &MultiPlayerWindow::m_sizeButton2,
    &MultiPlayerWindow::m_sizeButton3,    &MultiPlayerWindow::m_loadButton,     &MultiPlayerWindow::m_typeButton1,
    &MultiPlayerWindow::m_typeButton2,
};

struct StateBitmap {
    CoolStrobeButton* MultiPlayerWindow::*button;
    int state;
    uint32_t bitmapId;
};

constexpr StateBitmap kStateBitmaps[] = {
    {&MultiPlayerWindow::m_hostModeButton, 4, 0x27a4}, {&MultiPlayerWindow::m_joinModeButton, 4, 0x27a4},
    {&MultiPlayerWindow::m_sideButtonA, 4, 0x2786},    {&MultiPlayerWindow::m_sideButtonB, 4, 0x2783},
    {&MultiPlayerWindow::m_sizeButton1, 4, 0x2789},    {&MultiPlayerWindow::m_sizeButton2, 4, 0x278c},
    {&MultiPlayerWindow::m_sizeButton3, 4, 0x278f},    {&MultiPlayerWindow::m_typeButton1, 4, 0x27a1},
    {&MultiPlayerWindow::m_typeButton2, 4, 0x27a1},    {&MultiPlayerWindow::m_sideButtonA, 2, 0x2787},
    {&MultiPlayerWindow::m_sideButtonB, 2, 0x2784},    {&MultiPlayerWindow::m_sizeButton1, 2, 0x278a},
    {&MultiPlayerWindow::m_sizeButton2, 2, 0x278d},    {&MultiPlayerWindow::m_sizeButton3, 2, 0x2790},
    {&MultiPlayerWindow::m_backButton, 2, 0x2793},     {&MultiPlayerWindow::m_loadButton, 2, 0x279f},
    {&MultiPlayerWindow::m_nextButton, 2, 0x2796},     {&MultiPlayerWindow::m_startButton, 2, 0x279c},
};

bool AllButtonsExist(const MultiPlayerWindow* window) {
    for (auto member : kButtonMembers)
        if (!(window->*member)) return false;
    return true;
}

bool SameGuid(const GUID& a, const GUID& b) {
    const uint32_t* first = reinterpret_cast<const uint32_t*>(&a);
    const uint32_t* second = reinterpret_cast<const uint32_t*>(&b);
    return first[0] == second[0] && first[1] == second[1] && first[2] == second[2] && first[3] == second[3];
}

void DeleteProviderData(CoolDragList* list) {
    for (CoolListItem* item = list->m_items->First(); item; item = item->Next())
        if (item->m_userData) delete reinterpret_cast<ServiceProviderInfo*>(item->m_userData);
}

void DeleteSessionData(CoolDragList* list) {
    for (CoolListItem* item = list->m_items->First(); item; item = item->Next())
        if (item->m_userData) delete reinterpret_cast<SessionInfo*>(item->m_userData);
}

CoolDragList* CreateList(MultiPlayerWindow* window, const RECT& area, uint32_t overlayId, uint32_t childId,
                         int itemWidth) {
    DibBitmap* backdrop = window->FindOverlayBitmap(overlayId);
    HMODULE module = CoolAppGetResourceModule(window->m_bitmapSlot);
    return NewOrNull<CoolDragList>(module, area.left, area.top, area.right - area.left, area.bottom - area.top, window,
                                   backdrop, childId, itemWidth, kListItemHeight, 0, 0, 0, 0, 0, 1, nullptr);
}

void FinishList(MultiPlayerWindow* window, CoolDragList* list) {
    list->m_options.m_value = kListOptions;
    list->Create(0);
    list->m_textFormat = kListTextFormat;
    list->SetSelectedTextColor(kSelectedTextColor);
    list->CreateVerticalScrollBar(window->m_instance, list, static_cast<uint16_t>(window->m_bitmapSlot), kScrollBarId,
                                  list->m_clientHeight, 0, list->m_clientHeight, 1, nullptr);
}

CoolStringField* CreateField(MultiPlayerWindow* window, uint32_t overlayId, int x, int y, int width, int height,
                             uint32_t childId) {
    DibBitmap* backdrop = window->FindOverlayBitmap(overlayId);
    HMODULE module = CoolAppGetResourceModule(window->m_bitmapSlot);
    auto* field = NewOrNull<CoolStringField>(module, x, y, width, height, window, backdrop, 0);
    if (!field) return nullptr;
    field->m_childId = childId;
    field->SetFont(kFieldFont);
    field->m_shadowOffset = 0;
    field->m_textColor = kTextColor;
    field->m_unknownEc = kFieldUnknownEc;
    field->Create(0);
    return field;
}

void SetCheckedPair(CoolStrobeButton* first, CoolStrobeButton* second, bool firstChecked) {
    if (firstChecked) {
        first->SetStateFlags(kButtonChecked);
        second->ClearStateFlags(kButtonChecked);
    } else {
        first->ClearStateFlags(kButtonChecked);
        second->SetStateFlags(kButtonChecked);
    }
}

}  // namespace

// ---- the controls -----------------------------------------------------------------------------------------------

void MultiPlayerWindow::OnCreate3() {
    if (m_paintBackground) {
        HDC dc = GetDC(m_hwnd);
        HPALETTE palette = m_paintBackground->CreatePaletteFromColorTable();
        SetIdentityPalette(dc, palette);
        ReleaseDC(m_hwnd, dc);
    }
    m_providerList = CreateList(this, g_rectProviderList, kLobbyPageProvider, kLobbyChildProviderList,
                                g_lobbyProviderItemWidth);
    if (m_providerList) {
        FinishList(this, m_providerList);
        FillProviderList();
    }
    m_sessionList = CreateList(this, g_rectSessionList, kLobbyPageJoin, kLobbyChildSessionList, g_flowRangeSpan);
    if (m_sessionList) {
        FinishList(this, m_sessionList);
        m_sessionList->RemoveAll();
    }
    String name;
    m_hostNameField = CreateField(this, kLobbyPageHost, 0x7c, 0x91, 0x86, 0x11, 200);
    if (m_hostNameField) {
        m_hostNameField->SetMaxLength(kNameMaxLength);
        String defaultName;
        GetDefaultPlayerName(&defaultName);
        name.Assign(defaultName);
        m_hostNameField->SetText(name.m_text);
    }
    m_gameNameField = CreateField(this, kLobbyPageHost, 0x7c, 0x12e, 0x86, 0x11, 0xca);
    if (m_gameNameField) {
        m_gameNameField->SetMaxLength(kNameMaxLength);
        String defaultName;
        GetDefaultGameName(&defaultName);
        name.Assign(defaultName);
        m_gameNameField->SetText(name.m_text);
    }
    m_joinNameField = CreateField(this, kLobbyPageJoin, 0x7c, 0x91, 0x86, 0x11, 0xc9);
    if (m_joinNameField) {
        m_joinNameField->SetMaxLength(kNameMaxLength);
        String defaultName;
        GetDefaultPlayerName(&defaultName);
        name.Assign(defaultName);
        m_joinNameField->SetText(name.m_text);
    }
    m_chatInput = CreateField(this, kLobbyPageSetup, 0xd9, 0x107, 0x142, 0x15, 0xcb);
    if (m_chatInput) {
        m_chatInput->SetMaxLength(kChatMaxLength);
        m_chatInput->Clear();
    }
    DibBitmap* chatBackdrop = FindOverlayBitmap(kLobbyPageSetup);
    HMODULE module = CoolAppGetResourceModule(m_bitmapSlot);
    m_chatLog = NewOrNull<TextScrollField>(module, 0x8f, 0x126, 0x18c, 0x47, this, chatBackdrop, 0);
    if (m_chatLog) {
        m_chatLog->m_scrollBarId = kScrollBarId;
        m_chatLog->m_scrollBarSlot = static_cast<uint16_t>(m_bitmapSlot);
        m_chatLog->Create(0);
        m_chatLog->m_textColor = kTextColor;
        InvalidateRect(m_chatLog->m_hwnd, nullptr, FALSE);
    }
    for (const ButtonSpec& spec : kButtons) {
        AddButton(spec.x, spec.y, spec.normalId, spec.pressedId, spec.childId, spec.tooltipId, spec.labelStringId,
                  spec.fontSlot, spec.color, spec.pressedColor, spec.repeatRate, spec.pixelHit);
    }
    for (size_t index = 0; index < sizeof(kButtonChildIds) / sizeof(kButtonChildIds[0]); ++index)
        this->*kButtonMembers[index] = static_cast<CoolStrobeButton*>(FindChild(kButtonChildIds[index]));
    if (AllButtonsExist(this)) {
        for (const StateBitmap& bitmap : kStateBitmaps) (this->*bitmap.button)->LoadStateBitmap(bitmap.state, bitmap.bitmapId);
    }
    const int host = m_isHost;
    AppSetUnknown138(host);
    m_isHost = host;
    if (m_hostModeButton && m_joinModeButton) SetCheckedPair(m_hostModeButton, m_joinModeButton, host != 0);
    SetGameType(m_gameType, 1, 0);
    SetSide(m_side, 1, 0);
    SetGalaxySize(m_galaxySize, 1, 0);
    if (CommLaunchedFromLobby() == 0) {
        ReturnToProviderPage();
    } else {
        ShowSetupPage();
    }
    m_ready = 1;
}

// ---- the lists --------------------------------------------------------------------------------------------------

void MultiPlayerWindow::FillProviderList() {
    if (!m_providerList) return;
    DeleteProviderData(m_providerList);
    m_providerList->RemoveAll();
    ServiceProviderList* providers = CommEnumerateServiceProviders();
    for (auto* node = static_cast<ServiceProviderInfo*>(providers->First()); node;
         node = static_cast<ServiceProviderInfo*>(node->m_successor)) {
        auto* copy = NewOrNull<ServiceProviderInfo>(*node);
        m_providerList->AddItem(0, 0, 0, node->m_name.m_text, 1, reinterpret_cast<uint32_t>(copy));
    }
    if (CoolListItem* first = m_providerList->FirstItem()) m_providerList->SelectById(first->m_id, 1);
}

int MultiPlayerWindow::RefreshSessionList() {
    int status = kNotCreated;
    if (!m_sessionList || m_selectedSession) return status;
    SessionList* sessions = CommGetSessionList(0, &status);
    if (status != 0) {
        ReturnToProviderPage();
        return status;
    }
    if (SessionListUpToDate(sessions) == 0) return status;
    DeleteSessionData(m_sessionList);
    m_sessionList->RemoveAll();
    for (auto* node = static_cast<SessionInfo*>(sessions->First()); node;
         node = static_cast<SessionInfo*>(node->m_successor)) {
        auto* copy = NewOrNull<SessionInfo>(*node);
        m_sessionList->AddItem(0, 0, 0, node->m_name.m_text, 1, reinterpret_cast<uint32_t>(copy));
    }
    if (CoolListItem* first = m_sessionList->FirstItem()) m_sessionList->SelectById(first->m_id, 1);
    if (m_nextButton) {
        if (!m_sessionList->FirstItem()) {
            m_nextButton->Disable();
        } else {
            m_nextButton->Enable();
        }
    }
    return status;
}

int MultiPlayerWindow::SessionListUpToDate(SessionList* sessions) {
    int differs = 0;
    if (!sessions || !m_sessionList) return differs;
    if (static_cast<uint32_t>(sessions->Count()) != static_cast<uint32_t>(m_sessionList->m_items->m_count)) return 1;
    for (auto* node = static_cast<SessionInfo*>(sessions->First()); node && !differs;
         node = static_cast<SessionInfo*>(node->m_successor)) {
        int found = 0;
        for (CoolListItem* item = m_sessionList->FirstItem(); item && !found; item = item->Next()) {
            const auto* known = reinterpret_cast<const SessionInfo*>(item->m_userData);
            found = SameGuid(known->m_instance, node->m_instance);
        }
        differs = !found;
    }
    return differs;
}

void MultiPlayerWindow::RememberSelectedProvider() {
    if (m_selectedProvider) {
        delete m_selectedProvider;
        m_selectedProvider = nullptr;
    }
    if (!m_providerList) return;
    CoolListItemList selection(1);
    m_providerList->CopySelectionTo(&selection);
    if (auto* item = static_cast<CoolListItem*>(selection.m_head)) {
        const auto* source = reinterpret_cast<const ServiceProviderInfo*>(item->m_userData);
        m_selectedProvider = NewOrNull<ServiceProviderInfo>(*source);
    }
}

void MultiPlayerWindow::RememberSelectedSession() {
    if (m_selectedSession) {
        delete m_selectedSession;
        m_selectedSession = nullptr;
    }
    if (!m_sessionList) return;
    CoolListItemList selection(1);
    m_sessionList->CopySelectionTo(&selection);
    if (auto* item = static_cast<CoolListItem*>(selection.m_head)) {
        const auto* source = reinterpret_cast<const SessionInfo*>(item->m_userData);
        m_selectedSession = NewOrNull<SessionInfo>(*source);
    }
}

// ---- buttons ----------------------------------------------------------------------------------------------------

void MultiPlayerWindow::HideControls() {
    if (AllButtonsExist(this)) {
        m_backButton->Enable();
        m_nextButton->Enable();
        m_nextButton->Hide();
        m_startButton->Hide();
        m_joinModeButton->Hide();
        m_hostModeButton->Hide();
        m_sideButtonA->Hide();
        m_sideButtonB->Hide();
        m_sizeButton1->Hide();
        m_sizeButton2->Hide();
        m_sizeButton3->Hide();
        m_loadButton->Hide();
        m_typeButton1->Hide();
        m_typeButton2->Hide();
    }
    if (m_gameNameField) m_gameNameField->Hide();
    if (m_hostNameField) m_hostNameField->Hide();
    if (m_joinNameField) m_joinNameField->Hide();
    if (m_chatInput) m_chatInput->Hide();
    if (m_chatLog) m_chatLog->Hide();
    if (m_providerList) m_providerList->Hide();
    if (m_sessionList) m_sessionList->Hide();
}

void MultiPlayerWindow::SetGameType(int type, int force, int apply) {
    if (m_gameType == type && force == 0) return;
    CoolStrobeButton* first = m_typeButton1;
    m_gameType = type;
    if (first && m_typeButton2) {
        if (type == 1) {
            SetCheckedPair(first, m_typeButton2, true);
        } else if (type == 2) {
            SetCheckedPair(first, m_typeButton2, false);
        }
    }
    if (apply != 0) {
        AnnounceSettings();
        SendSettings(0);
    }
}

void MultiPlayerWindow::SetSide(int side, int force, int apply) {
    if (m_side == side && force == 0) return;
    CoolStrobeButton* first = m_sideButtonA;
    m_side = side;
    if (first && m_sideButtonB) {
        if (side == 1) {
            first->ClearStateFlags(kButtonChecked);
            m_sideButtonB->SetStateFlags(kButtonChecked);
        } else {
            first->SetStateFlags(kButtonChecked);
            m_sideButtonB->ClearStateFlags(kButtonChecked);
        }
    }
    if (apply != 0) {
        AnnounceSettings();
        SendSettings(0);
    }
}

void MultiPlayerWindow::SetGalaxySize(int size, int force, int apply) {
    if (size == m_galaxySize && force == 0) return;
    CoolStrobeButton* first = m_sizeButton1;
    m_galaxySize = size;
    if (first && m_sizeButton2 && m_sizeButton3) {
        if (size == 1) {
            first->SetStateFlags(kButtonChecked);
            m_sizeButton2->ClearStateFlags(kButtonChecked);
            m_sizeButton3->ClearStateFlags(kButtonChecked);
        } else if (size == 2) {
            first->ClearStateFlags(kButtonChecked);
            m_sizeButton2->SetStateFlags(kButtonChecked);
            m_sizeButton3->ClearStateFlags(kButtonChecked);
        } else if (size == 3) {
            first->ClearStateFlags(kButtonChecked);
            m_sizeButton2->ClearStateFlags(kButtonChecked);
            m_sizeButton3->SetStateFlags(kButtonChecked);
        }
    }
    if (apply != 0) {
        AnnounceSettings();
        SendSettings(0);
    }
}

void MultiPlayerWindow::ShowSetupButtons(int show) {
    if (!AllButtonsExist(this)) return;
    if (show != 0) {
        m_sideButtonA->Enable();
        m_sideButtonB->Enable();
        m_sizeButton1->Enable();
        m_sizeButton2->Enable();
        m_sizeButton3->Enable();
        m_loadButton->Disable();
        m_typeButton1->Enable();
        m_typeButton2->Enable();
        m_startButton->Disable();
        return;
    }
    m_sideButtonA->Disable();
    m_sideButtonB->Disable();
    m_sizeButton1->Disable();
    m_sizeButton2->Disable();
    m_sizeButton3->Disable();
    m_loadButton->Disable();
    m_typeButton1->Disable();
    m_typeButton2->Disable();
    m_startButton->Disable();
}

void MultiPlayerWindow::UpdateStartButtons() {
    if (!m_loadButton || !m_startButton) return;
    const uint32_t playerCount = CommGetPlayerCount();
    if (m_isHost == 0) return;
    if (playerCount == 2) {
        const int loadDisabled = m_loadButton->HasStateFlags(kButtonDisabled);
        const int startDisabled = m_startButton->HasStateFlags(kButtonDisabled);
        if (!loadDisabled && !startDisabled) return;
        bool matched = false;
        for (int slot = 1; slot <= 6 && !matched; ++slot) {
            if (AppGetSavedGameType(slot, 1) != 2) continue;
            const uint32_t saved = GetSlotSaveId(slot);
            const uint32_t announced = g_lobbySlotTable[slot];
            if (saved != 0 && announced != 0) matched = saved == announced;
        }
        if (matched) m_loadButton->Enable();
        m_startButton->Enable();
        return;
    }
    const int loadDisabled = m_loadButton->HasStateFlags(kButtonDisabled);
    const int startDisabled = m_startButton->HasStateFlags(kButtonDisabled);
    if (!loadDisabled || !startDisabled) {
        m_loadButton->Disable();
        m_startButton->Disable();
    }
}

// ---- the pages --------------------------------------------------------------------------------------------------

void MultiPlayerWindow::ReturnToProviderPage() {
    m_closing = 1;
    HideControls();
    KillTimer(m_hwnd, kLobbyTimerSessions);
    CommTrace(kCancelTrace);
    CommSendPlayerQuit();
    CommTrace(kSetupTrace);
    CommCleanUp();
    OnCreate1();
    m_page = kLobbyPageProvider;
    SetPaintBackgroundById(kLobbyPageProvider);
    if (!AllButtonsExist(this)) return;
    m_backButton->Disable();
    m_nextButton->Show(kShowCommand);
    m_joinModeButton->Show(kShowCommand);
    m_hostModeButton->Show(kShowCommand);
    if (m_providerList) m_providerList->Show(kShowCommand);
    if (!m_providerList->FirstItem()) m_nextButton->Disable();
}

void MultiPlayerWindow::ShowJoinPage() {
    if (CommOpenProvider(m_selectedProvider) != 0) {
        AppShowNoticeDialog(kNoticeProviderFailed, 0x1208, 0x1207, 0x190f, 500, 0x50);
        ReturnToProviderPage();
        return;
    }
    if (m_selectedSession) {
        delete m_selectedSession;
        m_selectedSession = nullptr;
    }
    SetCursor(LoadCursorA(nullptr, IDC_WAIT));
    const int status = RefreshSessionList();
    if (status != 0) {
        SetCursor(LoadCursorA(nullptr, IDC_ARROW));
        ReturnToProviderPage();
        return;
    }
    SetTimer(m_hwnd, kLobbyTimerSessions, kSessionTimerMs, nullptr);
    SetCursor(LoadCursorA(nullptr, IDC_ARROW));
    HideControls();
    m_page = kLobbyPageJoin;
    SetPaintBackgroundById(kLobbyPageJoin);
    if (m_nextButton) {
        if (!m_sessionList->FirstItem()) m_nextButton->Disable();
        m_nextButton->Show(kShowCommand);
    }
    if (m_sessionList) m_sessionList->Show(kShowCommand);
    if (m_joinNameField) m_joinNameField->Show(kShowCommand);
    if (m_joinNameField) SetFocus(m_joinNameField->m_hwnd);
}

void MultiPlayerWindow::ShowHostPage() {
    if (CommOpenProvider(m_selectedProvider) != 0) {
        AppShowNoticeDialog(kNoticeProviderFailed, 0x1208, 0x1207, 0x190f, 500, 0x50);
        ReturnToProviderPage();
        return;
    }
    HideControls();
    m_page = kLobbyPageHost;
    SetPaintBackgroundById(kLobbyPageHost);
    if (m_nextButton) m_nextButton->Show(kShowCommand);
    if (m_gameNameField) m_gameNameField->Show(kShowCommand);
    if (m_gameNameField) SetFocus(m_gameNameField->m_hwnd);
    if (m_hostNameField) m_hostNameField->Show(kShowCommand);
    if (m_hostNameField) SetFocus(m_hostNameField->m_hwnd);
}

void MultiPlayerWindow::ShowSetupPage() {
    g_lobbyFlags = 0;
    int result = 0;
    KillTimer(m_hwnd, kLobbyTimerSessions);
    if (CommLaunchedFromLobby() == 0) {
        // BUG(original): with a missing name field the original reads an uninitialised local instead.
        String* nameSource = nullptr;
        if (m_isHost != 0) {
            String* gameName = m_gameNameField ? &m_gameNameField->m_text : nullptr;
            if (gameName->Length() == 0) {
                String defaultName;
                GetDefaultGameName(&defaultName);
                gameName->Assign(defaultName);
            }
            result = CommHostNewSession(gameName->m_text);
            nameSource = m_hostNameField ? &m_hostNameField->m_text : nullptr;
        } else {
            RememberSelectedSession();
            result = CommJoinSession(m_selectedSession);
            nameSource = m_joinNameField ? &m_joinNameField->m_text : nullptr;
        }
        CommSetPlayerName(*nameSource);
    }
    if (result != 0) {
        if (m_isHost == 0) {
            ShowJoinPage();
        } else {
            ShowHostPage();
        }
        return;
    }
    if (m_isHost != 0) {
        ShowSetupButtons(1);
        UpdateStartButtons();
    } else {
        ShowSetupButtons(0);
        {
            String marker(kStatusMarker);
            PrintChatLine(&marker);
        }
        MpSlotTableMsg* slots = NewOrNull<MpSlotTableMsg>();
        if (slots) {
            for (int slot = 1; slot < 6; ++slot) {
                uint32_t value = 0;
                if (slot < 7) {
                    SavedGameInfo header = AppReadSaveHeader(slot, 1);
                    value = static_cast<uint32_t>(header.m_unknown24);
                }
                slots->m_values[slot - 1] = static_cast<int>(value);
            }
            SendToPeer(slots);
        }
    }
    HideControls();
    m_page = kLobbyPageSetup;
    SetPaintBackgroundById(kLobbyPageSetup);
    if (AllButtonsExist(this)) {
        if (CommLaunchedFromLobby() == 0) {
            m_backButton->Enable();
        } else {
            m_backButton->Disable();
        }
        m_startButton->Show(kShowCommand);
        m_sideButtonA->Show(kShowCommand);
        m_sideButtonB->Show(kShowCommand);
        m_sizeButton1->Show(kShowCommand);
        m_sizeButton2->Show(kShowCommand);
        m_sizeButton3->Show(kShowCommand);
        m_loadButton->Show(kShowCommand);
        m_typeButton1->Show(kShowCommand);
        m_typeButton2->Show(kShowCommand);
    }
    if (m_chatInput) {
        m_chatInput->Show(kShowCommand);
        SetFocus(m_chatInput->m_hwnd);
    }
    if (m_chatLog) {
        m_chatLog->ClearText();
        m_chatLog->Show(kShowCommand);
    }
    SetTimer(m_hwnd, kLobbyTimerNetwork, kNetworkTimerMs, nullptr);
}

// ---- default names ----------------------------------------------------------------------------------------------

String* MultiPlayerWindow::GetDefaultPlayerName(String* out) {
    ResourceStringId prefix = {0x110c, static_cast<int16_t>(m_stringSlot)};
    String name;
    name.SetFromResource(&prefix);
    char buffer[256];
    DWORD size = 0xff;
    if (GetUserNameA(buffer, &size)) name = buffer;
    new (out) String(name);
    return out;
}

String* MultiPlayerWindow::GetDefaultGameName(String* out) {
    ResourceStringId prefix = {0x1110, static_cast<int16_t>(m_stringSlot)};
    String name;
    name.SetFromResource(&prefix);
    char buffer[256];
    DWORD size = 0xff;
    if (GetComputerNameA(buffer, &size)) name = buffer;
    new (out) String(name);
    return out;
}
