#include "character/character.h"
#include "core/reb_abi.h"
#include "strategy_ui/battle_window.h"

#include <new>

#include "app/rebapp_globals.h"
#include "audio/music.h"
#include "character/role.h"
#include "core/checked_new.h"
#include "core/containers.h"
#include "core/vptr_store.h"
#include "fleet/capship.h"
#include "fleet/fight_squad.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "strategy_ui/sector_window.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_tab_control.h"
#include "ui/object_list_item.h"

extern const char g_gokresDllName[];  // 0x6a8708 "gokres.dll"


namespace {

constexpr uint32_t kStringSlot = 0;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kGokresSlot = 10;
constexpr uint32_t kHelperBitmapKind = 10;
constexpr int16_t kStringGroup = 2;
constexpr int kAlliance = 1;
constexpr int kEmpire = 2;
constexpr int kShow = 5;
constexpr uint32_t kHiddenStyle = 0x10000000;
constexpr uint16_t kClickSound = 0x260;

constexpr int kPageSummary = 0x6f;
constexpr int kPageAlliance = 0x70;
constexpr int kPageEmpire = 0x71;
constexpr int kPageGoTo = 0x72;
constexpr uint32_t kPagesId = 0x6e;
constexpr uint32_t kKindsId = 100;
constexpr uint32_t kListId = 200;
constexpr uint32_t kCloseId = 0xc9;
constexpr uint32_t kFleetId = 0xca;
constexpr uint32_t kSystemId = 0xcb;

constexpr int kKindShips = 0x65;
constexpr int kKindFighters = 0x66;
constexpr int kKindTroops = 0x67;
constexpr int kKindPersonnel = 0x68;

constexpr uint32_t kBackgroundSummary = 1;
constexpr uint32_t kBackgroundGoTo = 4;
constexpr uint32_t kBackgroundShips = 2;
constexpr uint32_t kBackgroundPersonnel = 3;

ResourceStringId StringId(uint16_t id) { return ResourceStringId{static_cast<int16_t>(id), kStringGroup}; }

DibBitmap* LoadBitmap(HMODULE module, uint32_t id) { return NewOrNull<DibBitmap>(module, id, kHelperBitmapKind); }

DibBitmap* NewPage(DibBitmap* frame, HDC dc) {
    return NewOrNull<DibBitmap>(static_cast<LONG>(frame->Width()), static_cast<LONG>(frame->Height()), frame, dc);
}

void DeleteBitmap(DibBitmap* bitmap) {
    if (bitmap) {
        bitmap->~DibBitmap();
        operator delete(bitmap);
    }
}

bool InFamilies(const ObjectKey& key, uint32_t first, uint32_t last) { return key.Family() >= first && key.Family() < last; }

ObjectKeyNode* NewKeyNode(GameObjNotifier* object) {
    void* memory = ::operator new(sizeof(ObjectKeyNode));
    if (!memory) return nullptr;
    ObjectKey key;
    return new (memory) ObjectKeyNode(*object->GetKey(&key));
}

ObjectKeyNode* NewKeyNode(const ObjectKey& key) {
    void* memory = ::operator new(sizeof(ObjectKeyNode));
    return memory ? new (memory) ObjectKeyNode(key) : nullptr;
}

uint32_t StateFlags(GameObjNotifier* object) { return static_cast<GameObj*>(object)->m_stateFlags; }

CoolStrobeButton* NewButton(HMODULE art, int x, int y, int width, int height, CoolWindow* parent, uint32_t id,
                            uint32_t normal, uint32_t pressed, DibBitmap* backdrop, int repeatRate, int pixelHit) {
    return NewOrNull<CoolStrobeButton>(art, x, y, width, height, parent, id, normal, pressed, backdrop, repeatRate, pixelHit);
}

struct PageButton {
    int y;
    uint32_t id;
    uint32_t allianceNormal;
    uint32_t alliancePressed;
    uint32_t empireNormal;
    uint32_t empirePressed;
    uint16_t tooltip;
};

// A button of a tab control; the first one is selected when it is added.
void AddTabButton(BattleWindow* window, CoolTabControl* tabs, HMODULE art, int x, int y, int width, int height, uint32_t id,
                  uint32_t normal, uint32_t pressed, uint16_t tooltip, bool first, bool clickSound) {
    CoolStrobeButton* button = NewButton(art, x, y, width, height, tabs, id, normal, pressed, tabs->m_background, 0, 0);
    if (!button) return;
    button->Create(0);
    if (clickSound) button->SetClickSound(kClickSound);
    const ResourceStringId tip = StringId(tooltip);
    window->AddToolFromString(button, &tip);
    tabs->AddTab(button, first ? 1 : 0, 0);
}

// Gives a big button its pressed picture drawn over its normal one and hides it until the window shows it.
void PrepareFlatButton(CoolStrobeButton* button) {
    DibBitmap* normal = button->GetStateBitmap(0);
    DibBitmap* pressed = button->GetStateBitmap(1);
    DibBitmap* merged = NewOrNull<DibBitmap>(normal->Width(), normal->Height(), normal);
    normal->Blit(merged, 0, 0, 0, 0, 0, 0);
    pressed->DrawKeyed(merged, 0, 0);
    button->ReplaceBitmap(1, merged);
    button->m_style &= ~kHiddenStyle;
    button->MoveLabel(0, 0x60);
    button->Create(0);
}

}  // namespace

