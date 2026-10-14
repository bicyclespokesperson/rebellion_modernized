#include <cstdint>
#include <new>
#include <string>

#include "ai/ai_common.h"
#include "core/checked_new.h"
#include "economy/manufacturing_facility.h"
#include "economy/production_facility_types.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/system_missions_window.h"
#include "strategy_ui/system_window.h"
#include "ui/cool_app.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_list_item.h"
#include "ui/cool_tab_control.h"
#include "ui/object_list_item.h"

namespace
{

constexpr int kItemWidth = 0x45;
constexpr int kItemHeight = 0x28;
constexpr int kFirstPage = 0x67;
constexpr uint32_t kRecordBitmapKind = 2;

void DeleteBitmap(DibBitmap* bitmap)
{
  if (!bitmap)
    return;
  bitmap->~DibBitmap();
  ::operator delete(bitmap);
}

bool InFamily(uint32_t family, uint32_t first, uint32_t end)
{
  return family >= first && family < end;
}

// How a page of facilities is built for an added facility.
struct FacilityPage
{
  uint32_t m_family;
  int m_page;
  uint32_t m_tabId;
  uint16_t m_tabBitmap;
  bool m_sizedByPicture; // the item pictures take the size of the facility picture instead of 0x45 x 0x28
  bool m_clearsDisabled; // an object that is not in the system any more only clears the item's disabled bit
  int m_invalidateBottom; // the part of the first page's panel list that changes (0: none)
};

constexpr FacilityPage kFacilityPages[] = {
  {0x2c, 0x6c, 0x6c, 0x2851, false, true, 0},      {0x2d, 0x6b, 0x6b, 0x2854, true, false, 0},
  {0x28, 0x68, 0x68, 0x2857, false, false, 0x87},  {0x29, 0x69, 0x69, 0x285a, false, false, 0xd8},
  {0x2a, 0x6a, 0x6a, 0x285d, false, false, 0x128},
};

uint32_t ControlBits(GameObjNotifier* object)
{
  return static_cast<GameObjRoot*>(object)->m_flags;
}

} // namespace

// ---- Pictures --------------------------------------------------------------------------------------------

DibBitmap* SystemObjectPicture(GameObjNotifier* object)
{
  uint16_t base = 0;
  uint16_t id = 0;
  DibBitmap* result = nullptr;
  if (object)
  {
    ObjectKey recordKey;
    GetObjectRecordKey(object, &recordKey);
    if (InFamily(recordKey.Family(), 0x28, 0x2c))
    {
      switch (object->GetRecordId())
      {
        case 1:
          base = 0x232e;
          break;
        case 2:
          base = 0x2336;
          break;
        case 3:
          base = 0x233e;
          break;
        case 4:
          base = 0x2332;
          break;
        case 5:
          base = 0x233a;
          break;
        case 6:
          base = 0x2342;
          break;
      }
    }
    else
    {
      ObjectKey secondKey;
      GetObjectRecordKey(object, &secondKey);
      if (InFamily(secondKey.Family(), 0x2c, 0x30))
      {
        const uint32_t recordId = object->GetRecordId();
        if (recordId == 1)
        {
          base = 0x2329;
        }
        else if (recordId == 2)
        {
          base = 0x2346;
        }
      }
    }
    const uint32_t state = static_cast<GameObj*>(object)->m_stateFlags;
    if ((state & 4) == 0)
    {
      id = static_cast<uint16_t>(base + 1);
      if ((ControlBits(object) & 0xc0) == 0x40)
        id = static_cast<uint16_t>(base + 2);
    }
    else
    {
      id = base;
      if ((state & 0x10) != 0)
        id = static_cast<uint16_t>(base + 3);
    }
  }
  if (id != 0)
  {
    HMODULE art = CoolAppGetResourceModule(7);
    result = NewOrNull<DibBitmap>(art, static_cast<uint32_t>(id), kRecordBitmapKind);
  }
  return result;
}

