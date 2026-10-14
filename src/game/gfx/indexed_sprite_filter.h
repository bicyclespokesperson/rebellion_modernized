#pragma once

#include <array>
#include <cstdint>

class DibBitmap;

// Builds indexed, transparent-aware 4x detail for a decoded animation frame.
class IndexedSpriteFilter
{
public:
  bool EnsureDetail(DibBitmap& bitmap);

private:
  void UpdatePalette(const DibBitmap& bitmap);

  std::array<uint8_t, 256 * 4> m_palette{};
  std::array<uint8_t, 32 * 32 * 32> m_nearest{};
  int m_colorCount = 0;
  uint8_t m_colorKey = 0;
};
