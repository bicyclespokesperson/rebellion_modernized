#include <string>
#include "strategy_ui/message_index_window.h"

#include <new>

#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/containers.h"
#include "core/game_string.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/advisor_notices.h"
#include "strategy_ui/encyclopedia_window.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_tab_control.h"
#include "ui/message_list_item.h"
#include "ui/text_scroll_field.h"

extern const char *g_encyBitmapDllName;  // 0x6a8750 "encybmap.dll"

extern int g_modalReportId;       // 0x6b2904 the notice a report window was opened for
extern int g_modalReportMode;     // 0x6b2908 the mode and tab the message index was in
extern int g_modalReportTab;      // 0x6b290c
extern int g_modalReportOwner;    // 0x6b2910 the child id of the report window (0x1c), 0 for none
extern int g_modalReportPending;  // 0x6b2914 the report was closed with its notice: reopen the message index on it

namespace {

constexpr uint32_t kStringSlot = 0;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kEncyclopediaSlot = 5;
constexpr uint32_t kHelperBitmapKind = 10;
constexpr uint32_t kResourceBitmapKind = 2;
constexpr int16_t kStringGroup = 2;
constexpr int kAlliance = 1;
constexpr uint16_t kClickSound = 0x260;
constexpr int kShow = 5;
constexpr uint32_t kHiddenStyle = 0x10000000;
constexpr int kSoundModuleA = 7;
constexpr int kSoundModuleB = 9;
constexpr int kNoticeFontRead = 10;
constexpr int kNoticeFontUnread = 0xd;

constexpr uint32_t kBackgroundList = 0x32;
constexpr uint32_t kBackgroundMessage = 0x33;
constexpr uint32_t kBackgroundCompose = 0x34;

constexpr uint32_t kListId = 0x6e;
constexpr uint32_t kTabsId = 0x78;
constexpr uint32_t kBackButtonId = 0x65;
constexpr uint32_t kToggleButtonId = 0x66;
constexpr uint32_t kOpenButtonId = 0x67;
constexpr uint32_t kComposeButtonId = 0x68;
constexpr uint32_t kSelectAllButtonId = 0x90;
constexpr uint32_t kDeleteButtonId = 0x91;
constexpr uint32_t kNextButtonId = 0x96;
constexpr uint32_t kSendButtonId = 0x97;
constexpr uint32_t kAcceptButtonId = 0x98;
constexpr uint32_t kDeclineButtonId = 0x99;
constexpr uint32_t kPreviousButtonId = 0x9a;

constexpr uint32_t kTabAll = 0x79;

constexpr uint32_t kCategoryAll = 0;

ResourceStringId StringId(uint16_t id) { return ResourceStringId{static_cast<int16_t>(id), kStringGroup}; }

DibBitmap* LoadBitmap(HMODULE module, uint32_t id, uint32_t kind) { return NewOrNull<DibBitmap>(module, id, kind); }

DibBitmap* CopyOf(DibBitmap* source) { return NewOrNull<DibBitmap>(source->Width(), source->Height(), source); }

void DeleteBitmap(DibBitmap* bitmap) {
    if (bitmap) delete bitmap;
}

bool IsAlliance(GalaxyWindow* galaxy) { return galaxy->m_side == kAlliance; }

CoolStrobeButton* AsButton(CoolWindow* window) { return static_cast<CoolStrobeButton*>(window); }

// The picture ids a button that opens a message by class shows: 3 and 5 go to the object, 4 to the encyclopedia.
struct OpenButtonLook {
    uint32_t normal;
    uint32_t pressed;
    uint16_t tooltip;
};

OpenButtonLook ObjectLook(bool alliance) {
    return alliance ? OpenButtonLook{0x2916, 0x2917, 0x8030} : OpenButtonLook{0x2918, 0x2919, 0x8030};
}

OpenButtonLook EncyclopediaLook(bool alliance) {
    return alliance ? OpenButtonLook{0x2d20, 0x2d21, 0x8031} : OpenButtonLook{0x2d47, 0x2d48, 0x8031};
}

}  // namespace

// ---- Construction ---------------------------------------------------------------------------------------------

MessageIndexWindow::MessageIndexWindow(HINSTANCE instance, CoolWindow* parent, uint32_t childId, int mode, int tab)
    : CoolGameWindow(instance, 0, 0, 0x1d6, 0x14b, parent, kStringSlot, kBitmapSlot, 0, childId, 0, 1, 0, 0),
      m_soundA(0, 0, 0, 0, 0, 0),
      m_soundB(0, 0, 0, 0, 0, 0),
      m_title(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u),
      m_tabLabel(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u) {
    m_style = 0x46000000;
    m_galaxy = static_cast<GalaxyWindow*>(parent);
    m_unk15c = nullptr;
    m_tabs = nullptr;
    m_list = nullptr;
    m_items = nullptr;
    m_mode = mode;
    m_tab = tab;
    m_current = nullptr;
    m_pictureBackdrop = nullptr;
    m_text = nullptr;
    m_encyclopediaModule = FindResourceModule(kEncyclopediaSlot);
    if (!m_encyclopediaModule) {
        char* dllLocation = GetDllLocation();
        m_encyclopediaModule = LoadResourceModule(dllLocation, g_encyBitmapDllName, kEncyclopediaSlot);
        if (dllLocation) operator delete(dllLocation);
    }
    m_artModule = CoolAppGetResourceModule(kBitmapSlot);
}

MessageIndexWindow::~MessageIndexWindow() {
    if (m_unk15c) delete m_unk15c;
    if (m_tabs) delete m_tabs;
    if (m_pictureBackdrop) delete m_pictureBackdrop;
    if (m_items) {
        for (LinkNode* row = m_items->m_head; row; row = row->m_next) static_cast<MessageListItem*>(row)->m_visible = 1;
        m_items->Clear();
        if (m_items) delete m_items;
    }
    if (m_list) delete m_list;
    if (m_text) delete m_text;
    UiSetHumanDispatchFlag(0);

}

// ---- OnCreate1 ------------------------------------------------------------------------------------------------

