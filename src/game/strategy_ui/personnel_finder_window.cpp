#include "strategy_ui/personnel_finder_window.h"
#include <string>

#include <new>

#include "character/character.h"
#include "character/special_forces.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "data/dat_record.h"
#include "fleet/capship.h"
#include "fleet/fleet_iterator.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/finder_helpers.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/visible_item_list.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_string_field.h"
#include "ui/cool_tab_control.h"
#include "ui/object_list_item.h"

extern const char* g_detailCloseText; // 0x6a878c " ) "
extern const char* g_detailOpenText; // 0x6a8790 " ( "

namespace
{

constexpr uint32_t kStringSlot = 2;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kHelperBitmapKind = 10;
constexpr int16_t kStringGroup = 2;
constexpr COLORREF kLabelColor = 0x2ffffff;
constexpr uint16_t kClickSound = 0x260;
constexpr int kAlliance = 1;
constexpr int kEmpire = 2;
constexpr uint32_t kStateFilterExisting = 3;

constexpr uint32_t kFilterTabsId = 100;
constexpr uint32_t kPageTabsId = 0x6e;
constexpr uint32_t kCloseId = 200;
constexpr uint32_t kOpenId = 0xc9;
constexpr uint32_t kListId = 0xca;
constexpr uint32_t kNameFieldId = 0xcb;
constexpr uint32_t kAllianceTabId = 1;
constexpr uint32_t kEmpireTabId = 2;
constexpr uint32_t kForcesPageId = 2;
constexpr uint32_t kCharactersPageId = 3;

constexpr uint32_t kPlainBackground = 2;
constexpr uint32_t kEmpireForcesBackground = 3;
constexpr uint32_t kAllianceCharactersBackground = 4;
constexpr uint32_t kEmpireCharactersBackground = 5;

constexpr int kCellCount = 4;

constexpr uint32_t kGameObjUsable = 0x1;
constexpr uint32_t kGameObjCreated = 0x2;
constexpr uint32_t kGameObjDestroyed = 0x8;
constexpr uint32_t kGameObjEnroute = 0x10;
constexpr uint32_t kCharacterCaptured = 0x1;

ResourceStringId StringId(int16_t id)
{
  return ResourceStringId{id, kStringGroup};
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

uint32_t SelectedTabId(CoolTabControl* tabs)
{
  return tabs->m_selected ? tabs->m_selected->m_childId : 0;
}

bool FamilyIn(const ObjectKey& key, uint32_t first, uint32_t last)
{
  return key.Family() >= first && key.Family() < last;
}

void AppendParenthesized(std::string* text, const std::string* inner)
{
  {
    std::string open = StringFromText(g_detailOpenText);
    *text += open;
  }
  *text += *inner;
  {
    std::string close = StringFromText(g_detailCloseText);
    *text += close;
  }
}

} // namespace

PersonnelFinderWindow::PersonnelFinderWindow(HINSTANCE instance,
                                             int x,
                                             int y,
                                             int width,
                                             int height,
                                             GalaxyWindow* parent,
                                             uint32_t childId)
: CustomDialogBox(instance, x, y, width, height, parent, childId, nullptr)
{
  m_stringSlot = kStringSlot;
  m_bitmapSlot = kBitmapSlot;
  m_flags |= kGameWindowPopup;
  m_filterTabs = nullptr;
  m_nameField = nullptr;
  m_list = nullptr;
  m_filterLabel = nullptr;
  m_galaxy = parent;
  m_forceRows = NewOrNull<VisibleItemList>(kSortByLabel);
  m_characterRows = NewOrNull<VisibleItemList>(kSortByLabel);
}

PersonnelFinderWindow::~PersonnelFinderWindow()
{
  if (m_filterTabs)
    delete m_filterTabs;
  if (m_pageTabs)
    delete m_pageTabs;
  if (m_nameField)
    delete m_nameField;
  if (m_list)
    delete m_list;
  if (m_filterLabel)
    delete m_filterLabel;
  if (m_forceRows)
  {
    ClearAllRows(m_forceRows);
    delete m_forceRows;
  }
  if (m_characterRows)
  {
    ClearAllRows(m_characterRows);
    delete m_characterRows;
  }
  for (HBRUSH brush : m_brushes)
    if (brush)
      DeleteObject(brush);
}

void PersonnelFinderWindow::OnCreate1()
{
  HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
  CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right,
           m_galaxy->m_panelArea.bottom);

