// Pen output: MoveToEx / LineTo / Polyline and Rectangle. Cosmetic (1 pixel) pens only; PS_SOLID, PS_DASH, PS_DOT, PS_NULL.
#define MINWIN_IMPLEMENTATION 1
#include <stdlib.h>

#include "gdi_internal.h"

namespace mwgdi
{

namespace
{

constexpr int kPsNull = 5;

struct Stroker
{
  Dc* dc;
  uint8_t pen;
  uint8_t gap;
  bool dashed;
  int dashOn = 0, dashOff = 0;
  int phase = 0;

  explicit Stroker(Dc* d) : dc(d)
  {
    pen = ResolveColor(d, d->pen->color);
    gap = ResolveColor(d, d->bkColor);
    int style = d->pen->style;
    dashed = style == 1 || style == 2;
    dashOn = dashOff = style == 1 ? 0 : 3;
    if (style == 1)
    {
      dashOn = 18;
      dashOff = 6;
    }
  }

  void Pixel(int x, int y)
  {
    if (!dashed)
    {
      PlotPixel(dc, x, y, pen, dc->rop2);
      return;
    }
    int period = dashOn + dashOff;
    bool on = (phase % period) < dashOn;
    ++phase;
    if (on)
      PlotPixel(dc, x, y, pen, dc->rop2);
    else if (dc->bkMode == 2)
      PlotPixel(dc, x, y, gap, dc->rop2);
  }
};

// The line rasteriser: one pixel per step of the major axis, the minor coordinate rounded to nearest. Exact ties go to the
// smaller coordinate (measured against Wine's gdi32: x-major lines tie toward smaller y, y-major toward smaller x). The end
// point is excluded unless includeEnd.
template <typename Plot>
void BresenhamLine(int x0, int y0, int x1, int y1, bool includeEnd, Plot plot)
{
  int dx = abs(x1 - x0), dy = abs(y1 - y0);
  int sx = x1 >= x0 ? 1 : -1, sy = y1 >= y0 ? 1 : -1;
  if (dx >= dy)
  {
    int tie = sy > 0 ? -1 : 0;
    for (int i = 0; i < dx; ++i)
      plot(x0 + sx * i, y0 + sy * ((2 * dy * i + dx + tie) / (2 * dx)));
  }
  else
  {
    int tie = sx > 0 ? -1 : 0;
    for (int i = 0; i < dy; ++i)
      plot(x0 + sx * ((2 * dx * i + dy + tie) / (2 * dy)), y0 + sy * i);
  }
  if (includeEnd)
    plot(x1, y1);
}

} // namespace

namespace
{
void StrokeLine(Stroker& s, int x0, int y0, int x1, int y1)
{
  BresenhamLine(x0, y0, x1, y1, false,
                [&](int x, int y)
                {
                  s.Pixel(x, y);
                });
}
} // namespace

} // namespace mwgdi

using namespace mwgdi;

extern "C" {

BOOL REB_STDCALL MoveToEx(HDC hdc, int x, int y, LPPOINT lppt)
{
  Dc* dc = DcOf(hdc);
  if (!dc)
    return FALSE;
  if (lppt)
  {
    lppt->x = dc->curX;
    lppt->y = dc->curY;
  }
  dc->curX = x;
  dc->curY = y;
  return TRUE;
}

BOOL REB_STDCALL LineTo(HDC hdc, int x, int y)
{
  Dc* dc = DcOf(hdc);
  if (!dc)
    return FALSE;
  if (dc->pen->style != kPsNull && dc->surface.bits)
  {
    Stroker s(dc);
    StrokeLine(s, dc->curX, dc->curY, x, y);
  }
  dc->curX = x;
  dc->curY = y;
  return TRUE;
}

BOOL REB_STDCALL Polyline(HDC hdc, const POINT* apt, int cpt)
{
  Dc* dc = DcOf(hdc);
  if (!dc || !apt || cpt < 2)
    return FALSE;
  if (dc->pen->style == kPsNull || !dc->surface.bits)
    return TRUE;
  Stroker s(dc); // the dash pattern runs on from one segment to the next
  for (int i = 0; i + 1 < cpt; ++i)
    StrokeLine(s, apt[i].x, apt[i].y, apt[i + 1].x, apt[i + 1].y);
  return TRUE;
}

BOOL REB_STDCALL Rectangle(HDC hdc, int left, int top, int right, int bottom)
{
  Dc* dc = DcOf(hdc);
  if (!dc)
    return FALSE;
  if (left > right)
  {
    int t = left;
    left = right;
    right = t;
  }
  if (top > bottom)
  {
    int t = top;
    top = bottom;
    bottom = t;
  }
  if (left == right || top == bottom || !dc->surface.bits)
    return TRUE;
  bool penVisible = dc->pen->style != 5;
  if (dc->brush->style != 1)
  {
    uint8_t fill = ResolveColor(dc, dc->brush->color);
    if (penVisible)
      FillSurfaceRect(dc, left + 1, top + 1, right - 1, bottom - 1, fill, dc->rop2);
    else
      FillSurfaceRect(dc, left, top, right - 1, bottom - 1, fill, dc->rop2);
  }
  if (penVisible)
  {
    Stroker s(dc);
    int r = right - 1, b = bottom - 1;
    for (int x = left; x < r; ++x)
      s.Pixel(x, top);
    for (int y = top; y < b; ++y)
      s.Pixel(r, y);
    for (int x = r; x > left; --x)
      s.Pixel(x, b);
    for (int y = b; y > top; --y)
      s.Pixel(left, y);
    if (r == left && b == top)
      s.Pixel(left, top);
  }
  return TRUE;
}

} // extern "C"
