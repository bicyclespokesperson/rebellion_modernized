#include "strategy_ui/manufacture_order_window.h"
#include <string>

#include <cstdlib>
#include <cstring>

#include "ai/ai_common.h"
#include "app/game_session.h"
#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/iostream_lib.h"
#include "core/strstream_io.h"
#include "data/dat_record.h"
#include "economy/side.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_name_node.h"
#include "gameobj/object_type_db.h"
#include "gfx/dib_bitmap.h"
#include "orders/orders_210_216.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/game_ui_facade.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"
#include "ui/cool_selection_box.h"
#include "ui/cool_string_field.h"
#include "ui/resource_dll.h"

extern const char* g_gokresDllName; // 0x6a8708 "gokres.dll"
extern const char* g_spaceString; // 0x6a7f40 " "

namespace
{

constexpr uint32_t kGokresSlot = 10;
constexpr uint32_t kSpriteSlot = 7;
constexpr uint32_t kStyleChild = 0x46000000;
constexpr uint32_t kBitmapKind = 2;
constexpr int kPictureWidth = 0xc3;
constexpr int kPictureHeight = 0x3f;
constexpr int kMaxQuantity = 0xff;
constexpr int kMaxShownTotal = 9999;
constexpr uint32_t kWhite = 0x2ffffff;
constexpr int kQuantityFieldId = 0x6b;
constexpr UINT kQuantityEntered = 0x407;
constexpr UINT kQuantityEdited = 0x408;
constexpr int kSelectionChangedCommand = 0x29b;

enum Command : int
{
  kCloseButton = 100,
  kOkButton = 0x65,
  kCancelButton = 0x66,
  kInfoButton = 0x67,
  kMoreButton = 0x69,
  kLessButton = 0x6a,
  kSelectorButton = 0x6c,
};

constexpr ResourceStringId kUnavailableText = {0x3816, 2};
constexpr ResourceStringId kUnitSuffixText = {0x3815, 2};

bool InRange(uint32_t family, uint32_t first, uint32_t last)
{
  return family >= first && family < last;
}
bool IsFacilityFamily(uint32_t family)
{
  return InRange(family, 0x20, 0x30);
}
bool IsShipFamily(uint32_t family)
{
  return InRange(family, 0x14, 0x20);
}
bool IsTroopFamily(uint32_t family)
{
  return InRange(family, 0x10, 0x14) || InRange(family, 0x3c, 0x40);
}

void DeleteLabel(CoolLabel* label)
{
  if (label)
  {
    label->~CoolLabel();
    operator delete(label);
  }
}

void DrawIfPresent(CoolLabel* label, HDC dc)
{
  if (label)
    label->Draw(dc);
}

CoolLabel* NewSpaceLabel(int x, int y, int width, int height, UINT drawFlags, int fontSlot)
{
  CoolLabel* label = NewOrNull<CoolLabel>(static_cast<const char*>(g_spaceString), x, y, width, height, drawFlags, 3u);
  if (label)
  {
    label->SetFont(fontSlot);
    label->SetColor(kWhite, nullptr);
  }
  return label;
}

int ClampTotal(int value)
{
  return value > kMaxShownTotal ? kMaxShownTotal : value;
}

void SetLabelText(CoolLabel* label, HWND hwnd, const std::string& text)
{
  label->SetText(text.c_str());
  label->Invalidate(hwnd, TRUE);
}

} // namespace

ManufactureOrderWindow::ManufactureOrderWindow(HINSTANCE instance,
                                               int x,
                                               int y,
                                               int width,
                                               int height,
                                               CoolWindow* parent,
                                               uint32_t stringSlot,
                                               uint32_t bitmapSlot,
                                               uint32_t backgroundId,
                                               uint32_t childId,
                                               uint32_t style,
                                               uint32_t flags,
                                               ManufactureOrderBase* order,
                                               uint32_t unused)
