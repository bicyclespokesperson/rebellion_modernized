#include "ui/cool_drag_list.h"
#include "core/game_string.h"
#include <algorithm>
#include <string>

#include <cstring>

#include "core/checked_new.h"
#include "gfx/dib_bitmap.h"

namespace
{

constexpr uint32_t kDragListStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_TABSTOP;
constexpr int kDefaultLineStep = 10;
constexpr COLORREF kDefaultTextColor = 0x787878;
constexpr COLORREF kDefaultSelectedTextColor = 0xffffff;
constexpr UINT kDefaultTextFormat = DT_CENTER | DT_BOTTOM | DT_SINGLELINE;
constexpr SHORT kNoDragStart = -1000;
constexpr char kBitBltMode = '@';
// A press and release closer than this (squared distance, pixels) is a click, not a drag.
constexpr int kDragThresholdSquared = 0x18;

// Messages exchanged with the parent besides WM_COMMAND.
constexpr UINT kCancelModeMessage = 0x40d; // capture/drag is cancelled (DismissPopup)
constexpr UINT kButtonDownMessage = 0x40e; // a button went down in the list (relayed to the parent)

POINTS PointsFromLParam(LPARAM lParam)
{
  POINTS point;
  point.x = static_cast<SHORT>(LOWORD(lParam));
  point.y = static_cast<SHORT>(HIWORD(lParam));
  return point;
}

} // namespace

// ---- CoolListItemList ------------------------------------------------------------------------------------------

CoolListItemList::CoolListItemList(int sortMode) : m_sortMode(0)
{
  m_sortMode.m_value = sortMode;
}

CoolListItemList::~CoolListItemList()
{
  Clear();
}

bool CoolListItemList::IsLess(const CoolListItem* first, const CoolListItem* second) const
{
  const uint32_t mode = static_cast<uint32_t>(m_sortMode.m_value);
  if (mode & kSortById)
    return first->m_id < second->m_id;
  if (mode & kSortByLabel)
    return _stricmp(first->m_text.c_str(), second->m_text.c_str()) < 0;
  if (mode & kSortByUserData)
    return static_cast<int>(first->m_userData) < static_cast<int>(second->m_userData);
  return false;
}

int CoolListItemList::Insert(CoolListItem* item)
{
  if (!item || PositionOf(item) != m_items.cend())
    return false;
  auto position = m_items.begin();
  while (position != m_items.end() && !IsLess(item, (*position).get()))
    ++position;
  std::unique_ptr<CoolListItem> pending(item);
  try
  {
    m_items.insert(position, std::move(pending));
  }
  catch (...)
  {
    pending.release();
    throw;
  }
  return true;
}

int CoolListItemList::InsertBefore(CoolListItem* item, CoolListItem* anchor)
{
  if (!item || PositionOf(item) != m_items.cend())
    return false;
  auto position = m_items.cend();
  if (anchor)
  {
    position = PositionOf(anchor);
    if (position == m_items.cend())
      return false;
  }
  std::unique_ptr<CoolListItem> pending(item);
  try
  {
    m_items.insert(position, std::move(pending));
  }
  catch (...)
  {
    pending.release();
    throw;
  }
  return true;
}

int CoolListItemList::Remove(CoolListItem* item)
{
  const auto position = std::find_if(m_items.begin(), m_items.end(),
                                     [item](const auto& owned)
                                     {
                                       return owned.get() == item;
                                     });
  if (position == m_items.end())
    return false;
  position->release();
  m_items.erase(position);
  return true;
}

void CoolListItemList::Clear()
{
  ItemList detached;
  detached.swap(m_items);
}

std::list<std::unique_ptr<CoolListItem>>::const_iterator CoolListItemList::PositionOf(const CoolListItem* item) const
{
  return std::find_if(m_items.cbegin(), m_items.cend(),
                      [item](const auto& owned)
                      {
                        return owned.get() == item;
                      });
}

// Return the first item through the owning sorted list.
CoolListItem* CoolListItemList::First()
{
  return m_items.empty() ? nullptr : m_items.front().get();
}

CoolListItem* CoolListItemList::Last()
{
  return m_items.empty() ? nullptr : m_items.back().get();
}

CoolListItem* CoolListItemList::FindById(uint32_t id)
{
  for (const auto& entry : m_items)
  {
    auto* item = entry.get();
    if (item->m_id == id)
      return item;
  }
  return nullptr;
}

// ---- CoolDragList: construction ----------------------------------------------------------------------------------

CoolDragList::CoolDragList(HINSTANCE instance,
                           int x,
                           int y,
                           int width,
                           int height,
                           CoolWindow* parent,
                           DibBitmap* backdrop,
                           uint32_t childId,
                           int itemWidth,
                           int itemHeight,
                           int,
                           int,
                           int,
                           int,
                           int lineStep,
                           int vertical,
                           HCURSOR dragCursor)