// ---- Construction ---------------------------------------------------------------------------------------------

BattleWindow::BattleWindow(HINSTANCE instance, CoolWindow* parent, uint32_t childId, BattleMessage* battle)
    : CoolGameWindow(instance, 0, 0, 0x1d6, 0x14b, parent, kStringSlot, kBitmapSlot, 0, childId, 0, 1, 0, 0),
      m_label(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u) {
    m_style = 0x46000000;
    m_galaxy = static_cast<GalaxyWindow*>(parent);
    m_battle = battle;
    m_list = nullptr;
    m_items = nullptr;
    m_pageTabs = nullptr;
    m_kindTabs = nullptr;
    m_page = 0;
    m_fleetButton = nullptr;
    m_systemButton = nullptr;
    m_nextItemId = 0;
    m_kind = kKindShips;
    m_gokres = FindResourceModule(kGokresSlot);
    if (!m_gokres) {
        char* dllLocation = GetDllLocation();
        m_gokres = LoadResourceModule(dllLocation, g_gokresDllName, kGokresSlot);
        if (dllLocation) operator delete(dllLocation);
    }
}

BattleWindow::~BattleWindow() {
    if (m_list) delete m_list;
    if (m_items) {
        m_items->Clear();
        if (m_items) delete m_items;
    }
    if (m_pageTabs) delete m_pageTabs;
    if (m_kindTabs) delete m_kindTabs;
    if (m_fleetButton) delete m_fleetButton;
    if (m_systemButton) delete m_systemButton;
    if (m_closeButton) delete m_closeButton;
    REB_KEEP_VPTR_STORE();
}

// ---- OnCreate1 ------------------------------------------------------------------------------------------------