  auto* panelC = NewOrNull<DibBitmap>(art, 0x292au, kHelperBitmapKind);
  auto* panelD = NewOrNull<DibBitmap>(art, 0x292bu, kHelperBitmapKind);
  auto* panelA = NewOrNull<DibBitmap>(art, 0x2928u, kHelperBitmapKind);
  auto* panelB = NewOrNull<DibBitmap>(art, 0x2929u, kHelperBitmapKind);
  DibBitmap* frame = nullptr;
  DibBitmap* rightEdge = nullptr;
  if (m_galaxy->m_side == kAlliance)
  {
    frame = NewOrNull<DibBitmap>(art, 0x285fu, kHelperBitmapKind);
    rightEdge = NewOrNull<DibBitmap>(art, 0x295au, kHelperBitmapKind);
  }
  else if (m_galaxy->m_side == kEmpire)
  {
    frame = NewOrNull<DibBitmap>(art, 0x2860u, kHelperBitmapKind);
    rightEdge = NewOrNull<DibBitmap>(art, 0x295eu, kHelperBitmapKind);
  }

  m_brushes[0] = CreateSolidBrush(0x20000ff);
  m_brushes[1] = CreateSolidBrush(0x200ff00);
  m_brushes[2] = CreateSolidBrush(0x808080);
  m_brushes[3] = CreateSolidBrush(0x80);
  m_brushes[4] = CreateSolidBrush(0x8000);

  HDC windowDc = GetDC(m_hwnd);
  HDC dc = CreateCompatibleDC(windowDc);
  ReleaseDC(m_hwnd, windowDc);
  auto newBackground = [&]
  {
    return NewOrNull<DibBitmap>(static_cast<LONG>(frame->Width()), static_cast<LONG>(frame->Height()), frame, dc);
  };
  DibBitmap* backgroundA = newBackground();
  DibBitmap* backgroundB = newBackground();
  DibBitmap* backgroundC = newBackground();
  DibBitmap* backgroundD = newBackground();
  panelA->Blit(backgroundA, 0xc, 0xd, 0, 0, 0, 0);
  panelB->Blit(backgroundB, 0xc, 0xd, 0, 0, 0, 0);
  panelC->Blit(backgroundC, 0xc, 0xd, 0, 0, 0, 0);
  panelD->Blit(backgroundD, 0xc, 0xd, 0, 0, 0, 0);
  frame->DrawKeyed(backgroundA, 0, 0);
  frame->DrawKeyed(backgroundB, 0, 0);
  frame->DrawKeyed(backgroundC, 0, 0);
  frame->DrawKeyed(backgroundD, 0, 0);
  rightEdge->Blit(backgroundA, 0x19c, 0, 0, 0, 0, 0);
  rightEdge->Blit(backgroundB, 0x19c, 0, 0, 0, 0, 0);
  rightEdge->Blit(backgroundC, 0x19c, 0, 0, 0, 0, 0);
  rightEdge->Blit(backgroundD, 0x19c, 0, 0, 0, 0, 0);

  const ResourceStringId titleText = StringId(0x1890);
  auto* title = NewOrNull<CoolLabel>(&titleText, 0x24, 0xe, 0, 0, 0u, 0u);
  title->SetFont(5);
  title->SetColor(kLabelColor, nullptr);
  title->m_width = 0x15e;
  title->m_layoutFlags = kLabelFixedWidth;
  title->m_drawFlags = 1;
  const ResourceStringId searchText = StringId(0x1891);
  auto* search = NewOrNull<CoolLabel>(&searchText, 0x24, 0x30, 0, 0, 0u, 0u);
  search->SetFont(4);
  search->SetColor(kLabelColor, nullptr);
  DibBitmap* const drawOrder[] = {backgroundC, backgroundD, backgroundA, backgroundB};
  for (DibBitmap* background : drawOrder)
  {
    HGDIOBJ saved = SelectObject(dc, background->DibSectionHandle());
    title->Draw(dc);
    search->Draw(dc);
    SelectObject(dc, saved);
  }
  if (title)
    delete title;
  if (search)
    delete search;
  SetPaintBackground(backgroundC, kPlainBackground);
  AddOverlay(backgroundD, kEmpireForcesBackground);
  AddOverlay(backgroundA, kAllianceCharactersBackground);
  AddOverlay(backgroundB, kEmpireCharactersBackground);
  DibBitmap* const sources[] = {panelA, panelB, panelC, panelD, frame, rightEdge};
  for (DibBitmap* bitmap : sources)
    if (bitmap)
      delete bitmap;