: CoolWindow(instance,
             g_coolDragListClassName,
             kDragListStyle,
             x,
             y,
             width,
             height,
             parent,
             nullptr,
             static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)),
             nullptr,
             nullptr,
             nullptr,
             0,
             0),
  m_hoverTextColor(0),
  m_options(0)
{
  m_items = NewOrNull<CoolListItemList>(static_cast<int>(kSortById));
  m_childId = childId;
  m_unknownB8 = 0;
  m_unknownBC = 0;
  m_background = nullptr;
  m_vertical = vertical;
  m_itemWidth = itemWidth;
  m_itemHeight = itemHeight;
  if (vertical)
    m_unknownBC = itemWidth;
  else
    m_unknownB8 = itemHeight;
  m_textColor = kDefaultTextColor;
  m_selectedTextColor = kDefaultSelectedTextColor;
  m_fillColor = 0;
  m_background = NewOrNull<DibBitmap>(m_clientWidth, m_clientHeight, backdrop);
  backdrop->Blit(m_background, 0, 0, x, y, 0, 0);
  m_canvas = NewOrNull<DibBitmap>(m_clientWidth, m_clientHeight, backdrop);
  m_composite = NewOrNull<DibBitmap>(m_clientWidth, m_clientHeight, backdrop);
  m_selectionImage = nullptr;
  m_scrollPos = 0;
  m_scrollMax = 0;
  m_lineStep = lineStep;
  if (lineStep == 0)
    m_lineStep = kDefaultLineStep;
  m_dragCursor = dragCursor;
  if (dragCursor)
  {
    m_cursor = dragCursor;
  }
  else
  {
    m_cursor = g_defaultCursor;
    if (!g_defaultCursor)
      m_cursor = LoadCursorA(nullptr, IDC_ARROW);
  }
  m_textFormat = kDefaultTextFormat;
  m_dragStart.x = kNoDragStart;
  m_dragStart.y = kNoDragStart;
  m_textOffsetX = 1;
  m_textOffsetY = 0;
  m_dragImage = nullptr;
  m_hoverItem = nullptr;
  m_dragWindow = m_parent->m_hwnd;
}

CoolDragList::~CoolDragList()
{
  if ((m_options.m_value & kDragListItemsNotOwned) == 0 && m_items)
    delete m_items;
  if (m_canvas)
    delete m_canvas;
  if (m_composite)
    delete m_composite;
  if (m_background)
    delete m_background;
  if (m_selectionImage)
    delete m_selectionImage;
  if (m_dragImage)
    delete m_dragImage;
}

void CoolDragList::SetItemList(CoolListItemList* items)
{
  if ((m_options.m_value & kDragListItemsNotOwned) == 0 && m_items)
    delete m_items;
  m_options.m_value |= kDragListItemsNotOwned;
  m_items = items;
}

void CoolDragList::SetItemSize(int width, int height)
{
  m_itemWidth = width;
  m_itemHeight = height;
  if (m_vertical)
    m_unknownBC = width;
  else
    m_unknownB8 = height;
}

void CoolDragList::SetBackground(DibBitmap* source, int srcX, int srcY)
{
  if (!source)
    return;
  if (!m_background)
  {
    HDC dc = GetDC(m_hwnd);
    m_background =
      NewOrNull<DibBitmap>(static_cast<LONG>(m_clientWidth), static_cast<LONG>(m_clientHeight), source, dc);
    ReleaseDC(m_hwnd, dc);
  }
  source->Blit(m_background, 0, 0, srcX, srcY, 0, 0);
}

void CoolDragList::SetDragTarget(HWND target)
{
  if (target && IsWindow(target) && IsWindowVisible(target))
    m_dragWindow = target;
  if (m_dragImage)
    delete m_dragImage;
  m_dragImage = nullptr;
  CoolWindow* window = GetCoolWindow(target);
  if (window)
    m_dragImage = NewOrNull<CoolDragImage>(window);
}

// ---- Items and selection -----------------------------------------------------------------------------------------