void MessageIndexWindow::OnCreate1() {
    HINSTANCE instance = m_instance;
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right,
             m_galaxy->m_panelArea.bottom);

    DibBitmap* titleBar = LoadBitmap(art, 0x2a46, kHelperBitmapKind);
    DibBitmap* frame;
    DibBitmap* rightEdge;
    if (IsAlliance(m_galaxy)) {
        frame = LoadBitmap(art, 0x285f, kHelperBitmapKind);
        rightEdge = LoadBitmap(art, 0x2a44, kHelperBitmapKind);
    } else {
        frame = LoadBitmap(art, 0x2860, kHelperBitmapKind);
        rightEdge = LoadBitmap(art, 0x2a45, kHelperBitmapKind);
    }
    DibBitmap* listBackground = CopyOf(frame);
    titleBar->Blit(listBackground, 0xc, 0xd, 0, 0, 0, 0);
    frame->DrawKeyed(listBackground, 0, 0);
    rightEdge->DrawKeyed(listBackground, 0x19c, 0);
    SetPaintBackground(listBackground, kBackgroundList);
    DeleteBitmap(titleBar);

    DibBitmap* messageTitleBar = LoadBitmap(art, 0x2a47, kHelperBitmapKind);
    DibBitmap* messageBackground = CopyOf(frame);
    messageTitleBar->Blit(messageBackground, 0xc, 0xd, 0, 0, 0, 0);
    frame->DrawKeyed(messageBackground, 0, 0);
    rightEdge->DrawKeyed(messageBackground, 0x19c, 0);
    DibBitmap* pictureFrame = LoadBitmap(art, 0x2a4b, kResourceBitmapKind);
    m_pictureBackdrop = CopyOf(pictureFrame);
    messageBackground->Blit(m_pictureBackdrop, 0, 0, 0x16, 0x6c, 0, 0);
    DeleteBitmap(pictureFrame);
    AddOverlay(messageBackground, kBackgroundMessage);
    DeleteBitmap(messageTitleBar);

    DibBitmap* composeBackground = CopyOf(frame);
    DibBitmap* composePanel = LoadBitmap(art, 0x2ab4, kResourceBitmapKind);
    composePanel->Blit(composeBackground, 0xc, 0xe8, 0, 0, 0, 0);
    DeleteBitmap(composePanel);
    DibBitmap* composeTitle = LoadBitmap(art, 0x2ab5, kResourceBitmapKind);
    composeTitle->Blit(composeBackground, 0xc, 0xe, 0, 0, 0, 0);
    DeleteBitmap(composeTitle);
    frame->DrawKeyed(composeBackground, 0, 0);
    rightEdge->DrawKeyed(composeBackground, 0x19c, 0);
    DeleteBitmap(rightEdge);
    DeleteBitmap(frame);
    AddOverlay(composeBackground, kBackgroundCompose);

    m_title.m_width = 0x18b;
    m_title.m_layoutFlags = 1;
    m_title.SetColor(0xffffff, nullptr);
    m_tabLabel.MoveTo(0x23, 0x5a);
    m_tabLabel.SetFont(4);
    m_tabLabel.SetColor(0xffffff, nullptr);
    m_tabLabel.m_drawFlags = 0;

    m_items = NewOrNull<MessageItemList>(4);
    m_list = NewOrNull<CoolDragList>(instance, 0x19, 0x6c, 0x170, 0xc2, this, GetPaintBackgroundIfOwned(), kListId, 0x15a,
                                     0x15, 0, 0, 0, 0, 10, 1, nullptr);
    if (m_list) {
        m_list->m_style &= ~kHiddenStyle;
        m_list->Create(0);
        m_list->m_options.m_value |= kDragListNoDrag;
        CoolDragList* list = m_list;
        list->CreateVerticalScrollBar(m_instance, list, 7, 0x299d, list->m_clientHeight, 0, list->m_clientHeight, 0,
                                      nullptr);
        m_list->m_textFormat = 0;
        m_list->SetItemList(m_items);
    }

    m_tabs = NewOrNull<CoolTabControl>(instance, 0x16, 0x2e, 0x17a, 0x29, kTabsId, this, GetPaintBackgroundIfOwned());
    if (m_tabs) {
        m_tabs->Create(0);
        struct TabSpec {
            int x;
            int width;
            uint32_t id;
            uint32_t allianceNormal;
            uint32_t alliancePressed;
            uint32_t empireNormal;
            uint32_t empirePressed;
            uint16_t tooltip;
        };
        static const TabSpec tabs[] = {
            {0x00, 0x24, 0x79, 0x2a4e, 0x2a4f, 0x2a4e, 0x2a4f, 0x8010}, {0x26, 0x24, 0x7a, 0x2a50, 0x2a51, 0x2a52, 0x2a53, 0x8011},
            {0x4c, 0x24, 0x7b, 0x2a58, 0x2a59, 0x2a56, 0x2a57, 0x8017}, {0x72, 0x24, 0x7c, 0x2a5a, 0x2a5b, 0x2a5c, 0x2a5d, 0x8014},
            {0x97, 0x24, 0x7d, 0x2a54, 0x2a55, 0x2a54, 0x2a55, 0x8012}, {0xbd, 0x24, 0x7e, 0x2a64, 0x2a65, 0x2a64, 0x2a65, 0x8013},
            {0xe3, 0x24, 0x7f, 0x2a68, 0x2a69, 0x2a68, 0x2a69, 0x8016}, {0x107, 0x24, 0x80, 0x2a66, 0x2a67, 0x2a66, 0x2a67, 0x8018},
            {0x12d, 0x24, 0x81, 0x2a62, 0x2a63, 0x2a62, 0x2a63, 0x8015}, {0x152, 0x25, 0x82, 0x2a5e, 0x2a5f, 0x2a60, 0x2a61, 0x8032},
        };
        const bool alliance = IsAlliance(m_galaxy);
        for (const TabSpec& spec : tabs) {
            const uint32_t normal = alliance ? spec.allianceNormal : spec.empireNormal;
            const uint32_t pressed = alliance ? spec.alliancePressed : spec.empirePressed;
            CoolStrobeButton* tab = NewOrNull<CoolStrobeButton>(art, spec.x, 0, spec.width, 0x29, m_tabs, spec.id, normal,
                                                                pressed, m_tabs->m_background, 0, 0);
            tab->Create(0);
            const ResourceStringId tip = StringId(spec.tooltip);
            AddToolFromString(tab, &tip);
            tab->SetStateFlags(0x80);
            tab->ClearStateFlags(0x10);
            if (spec.id == kTabAll) {
                m_tabs->AddTab(tab, 1, 0);
            } else {
                m_tabs->AddTab(tab, 0, 1);
            }
        }
    }

    const bool alliance = IsAlliance(m_galaxy);
    struct ButtonGeometry {
        int x;
        int y;
        int width;
        int height;
    };
    const int buttonX = alliance ? 0x1a7 : 0x1aa;
    const int buttonWidth = alliance ? 0x20 : 0x2c;
    const int buttonHeight = alliance ? 0x1f : 0x29;

    CoolStrobeButton* tipButton = NewOrNull<CoolStrobeButton>(art, buttonX, alliance ? 0x19 : 0x15, buttonWidth,
                                                              buttonHeight, this, 0x28, alliance ? 0x2882 : 0x2888,
                                                              alliance ? 0x2883 : 0x2889, GetPaintBackgroundIfOwned(), 0, 0);
    if (tipButton) {
        tipButton->Create(0);
        tipButton->SetClickSound(kClickSound);
        const ResourceStringId tip = StringId(0x1954);
        AddToolFromString(tipButton, &tip);
        m_children.Insert(tipButton);
    }

    CoolStrobeButton* backButton = NewOrNull<CoolStrobeButton>(art, buttonX, alliance ? 0x5d : 0x59, buttonWidth, buttonHeight,
                                                               this, kBackButtonId, alliance ? 0x2884 : 0x288a,
                                                               alliance ? 0x2885 : 0x288b, GetPaintBackgroundIfOwned(), 0, 0);
    backButton->Create(0);
    backButton->SetClickSound(kClickSound);
    {
        const ResourceStringId tip = StringId(0x8020);
        AddToolFromString(backButton, &tip);
    }
    m_children.Insert(backButton);

    CoolStrobeButton* toggleButton = NewOrNull<CoolStrobeButton>(art, buttonX, alliance ? 0x93 : 0x94, buttonWidth, buttonHeight,
                                                                 this, kToggleButtonId, alliance ? 0x2a78 : 0x2a7a,
                                                                 alliance ? 0x2a79 : 0x2a7b, GetPaintBackgroundIfOwned(), 0, 0);
    toggleButton->Create(0);
    toggleButton->SetClickSound(kClickSound);
    {
        const ResourceStringId tip = StringId(0x8005);
        AddToolFromString(toggleButton, &tip);
    }
    m_children.Insert(toggleButton);

    CoolStrobeButton* openButton = NewOrNull<CoolStrobeButton>(art, buttonX, alliance ? 0xc9 : 0xcf, buttonWidth, buttonHeight,
                                                               this, kOpenButtonId, alliance ? 0x2a78 : 0x2a7a,
                                                               alliance ? 0x2a79 : 0x2a7b, GetPaintBackgroundIfOwned(), 0, 0);
    openButton->Create(0);
    openButton->SetClickSound(kClickSound);
    {
        const ResourceStringId tip = StringId(0x8030);
        AddToolFromString(openButton, &tip);
    }
    openButton->LoadStateBitmap(2, alliance ? 0x2acf : 0x2ad0);
    m_children.Insert(openButton);

    const uint32_t composeDisabled = alliance ? 0x2a7e : 0x2a81;
    CoolStrobeButton* composeButton = NewOrNull<CoolStrobeButton>(art, buttonX, alliance ? 0xff : 0x10a, buttonWidth,
                                                                  buttonHeight, this, kComposeButtonId,
                                                                  alliance ? 0x2a7c : 0x2a7f, alliance ? 0x2a7d : 0x2a80,
                                                                  GetPaintBackgroundIfOwned(), 0, 0);
    composeButton->Create(0);
    composeButton->SetClickSound(kClickSound);
    {
        const ResourceStringId tip = StringId(0x8022);
        AddToolFromString(composeButton, &tip);
    }
    m_children.Insert(composeButton);
    if (AppGetGameType() == 1) {
        composeButton->LoadStateBitmap(2, composeDisabled);
        composeButton->Disable();
    }

    struct HiddenButton {
        int x;
        int y;
        int width;
        int height;
        uint32_t id;
        uint32_t normal;
        uint32_t pressed;
        uint32_t disabled;
        uint16_t tooltip;
        bool useMessageBackground;
    };
    static const HiddenButton hiddenButtons[] = {
        {0x11a, 0x57, 0x38, 0x14, kSelectAllButtonId, 0x2a94, 0x2a95, 0, 0x8008, false},
        {0x154, 0x57, 0x38, 0x14, kDeleteButtonId, 0x2a96, 0x2a97, 0, 0x8009, false},
        {0x186, 0xf, 0x13, 0xf, kNextButtonId, 0x2aa7, 0x2aa8, 0x2aa9, 0, true},
        {0x16f, 0xf, 0x13, 0xf, kPreviousButtonId, 0x2ac4, 0x2ac5, 0x2ac6, 0, true},
        {0x163, 0xf4, 0x33, 0x23, kAcceptButtonId, 0x2ab6, 0x2ab7, 0, 0, true},
        {0x163, 0x119, 0x33, 0x23, kDeclineButtonId, 0x2aba, 0x2abb, 0, 0, true},
        {0x160, 0xff, 0x33, 0x34, kSendButtonId, 0x2aba, 0x2abb, 0, 0x8030, true},
    };
    for (const HiddenButton& spec : hiddenButtons) {
        DibBitmap* backdrop = spec.useMessageBackground ? FindOverlayBitmap(kBackgroundCompose) : GetPaintBackgroundIfOwned();
        CoolStrobeButton* button = NewOrNull<CoolStrobeButton>(art, spec.x, spec.y, spec.width, spec.height, this, spec.id,
                                                               spec.normal, spec.pressed, backdrop, 0, 0);
        if (!button) continue;
        if (spec.disabled != 0) button->LoadStateBitmap(2, spec.disabled);
        button->m_style &= ~kHiddenStyle;
        if (spec.id == kSelectAllButtonId || spec.id == kDeleteButtonId) button->SetStateFlags(0x80);
        button->Create(0);
        button->Hide();
        if (spec.tooltip != 0) {
            const ResourceStringId tip = StringId(spec.tooltip);
            AddToolFromString(button, &tip);
        }
        m_children.Insert(button);
    }

    m_text = NewOrNull<TextScrollField>(instance, 0x11, 0xea, 0x18b, 0x50, this, FindOverlayBitmap(kBackgroundCompose), 0);
    if (m_text) {
        m_text->m_style &= ~kHiddenStyle;
        m_text->m_childId = 0xa0;
        m_text->m_scrollBarSlot = 7;
        m_text->m_scrollBarId = 0x299d;
        m_text->SetTextFont(4);
        m_text->Create(0);
    }
    UiSetHumanDispatchFlag(1);
    const int initialMode = m_mode;
    const int initialTab = m_tab;
    m_mode = 0;
    m_tab = 0;
    SetMode(initialMode);
    m_tabs->SelectTab(initialTab, 0);
    SelectCategory(initialTab);
    UiSetHumanUiFlag4000(1);
    if (g_modalReportPending != 0) {
        MessageListItem* row = static_cast<MessageListItem*>(m_items->First());
        row->m_state.m_value &= ~1;
        for (; row; row = static_cast<MessageListItem*>(row->Next())) {
            if (static_cast<int>(row->m_record->m_id) == g_modalReportId) {
                row->m_state.m_value |= 1;
                SetFocus(m_list->m_hwnd);
                m_list->Refresh();
                break;
            }
        }
    }
    ModalReportReset(g_modalReportOwner);
}