// ---- Changes of the system ---------------------------------------------------------------------------------

void SystemWindow::OnObjectUpdated(const ObjectKey* key, GameObjNotifier* object)
{
  bool controlChanged = false;
  if (InFamily(key->Family(), 0x90, 0x98) && m_controlSide != static_cast<int>((ControlBits(object) >> 6) & 3))
  {
    controlChanged = true;
  }
  if (controlChanged)
  {
    m_controlSide = (ControlBits(object) >> 6) & 3;
    LoadTabArt();
  }
  const int page = m_page;
  m_page = 0;
  m_tabs->SelectTab(page, 1);
  InvalidateRect(m_hwnd, nullptr, FALSE);
}

void SystemWindow::OnChildAdded(const ObjectKey* key, GameObjNotifier* object)
{
  m_objects.Find(key);
  CoolAppGetResourceModule(7);
  CoolWindow* tab = nullptr;
  uint16_t tabBitmap = 0;
  DibBitmap* picture = nullptr;
  ObjectListItem* item = nullptr;
  if (!reinterpret_cast<SystemMissionsWindow*>(this)->SystemMissionsWindow::GetSystem())
    return;
  const uint32_t family = key->Family();
  for (const FacilityPage& page : kFacilityPages)
  {
    if (family != page.m_family)
      continue;
    if (m_page == page.m_page)
    {
      picture = SystemObjectPicture(object);
      const uint32_t state = static_cast<GameObj*>(object)->m_stateFlags;
      if ((state & 4) == 0)
      {
        if (!m_items->FindById(key->m_value & 0xffffff))
        {
          item = NewOrNull<ObjectListItem>(key);
          const int width = page.m_sizedByPicture ? picture->Width() : kItemWidth;
          const int height = page.m_sizedByPicture ? picture->Height() : kItemHeight;
          auto* normal = NewOrNull<DibBitmap>(width, height, picture);
          auto* selected = NewOrNull<DibBitmap>(width, height, picture);
          item->SetImages(normal, selected);
          m_items->Insert(item);
        }
      }
      else
      {
        item = static_cast<ObjectListItem*>(m_items->FindById(key->m_value & 0xffffff));
        if (page.m_clearsDisabled)
          item->m_state.m_value &= ~static_cast<uint32_t>(kListItemDisabled);
      }
    }
    else if (m_page == kFirstPage && page.m_invalidateBottom != 0)
    {
      RECT area = {6, 0x34, 0x47, page.m_invalidateBottom};
      InvalidateRect(m_hwnd, &area, FALSE);
    }
    tab = m_tabs->m_children.FindById(page.m_tabId);
    tabBitmap = page.m_tabBitmap;
    break;
  }
  if (picture)
  {
    if (item)
    {
      DibBitmap* normal = item->m_image;
      DibBitmap* selected = item->m_selectedImage;
      const int x = (kItemWidth - picture->Width()) / 2;
      const int y = (kItemHeight - picture->Height()) / 2;
      const uint8_t colorKey = picture->m_colorKey;
      if (normal)
      {
        normal->FillRect(0, 0, kItemWidth, kItemHeight, colorKey);
        picture->Blit(normal, x, y, 0, 0, 0, 0);
      }
      if (selected)
      {
        selected->FillRect(0, 0, kItemWidth, kItemHeight, colorKey);
        picture->Blit(selected, x, y, 0, 0, 0, 0);
      }
      m_tabIcon->DrawKeyed(selected, x, y);
      m_list->Refresh();
    }
    DeleteBitmap(picture);
  }
  if (!m_objects.Find(key))
  {
    auto* node = NewOrNull<ObjectKeyIdNode>();
    node->m_key = *key;
    m_objects.Add(node, 0);
  }
  if (tab && tabBitmap)
  {
    auto* button = static_cast<CoolStrobeButton*>(tab);
    if (button->GetStateResourceId(0) != static_cast<int16_t>(tabBitmap))
    {
      button->LoadStateBitmap(0, tabBitmap);
      InvalidateRect(button->m_hwnd, nullptr, FALSE);
    }
  }
}