void BattleWindow::OnCreate1() {
    CoolLabel title(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
    HINSTANCE instance = m_instance;
    HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
    m_system = GetObjectInFamilies_90_98(m_galaxy->m_side, &m_battle->m_systemKey);
    const int viewSide = m_galaxy->m_side == kAlliance ? 1 : (m_galaxy->m_side == kEmpire ? 2 : 0);
    m_advicePlaying = 1;
    uint32_t bannerId = 0;
    if (m_battle->m_attackerSide == 0) {
        bannerId = 0x2d63;
        m_adviceId = viewSide == kAlliance ? 0xd : 0xf;
    } else if (m_battle->m_attackerSide == viewSide) {
        if (viewSide == kAlliance) {
            m_adviceId = 0xc;
            bannerId = (m_battle->m_defenderSide != 2 ? 1 : 0) + 0x2a06;
        } else if (viewSide == kEmpire) {
            m_adviceId = 0xe;
            bannerId = ((m_battle->m_defenderSide != 1 ? 3u : 0u)) + 0x2a05;
        }
    } else if (m_battle->m_defenderSide == viewSide) {
        if (viewSide == kAlliance) {
            m_adviceId = 0xd;
            bannerId = 0x2a05;
        } else if (viewSide == kEmpire) {
            m_adviceId = 0xf;
            bannerId = 0x2a06;
        }
    } else if (viewSide == kAlliance) {
        m_adviceId = 0xd;
        bannerId = 0x2a08;
    } else if (viewSide == kEmpire) {
        m_adviceId = 0xf;
        bannerId = 0x2a07;
    }
    DibBitmap* banner = LoadBitmap(art, bannerId);
    DibBitmap* frame;
    DibBitmap* rightEdge;
    if (m_galaxy->m_side == kAlliance) {
        frame = LoadBitmap(art, 0x285f);
        rightEdge = LoadBitmap(art, 0x2a44);
    } else {
        frame = LoadBitmap(art, 0x2860);
        rightEdge = LoadBitmap(art, 0x2a45);
    }
    DibBitmap* summaryBanner = LoadBitmap(art, 0x2a09);
    DibBitmap* forcesBanner = LoadBitmap(art, 0x2a0a);
    DibBitmap* goToBanner = LoadBitmap(art, 0x2a0b);
    HDC windowDc = GetDC(m_hwnd);
    HDC dc = CreateCompatibleDC(windowDc);
    ReleaseDC(m_hwnd, windowDc);
    const COLORREF color = m_galaxy->m_side == kEmpire ? 0xff00 : 0xff;
    title.SetText(m_battle->m_title.m_text);
    title.SetFont(0xb);
    title.m_drawFlags = 1;
    title.MoveTo(0xc, 0x12);
    title.SetColor(color, nullptr);
    title.m_width = 400;
    title.m_layoutFlags = 1;
    m_label.SetFont(0xb);
    m_label.m_drawFlags = 1;
    m_label.SetColor(color, nullptr);
    m_label.m_layoutFlags = 1;

    DibBitmap* page = NewPage(frame, dc);
    banner->Blit(page, 0xc, 0xd, 0, 0, 0, 0);
    frame->DrawKeyed(page, 0, 0);
    rightEdge->DrawKeyed(page, 0x19c, 0);
    HGDIOBJ saved = SelectObject(dc, page->DibSectionHandle());
    title.Draw(dc);
    m_label.MoveTo(0xc, 0xe1);
    m_label.m_width = 400;
    m_label.SetText(m_battle->m_summary.m_text);
    m_label.Draw(dc);
    m_label.SetFont(6);
    m_label.m_drawFlags = 0x110;
    m_label.m_layoutFlags = 1;
    m_label.m_width = 0x160;
    m_label.MoveTo(0x19, 0x100);
    m_label.SetColor(0, nullptr);
    m_label.Measure(dc);
    m_label.Draw(dc);
    m_label.SetText(m_battle->m_caption.m_text);
    m_label.SetColor(color, nullptr);
    m_label.MoveTo(0x18, 0xff);
    m_label.Measure(dc);
    m_label.Draw(dc);
    SetPaintBackground(page, kBackgroundSummary);

    page = NewPage(frame, dc);
    goToBanner->Blit(page, 0xc, 0xd, 0, 0, 0, 0);
    frame->DrawKeyed(page, 0, 0);
    rightEdge->DrawKeyed(page, 0x19c, 0);
    SelectObject(dc, page->DibSectionHandle());
    title.Draw(dc);
    m_label.SetFont(6);
    m_label.m_drawFlags = 0x110;
    m_label.SetColor(color, nullptr);
    m_label.m_layoutFlags = 1;
    m_label.m_width = 0x148;
    m_label.MoveTo(0x18, 0x28);
    m_label.SetText(m_battle->m_sideTextA.m_text);
    m_label.Draw(dc);
    m_label.SetFont(7);
    m_label.m_drawFlags = 0x110;
    m_label.SetColor(color, nullptr);
    m_label.m_layoutFlags = 1;
    m_label.m_width = 0x148;
    m_label.MoveTo(0x18, 0x82);
    m_label.SetText(m_battle->m_sideTextB.m_text);
    m_label.Draw(dc);
    AddOverlay(page, kBackgroundGoTo);
    m_label.MoveTo(0x18, 0xff);
    m_label.SetFont(4);
    m_label.SetColor(color, nullptr);
    m_label.m_drawFlags = 1;
    m_label.m_layoutFlags = 1;
    title.SetText(m_battle->m_title.m_text);

    page = NewPage(frame, dc);
    summaryBanner->Blit(page, 0xc, 0xd, 0, 0, 0, 0);
    frame->DrawKeyed(page, 0, 0);
    rightEdge->DrawKeyed(page, 0x19c, 0);
    SelectObject(dc, page->DibSectionHandle());
    title.Draw(dc);
    m_label.SetColor(0xffffff, nullptr);
    m_label.m_width = 0xa5;
    m_label.MoveTo(0x25, 0x76);
    const ResourceStringId alliedHeading = StringId(0x1932);
    m_label.SetTextFromResource(&alliedHeading);
    m_label.Draw(dc);
    m_label.MoveTo(0xcc, 0x76);
    const ResourceStringId enemyHeading = StringId(0x1933);
    m_label.SetTextFromResource(&enemyHeading);
    m_label.Draw(dc);
    m_label.MoveTo(0x56, 0x46);
    m_label.m_drawFlags = 0;
    const ResourceStringId versusText = StringId(0x1952);
    m_label.SetTextFromResource(&versusText);
    m_label.Draw(dc);
    AddOverlay(page, kBackgroundShips);
    m_label.m_drawFlags = 1;
    SelectObject(dc, saved);

    page = NewPage(frame, dc);
    forcesBanner->Blit(page, 0xc, 0xd, 0, 0, 0, 0);
    frame->DrawKeyed(page, 0, 0);
    rightEdge->DrawKeyed(page, 0x19c, 0);
    saved = SelectObject(dc, page->DibSectionHandle());
    title.Draw(dc);
    m_label.m_width = 0x6e;
    const ResourceStringId shipsHeading = StringId(0x1934);
    m_label.SetTextFromResource(&shipsHeading);
    m_label.MoveTo(0x25, 0x76);
    m_label.Measure(dc);
    m_label.Draw(dc);
    const ResourceStringId fightersHeading = StringId(0x1935);
    m_label.SetTextFromResource(&fightersHeading);
    m_label.MoveTo(0x95, 0x76);
    m_label.Draw(dc);
    const ResourceStringId troopsHeading = StringId(0x1936);
    m_label.SetTextFromResource(&troopsHeading);
    m_label.MoveTo(0x104, 0x76);
    m_label.Draw(dc);
    m_label.MoveTo(0x56, 0x46);
    m_label.m_drawFlags = 0;
    m_label.SetTextFromResource(&versusText);
    m_label.Draw(dc);
    AddOverlay(page, kBackgroundPersonnel);
    SelectObject(dc, saved);
    DeleteBitmap(frame);
    DeleteBitmap(banner);
    DeleteBitmap(summaryBanner);
    DeleteBitmap(forcesBanner);
    DeleteBitmap(goToBanner);
    DeleteBitmap(rightEdge);

    const bool alliance = m_galaxy->m_side == kAlliance;
    const int tabsX = alliance ? 0x1a2 : 0x1aa;
    const int tabsY = alliance ? 0x58 : 0x59;
    const int tabWidth = alliance ? 0x29 : 0x2c;
    const int tabsHeight = alliance ? 0xcb : 0xda;
    m_pageTabs = NewOrNull<CoolTabControl>(instance, tabsX, tabsY, tabWidth, tabsHeight, kPagesId, this, GetPaintBackgroundIfOwned());
    if (m_pageTabs) {
        m_pageTabs->Create(0);
        AddTabButton(this, m_pageTabs, art, 0, 0, tabWidth, 0x29, kPageSummary, alliance ? 0x29e8 : 0x29f2,
                     alliance ? 0x29e9 : 0x29f3, 0x1922, true, true);
        AddTabButton(this, m_pageTabs, art, 0, alliance ? 0x36 : 0x3b, tabWidth, 0x29, kPageAlliance, alliance ? 0x29ea : 0x29f4,
                     alliance ? 0x29eb : 0x29f5, 0x1937, false, true);
        AddTabButton(this, m_pageTabs, art, 0, alliance ? 0x6c : 0x76, tabWidth, 0x29, kPageEmpire, alliance ? 0x29ec : 0x29f6,
                     alliance ? 0x29ed : 0x29f7, 0x1938, false, true);
        AddTabButton(this, m_pageTabs, art, 0, alliance ? 0xa2 : 0xb1, tabWidth, 0x29, kPageGoTo, alliance ? 0x2a27 : 0x2918,
                     alliance ? 0x2a28 : 0x2919, 0x1939, false, true);
        AddTabButton(this, m_pageTabs, art, 0, alliance ? 0xa2 : 0xb1, tabWidth, 0x29, kPageGoTo, alliance ? 0x2916 : 0x2918,
                     alliance ? 0x2917 : 0x2919, 0x1931, false, true);
    }

    m_items = NewOrNull<CoolListItemList>(4);
    m_list = NewOrNull<CoolDragList>(instance, 0x25, 0x88, 0x15b, 0xa6, this, GetPaintBackgroundIfOwned(), kListId, 0xa5, 0x46,
                                     0, 0, 0, 0, 10, 1, nullptr);
    if (m_list) {
        m_list->m_options.m_value = 0xb0000;
        m_list->m_style &= ~kHiddenStyle;
        m_list->Create(0);
        m_list->CreateVerticalScrollBar(m_instance, m_list, 7, 0x299d, m_list->m_clientHeight, 0, m_list->m_clientHeight, 0,
                                        nullptr);
        m_list->m_textFormat = 4;
        m_list->SetItemList(m_items);
        m_list->SetTextColor(0xffffff);
    }

    m_kindTabs = NewOrNull<CoolTabControl>(instance, 0x82, 0x3b, 0xc4, 0x2a, kKindsId, this, FindOverlayBitmap(kBackgroundShips));
    if (m_kindTabs) {
        m_kindTabs->m_style &= ~kHiddenStyle;
        m_kindTabs->Create(0);
        const int kindWidth = alliance ? 0x31 : 0x2c;
        const int kindHeight = alliance ? 0x2a : 0x29;
        AddTabButton(this, m_kindTabs, art, 0, 0, kindWidth, kindHeight, kKindShips, alliance ? 0x286c : 0x2878,
                     alliance ? 0x286b : 0x2877, 0x1928, true, false);
        AddTabButton(this, m_kindTabs, art, 0x31, 0, kindWidth, kindHeight, kKindFighters, alliance ? 0x2872 : 0x287c,
                     alliance ? 0x2871 : 0x287b, 0x1929, false, false);
        AddTabButton(this, m_kindTabs, art, 0x62, 0, kindWidth, kindHeight, kKindTroops, alliance ? 0x2870 : 0x287a,
                     alliance ? 0x286f : 0x2879, 0x1930, false, false);
        AddTabButton(this, m_kindTabs, art, 0x93, 0, kindWidth, kindHeight, kKindPersonnel, alliance ? 0x286a : 0x2876,
                     alliance ? 0x2869 : 0x2875, 0x1931, false, false);
    }

    m_closeButton = NewButton(art, alliance ? 0x1a7 : 0x1aa, alliance ? 0x19 : 0x11, alliance ? 0x20 : 0x2c, alliance ? 0x1f : 0x29,
                              this, kCloseId, alliance ? 0x2882 : 0x2888, alliance ? 0x2883 : 0x2889, GetPaintBackgroundIfOwned(),
                              0, 0);
    m_closeButton->Create(0);
    m_closeButton->SetClickSound(kClickSound);

    ObjectKeySet unusedSet;
    bool hasFleet = false;
    ObjectKey fleetKey;
    m_battle->GetFleetKey(&fleetKey);
    if (fleetKey.IsSet() != 0) {
        GameObjNotifier* fleet = GetObjectInFamilies_08_10(m_galaxy->m_side, &fleetKey);
        if (fleet) {
            hasFleet = true;
            m_fleetSystem = fleet->m_parent;
        }
    }
    if (hasFleet) {
        m_fleetButton = NewButton(art, 0xe5, 0xb6, 0xa9, 0x60, this, kFleetId, alliance ? 0x2a0d : 0x2a0e,
                                  alliance ? 0x2a0f : 0x2a10, FindOverlayBitmap(kBackgroundGoTo), 0, 1);
        PrepareFlatButton(m_fleetButton);
        HGDIOBJ previous = SelectObject(dc, FindOverlayBitmap(kBackgroundGoTo)->DibSectionHandle());
        const ResourceStringId fleetText = StringId(0x1942);
        m_label.SetTextFromResource(&fleetText);
        m_label.SetFont(4);
        m_label.SetColor(0xffffff, nullptr);
        m_label.m_drawFlags = 0x101;
        m_label.m_layoutFlags = 1;
        m_label.m_width = 0xa9;
        m_label.MoveTo(0xe5, 0x116);
        m_label.Draw(dc);
        SelectObject(dc, previous);
    }
    m_systemButton = NewButton(art, 0x1d, 0xb6, 0xa9, 0x60, this, kSystemId, 0x2a0c, (m_galaxy->m_side != 1 ? 1 : 0) + 0x2a0f,
                               FindOverlayBitmap(kBackgroundGoTo), 0, 1);
    if (m_systemButton) {
        PrepareFlatButton(m_systemButton);
        HGDIOBJ previous = SelectObject(dc, FindOverlayBitmap(kBackgroundGoTo)->DibSectionHandle());
        if (m_system) m_label.SetText(static_cast<GameObj*>(m_system)->GetName()->m_text);
        m_label.SetFont(4);
        m_label.SetColor(0xffffff, nullptr);
        m_label.m_drawFlags = 0x101;
        m_label.m_layoutFlags = 1;
        m_label.m_width = 0xa9;
        m_label.MoveTo(0x1d, 0x116);
        m_label.Draw(dc);
        SelectObject(dc, previous);
    }
    DeleteDC(dc);
    CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right, m_galaxy->m_panelArea.bottom);
    SelectPage(kPageSummary);
}