// ---- Messages -------------------------------------------------------------------------------------------------

LRESULT MessageIndexWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_SETFOCUS) {
        SetFocus(m_list->m_hwnd);
    } else {
        if (message == 0x40d) {
            PostCloseRequest();
            return 0;
        }
        if (message != 0x483) return CoolGameWindow::WindowProc(message, wParam, lParam);
        if (wParam == 0xbbe && m_mode != kIndexModeList) {
            OnCommand(kBackButtonId, 0, 0);
            return 0;
        }
    }
    return 0;
}

void MessageIndexWindow::DrawOverlay(HDC dc) {
    if (m_mode == kIndexModeList) {
        m_title.MoveTo(0xf, 0x14);
        m_title.SetFont(0xb);
        const ResourceStringId heading = StringId(0x8019);
        m_title.SetTextFromResource(&heading);
        m_tabLabel.Draw(dc);
    } else if (m_mode == kIndexModeMessage) {
        if (m_current->m_image) m_current->m_image->DrawToDc(dc, 0x13, 0xf, 0, 0, 0, 0, 0, 0);
        m_title.m_drawFlags = 0;
        m_title.MoveTo(0x28, 0xd);
        m_title.SetFont(4);
        m_title.SetText(m_current->m_text.c_str());
        m_title.Draw(dc);
        return;
    } else if (m_mode == kIndexModeCompose) {
        m_title.m_drawFlags = 0;
        m_title.MoveTo(0x28, 0xd);
        m_title.SetFont(4);
        const ResourceStringId heading = StringId(0x8022);
        m_title.SetTextFromResource(&heading);
        m_title.Draw(dc);
        return;
    }
    m_title.Draw(dc);
}