  int tabsX, tabsY, tabsWidth, tabsHeight;
  if (m_galaxy->m_side == kAlliance)
  {
    tabsX = 0x1a7;
    tabsY = 0x93;
    tabsWidth = 0x20;
    tabsHeight = 0x55;
  }
  else
  {
    tabsX = 0x1aa;
    tabsY = 0x8f;
    tabsWidth = 0x2a;
    tabsHeight = 0x5f;
  }
  m_pageTabs = NewOrNull<CoolTabControl>(m_instance, tabsX, tabsY, tabsWidth, tabsHeight, kPageTabsId, this,
                                         GetPaintBackgroundIfOwned());
  if (m_pageTabs)
  {
    m_pageTabs->Create(0);
    int buttonX, openY, buttonWidth, buttonHeight;
    uint32_t openNormal, openPressed;
    if (m_galaxy->m_side == kAlliance)
    {
      buttonX = 0x1a7;
      openY = 0x5d;
      buttonWidth = 0x20;
      buttonHeight = 0x1f;
      openNormal = 0x2916;
      openPressed = 0x2917;
    }
    else
    {
      buttonX = 0x1aa;
      openY = 0x59;
      buttonWidth = 0x2a;
      buttonHeight = 0x29;
      openNormal = 0x2918;
      openPressed = 0x2919;
    }
    CoolStrobeButton* openButton = NewButton(art, buttonX, openY, buttonWidth, buttonHeight, this, kOpenId, openNormal,
                                             openPressed, GetPaintBackgroundIfOwned());
    if (openButton)
    {
      openButton->Create(0);
      const ResourceStringId tip = StringId(0x1953);
      AddToolFromString(openButton, &tip);
      m_children.Insert(openButton);
    }
    uint32_t charactersNormal, charactersPressed;
    if (m_galaxy->m_side == kAlliance)
    {
      charactersNormal = 0x292e;
      charactersPressed = 0x292f;
    }
    else
    {
      charactersNormal = 0x2930;
      charactersPressed = 0x2931;
    }
    CoolStrobeButton* charactersTab = NewButton(art, 0, 0, buttonWidth, buttonHeight, m_pageTabs, kCharactersPageId,
                                                charactersNormal, charactersPressed, m_pageTabs->m_background);
    if (charactersTab)
    {
      charactersTab->Create(0);
      const ResourceStringId tip = StringId(0x1899);
      AddToolFromString(charactersTab, &tip);
      m_pageTabs->AddTab(charactersTab, 0, 0);
      charactersTab->LoadStateBitmap(2, charactersPressed);
    }
    uint32_t forcesNormal, forcesPressed;
    if (m_galaxy->m_side == kAlliance)
    {
      forcesNormal = 0x2932;
      forcesPressed = 0x2933;
    }
    else
    {
      forcesNormal = 0x2934;
      forcesPressed = 0x2935;
    }
    CoolStrobeButton* forcesTab = NewButton(art, 0, 0x36, buttonWidth, buttonHeight, m_pageTabs, kForcesPageId,
                                            forcesNormal, forcesPressed, m_pageTabs->m_background);
    if (forcesTab)
    {
      forcesTab->Create(0);
      const ResourceStringId tip = StringId(0x1898);
      AddToolFromString(forcesTab, &tip);
      m_pageTabs->AddTab(forcesTab, 0, 0);
      forcesTab->LoadStateBitmap(2, forcesPressed);
    }
  }

  m_filterTabs =
    NewOrNull<CoolTabControl>(m_instance, 0x24, 0x48, 0x99, 0x29, kFilterTabsId, this, GetPaintBackgroundIfOwned());
  if (m_filterTabs)
  {
    m_filterTabs->Create(0);
    CoolStrobeButton* allianceTab =
      NewButton(art, 0, 0, 0x31, 0x29, m_filterTabs, kAllianceTabId, 0x2906, 0x2907, m_filterTabs->m_background);
    if (allianceTab)
    {
      allianceTab->Create(0);
      const ResourceStringId tip = StringId(0x1893);
      AddToolFromString(allianceTab, &tip);
      m_filterTabs->AddTab(allianceTab, 0, 0);
    }
    CoolStrobeButton* empireTab =
      NewButton(art, 0x34, 0, 0x31, 0x29, m_filterTabs, kEmpireTabId, 0x2909, 0x290a, m_filterTabs->m_background);
    if (empireTab)
    {
      empireTab->Create(0);
      const ResourceStringId tip = StringId(0x1894);
      AddToolFromString(empireTab, &tip);
      m_filterTabs->AddTab(empireTab, 0, 0);
    }
  }