: CoolGameWindow(instance,
                 x,
                 y,
                 width,
                 height,
                 parent,
                 stringSlot,
                 bitmapSlot,
                 backgroundId,
                 childId,
                 style,
                 flags,
                 0,
                 0),
  m_choices(1),
  m_remembered{0}
{
  m_order = order;
  m_style = kStyleChild;
  m_quantity = 1;
  m_selector = nullptr;
  m_quantityField = nullptr;
  m_unitLabelA = nullptr;
  m_unitLabelB = nullptr;
  m_costLabelA = nullptr;
  m_costLabelB = nullptr;
  m_unknown13c = 0;
  m_unknown140 = 0;
  const int side = AppGetSide();
  GameObjNotifier* sideObject = GetSideObject(side, side);
  m_gokres = FindResourceModule(kGokresSlot);
  if (!m_gokres)
  {
    char* dllLocation = GetDllLocation();
    m_gokres = LoadResourceModule(dllLocation, g_gokresDllName, kGokresSlot);
    if (dllLocation)
      operator delete(dllLocation);
  }
  if (!sideObject)
    return;

  ObjectNameList names;
  m_order->FillChoices(&names);
  CoolAppGetResourceModule(kSpriteSlot);
  uint32_t nextItemId = 0;
  for (AvlNode* node = names.First(); node; node = node->m_successor)
  {
    ObjectNameNode* nameNode = static_cast<ObjectNameNode*>(node);
    std::string name;
    nameNode->GetName(&name);
    ObjectKey typeKey;
    GetObjectNameNodeKey(nameNode, &typeKey);
    ObjectKey* keyCopy = NewOrNull<ObjectKey>(typeKey);
    CoolListItem* item = nullptr;
    if (void* memory = ::operator new(sizeof(CoolListItem)))
      item = new (memory) CoolListItem(++nextItemId);
    if (item)
    {
      DatRecord* record = FindDataRecord(AsLookupKey(reinterpret_cast<NullableKey*>(&typeKey)));
      const uint32_t pictureId = record->m_small & 0xfff;
      DibBitmap* picture = NewOrNull<DibBitmap>(m_gokres, pictureId, kBitmapKind);
      DibBitmap* normal = NewOrNull<DibBitmap>(kPictureWidth, kPictureHeight, picture);
      DibBitmap* selected = NewOrNull<DibBitmap>(kPictureWidth, kPictureHeight, picture);
      normal->FillRect(0, 0, normal->Width(), normal->Height(), picture->m_colorKey);
      selected->FillRect(0, 0, selected->Width(), selected->Height(), picture->m_colorKey);
      const int left = (kPictureWidth - picture->Width()) / 2;
      const int top = (kPictureHeight - picture->Height()) / 2;
      picture->DrawKeyed(normal, left, top);
      picture->DrawKeyed(selected, left, top);
      item->SetImages(normal, selected);
      item->m_userPointer = reinterpret_cast<uintptr_t>(keyCopy);
      item->m_text = name.c_str();
      item->m_textOffsetY = picture->Height();
      item->m_ownColors = 1;
      item->m_color = 0x808080;
      item->m_fontSlot = 10;
      picture->~DibBitmap();
      operator delete(picture);
      m_choices.Insert(item);
    }
  }
}

ManufactureOrderWindow::~ManufactureOrderWindow()
{
  for (CoolListItem* item = m_choices.First(); item; item = item->Next())
  {
    if (item->m_userPointer)
      operator delete(reinterpret_cast<void*>(item->m_userPointer));
  }
  ReleaseCapture();
  if (m_selector)
    delete m_selector;
  if (m_quantityField)
    delete m_quantityField;
  DeleteLabel(m_unitLabelA);
  DeleteLabel(m_unitLabelB);
  DeleteLabel(m_costLabelA);
  DeleteLabel(m_costLabelB);
}

int ManufactureOrderWindow::Open(ManufactureOrderBase* order, uint32_t unused)
{
  if (!g_app)
    return 0;
  GalaxyWindow* main = static_cast<GalaxyWindow*>(static_cast<RebellionApplication*>(g_app)->m_activeWindow);
  if (!main || !order)
    return 0;
  ManufactureOrderWindow* window = NewOrNull<ManufactureOrderWindow>(main->m_instance, 0, 0, 0xd2, 0x105, main, 2u, 7u,
                                                                     0x2a30u, 0x62u, 0u, 5u, order, unused);
  g_manufactureOrderWindow = window;
  if (!window)
    return 0;
  if (window->Virtual14())
  {
    window->m_style = kStyleChild;
    window->Create(0);
    window->CenterIn(main->m_panelArea.left, main->m_panelArea.top, main->m_panelArea.right, main->m_panelArea.bottom);
    window->SetDragMode(kDragOutline);
    window->SetDragLimits(main->m_panelArea, nullptr);
    main->m_children.Insert(window);
    window->ShowAndTrack(5);
    return 1;
  }
  delete g_manufactureOrderWindow;
  g_manufactureOrderWindow = nullptr;
  return 0;
}

