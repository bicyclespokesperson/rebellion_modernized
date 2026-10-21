#include <algorithm>
#include <cstdint>
#include <new>
#include <string>

#include "ai/ai_common.h"
#include "audio/sound_manager.h"
#include "core/checked_new.h"
#include "core/game_string.h"
#include "core/reb_x87.h"
#include "galaxy/system.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "gfx/dib_bitmap.h"
#include "strategy_ui/advisor_panel.h"
#include "strategy_ui/galaxy_window.h"
#include "strategy_ui/panel_drag_button.h"
#include "ui/cool_app.h"
#include "ui/cool_game_window.h"
#include "ui/cool_label.h"
#include "ui/cool_list_item.h"
#include "ui/object_list_item.h"

extern const int16_t g_displayGridWidth;  // 0x658bd8
extern const int16_t g_displayGridHeight; // 0x658bda

namespace
{

constexpr int kDisplayOff = 0x80;
constexpr int kDisplayKeepsSelectionFirst = 0x92;
constexpr int kDisplayKeepsSelectionLast = 0x93;
constexpr double kDisplayMapWidth = 607.0;
constexpr double kDisplayMapHeight = 437.0;
constexpr int kSystemMarkerSize = 0x11;
constexpr uint32_t kSystemMarkerStateFilter = 4;
constexpr int kLegendWidth = 0xb4;
constexpr int kLegendHeight = 0xf0;
constexpr uint32_t kLegendChildId = 0x43;
constexpr uint32_t kSpriteSlot = 7;
constexpr int kGidSoundId = 0x259;
constexpr int kSortedOrderFlagSwpNoSizeNoMove = SWP_NOSIZE | SWP_NOMOVE;
constexpr uint32_t kSectorWindowType = 1;
constexpr uint32_t kChildTypeMask = 0x3f;
constexpr COLORREF kAllianceTitleColor = 0x20000ff;
constexpr COLORREF kEmpireTitleColor = 0x200ff00;

constexpr uint16_t kGidIconBitmaps[3][4]
    = {{0x27a5, 0x27a4, 0x27a3, 0x27a2}, {0x27a6, 0x27a7, 0x27a8, 0x27a9}, {0x27aa, 0x27ab, 0x27ac, 0x27ad}};
constexpr uint32_t kGidIconSheetFirst = 0x27ae;
constexpr uint32_t kGidIconSheetSecond = 0x27b6;
constexpr uint32_t kGidBitmapKind = 10;
constexpr uint32_t kBitmapResourceKind = 2;
constexpr uint32_t kBackgroundTintPlain = 0x386;
constexpr uint32_t kBackgroundTintGid = 0x387;
constexpr uint32_t kAllianceMap = 900;
constexpr uint32_t kEmpireMap = 0x385;

uint16_t TitleStringId(int mode, int side)
{
  switch (mode) {
    case 0x10:
    case 0x11:
      return 0x1615;
    case 0x12:
      return 0x1616;
    case 0x13:
    case 0x15:
      return (side == 1) == (mode == 0x13) ? 0x161f : 0x1620;
    case 0x14:
    case 0x16:
      return (mode == 0x14) != (side == 2) ? 0 : 0x1621;
    case 0x17:
      return 0x1629;
    case 0x20:
    case 0x21:
      return 0x1617;
    case 0x22:
      return 0x1618;
    case 0x41:
      return 0x1672;
    case 0x42:
      return 0x1669;
    case 0x43:
      return 0x161d;
    case 0x44:
      return 0x161e;
    case 0x51:
      return 0x1606;
    case 0x52:
      return 0x1607;
    case 0x53:
      return 0x1608;
    case 0x54:
      return 0x1609;
    case 0x61:
      return 0x160b;
    case 0x62:
      return 0x160c;
    case 0x63:
      return 0x160d;
    case 0x64:
      return 0x160e;
    case 0x65:
      return 0x161c;
    case 0x66:
      return 0x161b;
    case 0x67:
      return 0x161a;
    case 0x71:
      return 0x1610;
    case 0x72:
      return 0x1611;
    case 0x73:
      return 0x1612;
    case 0x74:
      return 0x1613;
    case 0x75:
      return 0x1619;
    case 0x91:
      return 0x162a;
    default:
      return 0;
  }
}

std::string* NewTitle(const ResourceStringId* id)
{
  void* memory = ::operator new(sizeof(std::string));
  return memory ? new (memory) std::string(LoadStringResource(id)) : nullptr;
}

void DeleteBitmap(DibBitmap* bitmap)
{
  if (!bitmap) {
    return;
  }
  bitmap->~DibBitmap();
  ::operator delete(bitmap);
}

DibBitmap* NewResourceBitmap(HMODULE module, uint32_t id, uint32_t kind)
{
  void* memory = ::operator new(sizeof(DibBitmap));
  return memory ? new (memory) DibBitmap(module, id, kind) : nullptr;
}

HWND SendToBottom()
{
  return HWND_BOTTOM;
}

} // namespace

