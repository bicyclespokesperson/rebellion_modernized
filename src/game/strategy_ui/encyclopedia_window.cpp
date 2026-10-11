#include "strategy_ui/encyclopedia_window.h"
#include "economy/manu_mgr.h"
#include <string>

#include <cstring>
#include <new>

#include "ai/ai_common.h"
#include "app/battle_handoff.h"
#include "app/rebapp_globals.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/registry.h"
#include "data/dat_record.h"
#include "galaxy/system.h"
#include "gameobj/game_obj_root.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_type_db.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_string_field.h"
#include "ui/cool_tab_control.h"
#include "ui/object_list_item.h"
#include "ui/text_scroll_field.h"

extern const char* g_encyBitmapDllName; // 0x6a8750 "encybmap.dll"
extern const char* g_encyTextDllName; // 0x6a8760 "encytext.dll"
extern const char* g_encyDefaultDataDirectory; // 0x6a8770 "EDATA\\"
extern const char* g_encyLocationValueName; // 0x6a8778 "EncyLocation"

namespace
{

constexpr uint32_t kEncyclopediaStyle = 0x56000000;
constexpr uint32_t kGameWindowFlags = 1;
constexpr uint32_t kStringSlot = 0;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kEncyTextSlot = 4;
constexpr uint32_t kEncyBitmapSlot = 5;
constexpr uint32_t kHelperBitmapKind = 10;
constexpr uint32_t kDataFileBitmapKind = 9;
constexpr int16_t kStringGroup = 2;
constexpr int16_t kEncyBitmapStringGroup = 5;
constexpr char kBitBltMode = '@';
constexpr COLORREF kLabelColor = 0x2ffffff;
constexpr int kAlliance = 1;
constexpr int kEmpire = 2;

constexpr uint32_t kEntryIdBase = 0x1000;
constexpr uint32_t kEmpireEntryIdBase = 0x2000;
constexpr uint32_t kRecordIdMask = 0xfff;

// Overlays registered with CoolGameWindow::AddOverlay.
constexpr uint32_t kArticleBackground = 1;
constexpr uint32_t kListBackground = 2;

// Child ids.
constexpr uint32_t kSearchFieldId = 100;
constexpr uint32_t kListId = 0x65;
constexpr uint32_t kPageTabsId = 0x66;
constexpr uint32_t kArticleTabId = 0x67;
constexpr uint32_t kListTabId = 0x68;
constexpr uint32_t kFilterTabsId = 0x6e;
constexpr int kAllEntriesTab = 0x6f;
constexpr int kSystemsTab = 0x70;
constexpr int kCapitalShipsTab = 0x71;
constexpr int kFacilitiesTab = 0x72;
constexpr int kMissionsTab = 0x73;
constexpr int kTroopsTab = 0x74;
constexpr int kCharactersTab = 0x75;
constexpr int kPreviousId = 0x83;
constexpr int kNextId = 0x84;
constexpr int kCloseId = 0xfb;

// Window messages.
constexpr UINT kCloseMessage = 0x40d;
constexpr UINT kHelpMessage = 0x483;
constexpr WPARAM kHelpEncyclopedia = 0xbbe;

constexpr uint16_t kClickSound = 0x260;
constexpr int kCheckedState = 2;

constexpr uint32_t Family(const ObjectKey& key)
{
  return key.m_value >> 24;
}

ResourceStringId StringId(int16_t id)
{
  return ResourceStringId{id, kStringGroup};
}

void DeleteBitmap(DibBitmap* bitmap)
{
  if (bitmap)
    delete bitmap;
}

// Families shown by each category tab: [first, last).
IntPair FilterRange(int tabId)
{
  switch (tabId)
  {
    case kSystemsTab:
      return IntPair(0x90, 0x98);
    case kCapitalShipsTab:
      return IntPair(0x14, 0x20);
    case kFacilitiesTab:
      return IntPair(0x20, 0x30);
    case kMissionsTab:
      return IntPair(0x40, 0x80);
    case kTroopsTab:
      return IntPair(0x10, 0x14);
    case kCharactersTab:
      return IntPair(0x30, 0x40);
    default:
      return IntPair(0, 0);
  }
}

int ManagerHasOwnerKey(GameObjNotifier* manager)
{
  return static_cast<ManuMgr*>(manager)->m_remainingGameObjCount;
}

uint32_t EntryIdOf(GameObjNotifier* object)
{
  return (object->m_record->m_small & kRecordIdMask) + kEntryIdBase;
}

CoolStrobeButton* NewButton(HINSTANCE instance,
                            int x,
                            int y,
                            int width,
                            int height,
                            CoolWindow* parent,
                            uint32_t childId,
                            uint32_t normalId,
                            uint32_t pressedId,
                            DibBitmap* backdrop)
{
  return NewOrNull<CoolStrobeButton>(instance, x, y, width, height, parent, childId, normalId, pressedId, backdrop, 0,
                                     0);
}

uint32_t HiddenStyle(CoolWindow* like)
{
  return like->m_style & ~static_cast<uint32_t>(WS_VISIBLE);
}

} // namespace

