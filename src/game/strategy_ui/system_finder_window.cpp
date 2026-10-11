#include "strategy_ui/system_finder_window.h"
#include <string>

#include <new>

#include "core/checked_new.h"
#include "core/game_string.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/finder_helpers.h"
#include "strategy_ui/galaxy_window.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_string_field.h"
#include "ui/cool_tab_control.h"
#include "ui/object_list_item.h"

namespace
{

constexpr uint32_t kStringSlot = 2;
constexpr uint32_t kBitmapSlot = 7;
constexpr uint32_t kHelperBitmapKind = 10;
constexpr int16_t kStringGroup = 2;
constexpr COLORREF kLabelColor = 0x2ffffff;
constexpr uint16_t kClickSound = 0x260;
constexpr int kAlliance = 1;

constexpr uint32_t kTabsId = 100;
constexpr uint32_t kCloseId = 200;
constexpr uint32_t kOpenId = 0xc9;
constexpr uint32_t kListId = 0xca;
constexpr uint32_t kNameFieldId = 0xcb;

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

bool IsSystemListed(GameObj* system, int filter)
{
  switch (filter)
  {
    case kSystemsAll:
      return true;
    case kSystemsAlliance:
      return system->ControlSide() == 1;
    case kSystemsEmpire:
      return system->ControlSide() == 2;
    case kSystemsContested:
      return system->ControlSide() == 3 && (static_cast<System*>(system)->m_flags & system_flag::kExplored);
    case kSystemsUnexplored:
      return !(static_cast<System*>(system)->m_flags & system_flag::kExplored);
    default:
      return false;
  }
}

} // namespace

SystemFinderWindow::SystemFinderWindow(HINSTANCE instance,
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
  m_tabs = nullptr;
  m_nameField = nullptr;
  m_list = nullptr;
  m_filterLabel = nullptr;
  m_galaxy = parent;
  m_filter = 0;
  m_items = NewOrNull<CoolListItemList>(kSortByLabel);
}

SystemFinderWindow::~SystemFinderWindow()
{
  if (m_tabs)
    delete m_tabs;
  if (m_nameField)
    delete m_nameField;
  if (m_list)
    delete m_list;
  if (m_filterLabel)
    delete m_filterLabel;
  if (m_items)
  {
    m_items->Clear();
    delete m_items;
  }
}

