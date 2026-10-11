#include "strategy_ui/finder_helpers.h"
#include <iterator>
#include <string>

#include "ai/ai_common.h"
#include "core/game_string.h"
#include "gameobj/game_obj.h"
#include "ui/cool_button.h"
#include "ui/cool_drag_list.h"
#include "ui/cool_label.h"
#include "ui/cool_string_field.h"
#include "ui/cool_tab_control.h"
#include "ui/custom_dialog_box.h"

namespace
{

void SelectNeighbourTab(CoolTabControl* tabs, WPARAM key)
{
  const uint32_t selectedId = tabs->m_selected ? tabs->m_selected->m_childId : 0;
  auto& children = tabs->m_children;
  auto position = children.PositionOf(children.FindById(selectedId));
  if (position != children.Entries().end())
  {
    if (key == VK_LEFT)
    {
      if (position == children.Entries().begin())
        position = children.Entries().end();
      else
        position = std::prev(position);
    }
    else
      position = std::next(position);
    while (position != children.Entries().end())
    {
      CoolWindow* tab = static_cast<CoolWindow*>(position->get());
      if (IsWindowVisible(tab->m_hwnd))
      {
        tabs->SelectTab(tab->m_childId, 1);
        return;
      }
      if (key == VK_LEFT)
      {
        if (position == children.Entries().begin())
          position = children.Entries().end();
        else
          position = std::prev(position);
      }
      else
        position = std::next(position);
    }
    return;
  }
  if (AvlNode* first = tabs->m_children.First())
    tabs->SelectTab(static_cast<CoolWindow*>(first)->m_childId, 1);
}

} // namespace

void FinderOnKeyDown(CustomDialogBox* window,
                     CoolTabControl* tabs,
                     CoolStringField* nameField,
                     CoolDragList* list,
                     WPARAM key,
                     LPARAM lParam,
                     bool forwardOtherKeys)
{
  switch (key)
  {
    case VK_TAB:
      break;
    default:
      if (forwardOtherKeys)
        window->m_parent->WindowProc(WM_KEYDOWN, key, lParam);
      break;
    case VK_ESCAPE:
      window->PostCloseRequest();
      return;
    case VK_LEFT:
    case VK_RIGHT:
      SelectNeighbourTab(tabs, key);
      break;
    case VK_UP:
      SetFocus(nameField->m_hwnd);
      return;
    case VK_DOWN:
      if (list->m_items->First())
      {
        SetFocus(list->m_hwnd);
        PostMessageA(list->m_hwnd, WM_KEYDOWN, key, lParam);
        return;
      }
  }
}

extern const char* g_noForcesText; // 0x6a8788 "-"

void CountIntoCell(ForceCell* cells, int cellCount, GameObj* force)
{
  ObjectKey recordKey;
  GetObjectRecordKey(force, &recordKey);
  for (int i = 0; i < cellCount; ++i)
  {
    if (recordKey.m_value != cells[i].m_key.m_value)
      continue;
    ++cells[i].m_count;
    cells[i].m_flags |= kCellFlagPresent;
    if (!(force->m_stateFlags & 1))
      cells[i].m_flags |= kCellFlagUnusable;
    return;
  }
}

namespace
{

constexpr int kNoBrush = 5;
constexpr int kEmpire = 2;

void ShowCount(CoolLabel* label, std::string* text, uint16_t count, int x)
{
  text->clear();
  text->append(std::to_string(static_cast<unsigned long>(static_cast<int16_t>(count))));
  label->MoveTo(x, 2);
  label->SetText(text->c_str());
}

} // namespace

void DrawForceCells(const ForceCell* cells,
                    int cellCount,
                    CoolLabel* label,
                    std::string* text,
                    int firstX,
                    int side,
                    const HBRUSH* brushes,
                    HDC dc)
{
  label->SetFont(10);
  label->m_drawFlags = 1;
  label->m_layoutFlags = kLabelFixedWidth;
  label->m_width = 0x1c;
  int x = firstX;
  int brush = kNoBrush;
  COLORREF color = ObjectKey().m_value;
  for (int i = 0; i < cellCount; ++i)
  {
    const ForceCell& cell = cells[i];
    ShowCount(label, text, cell.m_count, x);
    if (cell.m_count == 0)
    {
      label->SetText(g_noForcesText);
      color = 0xffffff;
      brush = kNoBrush;
    }
    else if (!(cell.m_flags & kCellFlagPresent))
    {
      if (cell.m_flags & kCellFlagUnusable)
      {
        color = 0xffffff;
        brush = 2;
      }
    }
    else if (!(cell.m_flags & kCellFlagUnusable))
    {
      color = side == kEmpire ? 0 : 0xffffff;
      brush = side == kEmpire ? 1 : 0;
    }
    else
    {
      color = side == kEmpire ? 0 : 0xffffff;
      brush = side == kEmpire ? 4 : 3;
    }
    if (brush < kNoBrush)
    {
      HGDIOBJ saved = SelectObject(dc, brushes[brush]);
      PatBlt(dc, x, 2, 0x15, 0x10, PATCOPY);
      SelectObject(dc, saved);
    }
    label->SetColor(color, nullptr);
    label->Draw(dc);
    x += 0x1c;
  }
}

const char* g_noForcesText = nullptr;