void MessageIndexWindow::PostCloseRequest() {
    UiHumanEndHold();
    UiSetHumanUiFlag4000(0);
    if (m_tab != 0) UiClearHumanAlerts(m_filter);
    CoolGameWindow::PostCloseRequest();
}

void MessageIndexWindow::Refresh() {
    if (m_mode == kIndexModeList) {
        m_mode = 0;
        SetMode(kIndexModeList);
        return;
    }
    if (m_mode == kIndexModeMessage) {
        RebuildList(0);
        UpdateNavButtons();
    }
}

// ---- The list -------------------------------------------------------------------------------------------------

namespace {

int16_t CategoryPicture(uint32_t categories, bool alliance) {
    const int16_t bySide = alliance ? 0 : 1;
    if (categories & 1) return 0x2a9a + bySide;
    if (categories & 0x40) return 0x2ad3;
    if (categories & 8) return 0x2a9c + bySide;
    if (categories & 0x10) return 0x2a9e + bySide;
    if (categories & 0x20) return 0x2aa4;
    if (categories & 0x80) return 0x2ad4 + bySide;
    if (categories & 4) return 0x2ad6;
    if (categories & 0x100) return 0x2ad7;
    if (categories & 0x200) return 0x2ad8 + bySide;
    return 0;
}

void AddSelected(IdAvlTree* selected, uint32_t id) {
    void* memory = ::operator new(sizeof(IdNode));
    IdNode* node = memory ? new (memory) IdNode() : nullptr;
    node->SetId(id);
    selected->AddWithId(node);
}

}  // namespace

int MessageIndexWindow::RebuildList(int keepAllSelected) {
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    IdAvlTree selected;
    const bool alliance = IsAlliance(m_galaxy);
    DibBitmap rowFrame(art, (alliance ? 0u : 1u) + 0x2aa2, kHelperBitmapKind);
    if (keepAllSelected == 0) {
        if (m_current) AddSelected(&selected, m_current->m_id);
    } else {
        for (LinkNode* row = m_items->m_head; row; row = row->m_next) {
            CoolListItem* item = static_cast<CoolListItem*>(row);
            if (item->m_state.m_value & 1) AddSelected(&selected, item->m_id);
        }
    }
    m_items->Clear();
    m_list->SetItemSize(0x15a, 0x15);
    IdAvlTree* notices = UiGetHumanOrderTree();
    for (AvlNode* node = notices->Last(); node; node = node->Predecessor()) {
        MessageRecord* record = static_cast<MessageRecord*>(node);
        MessageListItem* item = NewOrNull<MessageListItem>(record);
        const int16_t picture = CategoryPicture(record->m_categories, alliance);
        if (picture != 0) {
            DibBitmap icon(art, static_cast<uint32_t>(picture), kHelperBitmapKind);
            DibBitmap* normal = CopyOf(&icon);
            DibBitmap* highlighted = CopyOf(&rowFrame);
            rowFrame.DrawKeyed(highlighted, 0, 0);
            icon.Blit(normal, 1, 1, 0, 0, 0, 0);
            icon.Blit(highlighted, 1, 1, 0, 0, 0, 0);
            item->SetImages(normal, highlighted);
        }
        item->m_textOffsetX = 0x20;
        item->m_textOffsetY = 1;
        item->m_fontSlot = (record->m_flags.m_value & 1) == 0 ? kNoticeFontUnread : kNoticeFontRead;
        m_items->Insert(item);
        if (selected.FindById(item->m_id)) m_list->SelectById(item->m_id, 1);
    }
    UpdateRowVisibility();
    if (keepAllSelected == 0) {
        IdNode* first = static_cast<IdNode*>(selected.First());
        m_current = first ? static_cast<MessageListItem*>(m_items->FindById(first->m_id)) : nullptr;
    }
    return 1;
}

void MessageIndexWindow::UpdateRowVisibility() {
    bool selectedVisible = false;
    for (LinkNode* node = m_items->m_head; node; node = node->m_next) {
        MessageListItem* item = static_cast<MessageListItem*>(node);
        if (!item->m_record) continue;
        const uint32_t categories = item->m_record->m_categories;
        item->m_visible = 0;
        if ((m_filter & categories) == 0 && (m_tab != static_cast<int>(kTabAll) || categories == 0x200)) {
            item->m_visible = 0;
            item->m_state.m_value &= ~1u;
        } else {
            item->m_visible = 1;
            selectedVisible = selectedVisible || (item->m_state.m_value & 1) != 0;
        }
    }
    CoolListItem* first = m_items->First();
    if (first && !selectedVisible) first->m_state.m_value |= 1;
}

void MessageIndexWindow::UpdateNavButtons() {
    if (m_mode == kIndexModeList || m_mode != kIndexModeMessage) return;
    CoolWindow* next = m_children.FindById(kNextButtonId);
    if (m_current->Next() == nullptr) {
        next->Disable();
    } else {
        next->Enable();
    }
    CoolWindow* previous = m_children.FindById(kPreviousButtonId);
    if (previous) {
        if (m_current->Previous() == nullptr) {
            previous->Disable();
        } else {
            previous->Enable();
        }
    }
}

DibBitmap* MessageIndexWindow::LoadPicture(uint32_t pictureId) {
    DibBitmap* picture = NewOrNull<DibBitmap>(m_artModule, pictureId, kResourceBitmapKind);
    if (picture && picture->IsLoaded() == 0) {
        delete picture;
        picture = LoadEncyclopediaPicture(pictureId, m_encyclopediaModule);
    }
    return picture;
}