// ---- Construction ----------------------------------------------------------------------------------------------

EncyclopediaWindow::EncyclopediaWindow(HINSTANCE instance,
                                       int x,
                                       int y,
                                       int width,
                                       int height,
                                       GalaxyWindow* parent,
                                       uint32_t childId,
                                       const ObjectKey& objectKey,
                                       const NullableKey& typeKey)
: CoolGameWindow(instance, x, y, width, height, parent, kStringSlot, kBitmapSlot, 0, childId, 0, kGameWindowFlags, 0, 0)
{
  ObjectKey object(objectKey);
  NullableKey type = typeKey;
  const ObjectKey unset;
  const NullableKey none = {0};
  m_style = kEncyclopediaStyle;
  m_filterTab = 0;
  m_filterTabs = nullptr;
  m_searchField = nullptr;
  m_list = nullptr;
  m_titleLabel = nullptr;
  m_categoryLabel = nullptr;
  m_picture = nullptr;
  m_text = nullptr;
  m_articleText = nullptr;
  m_nextButton = nullptr;
  m_previousButton = nullptr;
  m_filteredItems = nullptr;
  m_galaxy = parent;
  m_mode = kEncyclopediaList;

  GameObjNotifier* found = nullptr;
  if (object.m_value != unset.m_value)
  {
    found = GetObjectInFamilies_01_ff(parent->m_side, &object);
    if (!found)
      object = unset;
  }
  DatRecord* record = nullptr;
  if (type.OrNull() && type.m_value != none.m_value && object.m_value == unset.m_value)
  {
    record = FindDataRecord(AsLookupKey(&type));
    if (!record)
      type = none;
  }

  if (object.m_value == unset.m_value)
  {
    if (type.m_value == none.m_value)
    {
      m_selected = nullptr;
    }
    else
    {
      m_selected = m_galaxy->m_encyclopediaItems->FindById((record->m_small & kRecordIdMask) + kEntryIdBase);
      m_mode = kEncyclopediaArticle;
    }
  }
  else
  {
    m_selected = m_galaxy->m_encyclopediaItems->FindById(EntryIdOf(found));
    if (!m_selected)
    {
      ObjectKey entryKey;
      GameObjNotifier* entryObject = GetObjectInFamilies_01_ff(m_galaxy->m_side, ResolveEntryKey(&entryKey, &object));
      m_selected = entryObject ? m_galaxy->m_encyclopediaItems->FindById(EntryIdOf(entryObject)) : nullptr;
    }
    m_mode = kEncyclopediaArticle;
  }
  if (!m_selected)
    m_mode = kEncyclopediaList;

  m_text = NewOrNull<std::string>();
  char* dllLocation = GetDllLocation();
  CoolAppRegisterResourceDll(dllLocation, g_encyTextDllName, kEncyTextSlot);
  CoolAppRegisterResourceDll(dllLocation, g_encyBitmapDllName, kEncyBitmapSlot);
  if (dllLocation)
    operator delete(dllLocation);
  m_textModule = CoolAppGetResourceModule(kEncyTextSlot);
  m_bitmapModule = CoolAppGetResourceModule(kEncyBitmapSlot);
}

EncyclopediaWindow::~EncyclopediaWindow()
{
  if (m_filterTabs)
    delete m_filterTabs;
  if (m_searchField)
    delete m_searchField;
  if (m_list)
    delete m_list;
  if (m_categoryLabel)
    delete m_categoryLabel;
  if (m_titleLabel)
    delete m_titleLabel;
  if (m_picture)
    delete m_picture;
  if (m_text)
    delete m_text;
  if (m_pageTabs)
    delete m_pageTabs;
  if (m_articleText)
    delete m_articleText;
  if (m_nextButton)
    delete m_nextButton;
  if (m_previousButton)
    delete m_previousButton;
  if (m_filteredItems)
  {
    m_filteredItems->Clear();
    delete m_filteredItems;
  }
  CoolAppUnloadResourceSlot(kEncyTextSlot);
  CoolAppUnloadResourceSlot(kEncyBitmapSlot);
}