  if (m_galaxy->m_side == kAlliance)
    AddButton(0x1a7, 0x19, 0x2912, 0x2913, kCloseId, 0x1954, 0, 0, 0, 0, 0, 0);
  else
    AddButton(0x1aa, 0x15, 0x2914, 0x2915, kCloseId, 0x1954, 0, 0, 0, 0, 0, 0);
  if (auto* close = static_cast<CoolStrobeButton*>(m_children.FindById(kCloseId)))
    close->SetClickSound(kClickSound);
  if (auto* open = static_cast<CoolStrobeButton*>(m_children.FindById(kOpenId)))
    open->SetClickSound(kClickSound);

  m_nameField = NewOrNull<CoolStringField>(m_instance, 0x8f, 0x2d, 0xfa, 0x12, this, GetPaintBackgroundIfOwned(), 0u);
  m_nameField->m_childId = kNameFieldId;
  m_nameField->m_textColor = 0xffffff;
  m_nameField->Create(0);
  m_nameField->SetText(nullptr);
  SetCaretPos(0, 0);

  m_filterLabel = NewOrNull<CoolLabel>(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
  if (m_filterLabel)
  {
    m_filterLabel->MoveTo(0x28, 0x73);
    m_filterLabel->m_width = 0x11b;
    m_filterLabel->m_height = 0x10;
    m_filterLabel->m_layoutFlags = kLabelFixedWidth | kLabelFixedHeight;
    m_filterLabel->m_drawFlags = 0;
    m_filterLabel->SetFont(5);
    m_filterLabel->SetColor(kLabelColor, nullptr);
    HDC labelDc = GetDC(m_hwnd);
    m_filterLabel->Measure(labelDc);
    ReleaseDC(m_hwnd, labelDc);
  }

  m_list = NewOrNull<CoolDragList>(art, 0x25, 0x8f, 0x15d, 0xa1, this, GetPaintBackgroundIfOwned(), kListId, 0x15e,
                                   0x14, 0x287f, 0x2880, 0x287d, 0x287e, 10, 1, nullptr);
  if (m_list)
  {
    m_list->m_options.m_value = kDragListSingleSelect | kDragListNoDrag;
    m_list->Create(0);
    m_list->SetSortMode(kSortByLabel);
    m_list->m_textFormat = DT_VCENTER | DT_SINGLELINE;
    CoolDragList* list = m_list;
    list->CreateVerticalScrollBar(m_instance, list, 7, 0x299d, list->m_clientHeight, 0, list->m_clientHeight, 0,
                                  nullptr);
    m_list->m_textOffsetX = 5;
    for (GameObjNotifier* character = GetFirstObjectInRangeForSide(m_galaxy->m_side, &family::kCharacters, 0);
         character; character = static_cast<GameObj*>(character)->FindNextSibling(&family::kCharacters, 0))
    {
      ObjectKey key;
      auto* row = NewOrNull<ObjectListItem>(character->GetKey(&key));
      row->m_visible = 0;
      row->m_text = character->m_record->m_name.c_str();
      row->m_userData = static_cast<Character*>(character)->GetRecordSide();
      m_characterRows->Insert(row);
    }
  }

  m_filterTabs->SelectTab(m_galaxy->m_side == kAlliance ? kAllianceTabId : kEmpireTabId, 1);
  m_pageTabs->SelectTab(kCharactersPageId, 1);
  SetFocus(m_nameField->m_hwnd);
  DeleteDC(dc);
}

void PersonnelFinderWindow::DrawOverlay(HDC dc)
{
  m_filterLabel->Draw(dc);
}

void PersonnelFinderWindow::OnCommand(int, int command, int)
{
  const uint32_t code = static_cast<uint32_t>(command) >> 16;
  switch (command & 0xffff)
  {
    case kFilterTabsId:
    {
      const uint32_t page = SelectedTabId(m_pageTabs);
      if (page == kForcesPageId)
        BuildList(m_forceRows, 0);
      else if (page == kCharactersPageId)
        BuildList(m_characterRows, 0);
      break;
    }
    case kPageTabsId:
      if (code == kForcesPageId)
      {
        m_pageTabs->m_children.FindById(kCharactersPageId)->Enable();
        m_pageTabs->m_children.FindById(kForcesPageId)->Disable();
        BuildList(m_forceRows, 0);
      }
      else if (code == kCharactersPageId)
      {
        m_pageTabs->m_children.FindById(kCharactersPageId)->Disable();
        m_pageTabs->m_children.FindById(kForcesPageId)->Enable();
        BuildList(m_characterRows, 0);
      }
      break;
    case kCloseId:
      PostCloseRequest();
      break;
    case kOpenId:
    {
      const ObjectKey unset;
      if (m_selected.m_value != unset.m_value)
      {
        m_galaxy->OpenWindowForObject(&m_selected);
        PostCloseRequest();
      }
      break;
    }
    case kListId:
    {
      CoolListItemList selection(kSortById);
      m_list->CopySelectionTo(&selection);
      auto* first = static_cast<CoolListItem*>(selection.First());
      if (!first)
        break;
      auto* row = static_cast<ObjectListItem*>(m_list->m_items->FindById(first->m_id));
      ObjectKey key;
      row->GetKey(&key);
      m_selected = key;
      if (static_cast<uint16_t>(code) == kDragListActivated && row)
      {
        m_galaxy->OpenWindowForObject(&m_selected);
        PostCloseRequest();
      }
      else if (static_cast<uint16_t>(code) == kDragListSelectionChanged)
      {
        ObjectKey rowKey;
        row->GetKey(&rowKey);
        auto* object = static_cast<GameObj*>(GetObjectInFamilies_01_ff(m_galaxy->m_side, &rowKey));
        if (object && (object->m_stateFlags & 0x40))
        {
          const std::string* name = FamilyIn(m_selected, 0x30, 0x3c) ? &object->m_record->m_name : object->GetName();
          m_nameField->SetText(name->c_str());
        }
      }
      break;
    }
  }
}

LRESULT PersonnelFinderWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  LRESULT result = 0;
  if (message < 0x11)
  {
    if (message == WM_CLOSE)
    {
      for (HBRUSH& brush : m_brushes)
        if (DeleteObject(brush))
          brush = nullptr;
      return CoolGameWindow::WindowProc(WM_CLOSE, wParam, lParam);
    }
    if (message == WM_SETFOCUS)
    {
      SetFocus(m_nameField->m_hwnd);
      return 0;
    }
    return CoolGameWindow::WindowProc(message, wParam, lParam);
  }
  if (message == kFieldReturnMessage)
  {
    const ObjectKey unset;
    if (static_cast<int16_t>(wParam) == kNameFieldId && m_selected.m_value != unset.m_value &&
        m_galaxy->OpenWindowForObject(&m_selected))
      PostCloseRequest();
  }
  else if (message == kFieldChangedMessage)
  {
    if (static_cast<int16_t>(wParam) != kNameFieldId)
      return 0;
    ObjectListItem* row = nullptr;
    const std::string* typed = &m_nameField->m_text;
    CoolListItemList* rows = m_list->m_items;
    if (!typed->empty())
    {
      const uint32_t id = m_list->SelectByPrefix(typed, 0, 0);
      row = static_cast<ObjectListItem*>(rows->FindById(id));
    }
    if (!row)
    {
      m_selected = ObjectKey();
    }
    else
    {
      ObjectKey key;
      row->GetKey(&key);
      m_selected = key;
    }
  }
  else
  {
    result = CoolGameWindow::WindowProc(message, wParam, lParam);
  }
  return result;
}