void GalaxyWindow::SetDisplayMode(int mode, const std::set<uint32_t>* selection, bool resizing)
{
  SoundRequest request(0, 0, 0, 0, 0, 0);
  const ResourceStringId overrideTitle = resizing ? m_activeTitleStringId : m_titleStringId;
  if (!resizing) {
    m_titleStringId = ResourceStringId{0, 0};
  }
  if (m_gidMenu) {
    m_gidMenu->Hide();
  }
  m_selectedIds.clear();
  if (mode != kDisplayKeepsSelectionFirst && mode != kDisplayKeepsSelectionLast) {
    selection = nullptr;
  }
  if (selection) {
    m_selectedIds.insert(selection->begin(), selection->end());
  }
  if (m_displayMode == mode && !selection) {
    return;
  }
  if (!resizing) {
    m_activeTitleStringId = overrideTitle;
  }
  m_displayMode = mode;
  if (mode == kDisplayOff) {
    BuildBackground(0);
    if (!m_gidLegend) {
      return;
    }
    m_legendX = m_gidLegend->m_x;
    m_legendY = m_gidLegend->m_y;
    m_gidLegend->Hide();
    m_gidLegend->DestroyNativeWindow();
    delete m_gidLegend;
    m_gidLegend = nullptr;
    return;
  }
  if (m_gidLegend) {
    if (reb::compat::IsWindowVisible(m_gidLegend->m_hwnd)) {
      m_legendX = m_gidLegend->m_x;
      m_legendY = m_gidLegend->m_y;
      m_gidLegend->Hide();
    }
    m_gidLegend->DestroyNativeWindow();
    delete m_gidLegend;
    m_gidLegend = nullptr;
  }
  DibBitmap* background = BuildBackground(1);
  m_displayMode = mode;
  m_systemItems.Clear();
  const FamilyRange sectorFamilies(0x80, 0x90);
  for (GameObjNotifier* sector = GetFirstObjectInFamilies_80_90(m_side); sector;
       sector = static_cast<GameObj*>(sector)->FindNextSibling(&sectorFamilies, 1)) {
    SystemIterator systems(sector, kSystemMarkerStateFilter);
    for (systems.Begin(); systems.Current(); systems.Advance()) {
      GameObjNotifier* system = systems.Current();
      uint32_t position;
      static_cast<System*>(system)->GetRecordField4c(&position);
      int x = reb_ftol(static_cast<double>(static_cast<int16_t>(position)) / static_cast<double>(g_displayGridWidth + 1)
                       * kDisplayMapWidth);
      int y = reb_ftol(static_cast<double>(static_cast<int16_t>(position >> 16))
                       / static_cast<double>(g_displayGridHeight + 1) * kDisplayMapHeight);
      if (m_side == 1) {
        x += 0x15;
        y += 0x19;
      } else {
        x += 0x54;
        y += 0x1b;
      }
      x += MapOffsetX();
      ObjectKey key;
      void* memory = ::operator new(sizeof(ObjectListItem));
      ObjectListItem* item = memory ? new (memory) ObjectListItem(GetObjectKey(system, &key)) : nullptr;
      item->m_x = x;
      item->m_y = y;
      RECT marker;
      reb::compat::SetRect(&marker, x, y, x + kSystemMarkerSize, y + kSystemMarkerSize);
      item->m_rect = marker;
      void* imageMemory = ::operator new(sizeof(DibBitmap));
      DibBitmap* image
          = imageMemory ? new (imageMemory) DibBitmap(kSystemMarkerSize, kSystemMarkerSize, background) : nullptr;
      background->Blit(image, 0, 0, x, y, 0, 0);
      item->SetImages(nullptr, image);
      m_systemItems.Insert(item);
      ObjectKey markerKey;
      UpdateSystemMarker(GetObjectKey(system, &markerKey), system, 0);
    }
    RedrawSectorMarkers(sector);
  }

  const std::string* title = nullptr;
  if (overrideTitle.m_id != 0) {
    title = NewTitle(&overrideTitle);
  } else if (const uint16_t titleId = TitleStringId(mode, m_side)) {
    const ResourceStringId id{static_cast<int16_t>(titleId), 2};
    title = NewTitle(&id);
  }
  if (title) {
    CoolLabel label(static_cast<const char*>(nullptr), 0, 0, 0, 0, 0, 0);
    const int left = m_panelArea.left;
    const int right = m_panelArea.right;
    label.MoveTo(left, m_panelArea.top);
    label.m_width = right - left;
    label.SetText(title->c_str());
    label.m_drawFlags = 1;
    label.m_layoutFlags = kLabelFixedWidth;
    label.SetColor(m_side != 1 ? kEmpireTitleColor : kAllianceTitleColor, nullptr);
    delete title;
    HGDIOBJ previousBitmap = reb::compat::SelectObject(m_offscreenDc, background->DibSectionHandle());
    label.Draw(m_offscreenDc);
    reb::compat::SelectObject(m_offscreenDc, previousBitmap);
    RECT labelArea = label.m_rect;
    reb::compat::InvalidateRect(m_hwnd, &labelArea, FALSE);
  }
  ShowDisplayLegend(mode);
  if (!resizing) {
    request.m_kind = 1;
    request.m_soundId = kGidSoundId;
    request.m_moduleIndex = kSpriteSlot;
    SoundManager::Play(&request);
  }
  for (const auto& ownedNode : m_children.Entries()) {
    auto* node = ownedNode.get();
    auto* child = static_cast<CoolWindow*>(node);
    if ((child->m_childId & kChildTypeMask) == kSectorWindowType) {
      reb::compat::InvalidateRect(child->m_hwnd, nullptr, FALSE);
    }
  }
}