ObjectKey* EncyclopediaWindow::ResolveEntryKey(ObjectKey* out, const ObjectKey* key)
{
  uint32_t family = Family(*key);
  if (family < 0xa0 || family > 0xaf)
  {
    new (out) ObjectKey(*key);
    return out;
  }
  GameObjNotifier* manager = GetObjectInFamilies_a0_b0(m_galaxy->m_side, key);
  if (!manager)
  {
    new (out) ObjectKey();
    return out;
  }
  if (ManagerHasOwnerKey(manager))
  {
    ManufacturingManagerProductKey(reinterpret_cast<ManuMgr*>(manager), out);
    return out;
  }
  GetObjectKey(manager->m_parent, out);
  return out;
}

// ---- Layout (WM_CREATE) ----------------------------------------------------------------------------------------

void EncyclopediaWindow::OnCreate1()
{
  HINSTANCE instance = m_instance;
  HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
  CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right,
           m_galaxy->m_panelArea.bottom);

  DibBitmap* frame;
  DibBitmap* rightEdge;
  if (m_galaxy->m_side == kAlliance)
  {
    frame = NewOrNull<DibBitmap>(art, 0x285fu, kHelperBitmapKind);
    rightEdge = NewOrNull<DibBitmap>(art, 0x2959u, kHelperBitmapKind);
  }
  else
  {
    frame = NewOrNull<DibBitmap>(art, 0x2860u, kHelperBitmapKind);
    rightEdge = NewOrNull<DibBitmap>(art, 0x295du, kHelperBitmapKind);
  }

  HDC windowDc = GetDC(m_hwnd);
  HDC dc = CreateCompatibleDC(windowDc);
  ReleaseDC(m_hwnd, windowDc);
  DibBitmap* articleBackground = NewOrNull<DibBitmap>(frame->Width(), frame->Height(), frame);
  DibBitmap* listBackground =
    NewOrNull<DibBitmap>(static_cast<LONG>(frame->Width()), static_cast<LONG>(frame->Height()), frame, dc);
  DibBitmap* articlePanel = NewOrNull<DibBitmap>(art, 0x2861u, kHelperBitmapKind);
  DibBitmap* listPanel = NewOrNull<DibBitmap>(art, 0x2862u, kHelperBitmapKind);

  articlePanel->Blit(articleBackground, 0xc, 0xe, 0, 0, 0, 0);
  frame->DrawKeyed(articleBackground, 0, 0);
  rightEdge->Blit(articleBackground, 0x19c, 0, 0, 0, 0, 0);
  AddOverlay(articleBackground, kArticleBackground);

  listPanel->Blit(listBackground, 0xc, 0xd, 0, 0, 0, 0);
  frame->DrawKeyed(listBackground, 0, 0);
  rightEdge->Blit(listBackground, 0x19c, 0, 0, 0, 0, 0);
  HGDIOBJ savedBitmap = SelectObject(dc, listBackground->DibSectionHandle());
  const ResourceStringId searchCaption = StringId(0x1843);
  CoolLabel* caption = NewOrNull<CoolLabel>(&searchCaption, 0x24, 0x30, 0, 0, 0u, 0u);
  caption->SetFont(4);
  caption->SetColor(kLabelColor, nullptr);
  caption->Draw(dc);
  AddOverlay(listBackground, kListBackground);
  SelectObject(dc, savedBitmap);
  DeleteDC(dc);
  if (caption)
    delete caption;
  DeleteBitmap(frame);
  DeleteBitmap(rightEdge);
  DeleteBitmap(articlePanel);
  DeleteBitmap(listPanel);

  SetPaintBackgroundById(m_mode == kEncyclopediaList ? kListBackground : kArticleBackground);

  CoolStrobeButton* closeButton;
  if (m_galaxy->m_side == kAlliance)
    closeButton = NewButton(art, 0x1a7, 0x19, 0x20, 0x1f, this, kCloseId, 0x2882, 0x2883, GetPaintBackgroundIfOwned());
  else
    closeButton = NewButton(art, 0x1aa, 0x15, 0x2c, 0x29, this, kCloseId, 0x2888, 0x2889, GetPaintBackgroundIfOwned());
  closeButton->Create(0);
  closeButton->SetClickSound(kClickSound);
  const ResourceStringId closeTip = StringId(0x1954);
  AddToolFromString(closeButton, &closeTip);
  m_children.Insert(closeButton);

  const ResourceStringId titleText = StringId(0x1842);
  m_titleLabel = NewOrNull<CoolLabel>(&titleText, 0, 0, 0, 0, 0u, 0u);
  if (m_titleLabel)
  {
    m_titleLabel->MoveTo(0x24, 0xe);
    m_titleLabel->m_width = 0x15e;
    m_titleLabel->m_layoutFlags = kLabelFixedWidth;
    m_titleLabel->m_drawFlags = DT_CENTER;
    m_titleLabel->SetFont(5);
    m_titleLabel->SetColor(kLabelColor, nullptr);
  }
  const ResourceStringId categoryText = StringId(0x1850);
  m_categoryLabel = NewOrNull<CoolLabel>(&categoryText, 0, 0, 0, 0, 0u, 0u);
  if (m_categoryLabel)
  {
    m_categoryLabel->MoveTo(0x28, 0x77);
    m_categoryLabel->m_width = 0x11b;
    m_categoryLabel->m_layoutFlags = kLabelFixedWidth;
    m_categoryLabel->m_drawFlags = DT_LEFT;
    m_categoryLabel->SetFont(5);
    m_categoryLabel->SetColor(kLabelColor, nullptr);
  }

  if (m_galaxy->m_side == kAlliance)
    m_pageTabs =
      NewOrNull<CoolTabControl>(instance, 0x1a7, 0x5d, 0x20, 0x74, kPageTabsId, this, GetPaintBackgroundIfOwned());
  else
    m_pageTabs =
      NewOrNull<CoolTabControl>(instance, 0x1aa, 0x59, 0x2c, 0x88, kPageTabsId, this, GetPaintBackgroundIfOwned());
  if (m_pageTabs)
  {
    m_pageTabs->Create(0);
    CoolStrobeButton* listTab;
    uint32_t listTabChecked;
    if (m_galaxy->m_side == kAlliance)
    {
      listTab = NewButton(art, 0, 0x36, 0x20, 0x1f, m_pageTabs, kListTabId, 0x2884, 0x2885, m_pageTabs->m_background);
      listTabChecked = 0x2885;
    }
    else
    {
      listTab = NewButton(art, 0, 0x36, 0x2c, 0x29, m_pageTabs, kListTabId, 0x288a, 0x288b, m_pageTabs->m_background);
      listTabChecked = 0x288b;
    }
    listTab->Create(0);
    listTab->SetClickSound(kClickSound);
    const ResourceStringId listTip = StringId(0x1859);
    AddToolFromString(listTab, &listTip);
    listTab->LoadStateBitmap(kCheckedState, listTabChecked);
    m_pageTabs->AddTab(listTab, 0, 0);

    CoolStrobeButton* articleTab;
    uint32_t articleTabChecked;
    if (m_galaxy->m_side == kAlliance)
    {
      articleTab =
        NewButton(art, 0, 0, 0x20, 0x1f, m_pageTabs, kArticleTabId, 0x2886, 0x2887, m_pageTabs->m_background);
      articleTabChecked = 0x2887;
    }
    else
    {
      articleTab =
        NewButton(art, 0, 0, 0x2c, 0x29, m_pageTabs, kArticleTabId, 0x288c, 0x288d, m_pageTabs->m_background);
      articleTabChecked = 0x288d;
    }
    articleTab->Create(0);
    articleTab->SetClickSound(kClickSound);
    const ResourceStringId articleTip = StringId(0x1860);
    AddToolFromString(articleTab, &articleTip);
    articleTab->LoadStateBitmap(kCheckedState, articleTabChecked);
    m_pageTabs->AddTab(articleTab, 0, 0);
  }

  m_filterTabs = NewOrNull<CoolTabControl>(instance, 0x24, 0x4e, 0x169, 0x29, kFilterTabsId, this, listBackground);
  if (m_filterTabs)
  {
    m_filterTabs->m_style = HiddenStyle(closeButton);
    m_filterTabs->Create(0);
    // The Empire's art follows the Alliance's in the bitmap DLL.
    int side = m_galaxy->m_side;
    uint32_t empire2 = side != kAlliance ? 2 : 0;
    uint32_t empireC = side != kAlliance ? 0xc : 0;
    uint32_t empireA = side != kAlliance ? 0xa : 0;
    struct FilterTab
    {
      int x;
      uint32_t id;
      uint32_t normalId;
      uint32_t pressedId;
      int16_t tooltip;
      int select;
    };
    const FilterTab tabs[] = {
      {0, kAllEntriesTab, 0x2864, 0x2863, 0x1850, 1},
      {0xd0, kMissionsTab, 0x2d60 + empire2, 0x2d5f + empire2, 0x1851, 0},
      {0x9c, kFacilitiesTab, 0x2868 + empireC, 0x2867 + empireC, 0x1852, 0},
      {0x138, kCharactersTab, 0x286a + empireC, 0x2869 + empireC, 0x1853, 0},
      {0x68, kCapitalShipsTab, 0x286c + empireC, 0x286b + empireC, 0x1854, 0},
      {0x34, kSystemsTab, 0x286e, 0x286d, 0x1855, 0},
      {0x104, kTroopsTab, 0x2870 + empireA, 0x286f + empireA, 0x1856, 0},
    };
    for (const FilterTab& tab : tabs)
    {
      CoolStrobeButton* button = NewButton(art, tab.x, 0, 0x31, 0x29, m_filterTabs, tab.id, tab.normalId, tab.pressedId,
                                           m_filterTabs->m_background);
      button->Create(0);
      const ResourceStringId tip = StringId(tab.tooltip);
      AddToolFromString(button, &tip);
      m_filterTabs->AddTab(button, tab.select, 0);
    }
  }

  m_searchField = NewOrNull<CoolStringField>(instance, 0x8f, 0x2d, 0xf5, 0x12, this, listBackground, 0u);
  m_searchField->m_childId = kSearchFieldId;
  m_searchField->m_textColor = 0xffffff;
  m_searchField->m_style = HiddenStyle(closeButton);
  m_searchField->Create(0);
  m_searchField->Clear();

  m_list = NewOrNull<CoolDragList>(art, 0x24, 0x89, 0x15e, 0xa0, this, listBackground, kListId, 0x15e, 0x14, 0x287f,
                                   0x2880, 0x287d, 0x287e, 10, 1, nullptr);
  if (m_list)
  {
    m_list->m_options.m_value = kDragListSingleSelect | kDragListNoDrag;
    m_list->m_style = HiddenStyle(m_list);
    m_list->Create(0);
    m_list->SetSortMode(kSortByLabel);
    m_list->m_textFormat = DT_VCENTER | DT_SINGLELINE;
    CoolDragList* list = m_list;
    list->CreateVerticalScrollBar(m_instance, list, 7, 0x299d, list->m_clientHeight, 0, list->m_clientHeight, 0,
                                  nullptr);
    m_list->m_textOffsetX = 5;
    m_filteredItems = NewOrNull<CoolListItemList>(static_cast<int>(kSortByLabel));
    SetFilter(kAllEntriesTab, 1);
  }

  m_articleText = NewOrNull<TextScrollField>(m_instance, 0x11, 0xe7, 0x18b, 0x50, this, articleBackground, 0);
  if (m_articleText)
  {
    m_articleText->m_style = HiddenStyle(closeButton);
    m_articleText->m_scrollBarSlot = 7;
    m_articleText->m_scrollBarId = 0x299d;
    m_articleText->Create(0);
  }

  m_nextButton = NewButton(art, 0x17c, 0xe, 0x15, 0x11, this, kNextId, 0x288e, 0x288f, articleBackground);
  m_nextButton->m_style = HiddenStyle(closeButton);
  m_nextButton->Create(0);
  m_nextButton->LoadStateBitmap(kCheckedState, 0x2890);
  const ResourceStringId nextTip = StringId(0x1857);
  AddToolFromString(m_nextButton, &nextTip);

  m_previousButton = NewButton(art, 0x1c, 0xe, 0x15, 0x11, this, kPreviousId, 0x2891, 0x2892, articleBackground);
  m_previousButton->m_style = HiddenStyle(closeButton);
  m_previousButton->Create(0);
  m_previousButton->LoadStateBitmap(kCheckedState, 0x2893);
  const ResourceStringId previousTip = StringId(0x1858);
  AddToolFromString(m_previousButton, &previousTip);

  m_pageTabs->SelectTab(m_mode == kEncyclopediaArticle && m_selected ? kArticleTabId : kListTabId, 1);
  UiHumanBeginHold();
}