void ManufactureOrderWindow::OnCreate1()
{
  HMODULE module = CoolAppGetResourceModule(kSpriteSlot);
  m_selector = NewOrNull<CoolSelectionBox>(
    module, this, 6, 0x16, kPictureWidth, kPictureHeight, 2, &m_choices, GetPaintBackgroundIfOwned(),
    static_cast<uint16_t>(0), static_cast<uint16_t>(0), static_cast<uint16_t>(7), static_cast<uint16_t>(0x299a), 2);
  if (m_selector)
  {
    m_selector->Create(0);
    m_selector->m_textFormat = 0x101;
    m_selector->SetFont(10);
    m_selector->m_dropDownShade = nullptr;
    m_selector->m_dropDownTile = NewOrNull<DibBitmap>(module, 0x2966u, kBitmapKind);
    m_selector->m_rowHeight = 0x46;
    m_selector->m_textColor = kWhite;
    const NullableKey none = {0};
    if (CoolListItem* first = m_choices.First())
    {
      const uint32_t family = reinterpret_cast<ObjectKey*>(first->m_userPointer)->Family();
      const NullableKey* remember = nullptr;
      if (IsFacilityFamily(family) && g_lastFacilityType.m_value != none.m_value)
      {
        remember = &g_lastFacilityType;
      }
      else if (!IsFacilityFamily(family) || g_lastFacilityType.m_value == none.m_value)
      {
        if (IsShipFamily(family))
        {
          if (g_lastShipType.m_value != none.m_value)
            remember = &g_lastShipType;
        }
        else if (IsTroopFamily(family) && g_lastTroopType.m_value != none.m_value)
        {
          remember = &g_lastTroopType;
        }
      }
      if (remember)
        m_remembered.m_value = remember->m_value;
      if (m_remembered.m_value != none.m_value)
      {
        const uint16_t wanted = FindDataRecord(AsLookupKey(&m_remembered))->m_small;
        for (CoolListItem* item = first; item; item = item->Next())
        {
          if (wanted == FindDataRecord(AsLookupKey(reinterpret_cast<NullableKey*>(item->m_userPointer)))->m_small)
          {
            m_selector->Select(item->m_id);
            break;
          }
        }
      }
    }
  }
  m_costLabelA = NewSpaceLabel(0x24, 0x6e, 0x40, 0x17, 0x25, 4);
  m_costLabelB = NewSpaceLabel(0x8a, 0x6e, 0x40, 0x17, 0x25, 4);
  ShowCostLabels(0, 0, 0);
  m_unitLabelA = NewSpaceLabel(0x8c, 0x91, 0x3c, 0xf, 0x26, 10);
  m_unitLabelB = NewSpaceLabel(0x8c, 0xa5, 0x3c, 0xf, 0x26, 10);
  ShowUnitLabels(0, 0, 0);
  void* fieldMemory = ::operator new(sizeof(CoolStringField));
  m_quantityField = fieldMemory ?
                      new (fieldMemory)
                        CoolStringField(m_instance, 0x8d, 0xc4, 0x2d, 0x11, this, GetPaintBackgroundIfOwned(), 0u) :
                      nullptr;
  if (m_quantityField)
  {
    m_quantityField->m_childId = kQuantityFieldId;
    m_quantityField->SetFont(4);
    m_quantityField->m_shadowOffset = 0;
    m_quantityField->m_textColor = kWhite;
    m_quantityField->m_unknownEc = 0x200ff00;
    m_quantityField->Create(0);
    m_quantityField->SetMaxLength(0x19);
    UpdateQuantityField();
    AddToolFromResource(m_quantityField, 0x190c);
    SetFocus(m_quantityField->m_hwnd);
  }
  RefreshTotals();
}

void ManufactureOrderWindow::OnCreate2()
{
  if (!BeginOffscreen(nullptr))
    return;
  DrawResourceBitmap(2, 2, AppGetSide() == 1 ? 0x2a31 : 0x2a32);
  DrawTextResource(0x3810, RECT{2, 2, 0xcc, 0x10}, 5, 0x2000000, 0x21);
  DrawTextResource(0x3811, RECT{0x14, 0xc4, 0x8a, 0xd3}, 4, 0x2f0fbff, 0x22);
  DrawTextResource(0x3812, RECT{10, 0x91, 0x87, 0xa0}, 10, 0x2f0fbff, 0x24);
  DrawTextResource(0x3813, RECT{10, 0xa5, 0x87, 0xb4}, 10, 0x2f0fbff, 0x24);
  EndOffscreen();
}

