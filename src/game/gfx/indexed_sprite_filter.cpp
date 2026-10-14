#include "gfx/indexed_sprite_filter.h"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "gfx/dib_bitmap.h"

namespace
{
constexpr int kScale = 4;
constexpr size_t kMaximumDetailBytes = 64u * 1024u * 1024u;

uint8_t LogicalPixel(const DibBitmap& bitmap, int x, int y)
{
  return bitmap.m_bits[static_cast<size_t>(bitmap.RowIndex(y)) * bitmap.RowStride() + x];
}

int ColorDistance(const uint8_t* color, int red, int green, int blue)
{
  const int dr = static_cast<int>(color[2]) - red;
  const int dg = static_cast<int>(color[1]) - green;
  const int db = static_cast<int>(color[0]) - blue;
  return dr * dr + dg * dg + db * db;
}
} // namespace

void IndexedSpriteFilter::UpdatePalette(const DibBitmap& bitmap)
{
  const auto* palette = reinterpret_cast<const uint8_t*>(bitmap.m_info) + bitmap.HeaderSize();
  const int count = bitmap.m_colorCount;
  const uint8_t key = bitmap.m_colorKey;
  if (m_colorCount == count && m_colorKey == key &&
      std::memcmp(m_palette.data(), palette, static_cast<size_t>(count) * 4) == 0)
    return;

  std::memcpy(m_palette.data(), palette, static_cast<size_t>(count) * 4);
  m_colorCount = count;
  m_colorKey = key;
  for (int red = 0; red < 32; ++red)
    for (int green = 0; green < 32; ++green)
      for (int blue = 0; blue < 32; ++blue)
      {
        const int r = red * 8 + 4;
        const int g = green * 8 + 4;
        const int b = blue * 8 + 4;
        int bestDistance = 3 * 256 * 256;
        uint8_t best = key;
        for (int index = 0; index < count; ++index)
        {
          if (index == key)
            continue;
          const int distance = ColorDistance(m_palette.data() + index * 4, r, g, b);
          if (distance < bestDistance)
          {
            bestDistance = distance;
            best = static_cast<uint8_t>(index);
          }
        }
        m_nearest[(red * 32 + green) * 32 + blue] = best;
      }
}

bool IndexedSpriteFilter::EnsureDetail(DibBitmap& bitmap)
{
  if (!bitmap.m_hdPixels.empty())
    return true;
  if (!bitmap.IsLoaded() || bitmap.BitCount() != 8 || bitmap.Compression() != 0 || !bitmap.m_bits ||
      bitmap.m_colorCount < 2 || bitmap.m_colorCount > 256 || bitmap.Width() < 1 ||
      bitmap.Height() == 0 || bitmap.Height() == INT_MIN || bitmap.m_colorKey >= bitmap.m_colorCount)
    return false;
  const int width = bitmap.Width();
  const int height = std::abs(bitmap.Height());
  if (static_cast<size_t>(width) * height > kMaximumDetailBytes / (kScale * kScale))
    return false;
  UpdatePalette(bitmap);

  std::vector<uint8_t> detail(static_cast<size_t>(width) * height * kScale * kScale);
  const uint8_t key = bitmap.m_colorKey;
  const int stride = width * kScale;
  for (int y = 0; y < height; ++y)
    for (int x = 0; x < width; ++x)
    {
      const uint8_t center = LogicalPixel(bitmap, x, y);
      if (center >= bitmap.m_colorCount)
        return false;
      for (int dy = 0; dy < kScale; ++dy)
        for (int dx = 0; dx < kScale; ++dx)
        {
          uint8_t output = key;
          if (center != key)
          {
            const int nearX = std::clamp(x + (dx < 2 ? -1 : 1), 0, width - 1);
            const int nearY = std::clamp(y + (dy < 2 ? -1 : 1), 0, height - 1);
            const int centerXWeight = dx == 0 || dx == 3 ? 5 : 7;
            const int centerYWeight = dy == 0 || dy == 3 ? 5 : 7;
            const int xWeights[2] = {centerXWeight, 8 - centerXWeight};
            const int yWeights[2] = {centerYWeight, 8 - centerYWeight};
            const int xs[2] = {x, nearX};
            const int ys[2] = {y, nearY};
            int sumBlue = 0, sumGreen = 0, sumRed = 0, sumWeight = 0;
            for (int yi = 0; yi < 2; ++yi)
              for (int xi = 0; xi < 2; ++xi)
              {
                const uint8_t index = LogicalPixel(bitmap, xs[xi], ys[yi]);
                if (index == key || index >= bitmap.m_colorCount)
                  continue;
                const int weight = xWeights[xi] * yWeights[yi];
                const uint8_t* color = m_palette.data() + index * 4;
                sumBlue += color[0] * weight;
                sumGreen += color[1] * weight;
                sumRed += color[2] * weight;
                sumWeight += weight;
              }
            const int blue = (sumBlue + sumWeight / 2) / sumWeight;
            const int green = (sumGreen + sumWeight / 2) / sumWeight;
            const int red = (sumRed + sumWeight / 2) / sumWeight;
            output = m_nearest[((red >> 3) * 32 + (green >> 3)) * 32 + (blue >> 3)];
            const uint8_t* centerColor = m_palette.data() + center * 4;
            const uint8_t* outputColor = m_palette.data() + output * 4;
            if (ColorDistance(centerColor, red, green, blue) <=
                ColorDistance(outputColor, red, green, blue))
              output = center;
          }
          detail[static_cast<size_t>(y * kScale + dy) * stride + x * kScale + dx] = output;
        }
    }
  return bitmap.SetHighResolutionPixels(kScale, detail);
}