// ---- Messages --------------------------------------------------------------------------------------------------

LRESULT EncyclopediaWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
    case kFieldReturnMessage:
      if (static_cast<int16_t>(wParam) == kSearchFieldId && m_selected)
      {
        SetMode(kEncyclopediaArticle);
        return 0;
      }
      break;
    case kFieldChangedMessage:
      if (wParam == kSearchFieldId)
      {
        uint32_t id = m_list->SelectByPrefix(&m_searchField->m_text, 0, 0);
        m_selected = m_list->FindItem(id);
        return 0;
      }
      break;
    case kCloseMessage:
      PostCloseRequest();
      return 0;
    case kHelpMessage:
      if (wParam == kHelpEncyclopedia && m_mode != kEncyclopediaList)
      {
        m_pageTabs->SelectTab(kListTabId, 1);
        return 0;
      }
      break;
    default:
      return CoolGameWindow::WindowProc(message, wParam, lParam);
  }
  return 0;
}

void EncyclopediaWindow::OnCommand(int, int command, int)
{
  uint32_t code = static_cast<uint32_t>(command) >> 16;
  RECT area;
  switch (command & 0xffff)
  {
    case kListId:
    {
      CoolListItemList selection(kSortById);
      m_list->CopySelectionTo(&selection);
      if (selection.m_head)
      {
        m_selected = m_list->FindItem(static_cast<CoolListItem*>(selection.m_head)->m_id);
        if (m_selected)
        {
          if (static_cast<uint16_t>(code) == kDragListActivated)
            m_pageTabs->SelectTab(kArticleTabId, 1);
          else if (static_cast<uint16_t>(code) == kDragListSelectionChanged)
            m_searchField->SetText(m_selected->m_text.c_str());
        }
      }
      return;
    }
    case kPageTabsId:
      if (code == kArticleTabId)
      {
        if (!m_selected)
        {
          m_pageTabs->SelectTab(kListTabId, 1);
          return;
        }
        SetMode(kEncyclopediaArticle);
        break;
      }
      if (code == kListTabId)
      {
        SetMode(kEncyclopediaList);
        SetFocus(m_searchField->m_hwnd);
      }
      return;
    case kFilterTabsId:
    {
      SetFilter(static_cast<int>(code), 0);
      uint32_t id = m_list->SelectByPrefix(&m_searchField->m_text, 0, 0);
      m_selected = m_list->FindItem(id);
      SetFocus(m_list->m_hwnd);
      return;
    }
    case kPreviousId:
    case kNextId:
    {
      CoolListItem* item = static_cast<int16_t>(command) == kNextId ? m_selected->Next() : m_selected->Previous();
      if (item)
      {
        SetRect(&area, 0xc, 0x1f, 0x19c, 0xe7);
        m_list->SelectById(item->m_id, 0);
        m_searchField->SetText(item->m_text.c_str());
        m_selected = item;
        LoadArticle();
        InvalidateRect(m_hwnd, &area, FALSE);
        SetRect(&area, 0x24, 0xe, 0x182, 0x1e);
        InvalidateRect(m_hwnd, &area, FALSE);
      }
      break;
    }
    case kCloseId:
      PostCloseRequest();
      return;
    default:
      return;
  }
  SetFocus(m_articleText->m_hwnd);
}