void ManufactureOrderWindow::OnCreate3()
{
  AddCloseButton(0x277c, 0x277d, kCloseButton, 0x1903);
  AddButton(0x4f, 0x5a, 0x296e, 0x296f, kSelectorButton, 0x1906, 0, 0, 0, 0, 0, 0);
  AddButton(0x49, 0xe0, 0x2962, 0x2963, kOkButton, 0x1909, 0, 0, 0, 0, 0, 0);
  if (CoolStrobeButton* ok = static_cast<CoolStrobeButton*>(FindChild(kOkButton)))
  {
    ok->LoadStateBitmap(kButtonDisabled, 0x2d64);
    ok->LoadStateBitmap(kButtonChecked, 0x2d64);
  }
  AddButton(0x8d, 0xe0, 0x2964, 0x2965, kCancelButton, 0x190a, 0, 0, 0, 0, 0, 0);
  AddButton(5, 0xe0, 0x2960, 0x2961, kInfoButton, 0x190b, 0, 0, 0, 0, 0, 0);
  AddButton(0xbd, 0xc4, 0x2972, 0x2973, kMoreButton, 0x1907, 0, 0, 0, 0, 500, 1);
  AddButton(0xbd, 0xcd, 0x2974, 0x2975, kLessButton, 0x1908, 0, 0, 0, 0, 500, 1);
  RefreshTotals();
}

LRESULT ManufactureOrderWindow::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message == kQuantityEntered)
  {
    if (wParam == m_quantityField->m_childId)
    {
      SendOrder();
      PostCloseRequest();
    }
    return 0;
  }
  if (message == kQuantityEdited)
  {
    if (wParam == m_quantityField->m_childId)
      RefreshTotals();
    return 0;
  }
  return CoolGameWindow::WindowProc(message, wParam, lParam);
}

void ManufactureOrderWindow::OnCommand(int a, int b, int c)
{
  const uint32_t command = static_cast<uint32_t>(a) & 0xffff;
  if (command >= 0x29c)
    return;
  if (command == kSelectionChangedCommand)
  {
    RefreshTotals();
    return;
  }
  switch (command)
  {
    case kCloseButton:
    case kCancelButton:
      if (m_order)
      {
        delete m_order;
        m_order = nullptr;
      }
      PostCloseRequest();
      return;
    case kOkButton:
      SendOrder();
      PostCloseRequest();
      return;
    case kInfoButton:
      if (CoolListItem* selection = m_selector->GetSelection())
      {
        ObjectKey none;
        UiOpenEncyclopedia(&none, reinterpret_cast<const NullableKey*>(selection->m_userPointer));
      }
      return;
    case kMoreButton:
      IncrementQuantity();
      return;
    case kLessButton:
      DecrementQuantity();
      return;
    case kSelectorButton:
      if (m_selector)
        m_selector->PressButton();
      return;
  }
}

void ManufactureOrderWindow::OnKeyDown(WPARAM key, LPARAM lParam)
{
  switch (key)
  {
    case VK_RETURN:
      SendOrder();
      PostCloseRequest();
      return;
    case VK_ESCAPE:
      PostCloseRequest();
      return;
    case VK_ADD:
      IncrementQuantity();
      return;
    case VK_SUBTRACT:
      DecrementQuantity();
      return;
  }
}

int ManufactureOrderWindow::Virtual14()
{
  return m_choices.First() && m_order ? 1 : 0;
}

void ManufactureOrderWindow::SendOrder()
{
  CoolListItem* selection = m_selector->GetSelection();
  if (!m_order || !selection)
    return;
  ObjectKey* type = reinterpret_cast<ObjectKey*>(selection->m_userPointer);
  ReadQuantityField();
  if (m_quantity < 1)
  {
    m_quantity = 1;
  }
  else if (m_quantity > kMaxQuantity)
  {
    m_quantity = kMaxQuantity;
  }
  m_order->m_count = m_quantity;
  m_order->m_item.m_value = type->m_value;
  const uint32_t family = type->Family();
  NullableKey* last = nullptr;
  if (IsFacilityFamily(family))
  {
    last = &g_lastFacilityType;
  }
  else if (IsShipFamily(family))
  {
    last = &g_lastShipType;
  }
  else if (IsTroopFamily(family))
  {
    last = &g_lastTroopType;
  }
  if (last)
    last->m_value = type->m_value;
  UiHumanReceiveMessage(reinterpret_cast<PlayerMessage*>(m_order), 0);
}

void ManufactureOrderWindow::IncrementQuantity()
{
  ReadQuantityField();
  if (m_quantity < kMaxQuantity)
  {
    ++m_quantity;
    UpdateQuantityField();
    RefreshTotals();
  }
}

