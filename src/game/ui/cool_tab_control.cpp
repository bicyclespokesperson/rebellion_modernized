#include "ui/cool_tab_control.h"

#include "core/checked_new.h"
#include "gfx/dib_bitmap.h"
#include "ui/cool_button.h"

namespace
{

constexpr uint32_t kTabControlStyle = WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
constexpr char kBitBltMode = '@';
constexpr int kPressedState = 1;
constexpr int kSelectedState = 4;

// The id comes back zero-extended in EAX and is passed on as a dword.
uint32_t PressedResourceId(CoolStrobeButton* tab)
{
  return static_cast<uint16_t>(tab->GetStateResourceId(kPressedState));
}

WPARAM TabCommand(const CoolStrobeButton* tab, uint32_t controlId)
{
  return static_cast<WPARAM>(tab->m_childId << 16 | (controlId & 0xffff));
}

} // namespace

CoolTabControl::CoolTabControl(HINSTANCE instance, int x, int y, int width, int height, uint32_t childId,
                               CoolWindow* parent, DibBitmap* parentBackground)
    : CoolWindow(instance, "CoolTabControl", kTabControlStyle, x, y, width, height, parent, nullptr,
                 static_cast<HBRUSH>(reb::compat::GetStockObject(WHITE_BRUSH)), nullptr, nullptr, nullptr, 0, 0)
{
  m_childId = childId;
  m_selected = nullptr;
  m_background = NewOrNull<DibBitmap>(width, height, parentBackground);
  parentBackground->Blit(m_background, 0, 0, x, y, 0, 0);
}

CoolTabControl::~CoolTabControl()
{
  if (m_background) {
    delete m_background;
  }
}

CoolStrobeButton* CoolTabControl::AddTab(CoolStrobeButton* tab, int select, int notify)
{
  if (m_children.FindByHwnd(tab->m_hwnd)) {
    return nullptr;
  }
  HWND hwnd = m_hwnd;
  // The tab's m_parent is left as it was.
  if (tab->m_parent->m_hwnd != hwnd) {
    reb::compat::SetParent(tab->m_hwnd, hwnd);
  }
  tab->SetStateFlags(kButtonToggle);
  if (!tab->GetStateBitmap(kSelectedState)) {
    tab->LoadStateBitmap(kSelectedState, PressedResourceId(tab));
  }
  if (hwnd) {
    tab->Show(SW_SHOW);
  }
  m_children.Insert(tab);
  if (select || !m_selected) {
    SelectTab(tab->m_childId, notify);
  }
  return tab;
}

void CoolTabControl::ReloadSelectedBitmaps()
{
  for (const auto& entry : m_children.Entries()) {
    auto* tab = static_cast<CoolStrobeButton*>(entry.get());
    tab->LoadStateBitmap(kSelectedState, PressedResourceId(tab));
  }
}

void CoolTabControl::SelectTab(uint32_t tabId, int notify)
{
  auto* tab = static_cast<CoolStrobeButton*>(m_children.FindById(tabId));
  if (!tab) {
    return;
  }
  if (m_selected) {
    m_selected->ClearStateFlags(kButtonChecked);
  }
  if (!tab->HasStateFlags(kButtonChecked)) {
    tab->SetStateFlags(kButtonChecked);
  }
  m_selected = tab;
  if (notify) {
    reb::compat::SendMessageA(m_parent->m_hwnd, WM_COMMAND, TabCommand(tab, m_childId), 0);
  }
}

LRESULT CoolTabControl::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  HWND hwnd = m_hwnd;
  HWND parentHwnd = m_parent->m_hwnd;
  if (message == WM_ERASEBKGND) {
    return 1;
  }
  if (message == WM_PAINT) {
    PAINTSTRUCT paint;
    HDC dc = reb::compat::BeginPaint(hwnd, &paint);
    HPALETTE previous = reb::compat::SelectPalette(dc, g_palette, FALSE);
    if (m_background) {
      m_background->DrawToDc(dc, kBitBltMode, SRCCOPY, 0, 0, 0, 0, 0, 0);
    }
    reb::compat::SelectPalette(dc, previous, FALSE);
    reb::compat::EndPaint(hwnd, &paint);
    return 0;
  }
  if (message == WM_COMMAND) {
    auto* tab = static_cast<CoolStrobeButton*>(m_children.FindById(static_cast<uint32_t>(wParam)));
    if (!tab) {
      return 0;
    }
    if (m_selected != tab) {
      if (m_selected && m_selected->HasStateFlags(kButtonChecked)) {
        m_selected->ClearStateFlags(kButtonChecked);
        reb::compat::UpdateWindow(m_selected->m_hwnd);
      }
      m_selected = tab;
    }
    if (!m_selected->HasStateFlags(kButtonChecked)) {
      m_selected->SetStateFlags(kButtonChecked);
      reb::compat::UpdateWindow(m_selected->m_hwnd);
    }
    reb::compat::PostMessageA(parentHwnd, WM_COMMAND, TabCommand(m_selected, m_childId), 0);
    return 0;
  }
  return CoolWindow::WindowProc(message, wParam, lParam);
}

void CoolTabControl::SetBackground(DibBitmap* source, int x, int y)
{
  if (!m_background || !source) {
    return;
  }
  delete m_background;
  m_background = NewOrNull<DibBitmap>(source->Width(), source->Height(), source);
  source->Blit(m_background, 0, 0, x, y, 0, 0);
  for (const auto& entry : m_children.Entries()) {
    auto* tab = static_cast<CoolStrobeButton*>(entry.get());
    tab->m_backdropX = 0;
    tab->m_backdrop = m_background;
    tab->m_backdropY = 0;
  }
  reb::compat::InvalidateRect(m_hwnd, nullptr, FALSE);
}