void PersonnelFinderWindow::OnKeyDown(WPARAM key, LPARAM lParam)
{
  FinderOnKeyDown(this, m_filterTabs, m_nameField, m_list, key, lParam);
}

void PersonnelFinderWindow::BuildList(CoolListItemList* rows, int)
{
  const uint32_t filterTab = SelectedTabId(m_filterTabs);
  int fontSlot = 4;
  const uint32_t pageTab = SelectedTabId(m_pageTabs);
  const uint32_t side = (filterTab != kAllianceTabId) + 1;
  if (pageTab == kForcesPageId)
  {
    SetPaintBackgroundById(filterTab == kAllianceTabId ? kPlainBackground : kEmpireForcesBackground);
    rows->Clear();
    HDC windowDc = GetDC(m_hwnd);
    HDC dc = CreateCompatibleDC(windowDc);
    ReleaseDC(m_hwnd, windowDc);
    int rowCount = 0;
    auto addRow = [&](GameObj* object, const ObjectKey& key)
    {
      auto* row = NewOrNull<ObjectListItem>(static_cast<uint32_t>(++rowCount), &key);
      row->m_state.m_value = kListItemDisabled;
      row->m_text = object->GetName()->c_str();
      auto* picture =
        NewOrNull<DibBitmap>(static_cast<LONG>(0x15e), static_cast<LONG>(0x14), GetPaintBackgroundIfOwned(), dc);
      picture->FillRect(0, 0, picture->Width(), picture->Height(), 0);
      HGDIOBJ saved = SelectObject(dc, picture->DibSectionHandle());
      DrawForceCounts(object, side, dc);
      SelectObject(dc, saved);
      row->SetImages(picture, nullptr);
      rows->Insert(row);
    };
    for (GameObjNotifier* system = GetFirstObjectInFamilies_90_98(m_galaxy->m_side); system;
         system = static_cast<GameObj*>(system)->FindNextSibling(&family::kSystems, 1))
    {
      SpecialForceIterator forces(system, kStateFilterExisting, side);
      if (forces.Count() != 0)
      {
        ObjectKey key;
        forces.Begin();
        forces.Current()->GetKey(&key);
        addRow(static_cast<GameObj*>(system), key);
      }
      FleetIterator fleets(system, kStateFilterExisting, side);
      for (fleets.Begin(); fleets.Current(); fleets.Advance())
      {
        auto* fleet = static_cast<GameObj*>(fleets.Current());
        if (fleet->ControlSide() != side)
          continue;
        bool holdsForces = false;
        CapShipIterator ships(fleet, kStateFilterExisting);
        for (ships.Begin(); ships.Current(); ships.Advance())
        {
          SpecialForceIterator shipForces(ships.Current(), kStateFilterExisting);
          if (shipForces.Count() != 0)
            holdsForces = true;
        }
        if (!holdsForces)
          continue;
        ObjectKey key;
        addRow(fleet, *fleet->GetKey(&key));
      }
    }
    DeleteDC(dc);
  }
  else
  {
    if (pageTab != kCharactersPageId)
      return;
    SetPaintBackgroundById(filterTab == kAllianceTabId ? kAllianceCharactersBackground : kEmpireCharactersBackground);
    fontSlot = 10;
    for (const auto& ownedRow : rows->Entries())
    {
      auto* row = static_cast<ObjectListItem*>(ownedRow.get());
      ObjectKey key;
      row->GetKey(&key);
      auto* character = static_cast<Character*>(GetObjectInFamilies_30_3c(m_galaxy->m_side, &key));
      row->m_visible = 0;
      if (character && (character->m_stateFlags & kGameObjCreated) && row->m_userData == side)
        DescribeCharacter(character, row);
    }
  }
  m_list->SetBackground(GetPaintBackgroundIfOwned(), m_list->m_x, m_list->m_y);
  m_list->SetItemList(rows);
  m_list->SetFont(fontSlot);
  m_list->Refresh();
  if (CoolWindow* tab = m_filterTabs->m_children.FindById(SelectedTabId(m_filterTabs)))
    m_filterLabel->SetText(tab->m_tooltipText);
  m_filterLabel->MoveTo(0x28, SelectedTabId(m_pageTabs) == kForcesPageId ? 0x77 : 0x73);
  RECT area = m_filterLabel->m_rect;
  InvalidateRect(m_hwnd, &area, FALSE);
}