void MessageIndexWindow::ShowPictures(uint32_t pictureA, uint32_t pictureB) {
    if (static_cast<int16_t>(pictureB) != 0 || static_cast<int16_t>(pictureA) != 0) {
        DibBitmap* composite = nullptr;
        DibBitmap* overlay = nullptr;
        if (static_cast<int16_t>(pictureB) != 0) {
            DibBitmap* base = LoadPicture(pictureB);
            if (base) {
                if (base->IsLoaded() != 0) {
                    composite = CopyOf(base);
                    base->Blit(composite, 0, 0, 0, 0, 0, 0);
                }
                delete base;
            }
        }
        if (static_cast<int16_t>(pictureA) != 0) {
            overlay = LoadPicture(pictureA);
            if (overlay && overlay->IsLoaded() != 0) {
                const int overlayWidth = overlay->Width();
                const int compositeWidth = composite->Width();
                const int overlayHeight = overlay->Height();
                const int compositeHeight = composite->Height();
                overlay->DrawKeyed(composite, (compositeWidth - overlayWidth) / 2, (compositeHeight - overlayHeight) / 2);
            }
        }
        if (composite && composite->IsLoaded() != 0) {
            composite->Blit(GetPaintBackgroundIfOwned(), 0xc, 0x21, 0, 0, 0, 0);
            delete composite;
            if (overlay) delete overlay;
        }
    }
    RECT page;
    SetRect(&page, 0, 0, 0x19c, 0x14b);
    InvalidateRect(m_hwnd, &page, 0);
}

void MessageIndexWindow::StopSounds() {
    if (m_soundA.m_soundId != 0) {
        SoundManager::Query(&m_soundA);
        if (m_soundA.m_playing) SoundManager::Stop(&m_soundA, 1);
    }
    if (m_soundB.m_soundId != 0) {
        SoundManager::Query(&m_soundB);
        if (m_soundB.m_playing) SoundManager::Stop(&m_soundB, 1);
    }
}

// ---- Modes and tabs -------------------------------------------------------------------------------------------

namespace {

void SetButtonPictures(CoolStrobeButton* button, uint32_t normal, uint32_t pressed) {
    button->LoadStateBitmap(0, normal);
    button->LoadStateBitmap(1, pressed);
}

}  // namespace

int MessageIndexWindow::SetMode(int request) {
    CoolAppGetResourceModule(kBitmapSlot);
    int changed = 0;
    int newMode;
    if (request == static_cast<int>(kBackButtonId)) {
        newMode = 2 - (m_mode != kIndexModeList ? 1 : 0);
    } else {
        newMode = request;
        if (request == static_cast<int>(kComposeButtonId)) newMode = kIndexModeCompose;
    }
    if (newMode == 0) return 0;
    const int previousMode = m_mode;
    if (newMode == previousMode) return 0;
    m_mode = newMode;
    const bool alliance = IsAlliance(m_galaxy);
    ChildWindowTree* children = &m_children;
    if (newMode == kIndexModeList) {
        m_list->m_options.m_value &= ~static_cast<int>(kDragListSingleSelect);
        SetPaintBackgroundById(kBackgroundList);
        m_list->Show(kShow);
        m_tabs->Show(kShow);
        children->FindById(kDeleteButtonId)->Show(kShow);
        children->FindById(kSelectAllButtonId)->Show(kShow);
        children->FindById(kNextButtonId)->Hide();
        children->FindById(kPreviousButtonId)->Hide();
        children->FindById(kAcceptButtonId)->Hide();
        children->FindById(kDeclineButtonId)->Hide();
        m_text->Hide();
        ChildWindowTree* tabs = &m_tabs->m_children;
        static const struct {
            uint32_t id;
            uint16_t tooltip;
        } tabTips[] = {{0x79, 0x8010}, {0x7a, 0x8011}, {0x7d, 0x8012}, {0x7e, 0x8013}, {0x7c, 0x8014}, {0x7f, 0x8016}};
        for (const auto& tip : tabTips) {
            const ResourceStringId text = StringId(tip.tooltip);
            UpdateToolFromString(tabs->FindById(tip.id), &text);
        }
        CoolWindow* lastTab = tabs->FindById(0x81);
        if (lastTab) {
            const ResourceStringId text = StringId(0x8015);
            UpdateToolFromString(lastTab, &text);
        }
        CoolStrobeButton* backButton = AsButton(FindChild(kBackButtonId));
        if (backButton) {
            const ResourceStringId text = StringId(0x8020);
            UpdateToolFromString(backButton, &text);
            SetButtonPictures(backButton, alliance ? 0x2a76 : 0x29f2, alliance ? 0x2a77 : 0x29f3);
            backButton->LoadStateBitmap(2, alliance ? 0x2ad1 : 0x2ad2);
            backButton->Show(kShow);
        }
        RebuildList(1);
        RECT header;
        SetRect(&header, 0xf, 0x14, 0x19a, 0x3c);
        InvalidateRect(m_hwnd, &header, 0);
        m_list->Refresh();
        const int tab = m_tab;
        m_tab = 0;
        SelectCategory(tab);
        SetFocus(m_list->m_hwnd);
        changed = 1;
    } else if (newMode == kIndexModeMessage) {
        CoolListItemList selection(1);
        m_list->CopySelectionTo(&selection);
        if (selection.m_head == nullptr) {
            m_mode = 0;
            OnCommand(kBackButtonId, 0, 0);
            m_mode = previousMode;
            return 0;
        }
        m_current = static_cast<MessageListItem*>(m_items->FindById(static_cast<CoolListItem*>(selection.m_head)->m_id));
        CoolStrobeButton* backButton = AsButton(FindChild(kBackButtonId));
        if (backButton) {
            const ResourceStringId text = StringId(0x8019);
            UpdateToolFromString(backButton, &text);
            SetButtonPictures(backButton, alliance ? 0x2884 : 0x288a, alliance ? 0x2885 : 0x288b);
            backButton->Show(kShow);
        }
        SetPaintBackgroundById(kBackgroundCompose);
        children->FindById(kDeleteButtonId)->Hide();
        children->FindById(kSelectAllButtonId)->Hide();
        children->FindById(kNextButtonId)->Show(kShow);
        CoolWindow* previous = children->FindById(kPreviousButtonId);
        if (previous) previous->Show(kShow);
        children->FindById(kAcceptButtonId)->Hide();
        children->FindById(kDeclineButtonId)->Hide();
        children->FindById(kSendButtonId)->Hide();
        m_text->SetEditable(0);
        m_text->m_lineWidth = 0x140;
        m_text->m_leftMargin = 0;
        m_text->Show(kShow);
        m_list->Hide();
        m_tabs->Hide();
        ShowMessage(m_current);
        FindChild(kBackButtonId)->Show(kShow);
        SetFocus(m_text->m_hwnd);
        changed = 1;
    } else if (newMode == kIndexModeCompose) {
        SetWindowPos(m_text->m_hwnd, nullptr, 0, 0, 0x140, m_text->m_height, SWP_NOMOVE | SWP_NOZORDER);
        SetPaintBackgroundById(kBackgroundCompose);
        ShowPictures(0, 0x411);
        CoolStrobeButton* backButton = AsButton(FindChild(kBackButtonId));
        if (backButton) {
            const ResourceStringId text = StringId(0x8019);
            UpdateToolFromString(backButton, &text);
            SetButtonPictures(backButton, alliance ? 0x2884 : 0x288a, alliance ? 0x2885 : 0x288b);
            backButton->Show(kShow);
        }
        children->FindById(kDeleteButtonId)->Hide();
        children->FindById(kSelectAllButtonId)->Hide();
        children->FindById(kNextButtonId)->Hide();
        children->FindById(kPreviousButtonId)->Hide();
        CoolStrobeButton* accept = AsButton(children->FindById(kAcceptButtonId));
        SetButtonPictures(accept, alliance ? 0x2ab6 : 0x2ab8, alliance ? 0x2ab7 : 0x2ab9);
        accept->Show(kShow);
        CoolStrobeButton* decline = AsButton(children->FindById(kDeclineButtonId));
        SetButtonPictures(decline, 0x2aba, 0x2abb);
        decline->Show(kShow);
        children->FindById(kSendButtonId)->Hide();
        std::string empty;
        if ((m_text->m_state & kTextEditable) == 0) m_text->SetEditable(1);
        m_text->m_lineWidth = 0x140;
        m_text->m_leftMargin = 0;
        m_text->m_text = empty;
        m_text->m_caret.m_index = 0;
        m_text->Reset();
        m_text->Show(kShow);
        SetFocus(m_text->m_hwnd);
        m_list->Hide();
        m_tabs->Hide();
        SetFocus(m_text->m_hwnd);
        changed = 1;
    }
    return changed;
}