// ---- Behaviour ------------------------------------------------------------------------------------------------

void BattleWindow::StopAdvice() {
    if (m_advicePlaying != 0) {
        PlayAdviceMusic(m_adviceId);
        m_advicePlaying = 0;
    }
}

void BattleWindow::PostCloseRequest() {
    StopMusic();
    StartMusicTrack(static_cast<unsigned>(UiHumanNextAdvice()));
    CoolGameWindow::PostCloseRequest();
}

void BattleWindow::DrawOverlay(HDC dc) {
    if (m_advicePlaying != 0) StopAdvice();
    if (static_cast<unsigned>(m_page) <= 0x6f || static_cast<unsigned>(m_page) >= 0x72) return;
    const uint32_t kindId = m_kindTabs->m_selected ? m_kindTabs->m_selected->m_childId : 0;
    CoolStrobeButton* kindTab = static_cast<CoolStrobeButton*>(m_kindTabs->m_children.FindById(kindId));
    m_label.m_layoutFlags = 1;
    m_label.MoveTo(0x25, 99);
    m_label.m_width = 0x15b;
    m_label.m_drawFlags = 0;
    m_label.SetFont(4);
    m_label.SetColor(0xffffff, nullptr);
    m_label.SetText(kindTab->m_tooltipText);
    m_label.Draw(dc);
    const uint32_t pageId = m_pageTabs->m_selected ? m_pageTabs->m_selected->m_childId : 0;
    CoolStrobeButton* pageTab = static_cast<CoolStrobeButton*>(m_pageTabs->m_children.FindById(pageId));
    m_label.MoveTo(0xc, 0x28);
    m_label.m_width = 400;
    m_label.m_drawFlags = 1;
    m_label.SetFont(6);
    const uint32_t id = pageTab->GetChildId();
    COLORREF color = 0xffffff;
    if (id == 0x70) {
        color = 0xff;
    } else if (id == 0x71) {
        color = 0xff00;
    }
    m_label.SetColor(color, nullptr);
    m_label.SetText(pageTab->m_tooltipText);
    m_label.Draw(dc);
}