void EncyclopediaWindow::OnKeyDown(WPARAM key, LPARAM lParam)
{
  switch (key)
  {
    case VK_TAB:
      return;
    case VK_ESCAPE:
      PostCloseRequest();
      return;
    case VK_PRIOR:
    case VK_NEXT:
    case VK_UP:
    case VK_DOWN:
      SetFocus(m_list->m_hwnd);
      PostMessageA(m_list->m_hwnd, WM_KEYDOWN, key, lParam);
      return;
    case VK_LEFT:
    case VK_RIGHT:
      if (m_mode == kEncyclopediaList)
      {
        CoolStrobeButton* current = m_filterTabs->m_selected;
        uint32_t currentId = current ? current->m_childId : 0;
        AvlNode* tab = m_filterTabs->m_children.FindById(currentId);
        tab = key == VK_LEFT ? tab->Predecessor() : tab->m_successor;
        if (!tab)
        {
          AvlNode* first = m_filterTabs->m_children.First();
          if (first)
            m_filterTabs->SelectTab(static_cast<CoolWindow*>(first)->m_childId, 1);
        }
        else
        {
          do
          {
            CoolWindow* window = static_cast<CoolWindow*>(tab);
            if (IsWindowVisible(window->m_hwnd))
            {
              m_filterTabs->SelectTab(window->m_childId, 1);
              break;
            }
            tab = key == VK_LEFT ? tab->Predecessor() : tab->m_successor;
          } while (tab);
        }
      }
      if (m_mode == kEncyclopediaArticle)
      {
        if (key == VK_RIGHT)
          OnCommand(kNextId, kNextId, 0);
        else
          OnCommand(kPreviousId, kPreviousId, 0);
      }
      return;
    default:
      m_parent->WindowProc(WM_KEYDOWN, key, lParam);
      return;
  }
}