int PersonnelFinderWindow::DrawForceCounts(GameObj* object, int side, HDC dc)
{
  ForceCell cells[kCellCount];
  for (ForceCell& cell : cells)
  {
    cell.m_key.m_value = 0;
    cell.m_count = 0;
    cell.m_flags = 0;
  }
  CoolLabel label(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0u, 0u);
  std::string text;
  const uint32_t forceFamily = 0x3c000000;
  if (side == kAlliance)
  {
    cells[0].m_key.m_value = forceFamily | 4;
    cells[1].m_key.m_value = forceFamily | 2;
    cells[2].m_key.m_value = forceFamily | 1;
    cells[3].m_key.m_value = forceFamily | 3;
  }
  else
  {
    cells[0].m_key.m_value = forceFamily | 7;
    cells[1].m_key.m_value = forceFamily | 6;
    cells[2].m_key.m_value = forceFamily | 8;
    cells[3].m_key.m_value = forceFamily | 5;
  }
  auto countForces = [&](SpecialForceIterator& forces)
  {
    for (forces.Begin(); forces.Current(); forces.Advance())
      CountIntoCell(cells, kCellCount, static_cast<GameObj*>(forces.Current()));
  };
  if (object->GetFamily() == family::kFleet)
  {
    CapShipIterator ships(object, kStateFilterExisting);
    for (ships.Begin(); ships.Current(); ships.Advance())
    {
      SpecialForceIterator forces(ships.Current(), kStateFilterExisting, side);
      countForces(forces);
    }
  }
  else
  {
    SpecialForceIterator forces(object, kStateFilterExisting, side);
    countForces(forces);
  }
  DrawForceCells(cells, kCellCount, &label, &text, 0xde, side, m_brushes, dc);
  return 0;
}