void BattleWindow::OnKeyDown(WPARAM key, LPARAM lParam) {
    if (key == VK_ESCAPE) {
        PostCloseRequest();
        UiHumanConfirmVote(reinterpret_cast<PlayerMessage*>(m_battle));
        AppEnableSpeedAfterDialog();
        return;
    }
    if (key != VK_LEFT && key != VK_RIGHT) {
        m_parent->WindowProc(WM_KEYDOWN, key, lParam);
        return;
    }
    const uint32_t selectedId = m_kindTabs->m_selected ? m_kindTabs->m_selected->m_childId : 0;
    AvlNode* tab = m_kindTabs->m_children.FindById(selectedId);
    tab = key == VK_LEFT ? tab->Predecessor() : tab->m_successor;
    if (tab) {
        do {
            CoolWindow* window = static_cast<CoolWindow*>(tab);
            if (IsWindowVisible(window->m_hwnd) != 0) {
                m_kindTabs->SelectTab(window->m_childId, 1);
                return;
            }
            tab = key == VK_LEFT ? tab->Predecessor() : tab->m_successor;
        } while (tab);
        return;
    }
    AvlNode* first = m_kindTabs->m_children.First();
    if (first) m_kindTabs->SelectTab(static_cast<CoolWindow*>(first)->m_childId, 1);
}