void EncyclopediaWindow::PostCloseRequest()
{
  UiHumanEndHold();
  CoolGameWindow::PostCloseRequest();
}

// ---- Pages -----------------------------------------------------------------------------------------------------

void EncyclopediaWindow::DrawOverlay(HDC dc)
{
  if (m_mode == kEncyclopediaArticle && m_picture)
  {
    m_picture->DrawToDc(dc, kBitBltMode, SRCCOPY, 0xc, 0x1f, 0, 0, 0, 0);
    m_titleLabel->Draw(dc);
    return;
  }
  m_categoryLabel->Draw(dc);
  m_titleLabel->Draw(dc);
}

void EncyclopediaWindow::SetFilter(int tabId, uint32_t reload)
{
  if (m_filterTab == tabId)
    return;
  if (m_mode == kEncyclopediaArticle && !reload)
    return;
  if (tabId == kAllEntriesTab || reload)
  {
    m_list->SetItemList(m_galaxy->m_encyclopediaItems);
    m_list->Refresh();
    if (reload && m_mode != kEncyclopediaArticle)
      m_selected = nullptr;
  }
  else
  {
    IntPair range = FilterRange(tabId);
    CoolListItem* item = m_galaxy->m_encyclopediaItems->First();
    m_filteredItems->Clear();
    for (; item; item = item->Next())
    {
      ObjectKey key;
      static_cast<ObjectListItem*>(item)->GetKey(&key);
      uint32_t family = Family(key);
      if (range.m_first <= family && family < range.m_second)
      {
        ObjectKey itemKey;
        ObjectListItem* copy =
          NewOrNull<ObjectListItem>(item->m_id, static_cast<ObjectListItem*>(item)->GetKey(&itemKey));
        if (!copy)
          continue;
        copy->m_text = item->m_text.c_str();
        m_filteredItems->Insert(copy);
      }
    }
    m_list->SetItemList(m_filteredItems);
    m_list->Refresh();
  }
  m_filterTab = tabId;
  CoolWindow* tab = m_filterTabs->m_children.FindById(static_cast<uint32_t>(tabId));
  m_categoryLabel->SetText(tab->m_tooltipText);
  RECT area;
  SetRect(&area, 0x28, 0x77, 0x143, 0x8a);
  InvalidateRect(m_hwnd, &area, FALSE);
}