void ManufactureOrderWindow::DecrementQuantity()
{
  ReadQuantityField();
  if (m_quantity > 1)
  {
    --m_quantity;
    UpdateQuantityField();
    RefreshTotals();
  }
}

void ManufactureOrderWindow::UpdateQuantityField()
{
  StrStream stream;
  strstream_ctor(&stream, 1);
  ostream_operator_ls_char(ostream_operator_ls_long(stream.Out(), m_quantity), 0);
  m_quantityField->SetText(strstreambuf_str(StreambufOf(&stream)));
  strstreambuf_freeze(StreambufOf(&stream), 0);
  strstream_dtor(stream.Ios());
  ios_dtor(stream.Ios());
}

void ManufactureOrderWindow::ReadQuantityField()
{
  m_quantity = atoi(m_quantityField->m_text.c_str());
  if (m_quantity < 1)
  {
    m_quantity = 1;
    return;
  }
  if (m_quantity > kMaxQuantity)
    m_quantity = kMaxQuantity;
}

void ManufactureOrderWindow::DrawOverlay(HDC dc)
{
  DrawIfPresent(m_costLabelA, dc);
  DrawIfPresent(m_costLabelB, dc);
  DrawIfPresent(m_unitLabelA, dc);
  DrawIfPresent(m_unitLabelB, dc);
}

void ManufactureOrderWindow::ShowCostLabels(int ok, int cost, int available)
{
  HWND hwnd = m_hwnd;
  const int shownCost = ClampTotal(cost);
  available = ClampTotal(available);
  if (m_costLabelA)
  {
    std::string text;
    if (ok == 0)
    {
      ResourceStringId id = kUnavailableText;
      text = LoadStringResource(&id);
    }
    else
    {
      text.append(std::to_string(static_cast<long>(shownCost)));
    }
    SetLabelText(m_costLabelA, hwnd, text);
  }
  if (m_costLabelB)
  {
    std::string text;
    if (ok == 0)
    {
      ResourceStringId id = kUnavailableText;
      text = LoadStringResource(&id);
    }
    else
    {
      text.append(std::to_string(static_cast<long>(available)));
    }
    SetLabelText(m_costLabelB, hwnd, text);
  }
}

void ManufactureOrderWindow::ShowUnitLabels(int ok, int cost, int available)
{
  HWND hwnd = m_hwnd;
  const int shownCost = ClampTotal(cost);
  available = ClampTotal(available);
  if (m_unitLabelA)
  {
    std::string text;
    if (ok == 0)
    {
      ResourceStringId id = kUnavailableText;
      text = LoadStringResource(&id);
    }
    else
    {
      ResourceStringId id = kUnitSuffixText;
      std::string suffixResource = LoadStringResource(&id);
      text += std::to_string(static_cast<long>(shownCost)) + suffixResource;
    }
    SetLabelText(m_unitLabelA, hwnd, text);
  }
  if (m_unitLabelB)
  {
    std::string text;
    if (ok == 0)
    {
      ResourceStringId id = kUnavailableText;
      text = LoadStringResource(&id);
    }
    else
    {
      ResourceStringId id = kUnitSuffixText;
      std::string suffixResource = LoadStringResource(&id);
      text += std::to_string(static_cast<long>(available)) + suffixResource;
    }
    SetLabelText(m_unitLabelB, hwnd, text);
  }
}

void ManufactureOrderWindow::RefreshTotals()
{
  CoolListItem* selection = m_selector->GetSelection();
  if (!selection)
    return;
  ObjectKey* type = reinterpret_cast<ObjectKey*>(selection->m_userPointer);
  int totalA = 0;
  int totalB = 0;
  int largestA = 0;
  int largestB = 0;
  StatusCode status(1, 1);
  if (m_order && type)
  {
    ReadQuantityField();
    m_order->m_count = m_quantity;
    m_order->m_item.m_value = type->m_value;
    m_order->ComputeTotals(&totalA, &totalB, &largestA, &largestB, &status);
  }
  const int ok = status.m_second == static_cast<uint32_t>(-1);
  ShowUnitLabels(ok, largestA, largestB);
  ShowCostLabels(ok, totalB, totalA);
  if (CoolWindow* ok_button = FindChild(kOkButton))
  {
    if (ok)
      ok_button->Enable();
    else
      ok_button->Disable();
  }
}

std::string* ObjectNameNode::GetName(std::string* out)
{
  new (out) std::string(m_name);
  return out;
}

int g_selectionBoxClosing{};