void BattleWindow::OnCommand(int a, int b, int c) {
    switch (static_cast<uint32_t>(a) & 0xffff) {
    case 100:
        FillList(static_cast<int>(static_cast<uint32_t>(b) >> 16), 0);
        return;
    default:
        return;
    case 0x6e:
        SelectPage(static_cast<int>(static_cast<uint32_t>(b) >> 16));
        return;
    case 0xc9:
        break;
    case 0xca: {
        if (!m_fleetSystem) return;
        if (!m_battle) return;
        ObjectKey sectorKey;
        m_fleetSystem->m_parent->GetKey(&sectorKey);
        SectorWindow* sector = static_cast<SectorWindow*>(m_galaxy->OpenSectorWindow(&sectorKey));
        ObjectKey systemKey;
        m_fleetSystem->GetKey(&systemKey);
        sector->OpenWindowOfType(&systemKey, 4);
        break;
    }
    case 0xcb: {
        if (!m_battle) return;
        ObjectKey systemKey = m_battle->m_systemKey;
        GameObjNotifier* system = GetObjectInFamilies_90_98(m_galaxy->m_side, &systemKey);
        if (system) {
            ObjectKey sectorKey;
            system->m_parent->GetKey(&sectorKey);
            m_galaxy->OpenSectorWindow(&sectorKey);
        }
        break;
    }
    }
    PostCloseRequest();
    UiHumanConfirmVote(reinterpret_cast<PlayerMessage*>(m_battle));
    AppEnableSpeedAfterDialog();
}

void BattleWindow::SelectPage(int page) {
    if (m_page == page) return;
    switch (page) {
    case kPageSummary:
        SetFocus(m_hwnd);
        if (m_fleetButton) m_fleetButton->Hide();
        m_systemButton->Hide();
        m_list->Hide();
        m_kindTabs->Hide();
        SetPaintBackgroundById(kBackgroundSummary);
        m_page = page;
        return;
    case kPageAlliance:
    case kPageEmpire:
        if (m_fleetButton) m_fleetButton->Hide();
        m_systemButton->Hide();
        FillList(m_kind, 1);
        m_list->Show(kShow);
        SetFocus(m_list->m_hwnd);
        m_kindTabs->Show(kShow);
        m_page = page;
        return;
    case kPageGoTo:
        SetFocus(m_hwnd);
        m_list->Hide();
        m_kindTabs->Hide();
        SetPaintBackgroundById(kBackgroundGoTo);
        if (m_fleetButton) m_fleetButton->Show(kShow);
        m_systemButton->Show(kShow);
    }
    m_page = page;
}

// ---- The participant list -------------------------------------------------------------------------------------

void BattleWindow::ClearRows() {
    if (!m_items) return;
    CoolListItem* item = m_items->First();
    while (item) {
        CoolListItem* next = item->Next();
        m_items->Remove(item);
        if (item) delete item;
        item = next;
    }
}

