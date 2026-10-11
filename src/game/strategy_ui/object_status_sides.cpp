#include <cstdint>
#include <new>
#include <string>

#include "core/checked_new.h"
#include "core/game_string.h"
#include "economy/side.h"
#include "events/game_clock_hooks.h"
#include "gameobj/game_settings.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/object_status_helpers.h"
#include "strategy_ui/object_status_window.h"
#include "ui/cool_app.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_list_item.h"

using namespace status_page;

namespace
{

constexpr uint32_t kSideIconsSlot = 7;
constexpr uint32_t kPanelBitmapKind = 10;
constexpr uint32_t kIconBitmapKind = 2;
constexpr int kPictureSortMode = 4;

struct SideItem
{
  uint32_t m_itemId;
  uint32_t m_bitmapId;
  uint32_t m_victoryMask;
  uint16_t m_offStringId;
  uint16_t m_onStringId;
};

// Ids of the victory-condition pictures: the player's side comes first for Alliance (1-3) and last for Empire (4-6).
constexpr SideItem kAllianceFirst = {1, 0x27fa, 0x300, 0x8781, 0x8782};
constexpr SideItem kAllianceGated1[] = {{2, 0x2b05, 0x2000, 0x8789, 0x878a}, {3, 0x2b2f, 0x1000, 0x878d, 0x878e}};
constexpr SideItem kAllianceTail = {4, 0x388, 0x1, 0x8784, 0x8787};
constexpr SideItem kAllianceGated2[] = {{5, 0x2b14, 0x10, 0x8794, 0x8797}, {6, 0x2b18, 0x20, 0x8790, 0x8793}};

constexpr SideItem kEmpireFirst = {4, 0x27fa, 0x300, 0x8780, 0x8783};
constexpr SideItem kEmpireGated1[] = {{5, 0x2b05, 0x2000, 0x8788, 0x878b}, {6, 0x2b2f, 0x1000, 0x878c, 0x878f}};
constexpr SideItem kEmpireTail = {1, 0x388, 0x1, 0x8785, 0x8786};
constexpr SideItem kEmpireGated2[] = {{2, 0x2b14, 0x10, 0x8795, 0x8796}, {3, 0x2b18, 0x20, 0x8791, 0x8792}};

CoolListItem* NewPictureItem(uint32_t id, DibBitmap* image)
{
  void* memory = ::operator new(sizeof(CoolListItem));
  CoolListItem* item = memory ? new (memory) CoolListItem(id) : nullptr;
  item->m_state.m_value = kPictureSortMode;
  item->SetImages(image, nullptr);
  return item;
}

void DeleteBitmap(DibBitmap* bitmap)
{
  if (!bitmap)
    return;
  bitmap->~DibBitmap();
  ::operator delete(bitmap);
}

void InsertPictureItem(CoolListItemList* items,
                       CoolListItem* item,
                       const SideItem& entry,
                       uint32_t victoryFlags,
                       std::string* text)
{
  SetText(text, (victoryFlags & entry.m_victoryMask) == 0 ? entry.m_offStringId : entry.m_onStringId);
  item->m_text = text->c_str();
  items->Insert(item);
}

// The picture `bitmapId` centred on a bitmap of the size of the first picture, filled with `fillColor`.
DibBitmap* CenteredPicture(HMODULE module,
                           uint32_t bitmapId,
                           uint32_t bitmapKind,
                           int width,
                           int height,
                           const uint8_t* fixedFillColor)
{
  DibBitmap* icon = NewOrNull<DibBitmap>(module, bitmapId, bitmapKind);
  DibBitmap* surface = NewOrNull<DibBitmap>(width, height, icon);
  surface->FillRect(0, 0, width, height, fixedFillColor ? *fixedFillColor : icon->m_colorKey);
  const int iconWidth = icon->Width();
  const int iconHeight = icon->Height();
  icon->Blit(surface, (width - iconWidth) / 2, (height - iconHeight) / 2, 0, 0, 0, 0);
  DeleteBitmap(icon);
  return surface;
}

void AddCenteredItem(CoolListItemList* items,
                     HMODULE module,
                     const SideItem& entry,
                     uint32_t bitmapKind,
                     int width,
                     int height,
                     const uint8_t* fixedFillColor,
                     uint32_t victoryFlags,
                     std::string* text)
{
  DibBitmap* picture = CenteredPicture(module, entry.m_bitmapId, bitmapKind, width, height, fixedFillColor);
  CoolListItem* item = NewPictureItem(entry.m_itemId, picture);
  InsertPictureItem(items, item, entry, victoryFlags, text);
}

void FillSidePage(CoolListItemList* items,
                  int side,
                  const SideItem& first,
                  const SideItem (&gated1)[2],
                  const SideItem& tail,
                  uint32_t tailBitmapKind,
                  const SideItem (&gated2)[2])
{
  GameObjNotifier* sideObject = GetSideObject(side, side);
  std::string text;
  GameSettings settings;
  GetGameSettings(&settings);
  HMODULE module = CoolAppGetResourceModule(kSideIconsSlot);
  const bool allVictoryPictures = settings.m_field08 == 0;
  const uint32_t victoryFlags =
    sideObject ? static_cast<uint32_t>(static_cast<Side*>(sideObject)->m_victoryConditions) : 0;

  DibBitmap* base = NewOrNull<DibBitmap>(module, first.m_bitmapId, kPanelBitmapKind);
  const int width = base->Width();
  const int height = base->Height();
  CoolListItem* firstItem = NewPictureItem(first.m_itemId, base);
  const uint8_t fillColor = base->m_colorKey;
  InsertPictureItem(items, firstItem, first, victoryFlags, &text);

  if (allVictoryPictures)
  {
    for (const SideItem& entry : gated1)
    {
      AddCenteredItem(items, module, entry, kIconBitmapKind, width, height, &fillColor, victoryFlags, &text);
    }
  }
  AddCenteredItem(items, module, tail, tailBitmapKind, width, height, nullptr, victoryFlags, &text);
  if (allVictoryPictures)
  {
    for (const SideItem& entry : gated2)
    {
      AddCenteredItem(items, module, entry, kIconBitmapKind, width, height, &fillColor, victoryFlags, &text);
    }
  }
}

} // namespace

void ObjectStatusWindow::FillAlliancePage()
{
  FillSidePage(m_items, 1, kAllianceFirst, kAllianceGated1, kAllianceTail, kPanelBitmapKind, kAllianceGated2);
}

void ObjectStatusWindow::FillEmpirePage()
{
  FillSidePage(m_items, 2, kEmpireFirst, kEmpireGated1, kEmpireTail, kIconBitmapKind, kEmpireGated2);
}
