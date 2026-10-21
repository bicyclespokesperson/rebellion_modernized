#include "strategy_ui/sector_marker_art.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <vector>

#include "gfx/dib_bitmap.h"

namespace
{
struct Point {
  double x;
  double y;
};

bool Polygon(double x, double y, std::initializer_list<Point> points)
{
  bool inside = false;
  Point previous = *(points.end() - 1);
  for (Point point : points) {
    if ((point.y > y) != (previous.y > y)
        && x < (previous.x - point.x) * (y - point.y) / (previous.y - point.y) + point.x) {
      inside = !inside;
    }
    previous = point;
  }
  return inside;
}

bool Rect(double x, double y, double left, double top, double right, double bottom)
{
  return x >= left && x <= right && y >= top && y <= bottom;
}

bool Ellipse(double x, double y, double cx, double cy, double rx, double ry)
{
  return std::pow((x - cx) / rx, 2) + std::pow((y - cy) / ry, 2) <= 1;
}

bool MarkerShape(int kind, bool empire, double x, double y)
{
  switch (kind) {
    case 0: // Production: three stacks above a factory with four open bays.
      if (Rect(x, y, 5, 1, 6, 4.5) || Rect(x, y, 7, 1, 8, 4.5) || Rect(x, y, 9, 1, 10, 4.5)) {
        return true;
      }
      return Rect(x, y, 1, 4, 12, 9)
             && !(y > 6 && y < 8.2 && (Rect(x, y, 3, 6, 4, 8.2) || Rect(x, y, 5, 6, 6, 8.2)
                                      || Rect(x, y, 7, 6, 8, 8.2) || Rect(x, y, 9, 6, 10, 8.2)));
    case 1: // Planetary defenses: a gun emplacement on a tapered pedestal.
      return Ellipse(x, y, 5, 10.5, 3.5, 1.5) || Rect(x, y, 5, 10, 11, 10.8)
             || Polygon(x, y, {{3.4, 11}, {6.5, 11}, {6.2, 16.4}, {8, 17.5}, {2, 17.5}, {3.7, 16.4}});
    case 2:
      if (empire) {
        return Polygon(x, y, {{7, 5.6}, {20.8, 4.4}, {20, 1}, {21.2, 1}, {25.2, 4.2},
                               {27, 6.4}, {23, 7}, {23, 8}, {21.8, 8}, {21.8, 7}, {12.2, 6.7}});
      }
      return Ellipse(x, y, 14.2, 2.5, 3.5, 1.5) || Rect(x, y, 14, .3, 15, 1.5)
             || Rect(x, y, 17, 2, 26.5, 2.8) || Rect(x, y, 25, 1, 26.8, 4)
             || Polygon(x, y, {{13, 3}, {15.8, 3}, {15.8, 8.8}, {13, 8.8}, {14.4, 7.5},
                               {13, 6.2}, {14.4, 5}, {13, 4.5}});
    case 3:
      if (empire) {
        const double dx = x - 21.5;
        const double dy = y - 12.5;
        const double radius = std::hypot(dx, dy);
        const double spoke = std::remainder(std::atan2(dy, dx), 3.141592653589793 / 3);
        return radius <= 5.4 && (radius >= 4.2 || radius <= 1.3 || std::abs(std::sin(spoke) * radius) < .65);
      }
      return (Ellipse(x, y, 21.5, 12.4, 5.4, 5.4)
              && !Ellipse(x, y, 21.5, 10.6, 3.8, 4.4) && (y > 10 || x < 18.5 || x > 24.5))
             || Polygon(x, y, {{20.5, 15.8}, {21, 10}, {20.1, 9.3}, {21.5, 7.1}, {22.9, 9.3},
                               {22, 10}, {22.5, 15.8}});
  }
  return false;
}

uint8_t NearestColor(const DibBitmap& bitmap, int red, int green, int blue)
{
  const auto* palette = reinterpret_cast<const RGBQUAD*>(reinterpret_cast<const uint8_t*>(bitmap.m_info)
                                                       + bitmap.HeaderSize());
  int bestDistance = std::numeric_limits<int>::max();
  uint8_t best = 0;
  for (int index = 0; index < bitmap.m_colorCount; ++index) {
    if (index == bitmap.m_colorKey) {
      continue;
    }
    const int dr = palette[index].rgbRed - red;
    const int dg = palette[index].rgbGreen - green;
    const int db = palette[index].rgbBlue - blue;
    const int distance = dr * dr + dg * dg + db * db;
    if (distance < bestDistance) {
      bestDistance = distance;
      best = static_cast<uint8_t>(index);
    }
  }
  return best;
}
} // namespace

void AddSectorMarkerDetail(DibBitmap& bitmap, uint16_t resourceId)
{
  if (resourceId < 0x2a13 || resourceId > 0x2a26 || !bitmap.IsLoaded() || bitmap.BitCount() != 8
      || bitmap.Width() < 27 || bitmap.Width() > 28 || bitmap.Height() < 18 || bitmap.Height() > 19) {
    return;
  }
  const int offset = resourceId - 0x2a13;
  const int kind = (offset % 8) / 2;
  const bool selected = (offset & 1) != 0;
  const bool empire = offset >= 8 && offset < 16;
  const bool neutral = offset >= 16;
  constexpr int scale = 4;
  const int width = bitmap.Width() * scale;
  const int height = bitmap.Height() * scale;
  std::array<uint8_t, 32> shades;
  for (size_t i = 0; i < shades.size(); ++i) {
    const double light = .45 + .55 * i / (shades.size() - 1);
    shades[i] = NearestColor(bitmap, static_cast<int>((neutral ? 110 : empire ? 85 : 250) * light),
                            static_cast<int>((neutral ? 210 : empire ? 250 : 65) * light),
                            static_cast<int>((neutral ? 225 : 45) * light));
  }
  const uint8_t outline = NearestColor(bitmap, 235, 240, 220);
  std::vector<uint8_t> pixels(static_cast<size_t>(width) * height, bitmap.m_colorKey);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const double px = (x + .5) / scale;
      const double py = (y + .5) / scale;
      uint8_t color = bitmap.m_colorKey;
      if (MarkerShape(kind, empire, px, py)) {
        const double sheen = .6 + .4 * std::cos((py - (kind < 2 ? 4.8 : 10)) * 1.1);
        color = shades[static_cast<size_t>(sheen * (shades.size() - 1))];
      } else if (selected) {
        for (int direction = 0; direction < 16; ++direction) {
          const double angle = direction * 3.141592653589793 / 8;
          if (MarkerShape(kind, empire, px + .7 * std::cos(angle), py + .7 * std::sin(angle))) {
            color = outline;
            break;
          }
        }
      }
      pixels[static_cast<size_t>(y) * width + x] = color;
    }
  }
  bitmap.SetHighResolutionPixels(scale, pixels, true);
}