void BattleWindow::AddRow(GameObj* object, int normal, String* heading) {
    const int itemHeight = m_list->m_itemHeight;
    const int itemWidth = m_list->m_itemWidth;
    if (!object) {
        ObjectKey unset;
        ObjectListItem* item = nullptr;
        if (void* memory = ::operator new(sizeof(ObjectListItem))) {
            item = new (memory) ObjectListItem(++m_nextItemId, &unset);
        }
        if (heading != nullptr) {
            item->m_text = heading->m_text;
            item->m_fontSlot = 6;
            item->m_textOffsetY = 10;
            item->m_textFormat = 5;
        }
        m_items->Insert(item);
        return;
    }
    ObjectKey key;
    object->GetKey(&key);
    DibBitmap* picture = CreateObjectPicture(m_gokres, &key, object, normal == 0, 1);
    DibBitmap* image = NewOrNull<DibBitmap>(itemWidth, itemHeight, picture);
    image->FillRect(0, 0, itemWidth, itemHeight, picture->m_colorKey);
    const int left = (itemWidth - picture->Width()) / 2;
    const int top = (itemHeight - picture->Height()) / 2;
    picture->DrawKeyed(image, left, top);
    ObjectListItem* item = nullptr;
    if (void* memory = ::operator new(sizeof(ObjectListItem))) {
        ObjectKey itemKey;
        object->GetKey(&itemKey);
        item = new (memory) ObjectListItem(++m_nextItemId, &itemKey);
    }
    item->SetImages(image, nullptr);
    item->m_state.m_value |= 4;
    item->m_text = object->GetName()->m_text;
    item->m_fontSlot = 10;
    item->m_textOffsetY = picture->Height() + top;
    item->m_textFormat = 1;
    m_items->Insert(item);
    DeleteBitmap(picture);
}

void REB_STDCALL CollectBattleParticipants(GameObjNotifier* fleet, ObjectKeySet* shown, ObjectKeySet* held, int kind) {
    CapShipIterator ships(fleet, 3);
    for (ships.Begin(); ships.Current(); ships.Advance()) {
        GameObjNotifier* ship = ships.Current();
        const uint32_t flags = StateFlags(ship);
        if ((flags & 4) == 0 || (flags & 0x20) != 0) continue;
        switch (kind) {
        case kKindShips:
            shown->Add(NewKeyNode(ship));
            break;
        case kKindFighters: {
            FightSquadIterator squadrons(ship, 3);
            for (squadrons.Begin(); squadrons.Current(); squadrons.Advance()) {
                if ((StateFlags(squadrons.Current()) >> 2 & 1) != 0) shown->Add(NewKeyNode(squadrons.Current()));
            }
            break;
        }
        case kKindTroops: {
            TroopRegIterator regiments(ship, 3);
            for (regiments.Begin(); regiments.Current(); regiments.Advance()) {
                if ((StateFlags(regiments.Current()) >> 2 & 1) != 0) shown->Add(NewKeyNode(regiments.Current()));
            }
            break;
        }
        case kKindPersonnel: {
            RoleIterator roles(ship, 3);
            for (roles.Begin(); roles.Current(); roles.Advance()) {
                GameObjNotifier* role = roles.Current();
                ObjectKey roleKey;
                role->GetKey(&roleKey);
                ObjectKeyNode* node = NewKeyNode(roleKey);
                ObjectKeySet* target = shown;
                if (InFamilies(roleKey, 0x30, 0x3c) && (static_cast<Character*>(role)->m_flagsAc & 1) != 0) target = held;
                target->Add(node);
            }
        }
        }
    }
}