void MessageIndexWindow::SelectCategory(int tab) {
    CoolAppGetResourceModule(kBitmapSlot);
    if (tab == m_tab) return;
    if (m_tab != 0) UiClearHumanAlerts(m_filter);
    m_tab = tab;
    DibBitmap* backdrop = FindOverlayBitmap(m_mode == kIndexModeList ? kBackgroundList : kBackgroundMessage);
    IdAvlTree unused;
    m_filter = 0;
    switch (tab) {
    case 0x79:
        UiHumanEndHold();
        break;
    case 0x7a:
        UiHumanEndHold();
        m_filter = 1;
        break;
    case 0x7b:
        UiHumanEndHold();
        m_filter = 0x80;
        break;
    case 0x7c:
        UiHumanEndHold();
        m_filter = 0x10;
        break;
    case 0x7d:
        UiHumanEndHold();
        m_filter = 4;
        break;
    case 0x7e:
        UiHumanEndHold();
        m_filter = 8;
        break;
    case 0x7f:
        UiHumanEndHold();
        m_filter = 0x40;
        break;
    case 0x80:
        UiHumanEndHold();
        m_filter = 0x100;
        break;
    case 0x81:
        UiHumanEndHold();
        m_filter = 0x20;
        break;
    case 0x82:
        UiHumanBeginHold();
        m_filter = 0x200;
    }
    const bool alliance = IsAlliance(m_galaxy);
    if (m_mode == kIndexModeList) {
        UpdateRowVisibility();
        CoolListItem* first = m_items->First();
        CoolWindow* backButton = FindChild(kBackButtonId);
        if (first == nullptr) {
            backButton->Disable();
        } else {
            backButton->Enable();
        }
        backButton->Show(kShow);
        CoolListItemList selection(1);
        m_list->CopySelectionTo(&selection);
        CoolStrobeButton* openButton = AsButton(FindChild(kOpenButtonId));
        if (openButton && (openButton->Disable(), selection.m_count == 1)) {
            CoolListItem* row = m_items->FindById(static_cast<CoolListItem*>(selection.m_head)->m_id);
            MessageRecord* record = static_cast<MessageListItem*>(row)->m_record;
            if (record) {
                const int recordClass = record->GetClass();
                bool known = true;
                OpenButtonLook look{};
                if (recordClass == 3 || (record->GetClass() == 5 && record->m_key.IsSet() != 0)) {
                    look = ObjectLook(alliance);
                } else if (record->GetClass() == 4) {
                    look = EncyclopediaLook(alliance);
                } else {
                    known = false;
                }
                if (known) {
                    SetButtonPictures(openButton, look.normal, look.pressed);
                    const ResourceStringId text = StringId(look.tooltip);
                    UpdateToolFromString(openButton, &text);
                    openButton->Enable();
                }
            }
        }
        const unsigned state = m_filter == 0 ? static_cast<unsigned>(UiGetHumanDefaultState()) : UiGetHumanStateValue(m_filter);
        CoolStrobeButton* toggleButton = AsButton(FindChild(kToggleButtonId));
        if (toggleButton) {
            toggleButton->Enable();
            if (((~state >> 2) & 1) == 0) {
                const ResourceStringId text = StringId(0x8006);
                UpdateToolFromString(toggleButton, &text);
                if (!alliance) {
                    SetButtonPictures(toggleButton, 0x2ac9, 0x2a7b);
                } else {
                    SetButtonPictures(toggleButton, 0x2ac7, 0x2a79);
                }
            } else {
                const ResourceStringId text = StringId(0x8005);
                UpdateToolFromString(toggleButton, &text);
                if (alliance) {
                    SetButtonPictures(toggleButton, 0x2a78, 0x2a79);
                } else {
                    SetButtonPictures(toggleButton, 0x2a7a, 0x2a7b);
                }
            }
        }
    }
    if (m_list->m_clientHeight != 0xc2) {
        RECT area;
        area.left = 0x16;
        area.top = 0x6c;
        area.right = m_pictureBackdrop->Width() + 0x16;
        area.bottom = m_pictureBackdrop->Height() + 0x6c;
        SetWindowPos(m_list->m_hwnd, nullptr, 0, 0, 0x170, 0xc2, SWP_NOMOVE | SWP_NOZORDER);
        m_list->SetBackground(backdrop, m_list->m_x, m_list->m_y);
        InvalidateRect(m_hwnd, &area, 0);
    }
    HDC dc = GetDC(m_hwnd);
    RECT label = m_tabLabel.m_rect;
    InvalidateRect(m_hwnd, &label, 0);
    const uint32_t selectedTab = m_tabs->m_selected ? m_tabs->m_selected->m_childId : 0;
    CoolStrobeButton* tabButton = AsButton(m_tabs->m_children.FindById(selectedTab));
    m_tabLabel.SetText(tabButton->GetTooltipText());
    m_tabLabel.Measure(dc);
    label = m_tabLabel.m_rect;
    InvalidateRect(m_hwnd, &label, 0);
    ReleaseDC(m_hwnd, dc);
    m_list->Refresh();
}

// ---- Showing one message --------------------------------------------------------------------------------------