void GalaxyWindow::ShowDisplayLegend(int mode)
{
  if (m_gidLegend) {
    m_gidLegend->DestroyNativeWindow();
    delete m_gidLegend;
    m_gidLegend = nullptr;
  }
  PanelDragButton* icon = m_gidLegendIcon;
  if (mode == kDisplayOff) {
    if (icon) {
      icon->Hide();
    }
    return;
  }
  if (icon) {
    icon->Show(5);
    reb::compat::SetWindowPos(icon->m_hwnd, SendToBottom(), 0, 0, 0, 0, kSortedOrderFlagSwpNoSizeNoMove);
  }
  if (m_panelArea.right < m_legendX + kLegendWidth) {
    m_legendX = m_panelArea.right - kLegendWidth;
  }
  if (m_panelArea.bottom < m_legendY + kLegendHeight) {
    m_legendY = m_panelArea.bottom - kLegendHeight;
  }
  void* memory = ::operator new(sizeof(AdvisorPanel));
  AdvisorPanel* legend = memory ? new (memory)
                                      AdvisorPanel(m_instance, m_legendX, m_legendY, kLegendWidth, kLegendHeight, this,
                                                   kLegendChildId, static_cast<uint32_t>(mode))
                                : nullptr;
  m_gidLegend = legend;
  legend->Create(0);
  reb::compat::SetWindowPos(m_gidLegend->m_hwnd, SendToBottom(), 0, 0, 0, 0, kSortedOrderFlagSwpNoSizeNoMove);
}