void EncyclopediaWindow::SetMode(int mode)
{
  if (mode == kEncyclopediaList)
  {
    SetPaintBackgroundById(kListBackground);
    m_articleText->Hide();
    m_previousButton->Hide();
    m_nextButton->Hide();
    m_list->Show(SW_SHOW);
    m_searchField->Show(SW_SHOW);
    m_filterTabs->Show(SW_SHOW);
    m_pageTabs->m_children.FindById(kArticleTabId)->Enable();
    m_pageTabs->m_children.FindById(kListTabId)->Disable();
    if (m_selected)
    {
      m_list->SelectById(m_selected->m_id, 0);
      m_list->ScrollIntoView(m_selected->m_id);
      m_searchField->Clear();
      m_searchField->SetText(m_selected->m_text.c_str());
      m_searchField->SetSelection(0, 0);
    }
    const ResourceStringId title = StringId(0x1842);
    m_titleLabel->SetTextFromResource(&title);
  }
  else if (mode == kEncyclopediaArticle)
  {
    if (!m_selected)
      return;
    SetPaintBackgroundById(kArticleBackground);
    m_list->Hide();
    m_searchField->Hide();
    m_filterTabs->Hide();
    m_articleText->Show(SW_SHOW);
    SetFocus(m_articleText->m_hwnd);
    m_nextButton->Show(SW_SHOW);
    m_previousButton->Show(SW_SHOW);
    m_pageTabs->m_children.FindById(kArticleTabId)->Disable();
    m_pageTabs->m_children.FindById(kListTabId)->Enable();
    LoadArticle();
  }
  m_mode = mode;
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

int EncyclopediaWindow::SystemPictureId(int pictureIndex)
{
  if (pictureIndex >= 1 && pictureIndex <= 0x17)
    return 0x2b5b + pictureIndex;
  switch (pictureIndex)
  {
    case 0x18:
      return 0x2b75;
    case 0x19:
      return 0x2b73;
    case 0x1a:
      return 0x2b74;
    default:
      return 0;
  }
}

void EncyclopediaWindow::LoadArticle()
{
  if (m_picture)
  {
    delete m_picture;
    m_picture = nullptr;
  }
  ObjectKey key;
  static_cast<ObjectListItem*>(m_selected)->GetKey(&key);
  uint32_t family = Family(key);
  uint32_t pictureId;
  if (family >= 0x90 && family < 0x98)
  {
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_galaxy->m_side, &key);
    pictureId = static_cast<uint32_t>(SystemPictureId(static_cast<System*>(system)->System::GetRecordField44()));
  }
  else if ((family >= 0x40 && family < 0x80) || (family >= 0x08 && family < 0x10))
  {
    // Missions and fleets have one picture per side.
    uint32_t recordId = m_selected->m_id & kRecordIdMask;
    pictureId = recordId + (m_galaxy->m_side == kEmpire ? kEmpireEntryIdBase : kEntryIdBase);
  }
  else
  {
    pictureId = m_selected->m_id;
  }
  m_picture = LoadEncyclopediaPicture(pictureId, m_bitmapModule);

  if (m_textModule)
  {
    HRSRC resource = FindResourceA(m_textModule, MAKEINTRESOURCEA(m_selected->m_id & 0xffff), RT_RCDATA);
    if (resource)
    {
      HGLOBAL handle = LoadResource(m_textModule, resource);
      const char* text = static_cast<const char*>(LockResource(handle));
      if (m_text)
        delete m_text;
      m_text = NewOrNull<std::string>(StringFromText(text));
      FreeResource(handle);
      TextScrollField* field = m_articleText;
      field->m_text = *m_text;
      field->m_caret.m_index = 0;
      field->Reset();
      m_articleText->Show(SW_SHOW);
      if (m_text)
        delete m_text;
      m_text = nullptr;
    }
  }
  UpdateNavigationButtons(m_selected);
  m_titleLabel->SetText(m_selected->m_text.c_str());
}