void MessageIndexWindow::ShowMessage(MessageListItem* item) {
    if (!item) return;
    MessageRecord* record = item->m_record;
    item->m_fontSlot = kNoticeFontRead;
    if (record->GetKind() == 0xf) {
        EndPopup();
        g_modalReportId = record->m_id;
        g_modalReportMode = m_mode;
        g_modalReportTab = m_tab;
        g_modalReportOwner = 0x1c;
        g_modalReportPending = 0;
        m_galaxy->OpenType1cWindow(reinterpret_cast<LPARAM>(record));
        PostCloseRequest();
        return;
    }
    std::string text(record->m_text1);
    m_text->m_text = text;
    m_text->m_caret.m_index = 0;
    m_text->Reset();
    m_text->Show(kShow);
    UpdateNavButtons();
    ChildWindowTree* children = &m_children;
    children->FindById(kSendButtonId)->Hide();
    children->FindById(kAcceptButtonId)->Hide();
    CoolStrobeButton* openButton = AsButton(children->FindById(kOpenButtonId));
    if (openButton) openButton->Hide();
    CoolStrobeButton* decline = AsButton(children->FindById(kDeclineButtonId));
    decline->Hide();
    const bool alliance = IsAlliance(m_galaxy);
    SetWindowPos(m_text->m_hwnd, nullptr, 0, 0, record->GetClass() == 5 ? 0x140 : 0x18b, m_text->m_height,
                 SWP_NOMOVE | SWP_NOZORDER);
    const int recordClass = record->GetClass();
    if (recordClass == 3 || recordClass == 4) {
        const OpenButtonLook look = recordClass == 3 ? ObjectLook(alliance) : EncyclopediaLook(alliance);
        SetButtonPictures(openButton, look.normal, look.pressed);
        const ResourceStringId tip = StringId(look.tooltip);
        UpdateToolFromString(openButton, &tip);
        openButton->Show(kShow);
        openButton->Enable();
    } else if (recordClass == 5) {
        CoolStrobeButton* openButton = AsButton(children->FindById(kOpenButtonId));
        if (record->m_key.IsSet() == 0) {
            if (openButton) openButton->Disable();
        } else {
            const OpenButtonLook look = ObjectLook(alliance);
            SetButtonPictures(openButton, look.normal, look.pressed);
            const ResourceStringId tip = StringId(look.tooltip);
            UpdateToolFromString(openButton, &tip);
            openButton->Show(kShow);
            openButton->Enable();
        }
        CoolStrobeButton* accept = AsButton(children->FindById(kAcceptButtonId));
        SetButtonPictures(accept, 0x2aae, 0x2aaf);
        accept->Show(kShow);
        CoolStrobeButton* declineButton = AsButton(children->FindById(kDeclineButtonId));
        SetButtonPictures(declineButton, 0x2ab1, 0x2ab2);
        declineButton->Show(kShow);
    }
    ShowPictures(record->m_picture[0], record->m_picture[1]);
    const uint16_t soundA = record->m_sound[0];
    const uint16_t soundB = record->m_sound[1];
    StopSounds();
    m_soundA.m_soundId = soundA;
    if (soundA != 0) {
        m_soundA.m_moduleIndex = kSoundModuleA;
        m_soundA.m_kind = 1;
        SoundManager::Play(&m_soundA);
    }
    m_soundB.m_soundId = soundB;
    if (soundB != 0) {
        m_soundB.m_moduleIndex = kSoundModuleB;
        m_soundB.m_kind = 1;
        SoundManager::Play(&m_soundB);
    }
}

// ---- Commands -------------------------------------------------------------------------------------------------

namespace {

bool OpensObject(MessageRecord* record) {
    const int recordClass = record->GetClass();
    return recordClass == 3 || (record->GetClass() == 5 && record->m_key.IsSet() != 0);
}

}  // namespace