void SystemFinderWindow::OnCreate1()
{
  HMODULE art = CoolAppGetResourceModule(kBitmapSlot);
  CenterIn(m_galaxy->m_panelArea.left, m_galaxy->m_panelArea.top, m_galaxy->m_panelArea.right,
           m_galaxy->m_panelArea.bottom);

  DibBitmap* panel = NewOrNull<DibBitmap>(art, 0x291au, kHelperBitmapKind);
  DibBitmap* frame;
  DibBitmap* rightEdge;
  if (m_galaxy->m_side == kAlliance)
  {
    frame = NewOrNull<DibBitmap>(art, 0x285fu, kHelperBitmapKind);
    rightEdge = NewOrNull<DibBitmap>(art, 0x2958u, kHelperBitmapKind);
  }
  else
  {
    frame = NewOrNull<DibBitmap>(art, 0x2860u, kHelperBitmapKind);
    rightEdge = NewOrNull<DibBitmap>(art, 0x295cu, kHelperBitmapKind);
  }

  HDC windowDc = GetDC(m_hwnd);
  HDC dc = CreateCompatibleDC(windowDc);
  ReleaseDC(m_hwnd, windowDc);
  DibBitmap* background =
    NewOrNull<DibBitmap>(static_cast<LONG>(frame->Width()), static_cast<LONG>(frame->Height()), frame, dc);
  panel->Blit(background, 0xc, 0xd, 0, 0, 0, 0);
  frame->DrawKeyed(background, 0, 0);
  rightEdge->Blit(background, 0x19c, 0, 0, 0, 0, 0);
  HGDIOBJ savedBitmap = SelectObject(dc, background->DibSectionHandle());

  const ResourceStringId titleText = StringId(0x1865);
  CoolLabel* title = NewOrNull<CoolLabel>(&titleText, 0x24, 0xe, 0, 0, 0u, 0u);
  title->m_width = 0x15e;
  title->m_layoutFlags = kLabelFixedWidth;
  title->m_drawFlags = 1;
  title->SetFont(5);
  title->SetColor(kLabelColor, nullptr);
  title->Draw(dc);
  const ResourceStringId searchText = StringId(0x1866);
  CoolLabel* search = NewOrNull<CoolLabel>(&searchText, 0x24, 0x30, 0, 0, 0u, 0u);
  search->SetFont(4);
  search->SetColor(kLabelColor, nullptr);
  search->Draw(dc);
  SelectObject(dc, savedBitmap);
  DeleteDC(dc);
  if (title)
    delete title;
  if (search)
    delete search;
  SetPaintBackground(background, 1);
  if (panel)
    delete panel;
  if (frame)
    delete frame;
  if (rightEdge)
    delete rightEdge;

  m_tabs = NewOrNull<CoolTabControl>(m_instance, 0x24, 0x4e, 0x101, 0x29, kTabsId, this, GetPaintBackgroundIfOwned());
  if (m_tabs)
  {
    m_tabs->Create(0);
    struct FilterTab
    {
      int x;
      uint32_t id;
      uint32_t normalId;
      uint32_t pressedId;
      int16_t tooltip;
    };
    const FilterTab tabs[] = {
      {0, kSystemsAll, 0x2904, 0x2905, 0x1867},           {0x34, kSystemsAlliance, 0x2906, 0x2907, 0x1868},
      {0x68, kSystemsEmpire, 0x2909, 0x290a, 0x1869},     {0x9c, kSystemsContested, 0x290c, 0x290d, 0x1870},
      {0xd0, kSystemsUnexplored, 0x290f, 0x2910, 0x1871},
    };
    for (const FilterTab& tab : tabs)
    {
      CoolStrobeButton* button =
        NewButton(art, tab.x, 0, 0x31, 0x2a, m_tabs, tab.id, tab.normalId, tab.pressedId, m_tabs->m_background);
      if (!button)
        continue;
      button->Create(0);
      const ResourceStringId tip = StringId(tab.tooltip);
      AddToolFromString(button, &tip);
      m_tabs->AddTab(button, tab.id == kSystemsAll ? 1 : 0, 0);
    }
  }

  uint32_t openId;
  uint32_t openPressedId;
  int openY;
  int closeX;
  if (m_galaxy->m_side == kAlliance)
  {
    AddButton(0x1ab, 0x19, 0x2912, 0x2913, kCloseId, 0x1954, 0, 0, 0, 0, 0, 0);
    closeX = 0x1ab;
    openY = 0x5d;
    openId = 0x2916;
    openPressedId = 0x2917;
  }
  else
  {
    AddButton(0x1aa, 0x15, 0x2914, 0x2915, kCloseId, 0x1954, 0, 0, 0, 0, 0, 0);
    closeX = 0x1aa;
    openY = 0x59;
    openId = 0x2918;
    openPressedId = 0x2919;
  }
  AddButton(closeX, openY, openId, openPressedId, kOpenId, 0x1953, 0, 0, 0, 0, 0, 0);
  if (CoolStrobeButton* close = static_cast<CoolStrobeButton*>(m_children.FindById(kCloseId)))
    close->SetClickSound(kClickSound);
  if (CoolStrobeButton* open = static_cast<CoolStrobeButton*>(m_children.FindById(kOpenId)))
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
    m_filterLabel->MoveTo(0x28, 0x77);
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

  m_list = NewOrNull<CoolDragList>(art, 0x24, 0x8a, 0x15e, 0xa5, this, GetPaintBackgroundIfOwned(), kListId, 0x15e,
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
    m_list->SetItemList(m_items);
  }

  SetFilter(kSystemsAll, 1);
  SetFocus(m_nameField->m_hwnd);
}

void SystemFinderWindow::DrawOverlay(HDC dc)
{
  m_filterLabel->Draw(dc);
}

namespace
{

void OpenSectorOf(GalaxyWindow* galaxy, const ObjectKey* systemKey)
{
  GameObjNotifier* system = GetObjectInFamilies_90_98(galaxy->m_side, systemKey);
  if (!system)
    return;
  ObjectKey sector;
  system->m_parent->GetKey(&sector);
  galaxy->OpenSectorWindow(&sector);
}

} // namespace