int GalaxyWindow::GidIconId(int value, int kind, int high, int midHigh, int midLow, int lowHigh, int lowLow)
{
  if (kind < 1 || kind > 3) {
    return 0;
  }
  int* row = m_gidIconIds[kind - 1];
  int index = 0;
  if (value >= high) {
    index = 3;
  } else if (value <= midHigh && value >= midLow) {
    index = 2;
  } else if (value <= lowHigh && value >= lowLow) {
    index = 1;
  }
  return row[index];
}

void GalaxyWindow::LoadGidIcons(HMODULE module)
{
  for (int kind = 0; kind < 3; ++kind) {
    for (int level = 0; level < 4; ++level) {
      m_gidIconIds[kind][level] = kGidIconBitmaps[kind][level];
    }
  }
  if (!m_gidIcons) {
    m_gidIcons = NewResourceBitmap(module, kGidIconSheetFirst, kGidBitmapKind);
  }
  if (!m_gidIcons2) {
    m_gidIcons2 = NewResourceBitmap(module, kGidIconSheetSecond, kGidBitmapKind);
  }
}

DibBitmap* GalaxyWindow::BuildBackground(int gidActive)
{
  HMODULE module = CoolAppGetResourceModule(kSpriteSlot);
  DibBitmap* tint
      = NewResourceBitmap(module, gidActive ? kBackgroundTintGid : kBackgroundTintPlain, kBitmapResourceKind);
  DibBitmap* map = NewResourceBitmap(module, m_side == 1 ? kAllianceMap : kEmpireMap, kBitmapResourceKind);
  void* memory = ::operator new(sizeof(DibBitmap));
  const int mapHeight = map->Height();
  const int mapWidth = map->Width();
  DibBitmap* canvas = memory ? new (memory) DibBitmap(std::max(mapWidth, m_clientWidth), mapHeight, map) : nullptr;
  if (!canvas) {
    ::operator delete(memory);
    DeleteBitmap(tint);
    DeleteBitmap(map);
    return nullptr;
  }
  const int canvasWidth = canvas->Width();
  const int mapOffsetX = (canvasWidth - mapWidth) / 2;
  const int tintX = (m_side != 1 ? 0x3f : 0) + 0x15;
  const int tintStart = tintX + mapOffsetX;
  const int tintWidth = tint->Width();
  const int tintEnd = tintStart + tintWidth;
  SetBackgroundPreservingAddress(canvas, 0, 0);
  const int tintY = (m_side != 1 ? 2 : 0) + 0x19;
  tint->Blit(GetBackground(), tintStart, tintY, 0, 0, 0, 0);
  if (canvasWidth > mapWidth) {
    const int edgeWidth = std::min(tintWidth, 64);
    const auto reflectedEdgeColumn = [edgeWidth](int distance) {
      if (edgeWidth <= 1) {
        return 0;
      }
      const int period = 2 * (edgeWidth - 1);
      const int offset = distance % period;
      return offset < edgeWidth ? offset : period - offset;
    };
    for (int destX = 0; destX < tintStart; ++destX) {
      const int sourceX = reflectedEdgeColumn(tintStart - destX - 1);
      tint->Blit(GetBackground(), destX, tintY, sourceX, 0, 1, 0);
    }
    for (int destX = tintEnd; destX < canvasWidth; ++destX) {
      const int sourceX = tintWidth - 1 - reflectedEdgeColumn(destX - tintEnd);
      tint->Blit(GetBackground(), destX, tintY, sourceX, 0, 1, 0);
    }
  }
  const auto fillMarginBand = [&](int destStart, int destEnd, int yStart, int yEnd, int sampleStart, int sampleWidth) {
    if (sampleWidth <= 0 || yStart >= yEnd) {
      return;
    }
    for (int destX = destStart; destX < destEnd; ++destX) {
      int sampleX = sampleStart;
      if (sampleWidth > 1) {
        const int reflectionPeriod = 2 * (sampleWidth - 1);
        const int reflectedOffset = (destX - destStart) % reflectionPeriod;
        sampleX += reflectedOffset < sampleWidth ? reflectedOffset : reflectionPeriod - reflectedOffset;
      }
      map->BlitKeyed(GetBackground(), destX, yStart, map->m_colorKey, sampleX, yStart, 1, yEnd - yStart);
    }
  };
  const auto drawFrameBand = [&](int yStart, int yEnd, int leftWidth, int rightStart, int leftSampleStart,
                                 int leftSampleWidth, int rightSampleStart, int rightSampleWidth) {
    yStart = std::clamp(yStart, 0, mapHeight);
    yEnd = std::clamp(yEnd, 0, mapHeight);
    if (yStart >= yEnd) {
      return;
    }
    const int centerWidth = rightStart - leftWidth;
    const int rightWidth = mapWidth - rightStart;
    const int canvasRight = canvasWidth - rightWidth;
    const int leftGapStart = leftWidth;
    const int leftGapEnd = leftWidth + mapOffsetX;
    const int rightGapStart = leftWidth + mapOffsetX + centerWidth;
    fillMarginBand(leftGapStart, leftGapEnd, yStart, yEnd, leftSampleStart, leftSampleWidth);
    fillMarginBand(rightGapStart, canvasRight, yStart, yEnd, rightSampleStart, rightSampleWidth);
    map->BlitKeyed(GetBackground(), 0, yStart, map->m_colorKey, 0, yStart, leftWidth, yEnd - yStart);
    map->BlitKeyed(GetBackground(), leftWidth + mapOffsetX, yStart, map->m_colorKey, leftWidth, yStart, centerWidth,
                   yEnd - yStart);
    if (rightWidth > 0) {
      map->BlitKeyed(GetBackground(), canvasRight, yStart, map->m_colorKey, rightStart, yStart, rightWidth,
                     yEnd - yStart);
    }
  };
  if (m_side == 1) {
    drawFrameBand(0, 40, 78, 560, 78, 1, 560, 1);
    drawFrameBand(40, 390, 75, 530, 0, 0, 0, 0);
    drawFrameBand(390, mapHeight, 75, 530, 55, 16, 506, 24);
  } else {
    drawFrameBand(0, 40, 128, 590, 128, 1, 590, 1);
    drawFrameBand(40, 400, 120, 590, 0, 0, 0, 0);
    drawFrameBand(400, 422, 140, 590, 140, 1, 590, 1);
    for (int y = 422; y < mapHeight; ++y) {
      const int leftWidth = std::max(71, 130 - (y - 422));
      drawFrameBand(y, y + 1, leftWidth, mapWidth, leftWidth, 1, mapWidth - 1, 1);
    }
  }
  SetDateAndSpeed(0, m_speed, 1);
  m_resourceLabels[0].Invalidate(m_hwnd, TRUE);
  m_resourceLabels[1].Invalidate(m_hwnd, TRUE);
  m_resourceLabels[2].Invalidate(m_hwnd, TRUE);
  reb::compat::InvalidateRect(m_hwnd, nullptr, FALSE);
  DeleteBitmap(tint);
  DeleteBitmap(map);
  return GetBackground();
}

const int16_t g_displayGridHeight = 1023;
const int16_t g_displayGridWidth = 1023;
