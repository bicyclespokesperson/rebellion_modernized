#include "ui/cool_status_bar.h"

namespace
{

HBRUSH CreateSolidColorBrush(COLORREF color)
{
  LOGBRUSH brush;
  brush.lbColor = color;
  brush.lbStyle = BS_SOLID;
  brush.lbHatch = 0;
  return CreateBrushIndirect(&brush);
}

void FillWithBrush(HDC dc, HBRUSH brush, int right, int bottom)
{
  HGDIOBJ previousBrush = SelectObject(dc, brush);
  HGDIOBJ previousPen = SelectObject(dc, GetStockObject(NULL_PEN));
  Rectangle(dc, 0, 0, right, bottom);
  SelectObject(dc, previousBrush);
  SelectObject(dc, previousPen);
}

} // namespace

CoolwinStatusBar::CoolwinStatusBar(int x,
                                   int y,
                                   int width,
                                   int height,
                                   CoolWindow* parent,
                                   COLORREF barColor,
                                   COLORREF backgroundColor,
                                   uint32_t style)
: CoolWindow(parent->m_instance,
             "CoolwinStatusBar",
             style,
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
             0)
{
  // The stored colours start different from the requested ones so that the setters create the brushes.
  m_backgroundColor = backgroundColor + 1;
  m_range = 100;
  m_tickStep = 0;
  m_position = 0;
  m_barColor = barColor + 1;
  m_barBrush = nullptr;
  m_backgroundBrush = nullptr;
  SetBarColor(barColor);
  SetBackgroundColor(backgroundColor);
  m_tickPen = nullptr;
}

CoolwinStatusBar::~CoolwinStatusBar()
{
  if (m_barBrush)
    DeleteObject(m_barBrush);
  if (m_backgroundBrush)
    DeleteObject(m_backgroundBrush);
  if (m_tickPen)
    DeleteObject(m_tickPen);
}

void CoolwinStatusBar::SetBarColor(COLORREF color)
{
  if (color == m_barColor)
    return;
  m_barColor = color;
  if (m_barBrush)
    DeleteObject(m_barBrush);
  m_barBrush = CreateSolidColorBrush(color);
  if (m_hwnd)
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void CoolwinStatusBar::SetBackgroundColor(COLORREF color)
{
  if (color == m_backgroundColor)
    return;
  m_backgroundColor = color;
  if (m_backgroundBrush)
    DeleteObject(m_backgroundBrush);
  m_backgroundBrush = CreateSolidColorBrush(color);
  if (m_hwnd)
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void CoolwinStatusBar::SetTickColor(COLORREF color)
{
  if (color == m_tickColor)
    return;
  if (m_tickPen)
    DeleteObject(m_tickPen);
  m_tickPen = CreatePen(PS_SOLID, 1, color);
  m_tickColor = color;
  if (m_hwnd)
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void CoolwinStatusBar::SetRange(uint32_t range, uint32_t tickStep)
{
  m_range = range;
  m_tickStep = tickStep;
  if (m_hwnd)
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void CoolwinStatusBar::SetPosition(uint32_t position)
{
  m_position = position;
  if (m_hwnd)
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

LRESULT CoolwinStatusBar::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
  if (message != WM_PAINT)
    return DefWindowProcA(m_hwnd, message, wParam, lParam);

  PAINTSTRUCT paint;
  HDC dc = BeginPaint(m_hwnd, &paint);
  if (m_backgroundBrush)
    FillWithBrush(dc, m_backgroundBrush, m_clientWidth + 1, m_clientHeight + 1);
  if (m_barBrush && m_position && m_range != 0)
  {
    const uint32_t barWidth = static_cast<uint32_t>(m_clientWidth) * m_position / m_range;
    FillWithBrush(dc, m_barBrush, static_cast<int>(barWidth + 1), m_clientHeight + 1);
  }
  if (m_tickStep)
  {
    if (m_range == 0)
    {
      EndPaint(m_hwnd, &paint);
      return 0;
    }
    HGDIOBJ previousPen = SelectObject(dc, m_tickPen ? static_cast<HGDIOBJ>(m_tickPen) : GetStockObject(BLACK_PEN));
    for (uint32_t tick = 0; tick < m_range; tick += m_tickStep)
    {
      const int x = static_cast<int>(tick * static_cast<uint32_t>(m_clientWidth) / m_range);
      MoveToEx(dc, x, 0, nullptr);
      LineTo(dc, x, m_clientHeight);
    }
    SelectObject(dc, previousPen);
  }
  EndPaint(m_hwnd, &paint);
  return 0;
}