void CoolDragList::SetSortMode(int sortMode)
{
  CoolListItemList* sorted = NewOrNull<CoolListItemList>(sortMode);
  while (!m_items->Empty())
  {
    auto* item = static_cast<CoolListItem*>(m_items->First());
    m_items->Remove(item);
    sorted->Insert(item);
  }
  if (m_items)
    delete m_items;
  m_items = sorted;
  Layout();
  Redraw();
  if (m_hwnd)
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void CoolDragList::CopySelectionTo(CoolListItemList* out)
{
  out->Clear();
  for (const auto& entry : m_items->Entries())
  {
    auto* item = static_cast<CoolListItem*>(entry.get());
    if (item->m_state.m_value & kListItemSelected)
      out->Insert(NewOrNull<CoolListItem>(*item));
  }
}

void CoolDragList::ClearSelection()
{
  bool hadSelection = false;
  for (const auto& entry : m_items->Entries())
  {
    auto* item = static_cast<CoolListItem*>(entry.get());
    if (item->m_state.m_value & kListItemSelected)
      hadSelection = true;
    item->m_state.m_value &= ~kListItemSelected;
  }
  if (hadSelection)
    Redraw();
}

void CoolDragList::SelectById(uint32_t id, int notify)
{
  if (m_options.m_value & kDragListSingleSelect)
    ClearSelection();
  for (const auto& entry : m_items->Entries())
  {
    auto* item = static_cast<CoolListItem*>(entry.get());
    if (item->m_id == id)
    {
      item->m_state.m_value |= kListItemSelected;
      Redraw();
      break;
    }
  }
  if (notify)
    PostMessageA(m_parent->m_hwnd, WM_COMMAND, MAKEWPARAM(m_childId & 0xffff, kDragListSelectionChanged), 0);
}

void CoolDragList::SelectRect(POINTS from, POINTS to)
{
  bool selected = false;
  SHORT left = from.x;
  SHORT right = to.x;
  if (to.x < from.x)
  {
    left = to.x;
    right = from.x;
  }
  SHORT top = from.y;
  SHORT bottom = to.y;
  if (to.y < from.y)
  {
    top = to.y;
    bottom = from.y;
  }
  for (const auto& entry : m_items->Entries())
  {
    auto* item = static_cast<CoolListItem*>(entry.get());
    if (item->m_x <= right && left <= m_itemWidth + item->m_x && item->m_y <= bottom && top <= m_itemHeight + item->m_y)
    {
      selected = true;
      item->m_state.m_value |= kListItemSelected;
    }
  }
  if (selected)
  {
    Redraw();
    if (m_hwnd)
      InvalidateRect(m_hwnd, nullptr, FALSE);
  }
}

// Selects the item whose text shares the longest prefix with `prefix` (the first such item; the scan stops at
// the first item that matches less).
uint32_t CoolDragList::SelectByPrefix(const std::string* prefix, int notify, int caseSensitive)
{
  int bestLength = 0;
  CoolListItem* best = nullptr;
  if (prefix->empty())
    return 0;
  for (const auto& entry : m_items->Entries())
  {
    auto* item = static_cast<CoolListItem*>(entry.get());
    std::string& text = item->m_text;
    if (CompareStringsN(text, *prefix, 1, caseSensitive != 0) != 0)
      continue;
    const int prefixSize = static_cast<uint16_t>((prefix->empty() ? 0 : prefix->size() + 1));
    const int textSize = static_cast<uint16_t>(text.empty() ? 0 : text.size() + 1);
    int length = 1;
    while (length < (prefixSize < textSize ? prefixSize : textSize) &&
           CompareStringsN(text, *prefix, length + 1, caseSensitive != 0) == 0)
      ++length;
    if (length > bestLength)
    {
      bestLength = length;
      best = item;
    }
    else if (length < bestLength)
    {
      break;
    }
  }
  if (!best)
    return 0;
  if ((best->m_state.m_value & kListItemSelected) == 0)
  {
    SelectById(best->m_id, notify);
    ScrollIntoView(best->m_id);
  }
  return best->m_id;
}

CoolListItem* CoolDragList::AddItem(uint32_t id,
                                    uint16_t imageId,
                                    uint16_t selectedImageId,
                                    const char* text,
                                    int redraw,
                                    uintptr_t userData)
{
  CoolListItem* item = NewOrNull<CoolListItem>(id);
  if (!item)
    return nullptr;
  item->m_userPointer = userData;
  item->SetImageIds(imageId, selectedImageId, m_instance);
  item->m_text = StringFromText(text);
  m_items->Insert(item);
  Layout();
  if (redraw)
    Redraw();
  return item;
}

CoolListItem* CoolDragList::AddItemWithImages(uint32_t id,
                                              DibBitmap* image,
                                              DibBitmap* selectedImage,
                                              const char* text,
                                              int redraw,
                                              uintptr_t userData)
{
  CoolListItem* item = NewOrNull<CoolListItem>(id);
  if (!item)
    return nullptr;
  item->m_userPointer = userData;
  item->SetImages(image, selectedImage);
  item->m_text = StringFromText(text);
  m_items->Insert(item);
  Layout();
  if (redraw)
    Redraw();
  return item;
}

CoolListItem* CoolDragList::FindItem(uint32_t id)
{
  return m_items->FindById(id);
}

void CoolDragList::RemoveAll()
{
  m_items->Clear();
  Layout();
  Redraw();
}

void CoolDragList::SetSelectedTextColor(COLORREF color)
{
  m_selectedTextColor = color;
}

void CoolDragList::SetTextColor(COLORREF color)
{
  m_textColor = color;
}

CoolListItem* CoolDragList::FirstItem()
{
  return m_items ? m_items->First() : nullptr;
}

// ---- Drawing and layout --------------------------------------------------------------------------------------------

// Draws the item's image into the canvas and, when it is selected, into the selection image (in client
// coordinates, as it is dragged from there).
void CoolDragList::DrawItem(CoolListItem* item)
{
  if (!item || !m_canvas)
    return;
  const int y = item->m_y;
  const int x = item->m_x;
  DibBitmap* image = item->CurrentImage();
  if (!image)
    return;
  image->DrawKeyed(m_canvas, x, y);
  POINTS position;
  position.x = static_cast<SHORT>(x);
  position.y = static_cast<SHORT>(y);
  POINTS client = ListToClient(position);
  if ((item->m_state.m_value & kListItemSelected) && m_selectionImage)
  {
    const uint8_t savedColorKey = m_selectionImage->m_colorKey;
    image->DrawKeyed(m_selectionImage, client.x, client.y);
    m_selectionImage->m_colorKey = savedColorKey;
  }
}

void CoolDragList::Redraw()
{
  uint8_t fill = 0;
  for (const auto& entry : m_items->Entries())
  {
    auto* item = static_cast<CoolListItem*>(entry.get());
    if (fill)
      break;
    if (item->m_image)
      fill = item->m_image->m_colorKey;
  }
  if (fill)
    m_fillColor = fill;
  m_canvas->FillRect(0, 0, m_canvas->Width(), m_canvas->Height(), m_fillColor);
  if (m_selectionImage)
    m_selectionImage->FillRect(0, 0, m_selectionImage->Width(), m_selectionImage->Height(), m_fillColor);
  for (const auto& entry : m_items->Entries())
    DrawItem(static_cast<CoolListItem*>(entry.get()));
  if (m_hwnd)
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// Assigns every item its canvas position: one row (or column, if m_vertical) without kDragListFlowLayout,
// otherwise wrapping into the space the scroll bars leave. Then grows the canvas and updates the scroll range.
void CoolDragList::Layout()
{
  int x = 0;
  int y = 0;
  int availableWidth = 0;
  int availableHeight = 0;
  for (const auto& entry : m_items->Entries())
  {
    auto* item = static_cast<CoolListItem*>(entry.get());
    item->m_x = x;
    item->m_y = y;
    if ((m_options.m_value & kDragListFlowLayout) == 0)
    {
      if (m_vertical)
        y += m_itemHeight;
      else
        x += m_itemWidth;
      continue;
    }
    if (!m_verticalBar)
    {
      if (m_horizontalBar)
        availableHeight = m_clientHeight - m_horizontalBar->m_clientHeight;
      else
        availableWidth = m_clientWidth;
    }
    else if (!m_horizontalBar)
    {
      availableWidth = m_clientWidth - m_verticalBar->m_clientWidth;
    }
    else
    {
      availableWidth = m_clientWidth;
    }
    if (availableHeight)
    {
      if (y + m_itemHeight * 2 <= m_clientHeight)
      {
        y += m_itemHeight;
      }
      else
      {
        y = 0;
        x += m_itemWidth;
      }
    }
    else if (availableWidth)
    {
      if (x + m_itemWidth * 2 > availableWidth)
      {
        x = 0;
        y += m_itemHeight;
      }
      else
      {
        x += m_itemWidth;
      }
    }
  }
  if (m_options.m_value & kDragListFlowLayout)
  {
    if (availableHeight && y)
      x += m_itemWidth;
    if (availableWidth && x)
      y += m_itemHeight;
  }
  int height = m_clientHeight;
  if (y >= height)
    height = y + 5;
  int width = x;
  if (width < m_clientWidth)
    width = m_clientWidth;
  if (m_canvas->Width() < width || m_canvas->Height() < height)
  {
    DibBitmap* old = m_canvas;
    m_canvas = NewOrNull<DibBitmap>(width, height, old);
    if (old)
      delete old;
  }
  int scrollMax = m_vertical ? height - m_clientHeight : width - m_clientWidth;
  m_scrollMax = scrollMax;
  if (scrollMax < 0)
    scrollMax = 0;
  m_scrollMax = scrollMax;
  if (scrollMax < m_scrollPos)
    m_scrollPos = scrollMax;
  SetVerticalRange(scrollMax + m_clientHeight);
  SetHorizontalRange(m_clientWidth + m_scrollMax);
  UpdateItemRects();
}

CoolListItem* CoolDragList::HitTest(POINTS point)
{
  for (const auto& entry : m_items->Entries())
  {
    auto* item = static_cast<CoolListItem*>(entry.get());
    if (item->m_x <= point.x && point.x <= m_itemWidth + item->m_x && item->m_y <= point.y &&
        point.y <= m_itemHeight + item->m_y)
      return item;
  }
  return nullptr;
}

POINTS CoolDragList::ClientToList(POINTS point)
{
  if (m_vertical)
    point.y = static_cast<SHORT>(m_scrollPos + point.y);
  else
    point.x = static_cast<SHORT>(m_scrollPos + point.x);
  return point;
}

POINTS CoolDragList::ListToClient(POINTS point)
{
  if (m_vertical)
    point.y = static_cast<SHORT>(point.y - m_scrollPos);
  else
    point.x = static_cast<SHORT>(point.x - m_scrollPos);
  return point;
}

void CoolDragList::UpdateItemRects()
{
  POINTS origin;
  origin.x = 0;
  origin.y = 0;
  const POINTS offset = ListToClient(origin);
  for (const auto& entry : m_items->Entries())
  {
    auto* item = static_cast<CoolListItem*>(entry.get());
    item->m_rect.left = item->m_x + offset.x;
    item->m_rect.top = item->m_y + offset.y;
    item->m_rect.right = item->m_x + offset.x + m_itemWidth;
    item->m_rect.bottom = item->m_y + offset.y + m_itemHeight;
  }
}

void CoolDragList::ScrollIntoView(uint32_t id)
{
  CoolListItem* item = FindItem(id);
  if (!item)
    return;
  bool scrolled = false;
  int start = item->m_rect.left;
  int end = item->m_rect.right;
  int extent;
  if (m_vertical)
  {
    end = item->m_rect.bottom;
    start = item->m_rect.top;
    extent = m_clientHeight;
  }
  else
  {
    extent = m_clientWidth;
  }
  int barFirst;
  int barLast;
  if (!m_vertical)
    GetHorizontalVisibleRange(&barFirst, &barLast);
  else
    GetVerticalVisibleRange(&barFirst, &barLast);
  int delta = start;
  if (start < 0)
  {
    scrolled = true;
    m_scrollPos += start;
  }
  else if (extent < end)
  {
    delta = end - extent;
    scrolled = true;
    m_scrollPos += delta;
  }
  if (!scrolled)
    return;
  if (m_vertical)
    ScrollVerticalBar(delta);
  UpdateItemRects();
  if (m_hwnd)
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void CoolDragList::Refresh()
{
  Layout();
  Redraw();
}

// The rectangle DrawText would fill with the item's text using the list's format.
RECT CoolDragList::GetItemTextRect(CoolListItem* item)
{
  const int itemRight = item->m_rect.right;
  RECT area = item->m_rect;
  const int itemLeft = item->m_rect.left;
  const int itemBottom = item->m_rect.bottom;
  HWND hwnd = m_hwnd;
  HDC dc = GetDC(hwnd);
  SelectCurrentFont(dc);
  if (!item->m_text.empty())
  {
    DrawTextA(dc, item->m_text.c_str(), -1, &area, (m_textFormat & ~(DT_CENTER | DT_RIGHT | DT_BOTTOM)) | DT_CALCRECT);
    const UINT format = m_textFormat;
    if (format & DT_BOTTOM)
    {
      const int measuredBottom = area.bottom;
      area.bottom = itemBottom;
      area.top += itemBottom - measuredBottom;
    }
    if (format & DT_RIGHT)
    {
      area.left += itemRight - area.right;
      area.right = itemRight;
    }
    else if (format & DT_CENTER)
    {
      const int width = area.right - area.left;
      area.left = itemLeft + (itemRight - itemLeft) / 2 - width / 2;
      area.right = width + area.left;
    }
    area.right += m_textOffsetX;
    area.left += m_textOffsetX;
    area.top += m_textOffsetY;
    area.bottom += m_textOffsetY;
  }
  RestoreFont(dc);
  ReleaseDC(m_hwnd, dc);
  return area;
}

// ---- Scrolling (CoolScrollable) ----------------------------------------------------------------------------------

namespace
{

int ScrollBack(CoolDragList* list, int distance)
{
  if (list->m_scrollPos == 0)
    return 0;
  list->m_scrollPos -= distance;
  if (list->m_scrollPos < 0)
    list->m_scrollPos = 0;
  list->UpdateItemRects();
  if (list->m_hwnd)
    InvalidateRect(list->m_hwnd, nullptr, FALSE);
  return 1;
}

int ScrollForward(CoolDragList* list, int distance)
{
  if (list->m_scrollPos == list->m_scrollMax)
    return 0;
  list->m_scrollPos += distance;
  if (list->m_scrollMax < list->m_scrollPos)
    list->m_scrollPos = list->m_scrollMax;
  list->UpdateItemRects();
  if (list->m_hwnd)
    InvalidateRect(list->m_hwnd, nullptr, FALSE);
  return 1;
}

} // namespace

int CoolDragList::PageUp()
{
  return ScrollBack(this, m_clientHeight);
}

int CoolDragList::PageDown()
{
  return ScrollForward(this, m_clientHeight);
}

int CoolDragList::LineUp()
{
  return ScrollBack(this, m_lineStep) ? -m_lineStep : 0;
}

int CoolDragList::LineDown()
{
  return ScrollForward(this, m_lineStep) ? m_lineStep : 0;
}

int CoolDragList::ScrollVertical(int delta)
{
  if (!((delta < 0 && m_scrollPos > 0) || (delta > 0 && m_scrollPos < m_scrollMax)))
    return 0;
  const int position = delta + m_scrollPos;
  m_scrollPos = position;
  if (position < 0)
    m_scrollPos = 0;
  else if (m_scrollMax < position)
    m_scrollPos = m_scrollMax;
  UpdateItemRects();
  if (m_hwnd)
    InvalidateRect(m_hwnd, nullptr, FALSE);
  return 1;
}

int CoolDragList::PageLeft()
{
  return ScrollBack(this, m_clientWidth);
}

int CoolDragList::PageRight()
{
  return ScrollForward(this, m_clientWidth);
}

int CoolDragList::LineRight()
{
  return LineDown();
}

int CoolDragList::LineLeft()
{
  return LineUp();
}

// Linker-folded with ObjectKeyNode::Save: calls slot 5.
int CoolDragList::ScrollHorizontal(int delta)
{
  return ScrollVertical(delta);
}

void CoolDragList::OnScrollEnd()
{
}

// ---- Messages ----------------------------------------------------------------------------------------------------------

int CoolDragList::OnRightButtonUp(int x, int y, CoolWindow* sender)
{
  CoolWindow* parent = m_parent;
  if (!GetCapture() && parent && sender)
  {
    POINT point;
    point.x = x;
    point.y = y;
    MapWindowPoints(sender->m_hwnd, m_hwnd, &point, 1);
    parent->OnRightButtonUp(point.x, point.y, this);
  }
  return 0;
}

namespace
{

WPARAM NotificationWParam(const CoolDragList* list, uint16_t code)
{
  return MAKEWPARAM(list->m_childId & 0xffff, code);
}

// The selection change of a click (button down, or button up after a press on a selected item): control toggles
// the item, shift selects the rectangle from the anchor, a plain click makes the item the only selection.
void ApplyClick(CoolDragList* list, UINT message, WPARAM keys, POINTS point)
{
  HWND parent = list->m_parent->m_hwnd;
  if (keys & MK_CONTROL)
  {
    CoolListItem* item = list->HitTest(point);
    if (!item)
      return;
    item->m_state.m_value ^= kListItemSelected;
    PostMessageA(parent, WM_COMMAND, NotificationWParam(list, kDragListSelectionChanged), 0);
    list->Redraw();
    return;
  }
  if (keys & MK_SHIFT)
  {
    if ((list->m_options.m_value & kDragListAnchorSet) == 0)
      list->m_anchor = point;
    list->SelectRect(list->m_anchor, point);
    PostMessageA(parent, WM_COMMAND, NotificationWParam(list, kDragListSelectionChanged), 0);
    return;
  }
  CoolListItem* item = list->HitTest(point);
  if (!item)
  {
    list->ClearSelection();
    PostMessageA(parent, WM_COMMAND, NotificationWParam(list, kDragListSelectionChanged), 0);
    return;
  }
  const uint32_t previousState = item->m_state.m_value;
  list->ClearSelection();
  item->m_state.m_value |= kListItemSelected;
  const uint16_t code = ((previousState & kListItemSelected) && message == WM_LBUTTONDOWN) ? kDragListReselected :
                                                                                             kDragListSelectionChanged;
  PostMessageA(parent, WM_COMMAND, NotificationWParam(list, code), 0);
  list->Redraw();
}

} // namespace

namespace
{

COLORREF ItemTextColor(const CoolDragList* list, const CoolListItem* item)
{
  const uint32_t state = item->m_state.m_value;
  if (state & kListItemSelected)
    return item->m_ownColors ? item->m_selectedColor : list->m_selectedTextColor;
  if (state & kListItemHovered)
    return item->m_ownColors ? item->m_hoverColor : list->m_hoverTextColor;
  return item->m_ownColors ? item->m_color : list->m_textColor;
}

void DrawItemText(CoolDragList* list, HDC dc, CoolListItem* item)
{
  SetTextColor(dc, ItemTextColor(list, item));
  if (item->m_fontSlot)
    list->SetFont(item->m_fontSlot);
  UINT format = item->m_textFormat;
  if (!format)
    format = list->m_textFormat;
  list->SelectCurrentFont(dc);
  RECT area = item->m_rect;
  area.left += list->m_textOffsetX + item->m_textOffsetX;
  area.top += list->m_textOffsetY + item->m_textOffsetY;
  area.right += list->m_textOffsetX + item->m_textOffsetX;
  area.bottom += list->m_textOffsetY + item->m_textOffsetY;
  if (format & DT_WORDBREAK)
    format |= DT_NOCLIP;
  switch (format & (DT_CENTER | DT_RIGHT))
  {
    case DT_CENTER:
      area.left += 2;
      area.right -= 2;
      break;
    case DT_RIGHT:
      area.left += 3;
      break;
    default:
      area.right -= 3;
      break;
  }
  if (!item->m_text.empty())
    DrawTextA(dc, item->m_text.c_str(), -1, &area, format);
  list->RestoreFont(dc);
}

LRESULT Paint(CoolDragList* list, HWND hwnd)
{
  PAINTSTRUCT paint;
  HDC dc = BeginPaint(hwnd, &paint);
  HPALETTE previousPalette = SelectPalette(dc, g_palette, FALSE);
  if (list->m_canvas)
  {
    int offsetX;
    int offsetY;
    if (list->m_vertical)
    {
      offsetX = 0;
      offsetY = list->m_scrollPos;
    }
    else
    {
      offsetX = list->m_scrollPos;
      offsetY = 0;
    }
    const int width = list->m_clientWidth;
    const int height = list->m_clientHeight;
    const int right = width + offsetX;
    const int bottom = height + offsetY;
    POINTS origin;
    origin.x = static_cast<SHORT>(offsetX);
    origin.y = static_cast<SHORT>(offsetY);
    if (list->m_background)
      list->m_background->Blit(list->m_composite, 0, 0, 0, 0, 0, 0);
    else
      list->m_composite->FillRect(0, 0, width, height, 0);
    list->m_canvas->BlitKeyed(list->m_composite, 0, 0, list->m_fillColor, offsetX, offsetY, width, height);
    list->m_composite->DrawToDc(dc, kBitBltMode, SRCCOPY, 0, 0, 0, 0, list->m_clientWidth, list->m_clientHeight);
    CoolListItem* item = list->HitTest(origin);
    SetBkMode(dc, TRANSPARENT);
    auto position = list->m_items->PositionOf(item);
    for (; position != list->m_items->Entries().end(); ++position)
    {
      item = static_cast<CoolListItem*>((*position).get());
      if (item->m_x > right || item->m_y > bottom)
        break;
      if (!item->m_text.empty())
        DrawItemText(list, dc, item);
    }
  }
  SelectPalette(dc, previousPalette, FALSE);
  EndPaint(hwnd, &paint);
  return 0;
}

// Keyboard navigation moves the (single) selection; Return activates it. Other keys go to the parent.
LRESULT KeyDown(CoolDragList* list, WPARAM key, LPARAM lParam)
{
  CoolListItem* first = list->m_items->First();
  if (!first || (list->m_options.m_value & kDragListForwardMouse))
    return list->m_parent->WindowProc(WM_KEYDOWN, key, lParam);
  CoolListItem* current = nullptr;
  for (const auto& entry : list->m_items->Entries())
  {
    auto* item = static_cast<CoolListItem*>(entry.get());
    if (item->m_state.m_value & kListItemSelected)
    {
      current = item;
      break;
    }
  }
  if (!current)
    current = first;
  CoolListItem* target = current;
  LRESULT result = 0;
  bool move = true;
  switch (key)
  {
    case VK_RETURN:
      if (!current)
      {
        target = list->m_items->First();
      }
      else
      {
        move = false;
        current->m_state.m_value |= kListItemSelected;
        PostMessageA(list->m_parent->m_hwnd, WM_COMMAND, NotificationWParam(list, kDragListActivated), lParam);
      }
      break;
    default:
      result = list->m_parent->WindowProc(WM_KEYDOWN, key, lParam);
      move = false;
      break;
    case VK_SHIFT:
    case VK_CONTROL:
    case VK_MENU:
      result = list->CoolWindow::WindowProc(WM_KEYDOWN, key, lParam);
      move = false;
      break;
    case VK_PRIOR:
      if (!current)
      {
        current = list->m_items->First();
      }
      else
      {
        target = current;
        auto position = list->m_items->PositionOf(target);
        for (int rows = list->m_clientHeight / list->m_itemHeight; rows > 0; --rows)
        {
          if (position == list->m_items->Entries().begin())
            rows = 0;
          else
          {
            --position;
            target = static_cast<CoolListItem*>((*position).get());
          }
        }
      }
      break;
    case VK_NEXT:
      if (!current)
      {
        current = list->m_items->First();
      }
      else
      {
        target = current;
        auto position = list->m_items->PositionOf(target);
        for (int rows = list->m_clientHeight / list->m_itemHeight; rows > 0; --rows)
        {
          if (position == list->m_items->Entries().end())
            rows = 0;
          else
          {
            auto next = position;
            ++next;
            if (next == list->m_items->Entries().end())
            {
              rows = 0;
              continue;
            }
            position = next;
            target = static_cast<CoolListItem*>((*position).get());
          }
        }
      }
      break;
    case VK_END:
      target = static_cast<CoolListItem*>(list->m_items->Last());
      break;
    case VK_HOME:
      target = list->m_items->First();
      break;
    case VK_UP:
      if (!current)
      {
        target = list->m_items->First();
      }
      else if (list->m_items->PositionOf(current) == list->m_items->Entries().begin())
      {
        move = false;
        target = current;
      }
      else
      {
        auto position = list->m_items->PositionOf(current);
        --position;
        target = static_cast<CoolListItem*>((*position).get());
      }
      break;
    case VK_DOWN:
      if (!current)
      {
        target = static_cast<CoolListItem*>(list->m_items->Last());
      }
      else
      {
        auto position = list->m_items->PositionOf(current);
        ++position;
        if (position == list->m_items->Entries().end())
        {
          move = false;
          target = current;
        }
        else
        {
          target = static_cast<CoolListItem*>((*position).get());
        }
      }
      break;
  }
  if (move)
  {
    if (current)
      current->m_state.m_value &= ~kListItemSelected;
    if (!target)
    {
      result = list->m_parent->WindowProc(WM_KEYDOWN, key, lParam);
    }
    else
    {
      list->SelectById(target->m_id, 1);
      list->ScrollIntoView(target->m_id);
    }
  }
  return result;
}

LRESULT ButtonDown(CoolDragList* list, HWND hwnd, UINT message, WPARAM keys, LPARAM lParam)
{
  if (list->m_options.m_value & kDragListForwardMouse)
    return list->m_parent->WindowProc(message, keys, lParam);
  if (message == WM_RBUTTONDOWN && (list->m_options.m_value & kDragListDragging))
    return 0;
  PostMessageA(list->m_parent->m_hwnd, kButtonDownMessage, keys, lParam);
  const POINTS point = list->ClientToList(PointsFromLParam(lParam));
  if (message == WM_LBUTTONDOWN)
  {
    SetCaptureWindow(hwnd);
    list->m_dragStart = PointsFromLParam(lParam);
  }
  SetFocus(list->m_hwnd);
  if (list->m_options.m_value & kDragListSingleSelect)
    keys = 0;
  CoolListItem* item = list->HitTest(point);
  if (item && (item->m_state.m_value & kListItemSelected))
    list->m_options.m_value |= kDragListClickedSelected;
  else
    ApplyClick(list, message, keys, point);
  list->m_options.m_value |= kDragListAnchorSet;
  list->m_anchor = point;
  return 0;
}

LRESULT ButtonUp(CoolDragList* list, HWND hwnd, UINT message, WPARAM keys, LPARAM lParam)
{
  const POINTS point = list->ClientToList(PointsFromLParam(lParam));
  if (list->m_options.m_value & kDragListDragging)
  {
    const POINTS client = PointsFromLParam(lParam);
    const int dx = client.x - list->m_dragStart.x;
    const int dy = client.y - list->m_dragStart.y;
    const bool outside =
      client.x < 0 || list->m_clientWidth < client.x || client.y < 0 || list->m_clientHeight < client.y;
    if (dx * dx + dy * dy > kDragThresholdSquared && (outside || (list->m_options.m_value & kDragListDropInside) == 0))
    {
      const uint32_t childId = list->m_childId;
      const LPARAM position = static_cast<LPARAM>(GetMessagePos());
      PostMessageA(list->m_parent->m_hwnd, WM_COMMAND, MAKEWPARAM(childId & 0xffff, kDragListDropped), position);
      list->m_options.m_value &= ~kDragListClickedSelected;
    }
  }
  if (list->m_options.m_value & kDragListClickedSelected)
  {
    if (list->m_options.m_value & kDragListSingleSelect)
      keys = 0;
    ApplyClick(list, message, keys, point);
  }
  if (GetCapture() == hwnd)
    ReleaseCapture();
  list->m_options.m_value &= ~kDragListClickedSelected;
  return 0;
}

// Starts the drag once the left button moves with the capture held; then moves the drag image, or tracks the
// hovered item when not dragging.
LRESULT MouseMove(CoolDragList* list, HWND hwnd, WPARAM keys, LPARAM lParam)
{
  POINT cursor;
  cursor.y = static_cast<SHORT>(HIWORD(lParam));
  cursor.x = static_cast<SHORT>(LOWORD(lParam));
  if (GetCapture() == hwnd && (keys & MK_LBUTTON) &&
      (list->m_options.m_value & (kDragListDragging | kDragListNoDrag)) == 0)
  {
    list->m_options.m_value |= kDragListDragging;
    if (list->m_dragStart.x == kNoDragStart && list->m_dragStart.y == kNoDragStart)
      list->m_dragStart = PointsFromLParam(lParam);
    UpdateWindow(list->m_dragWindow);
    list->m_dragImage->ClearImage();
    list->m_dragImage->SetImage(list->m_selectionImage);
    list->m_dragImage->SetHotspot(list->m_dragStart.x, list->m_dragStart.y);
    RECT client;
    SetRect(&client, 0, 0, list->m_clientWidth, list->m_clientHeight);
    POINT start;
    start.y = list->m_dragStart.y;
    start.x = list->m_dragStart.x;
    POINT at;
    if (PtInRect(&client, start))
    {
      at.x = list->m_dragStart.x;
      at.y = list->m_dragStart.y;
    }
    else
    {
      at.x = 0;
      at.y = 0;
      for (const auto& entry : list->m_items->Entries())
      {
        auto* item = static_cast<CoolListItem*>(entry.get());
        if (item->m_state.m_value & kListItemSelected)
        {
          at.x = item->m_x;
          at.y = item->m_y;
          break;
        }
      }
    }
    list->m_dragImage->BeginDrag(at);
  }
  if ((list->m_options.m_value & kDragListDragging) == 0)
  {
    SetCursor(list->m_cursor);
    if ((list->m_options.m_value & kDragListHoverTracking) == 0)
      return 0;
    CoolListItem* item = list->HitTest(list->ClientToList(PointsFromLParam(lParam)));
    if (item == list->m_hoverItem)
      return 0;
    if (list->m_hoverItem)
    {
      list->m_hoverItem->m_state.m_value &= ~kListItemHovered;
      InvalidateRect(hwnd, &list->m_hoverItem->m_rect, FALSE);
    }
    list->m_hoverItem = item;
    if (!item)
      return 0;
    item->m_state.m_value |= kListItemHovered;
    InvalidateRect(hwnd, &list->m_hoverItem->m_rect, FALSE);
    return 0;
  }
  ClientToScreen(hwnd, &cursor);
  ScreenToClient(list->m_dragWindow, &cursor);
  list->m_dragImage->MoveTo(&cursor);
  if (list->m_scrollMax == 0)
    SetCursor(list->m_cursor);
  else if (list->m_dragCursor)
    SetCursor(list->m_dragCursor);
  else
    SetCursor(list->m_cursor);
  return 0;
}

LRESULT CancelDrag(CoolDragList* list, HWND hwnd)
{
  if (list->m_options.m_value & kDragListDragging)
  {
    list->m_dragImage->EndDrag();
    list->m_options.m_value &= ~kDragListDragging;
    list->m_dragStart.x = kNoDragStart;
    list->m_dragStart.y = kNoDragStart;
    InvalidateRect(list->m_dragWindow, nullptr, FALSE);
  }
  if (GetCapture() == hwnd)
    ReleaseCapture();
  return 0;
}

} // namespace

LRESULT CoolDragList::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  HWND hwnd = m_hwnd;
  switch (message)
  {
    case WM_CREATE:
      if (!m_dragImage)
        m_dragImage = NewOrNull<CoolDragImage>(m_parent);
      m_selectionImage = NewOrNull<DibBitmap>(m_clientWidth, m_clientHeight, m_canvas);
      return 0;
    case WM_PAINT:
      return Paint(this, hwnd);
    case WM_ERASEBKGND:
      return 1;
    case WM_DRAWITEM:
      return 0;
    case WM_KEYDOWN:
      return KeyDown(this, wParam, lParam);
    case WM_COMMAND:
      return m_parent->WindowProc(WM_COMMAND, wParam, lParam);
    case WM_MOUSEMOVE:
      return MouseMove(this, hwnd, wParam, lParam);
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
      return ButtonDown(this, hwnd, message, wParam, lParam);
    case WM_LBUTTONUP:
      return ButtonUp(this, hwnd, message, wParam, lParam);
    case WM_LBUTTONDBLCLK:
      PostMessageA(m_parent->m_hwnd, WM_COMMAND, NotificationWParam(this, kDragListActivated), lParam);
      return 0;
    case WM_CAPTURECHANGED:
    case kCancelModeMessage:
      return CancelDrag(this, hwnd);
    case kButtonDownMessage:
      PostMessageA(m_parent->m_hwnd, kButtonDownMessage, wParam, lParam);
      return 0;
    default:
      return CoolWindow::WindowProc(message, wParam, lParam);
  }
}