void SystemFinderWindow::OnCommand(int, int command, int)
{
  const uint32_t code = static_cast<uint32_t>(command) >> 16;
  switch (command & 0xffff)
  {
    case kTabsId:
    {
      SetFilter(static_cast<int>(code), 0);
      const uint32_t id = m_list->SelectByPrefix(&m_nameField->m_text, 0, 0);
      if (id)
      {
        ObjectKey key;
        static_cast<ObjectListItem*>(m_items->FindById(id))->GetKey(&key);
        m_selected = key;
      }
      break;
    }
    case kCloseId:
      PostCloseRequest();
      break;
    case kOpenId:
      if (m_selected.IsComplete())
      {
        OpenSectorOf(m_galaxy, &m_selected);
        PostCloseRequest();
      }
      break;
    case kListId:
    {
      CoolListItemList selection(kSortById);
      m_list->CopySelectionTo(&selection);
      if (CoolListItem* first = static_cast<CoolListItem*>(selection.m_head))
      {
        ObjectListItem* item = static_cast<ObjectListItem*>(m_items->FindById(first->m_id));
        ObjectKey key;
        item->GetKey(&key);
        m_selected = key;
        if (static_cast<uint16_t>(code) == kDragListActivated && item)
        {
          OpenSectorOf(m_galaxy, &m_selected);
          PostCloseRequest();
        }
        else if (static_cast<uint16_t>(code) == kDragListSelectionChanged)
        {
          m_nameField->SetText(first->m_text.c_str());
        }
      }
      break;
    }
  }
}

LRESULT SystemFinderWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  LRESULT result = 0;
  if (message == WM_SETFOCUS)
  {
    SetFocus(m_nameField->m_hwnd);
  }
  else if (message == kFieldReturnMessage)
  {
    const ObjectKey unset;
    if (static_cast<int16_t>(wParam) == kNameFieldId && m_selected.m_value != unset.m_value)
    {
      OpenSectorOf(m_galaxy, &m_selected);
      PostCloseRequest();
    }
  }
  else if (message == kFieldChangedMessage)
  {
    if (static_cast<int16_t>(wParam) == kNameFieldId)
    {
      ObjectListItem* item = nullptr;
      const std::string* typed = &m_nameField->m_text;
      if (!typed->empty())
      {
        const uint32_t id = m_list->SelectByPrefix(typed, 0, 0);
        item = static_cast<ObjectListItem*>(m_items->FindById(id));
      }
      if (!item)
      {
        m_selected = ObjectKey();
      }
      else
      {
        ObjectKey key;
        item->GetKey(&key);
        m_selected = key;
      }
    }
  }
  else
  {
    result = CoolGameWindow::WindowProc(message, wParam, lParam);
  }
  return result;
}

void SystemFinderWindow::SetFilter(int filter, int)
{
  const int view = m_galaxy->m_side;
  if (m_filter == filter)
    return;
  m_items->Clear();
  for (GameObjNotifier* object = GetFirstObjectInFamilies_90_98(view); object;
       object = static_cast<GameObj*>(object)->FindNextSibling(&family::kSystems, 1))
  {
    GameObj* system = static_cast<GameObj*>(object);
    if (!IsSystemListed(system, filter))
      continue;
    ObjectKey key;
    ObjectListItem* item = NewOrNull<ObjectListItem>(system->GetKey(&key));
    if (!item)
      continue;
    {
      std::string name;
      system->GetNameCopy(&name);
      item->m_text = name.c_str();
    }
    m_items->Insert(item);
  }
  m_filter = filter;
  m_list->Refresh();
  m_filterLabel->SetText(m_tabs->m_children.FindById(filter)->m_tooltipText);
  RECT area = m_filterLabel->m_rect;
  InvalidateRect(m_hwnd, &area, FALSE);
}

void SystemFinderWindow::OnKeyDown(WPARAM key, LPARAM lParam)
{
  FinderOnKeyDown(this, m_tabs, m_nameField, m_list, key, lParam);
}