void BattleWindow::FillList(int kind, int force) {
    if (m_kind == kind && force == 0) return;
    IntPair familyRange;
    int normalFlag = 0;
    ObjectKeySet shown;
    ObjectKeySet held;
    ObjectKeySet matching;
    ObjectKeySet participants;
    const uint32_t selectedPage = m_pageTabs->m_selected ? m_pageTabs->m_selected->m_childId : 0;
    uint32_t side;
    if (selectedPage == static_cast<uint32_t>(kPageAlliance)) {
        participants = m_battle->m_allianceSide;
        side = 1;
    } else if (selectedPage == static_cast<uint32_t>(kPageEmpire)) {
        participants = m_battle->m_empireSide;
        side = 2;
    } else {
        return;
    }
    ClearRows();
    uint32_t backgroundId = 0;
    int itemWidth = 0;
    int itemHeight = 0;
    switch (kind) {
    case kKindShips:
        backgroundId = kBackgroundShips;
        itemWidth = 0xa5;
        itemHeight = 0x46;
        familyRange = IntPair(0x14, 0x1c);
        break;
    case kKindFighters:
        backgroundId = kBackgroundShips;
        itemWidth = 0xa5;
        itemHeight = 0x46;
        familyRange = IntPair(0x1c, 0x20);
        break;
    case kKindTroops:
        backgroundId = kBackgroundShips;
        itemWidth = 0xa5;
        itemHeight = 0x46;
        familyRange = IntPair(0x10, 0x14);
        break;
    case kKindPersonnel:
        backgroundId = kBackgroundPersonnel;
        itemWidth = 0x6e;
        itemHeight = 0x32;
        familyRange = IntPair(0x30, 0x40);
        normalFlag = 1;
        break;
    }
    if (FindOverlayBitmap(backgroundId) != GetPaintBackgroundIfOwned()) {
        DibBitmap* background = SetPaintBackgroundById(backgroundId);
        m_list->SetBackground(background, m_list->m_x, m_list->m_y);
        m_list->SetItemSize(itemWidth, itemHeight);
    }
    const int viewSide = side != 1 ? 2 : 1;
    for (ObjectKeyNode* node = m_battle->m_fleets.First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
        ObjectKey key;
        GameObjNotifier* fleet = GetObjectInFamilies_08_10(viewSide, node->GetKey(&key));
        if (fleet && fleet->ControlSide() == side) CollectBattleParticipants(fleet, &shown, &held, kind);
    }
    FleetIterator fleets(m_system, 1, side);
    for (fleets.Begin(); fleets.Current(); fleets.Advance()) CollectBattleParticipants(fleets.Current(), &shown, &held, kind);
    if (kind == kKindFighters) {
        FightSquadIterator squadrons(m_system, 1, side);
        for (squadrons.Begin(); squadrons.Current(); squadrons.Advance()) {
            if ((StateFlags(squadrons.Current()) >> 2 & 1) != 0) shown.Add(NewKeyNode(squadrons.Current()));
        }
        for (ObjectKeyNode* node = m_battle->m_fighters.First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
            ObjectKey key;
            GameObjNotifier* squadron = GetObjectInFamilies_1c_20(viewSide, node->GetKey(&key));
            if (squadron && squadron->ControlSide() == side) shown.Add(NewKeyNode(squadron));
        }
    } else if (kind == kKindPersonnel && m_battle->m_characters.First()) {
        for (ObjectKeyNode* node = m_battle->m_characters.First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
            ObjectKey key;
            GameObjNotifier* character = GetObjectInFamilies_30_3c(viewSide, node->GetKey(&key));
            if (character && character->ControlSide() == side) shown.Add(NewKeyNode(character));
        }
    }
    for (ObjectKeyNode* node = participants.First(); node; node = static_cast<ObjectKeyNode*>(node->m_successor)) {
        ObjectKey key;
        node->GetKey(&key);
        if (InFamilies(key, familyRange.m_first, familyRange.m_second)) {
            ObjectKey nodeKey;
            matching.Add(NewKeyNode(*node->GetKey(&nodeKey)));
        }
    }

    ObjectKeyNode* shownNode = shown.First();
    ObjectKeyNode* heldNode = held.First();
    ObjectKeyNode* matchingNode = matching.First();
    String heading;
    bool shownDone = false;
    bool matchingDone = false;
    bool heldDone = false;
    GalaxyWindow* galaxy = m_galaxy;
    auto objectOf = [&](ObjectKeyNode* node, int view) {
        ObjectKey key;
        return GetObjectInFamilies_01_ff(view, node->GetKey(&key));
    };
    for (;;) {
        if (shownNode == nullptr) {
            String* text = nullptr;
            if (!shownDone) {
                shownDone = true;
                const ResourceStringId id = (matching.First() == nullptr && held.First() == nullptr) ? StringId(0x8526) : StringId(0x1940);
                heading.SetFromResource(&id);
                text = &heading;
            }
            AddRow(nullptr, 0, text);
        } else {
            AddRow(static_cast<GameObj*>(objectOf(shownNode, viewSide)), normalFlag, nullptr);
            shownNode = static_cast<ObjectKeyNode*>(shownNode->m_successor);
            if (shownNode == nullptr) shownDone = true;
        }
        if (kind == kKindPersonnel) {
            if (heldNode == nullptr) {
                String* text = nullptr;
                if (!heldDone) {
                    heldDone = true;
                    const ResourceStringId id = StringId(0x8526);
                    heading.SetFromResource(&id);
                    text = &heading;
                }
                AddRow(nullptr, 0, text);
            } else {
                AddRow(static_cast<GameObj*>(objectOf(heldNode, galaxy->m_side)), normalFlag, nullptr);
                heldNode = static_cast<ObjectKeyNode*>(heldNode->m_successor);
                if (heldNode == nullptr) heldDone = true;
            }
        }
        if (matchingNode == nullptr) {
            String* text = nullptr;
            if (!matchingDone) {
                matchingDone = true;
                const ResourceStringId id = StringId(0x1941);
                heading.SetFromResource(&id);
                text = &heading;
            }
            AddRow(nullptr, 0, text);
        } else {
            AddRow(static_cast<GameObj*>(objectOf(matchingNode, galaxy->m_side)), normalFlag, nullptr);
            matchingNode = static_cast<ObjectKeyNode*>(matchingNode->m_successor);
            if (matchingNode != nullptr) continue;
            matchingDone = true;
        }
        if (shownNode == nullptr) break;
    }
    shown.Clear();
    matching.Clear();
    CoolListItem* item = m_items->First();
    CoolListItem* first = item;
    DibBitmap* image = item ? item->m_image : nullptr;
    while (image == nullptr) {
        item = item->Next();
        if (!item) break;
        image = item->m_image;
    }
    if (item && first) m_list->m_fillColor = image->m_colorKey;
    m_list->Refresh();
    RECT area;
    SetRect(&area, 0xc, 0x28, 0x167, 0x88);
    InvalidateRect(m_hwnd, &area, 0);
    m_kind = kind;
}

ObjectKey* BattleMessage::GetFleetKey(ObjectKey* out) { return new (out) ObjectKey(m_fleetKey); }
