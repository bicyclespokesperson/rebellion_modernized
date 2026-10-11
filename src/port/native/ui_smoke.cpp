#include "port/native/ui_smoke.h"

#include <memory>
#include <ranges>
#include <stdexcept>

#include "app/rebapp_globals.h"
#include "app/rebellion_application.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/fleet_list_item.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_game_window.h"

namespace
{
void Require(bool condition)
{
  if (!condition)
    throw std::runtime_error("Native UI smoke: fleet-row visibility regression");
}

FleetListItem* AddRow(CoolListItemList& items, DibBitmap& format, uint32_t id, uint32_t flags, uint8_t color)
{
  const ObjectKey key = ObjectKey::Make((flags & kFleetItemFleet) ? 8 : 0x14, id);
  auto row = std::make_unique<FleetListItem>(&key);
  row->m_flags.m_value = flags;
  auto image = std::make_unique<DibBitmap>(10, 10, &format);
  Require(image->IsLoaded());
  image->FillRect(0, 0, 10, 10, color);
  image->m_colorKey = 0;
  row->SetImages(image.release(), nullptr);
  Require(items.Insert(row.get()));
  return row.release();
}

uint8_t Pixel(const DibBitmap& bitmap, int x, int y)
{
  return bitmap.m_bits[bitmap.RowIndex(y) * bitmap.RowStride() + x];
}
} // namespace

void RunNativeUiSmoke(RebellionApplication& app)
{
  auto releaseLocation = [](char* value)
  {
    ::operator delete(value);
  };
  std::unique_ptr<char, decltype(releaseLocation)> location(GetDllLocation(), releaseLocation);
  HMODULE art = LoadResourceModule(location.get(), "strategy.dll", 7);
  DibBitmap background(art, 0x2b5du, kDibResource);
  Require(background.IsLoaded());
  CoolGameWindow parent(app.m_instance, 0, 0, 100, 40, &app, 0, 7, 0, 1, 0, 1, 0, 0);
  CoolDragList list(art, 0, 0, 100, 40, &parent, &background, 2, 10, 10, 0, 0, 0, 0, 10, 1, nullptr);
  auto& items = *list.m_items;
  FleetListItem* fleet = AddRow(items, background, 1, kFleetItemFleet, 1);
  FleetListItem* ship = AddRow(items, background, 2, kFleetItemHidden, 2);
  FleetListItem* nextFleet = AddRow(items, background, 3, kFleetItemFleet, 3);

  Require((fleet->m_flags.m_value & kFleetItemExpanded) == 0);
  list.Refresh();
  Require(items.Count() == 3 && std::ranges::distance(items.VisibleEntries()) == 2);
  Require(nextFleet->m_y == 10 && list.HitTest(POINTS{5, 15}) == nextFleet);
  Require(Pixel(*list.m_canvas, 5, 15) == 3);

  fleet->m_flags.m_value |= kFleetItemExpanded;
  ship->m_flags.m_value &= ~kFleetItemHidden;
  list.Refresh();
  Require(std::ranges::distance(items.VisibleEntries()) == 3);
  Require(ship->m_y == 10 && nextFleet->m_y == 20 && list.HitTest(POINTS{5, 15}) == ship);
  Require(Pixel(*list.m_canvas, 5, 15) == 2);

  fleet->m_flags.m_value &= ~kFleetItemExpanded;
  ship->m_flags.m_value |= kFleetItemHidden;
  list.Refresh();
  Require(nextFleet->m_y == 10 && list.HitTest(POINTS{5, 15}) == nextFleet);
  Require(Pixel(*list.m_canvas, 5, 15) == 3);
  ship->m_state.m_value |= kListItemSelected;
  CoolListItemList selected(kSortById);
  list.CopySelectionTo(&selected);
  Require(selected.Empty());
  list.ClearSelection();
  Require((ship->m_state.m_value & kListItemSelected) == 0);
  list.SelectById(ship->m_id, 0);
  Require((ship->m_state.m_value & kListItemSelected) == 0);
  list.SelectById(fleet->m_id, 0);
  list.WindowProc(WM_KEYDOWN, VK_DOWN, 0);
  Require((nextFleet->m_state.m_value & kListItemSelected) != 0);
  Require((ship->m_state.m_value & kListItemSelected) == 0);
  list.WindowProc(WM_KEYDOWN, VK_UP, 0);
  Require((fleet->m_state.m_value & kListItemSelected) != 0);
  list.ClearSelection();

  list.SetSortMode(kSortById);
  Require(list.m_items->Count() == 3 && std::ranges::distance(list.m_items->VisibleEntries()) == 2);
  Require(list.HitTest(POINTS{5, 15}) == nextFleet);
}