void EncyclopediaWindow::UpdateNavigationButtons(CoolListItem* item)
{
  if (!item)
  {
    m_previousButton->Disable();
    m_nextButton->Disable();
    return;
  }
  if (item->Previous())
    m_previousButton->Enable();
  else
    m_previousButton->Disable();
  if (item->Next())
    m_nextButton->Enable();
  else
    m_nextButton->Disable();
}

// ---- EData files -----------------------------------------------------------------------------------------------

std::string GetEncyclopediaDataPath()
{
  std::string key;
  std::string path;
  {
    std::string product("Star Wars Rebellion");
    std::string separator("\\");
    std::string company("SOFTWARE\\LucasArts Entertainment Company LLC");
    key += company;
    key += separator;
    key += product;
  }
  uint8_t* location = ReadRegistryValue(key.c_str(), g_encyLocationValueName);
  if (location)
  {
    path = StringFromText(reinterpret_cast<const char*>(location));
    operator delete(location);
  }
  if (path.empty())
    path = StringFromText(g_encyDefaultDataDirectory);
  if (!path.empty() && path.back() != '\\')
    path.push_back('\\');
  return path;
}

DibBitmap* LoadEncyclopediaPicture(uint32_t pictureId, HMODULE module)
{
  const ResourceStringId fileNameId = {static_cast<int16_t>(pictureId), kEncyBitmapStringGroup};
  std::string fileName = LoadStringResource(&fileNameId);
  std::string directory = GetEncyclopediaDataPath();
  std::string path = directory + fileName;
  return NewOrNull<DibBitmap>(module, static_cast<const char*>(path.c_str()), kDataFileBitmapKind);
}

const char* g_encyBitmapDllName = nullptr;
const char* g_encyDefaultDataDirectory = nullptr;
const char* g_encyLocationValueName = nullptr;
const char* g_encyTextDllName = nullptr;