void SystemWindow::OnChildMoved(const ObjectKey* key,
                                GameObjNotifier* object,
                                const ObjectKey* otherKey,
                                GameObjNotifier* otherObject)
{
  HMODULE art = CoolAppGetResourceModule(7);
  CoolWindow* tab = nullptr;
  uint16_t tabBitmap = 0;
  std::string unusedText;
  GameObjNotifier* system = reinterpret_cast<SystemMissionsWindow*>(this)->SystemMissionsWindow::GetSystem();
  if (!system)
    return;
  auto* systemObject = static_cast<GameObj*>(system);
  const uint32_t family = key->Family();
  const auto side = static_cast<uint32_t>(m_controlSide);
  if (family == 0x2c)
  {
    if (CoolListItem* found = m_items->FindById(key->m_value & 0xffffff))
    {
      ObjectKey unset;
      m_items->Remove(found);
      delete found;
      auto* placeholder = NewOrNull<DibBitmap>(art, 0x232du, 10u);
      auto* item = NewOrNull<ObjectListItem>(&unset);
      item->SetImages(placeholder, nullptr);
      item->m_state.m_value |= kListItemDisabled;
      m_items->Insert(item);
      m_list->Refresh();
    }
    if (CountProductionFacilityType2C(systemObject, side, 3) == 0)
    {
      tab = m_tabs->m_children.FindById(0x6c);
      tabBitmap = 0x2852;
    }
  }
  else
  {
    if (auto* item = static_cast<ObjectListItem*>(m_items->FindById(key->m_value & 0xffffff)))
    {
      ObjectKey itemKey;
      item->GetKey(&itemKey);
      if (itemKey.m_value == key->m_value)
      {
        m_items->Remove(item);
        m_list->Refresh();
        delete item;
      }
    }
    auto invalidatePanels = [&](int bottom)
    {
      if (m_page == kFirstPage)
      {
        RECT area = {6, 0x34, 0x47, bottom};
        InvalidateRect(m_hwnd, &area, FALSE);
      }
    };
    if (family == 0x2d && CountProductionFacilityType2D(systemObject, side, 3) == 0)
    {
      tab = m_tabs->m_children.FindById(0x6b);
      tabBitmap = 0x2855;
    }
    if (family == 0x28 && CountManufacturingFacility28Children(systemObject, side, 3) == 0)
    {
      tab = m_tabs->m_children.FindById(0x68);
      tabBitmap = 0x2858;
      invalidatePanels(0x87);
    }
    if (family == 0x29 && CountManufacturingFacility29Children(systemObject, side, 3) == 0)
    {
      tab = m_tabs->m_children.FindById(0x69);
      tabBitmap = 0x285b;
      invalidatePanels(0xd8);
    }
    if (family == 0x2a && CountManufacturingFacility2AChildren(systemObject, side, 3) == 0)
    {
      tab = m_tabs->m_children.FindById(0x6a);
      tabBitmap = 0x285e;
    }
  }
  if (m_page == kFirstPage)
  {
    RECT area = {6, 0x34, 0x47, 0x128};
    InvalidateRect(m_hwnd, &area, FALSE);
  }
  m_objects.Remove(static_cast<ObjectKeyIdNode*>(m_objects.Find(key)));
  if (tab && tabBitmap)
  {
    auto* button = static_cast<CoolStrobeButton*>(tab);
    if (button->GetStateResourceId(0) != static_cast<int16_t>(tabBitmap))
    {
      button->LoadStateBitmap(0, tabBitmap);
      InvalidateRect(button->m_hwnd, nullptr, FALSE);
    }
  }
}