void PersonnelFinderWindow::DescribeCharacter(Character* character, ObjectListItem* row)
{
  std::string status;
  ObjectKey ownerKey;
  std::string detail;
  if (character->GetOwnerKey(&ownerKey))
  {
    const ResourceStringId unknownLocation = StringId(0x1895);
    status = LoadStringResource(&unknownLocation);
    row->m_visible = 1;
    if (character->m_stateFlags & kGameObjDestroyed)
    {
      status.clear();
      const uint32_t reason = character->m_packed & 0xff;
      const ResourceStringId text = StringId(reason >= 0x14 && reason <= 0x16 ? 0x8799 : 0x8798);
      status = LoadStringResource(&text);
    }
    else if (ownerKey.IsSet())
    {
      const bool atSystemOrShip = FamilyIn(ownerKey, 0x90, 0x98) || FamilyIn(ownerKey, 0x14, 0x1c);
      if (atSystemOrShip)
      {
        const ObjectKey location = character->m_missionKey;
        status.clear();
        {
          std::string placeName(static_cast<GameObj*>(character->m_parent)->GetName()->c_str());
          status += placeName;
        }
        bool nothingToSay = true;
        if (character->m_flagsAc & kCharacterCaptured)
        {
          const ResourceStringId text = StringId(0x8746);
          detail = LoadStringResource(&text);
          nothingToSay = false;
        }
        else if (location.IsSet() && FamilyIn(location, 0x50, 0x80))
        {
          const ResourceStringId text = StringId(0x8745);
          detail = LoadStringResource(&text);
          nothingToSay = false;
        }
        if (character->m_injury != 0 && nothingToSay)
        {
          const ResourceStringId text = StringId(0x8747);
          detail = LoadStringResource(&text);
          nothingToSay = false;
        }
        if ((character->m_stateFlags & kGameObjEnroute) && nothingToSay)
        {
          const ResourceStringId text = StringId(0x8744);
          detail = LoadStringResource(&text);
        }
        if (detail.size() != 0)
          AppendParenthesized(&status, &detail);
        if (character->m_commandKind != 0)
        {
          detail.clear();
          switch (character->m_commandKind)
          {
            case 1:
            {
              const ResourceStringId text = StringId(0x8802);
              detail = LoadStringResource(&text);
              break;
            }
            case 2:
            {
              const ResourceStringId text = StringId(0x8801);
              detail = LoadStringResource(&text);
              break;
            }
            case 3:
            {
              const ResourceStringId text = StringId(0x8800);
              detail = LoadStringResource(&text);
              break;
            }
          }
          if (detail.size() != 0)
            AppendParenthesized(&status, &detail);
        }
        row->m_visible = 1;
      }
      else if (FamilyIn(ownerKey, 0x98, 0xa0))
      {
        status.clear();
        {
          std::string placeName(static_cast<GameObj*>(character->m_parent)->GetName()->c_str());
          status += placeName;
        }
        row->m_visible = 1;
      }
    }
  }
  if (row->m_visible != 0)
  {
    std::string line;
    std::string prefix;
    line = character->m_record->m_name.c_str();
    const ResourceStringId separator = StringId(0x1897);
    prefix = LoadStringResource(&separator);
    line += prefix;
    line += status;
    row->m_text = line.c_str();
  }
}

const char* g_detailCloseText = nullptr;
const char* g_detailOpenText = nullptr;