void MessageIndexWindow::OnCommand(int a, int b, int c) {
    const uint32_t command = static_cast<uint32_t>(a) & 0xffff;
    ChildWindowTree* children = &m_children;
    auto markCurrentRead = [this]() {
        m_current->m_record->m_flags.m_value = 1;
        m_current->m_fontSlot = kNoticeFontRead;
    };
    auto refreshOpenButton = [this](CoolListItemList& selection, bool alliance) {
        CoolStrobeButton* openButton = AsButton(FindChild(kOpenButtonId));
        if (!openButton) return;
        openButton->Disable();
        if (selection.m_count != 1) return;
        CoolListItem* row = m_items->FindById(static_cast<CoolListItem*>(selection.m_head)->m_id);
        MessageRecord* record = static_cast<MessageListItem*>(row)->m_record;
        if (!record) return;
        OpenButtonLook look;
        if (OpensObject(record)) {
            look = ObjectLook(alliance);
        } else if (record->GetClass() == 4) {
            look = EncyclopediaLook(alliance);
        } else {
            return;
        }
        SetButtonPictures(openButton, look.normal, look.pressed);
        const ResourceStringId tip = StringId(look.tooltip);
        UpdateToolFromString(openButton, &tip);
        openButton->Enable();
    };
    switch (command) {
    case 0x28:
        if (m_mode == kIndexModeMessage && m_current) markCurrentRead();
        SetMode(command);
        PostCloseRequest();
        break;
    case 0x65:
    case 0x68:
        if (m_mode == kIndexModeMessage && m_current) markCurrentRead();
        SetMode(command);
        break;
    case 0x66: {
        unsigned state;
        if (m_filter == 0) {
            state = static_cast<unsigned>(UiGetHumanDefaultState());
            UiSetHumanDefaultState(static_cast<int>(state ^ 4));
        } else {
            state = UiGetHumanStateValue(m_filter);
            UiSetHumanStateValue(m_filter, static_cast<int>(state ^ 4));
        }
        CoolStrobeButton* toggleButton = AsButton(FindChild(kToggleButtonId));
        if (!toggleButton) return;
        toggleButton->Enable();
        const bool alliance = IsAlliance(m_galaxy);
        if (((state >> 2) & 1) == 0) {
            const ResourceStringId text = StringId(0x8006);
            UpdateToolFromString(toggleButton, &text);
            if (!alliance) {
                SetButtonPictures(toggleButton, 0x2ac9, 0x2a7b);
            } else {
                SetButtonPictures(toggleButton, 0x2ac7, 0x2a79);
            }
        } else {
            const ResourceStringId text = StringId(0x8005);
            UpdateToolFromString(toggleButton, &text);
            if (!alliance) {
                SetButtonPictures(toggleButton, 0x2a7a, 0x2a7b);
            } else {
                SetButtonPictures(toggleButton, 0x2a78, 0x2a79);
            }
        }
        break;
    }
    case 0x67:
    case 0x97: {
        MessageRecord* record = nullptr;
        if (m_mode == kIndexModeList) {
            for (CoolListItem* row = m_items->First(); row; row = row->Next()) {
                if (row->m_state.m_value & 1) {
                    record = static_cast<MessageListItem*>(row)->m_record;
                    break;
                }
            }
        } else if (m_mode == kIndexModeMessage) {
            record = m_current->m_record;
            m_current->m_visible = 0;
        }
        if (!record) return;
        ObjectKey target = record->m_key;
        record->m_flags.m_value = 1;
        bool opened = false;
        const int recordClass = record->GetClass();
        if (recordClass == 3 || recordClass == 5) {
            if (target.IsSet() != 0) {
                m_galaxy->OpenWindowForObject(&target);
                opened = true;
            }
        } else if (recordClass == 4) {
            ObjectKey type(record->m_target);
            UiOpenEncyclopedia(&target, reinterpret_cast<NullableKey*>(&type));
            opened = true;
        }
        if (opened) PostMessageA(m_hwnd, WM_COMMAND, 0x28, 0);
        break;
    }
    case 0x6e: {
        const int16_t notification = static_cast<int16_t>(static_cast<uint32_t>(b) >> 16);
        if (notification == 0x309 && m_mode == kIndexModeList) {
            CoolListItemList selection(1);
            m_list->CopySelectionTo(&selection);
            if (selection.m_count != 0) OnCommand(kBackButtonId, 0, 0);
            return;
        }
        if (notification != 0x29b) return;
        if (m_mode != kIndexModeList) return;
        CoolListItemList selection(1);
        m_list->CopySelectionTo(&selection);
        refreshOpenButton(selection, IsAlliance(m_galaxy));
        break;
    }
    case 0x78:
        SelectCategory(static_cast<int>(static_cast<uint32_t>(b) >> 16));
        SetFocus(m_list->m_hwnd);
        break;
    case 0x90: {
        for (CoolListItem* row = m_items->First(); row; row = row->Next()) {
            if (static_cast<MessageListItem*>(row)->m_visible != 0) row->m_state.m_value |= 1;
        }
        SetFocus(m_list->m_hwnd);
        m_list->Refresh();
        CoolListItemList selection(1);
        m_list->CopySelectionTo(&selection);
        refreshOpenButton(selection, IsAlliance(m_galaxy));
        break;
    }
    case 0x91:
        if (m_mode == kIndexModeList) {
            CoolListItem* neighbour = nullptr;
            CoolListItem* row = m_items->First();
            while (row) {
                CoolListItem* following = row->Next();
                if (row->m_state.m_value & 1) {
                    if (neighbour == nullptr) {
                        neighbour = row->Next();
                        if (neighbour == nullptr) neighbour = row->Previous();
                    }
                    m_items->Remove(row);
                    UiDeleteHumanMessage(row->m_id);
                    delete row;
                }
                row = following;
            }
            SetFocus(m_list->m_hwnd);
            if (neighbour) m_list->SelectById(neighbour->m_id, 1);
            m_list->Refresh();
        } else if (m_current) {
            MessageListItem* neighbour = static_cast<MessageListItem*>(m_current->Next());
            if (neighbour == nullptr) neighbour = static_cast<MessageListItem*>(m_current->Previous());
            m_items->Remove(m_current);
            UiDeleteHumanMessage(m_current->m_id);
            if (neighbour == nullptr) {
                SetMode(kBackButtonId);
            } else {
                m_current = neighbour;
                ShowMessage(neighbour);
            }
        }
        break;
    case 0x96: {
        markCurrentRead();
        MessageListItem* next = static_cast<MessageListItem*>(m_current->Next());
        if (next == nullptr) return;
        m_current = next;
        ShowMessage(next);
        SetFocus(m_text->m_hwnd);
        break;
    }
    case 0x98:
        if (m_mode != kIndexModeMessage) {
            if (m_mode != kIndexModeCompose) return;
            if (m_text->m_text.empty()) return;
            std::string message(m_text->m_text);
            UiQueueHumanPopup(reinterpret_cast<intptr_t>(&message), 1);
            OnCommand(kBackButtonId, 0, 0);
            SetFocus(m_text->m_hwnd);
            return;
        }
        m_current->m_visible = 0;
        m_current->m_record->m_flags.m_value = 1;
        m_current->m_record->Virtual14();
        OnCommand(kBackButtonId, 0, 0);
        SetFocus(m_list->m_hwnd);
        break;
    case 0x99:
        if (m_mode == kIndexModeMessage) {
            m_current->m_visible = 0;
            m_current->m_record->m_flags.m_value = 1;
            m_current->m_fontSlot = kNoticeFontRead;
            m_current->m_record->Virtual15();
        } else if (m_mode != kIndexModeCompose) {
            return;
        }
        OnCommand(kBackButtonId, 0, 0);
        SetFocus(m_list->m_hwnd);
        break;
    case 0x9a: {
        markCurrentRead();
        MessageListItem* previous = static_cast<MessageListItem*>(m_current->Previous());
        if (previous == nullptr) return;
        m_current = previous;
        ShowMessage(previous);
        SetFocus(m_text->m_hwnd);
        break;
    }
    }
}

void MessageIndexWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    switch (key) {
    case VK_RETURN: {
        if (m_mode != kIndexModeMessage) return;
        if (!m_current) return;
        CoolStrobeButton* button = nullptr;
        const int recordClass = m_current->m_record->GetClass();
        if (recordClass > 2) {
            uint32_t id;
            if (recordClass < 5) {
                id = kSendButtonId;
            } else if (recordClass == 5) {
                id = kAcceptButtonId;
            } else {
                return;
            }
            button = AsButton(m_children.FindById(id));
        }
        if (button) {
            button->Click();
            button->CancelPress();
        }
        return;
    }
    default:
        m_parent->WindowProc(WM_KEYDOWN, key, lParam);
        return;
    case VK_ESCAPE:
        if (m_mode == kIndexModeList) {
            PostCloseRequest();
            return;
        }
        OnCommand(kBackButtonId, 0, 0);
        return;
    case VK_LEFT:
    case VK_RIGHT: {
        const uint32_t selectedId = m_tabs->m_selected ? m_tabs->m_selected->m_childId : 0;
        AvlNode* tab = m_tabs->m_children.FindById(selectedId);
        tab = key == VK_LEFT ? tab->Predecessor() : tab->m_successor;
        if (tab) {
            do {
                CoolWindow* window = static_cast<CoolWindow*>(tab);
                if (IsWindowVisible(window->m_hwnd) != 0) {
                    m_tabs->SelectTab(window->m_childId, 1);
                    return;
                }
                tab = key == VK_LEFT ? tab->Predecessor() : tab->m_successor;
            } while (tab);
            return;
        }
        AvlNode* first = m_tabs->m_children.First();
        if (first) m_tabs->SelectTab(static_cast<CoolWindow*>(first)->m_childId, 1);
        return;
    }
    case VK_DELETE:
        OnCommand(kDeleteButtonId, 0, 0);
        return;
    }
}

// ---- Modal report hand-over -----------------------------------------------------------------------------------

void ModalReportClose(int owner, int notice) {
    if (owner != g_modalReportOwner) return;
    if (notice != g_modalReportId) {
        ModalReportReset(owner);
        return;
    }
    g_modalReportPending = 1;
    UiPostOpenType0dWindow(1, g_modalReportTab);
}

void ModalReportReset(int owner) {
    if (owner != g_modalReportOwner) return;
    g_modalReportId = 0;
    g_modalReportMode = 0;
    g_modalReportTab = 0;
    g_modalReportOwner = 0;
    g_modalReportPending = 0;
}

int g_modalReportId{};
int g_modalReportMode{};
int g_modalReportOwner{};
int g_modalReportPending{};
int g_modalReportTab{};
