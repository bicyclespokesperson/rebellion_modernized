#include "gfx/dib_bitmap.h"

#include <algorithm>
#include <cstring>

#if defined(REB_MINWIN)
#include "port/minwin_impl/gdi_api.h"
#endif

namespace
{
constexpr size_t kMaximumDetailBytes = 64u * 1024u * 1024u;

int VisualHeight(const DibBitmap& bitmap)
{
  const int height = bitmap.Height();
  return height < 0 ? -height : height;
}

bool DetailSize(const DibBitmap& bitmap, int scale, size_t& size)
{
  if (!bitmap.IsLoaded() || bitmap.BitCount() != 8 || scale < 2 || scale > 4) {
    return false;
  }
  const size_t logical = static_cast<size_t>(bitmap.Width()) * VisualHeight(bitmap);
  if (logical > kMaximumDetailBytes / (scale * scale)) {
    return false;
  }
  size = logical * scale * scale;
  return true;
}

bool PrepareDetail(DibBitmap& bitmap, int scale)
{
  size_t size = 0;
  if (!DetailSize(bitmap, scale, size)) {
    return false;
  }
  if (bitmap.m_hdScale == scale && bitmap.m_hdPixels.size() == size) {
    return true;
  }
  std::vector<uint8_t> detail(size);
  const int oldScale = bitmap.m_hdScale;
  const bool hadDetail = oldScale > 1 && !bitmap.m_hdPixels.empty();
  const int width = bitmap.Width();
  for (int y = 0; y < VisualHeight(bitmap) * scale; ++y) {
    for (int x = 0; x < width * scale; ++x) {
      uint8_t value;
      if (hadDetail) {
        const int sourceX = (x / scale) * oldScale + (x % scale) * oldScale / scale;
        const int sourceY = (y / scale) * oldScale + (y % scale) * oldScale / scale;
        value = bitmap.m_hdPixels[static_cast<size_t>(sourceY) * width * oldScale + sourceX];
      } else {
        value = bitmap.m_bits[static_cast<size_t>(bitmap.RowIndex(y / scale)) * bitmap.RowStride() + x / scale];
      }
      detail[static_cast<size_t>(y) * width * scale + x] = value;
    }
  }
  bitmap.m_hdPixels = std::move(detail);
  bitmap.m_hdScale = scale;
  return true;
}

void PublishDetail(DibBitmap& bitmap, int x = 0, int y = 0, int width = 0, int height = 0)
{
#if defined(REB_MINWIN)
  if (!bitmap.DibSectionHandle()) {
    return;
  }
  if (!bitmap.m_hdPixels.empty()) {
    const int deviceScale = minwin_gdi::DeviceScale();
    if (bitmap.m_hdGdiScale != deviceScale) {
      x = y = width = height = 0;
    }
    const uint64_t published = minwin_gdi::SetDeviceBitmapPixels(bitmap.DibSectionHandle(), bitmap.m_hdScale,
                                                                 bitmap.m_hdPixels, x, y, width, height);
    if (published) {
      bitmap.m_hdGdiRevision = published;
    }
    bitmap.m_hdGdiScale = deviceScale;
  } else {
    minwin_gdi::InvalidateDeviceBitmap(bitmap.DibSectionHandle());
    bitmap.m_hdGdiRevision = 0;
    bitmap.m_hdGdiScale = 0;
  }
#else
  (void)bitmap;
#endif
}

uint8_t DetailAt(const DibBitmap& source, const std::vector<uint8_t>& pixels, int x, int y, int dx, int dy,
                 int targetScale)
{
  if (pixels.empty()) {
    return source.m_bits[static_cast<size_t>(source.RowIndex(y)) * source.RowStride() + x];
  }
  const int scale = source.m_hdScale;
  const int sourceX = x * scale + dx * scale / targetScale;
  const int sourceY = y * scale + dy * scale / targetScale;
  return pixels[static_cast<size_t>(sourceY) * source.Width() * scale + sourceX];
}
} // namespace

bool DibBitmap::SetHighResolutionPixels(int scale, std::span<const uint8_t> pixels, bool keyedByDetailPixel)
{
  size_t expected = 0;
  if (!DetailSize(*this, scale, expected) || pixels.size() != expected) {
    return false;
  }
  for (uint8_t pixel : pixels) {
    if (pixel >= m_colorCount) {
      return false;
    }
  }
  m_hdPixels.assign(pixels.begin(), pixels.end());
  m_hdScale = scale;
  m_hdKeyed = keyedByDetailPixel;
  PublishDetail(*this);
  return true;
}

void DibBitmap::PrepareHighResolutionDestination(const DibBitmap* source)
{
  if (m_hdPixels.empty() && source && source->m_hdKeyed && !source->m_hdPixels.empty()) {
    PrepareDetail(*this, source->m_hdScale);
  }
}

void DibBitmap::SyncHighResolutionFromGdi()
{
#if defined(REB_MINWIN)
  if (!DibSectionHandle() || BitCount() != 8) {
    return;
  }
  if (minwin_gdi::DeviceBitmapRevision(DibSectionHandle()) == m_hdGdiRevision) {
    return;
  }
  minwin_gdi::DeviceBitmapPixels device;
  if (!minwin_gdi::CopyDeviceBitmapPixels(DibSectionHandle(), device) || device.revision == 0
      || device.revision == m_hdGdiRevision) {
    return;
  }
  size_t expected = 0;
  if (!DetailSize(*this, device.scale, expected) || device.bits.size() != expected) {
    return;
  }
  m_hdPixels = std::move(device.bits);
  m_hdScale = device.scale;
  m_hdGdiRevision = device.revision;
  m_hdGdiScale = minwin_gdi::DeviceScale();
#endif
}

void DibBitmap::FinishHighResolutionBlit(const DibBitmap* source, int destX, int destY, int srcX, int srcY, int width,
                                         int height, HighResolutionBlit operation, uint8_t colorKey)
{
  if (!source || BitCount() != 8 || source->BitCount() != 8) {
    NotifyPixelsChanged();
    return;
  }
  if (srcX < 0) {
    width += srcX;
    destX -= srcX;
    srcX = 0;
  }
  if (srcY < 0) {
    height += srcY;
    destY -= srcY;
    srcY = 0;
  }
  if (destX < 0) {
    width += destX;
    srcX -= destX;
    destX = 0;
  }
  if (destY < 0) {
    height += destY;
    srcY -= destY;
    destY = 0;
  }
  width = std::min({width, source->Width() - srcX, Width() - destX});
  height = std::min({height, VisualHeight(*source) - srcY, VisualHeight(*this) - destY});
  if (width <= 0 || height <= 0) {
    return;
  }
  const bool sourceDetailUseful = !source->m_hdPixels.empty() && operation != HighResolutionBlit::RemapDestination;
  if (m_hdPixels.empty() && !sourceDetailUseful) {
    PublishDetail(*this);
    return;
  }
  const int sourceScale = sourceDetailUseful ? source->m_hdScale : 1;
  const int targetScale = std::max(m_hdScale, sourceScale);
  const std::vector<uint8_t> selfSource = source == this ? source->m_hdPixels : std::vector<uint8_t>{};
  const std::vector<uint8_t>& sourcePixels = source == this ? selfSource : source->m_hdPixels;
  const bool sourceHasDetailKey = source->m_hdKeyed && !sourcePixels.empty();
  const bool hadDestinationDetail = !m_hdPixels.empty();
  if (!PrepareDetail(*this, targetScale)) {
    m_hdPixels.clear();
    m_hdScale = 1;
    PublishDetail(*this);
    return;
  }
  const int scale = m_hdScale;
  const int targetWidth = Width() * scale;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const int sourceColumn = srcX + x;
      const int sourceRow = srcY + y;
      const uint8_t logicalSource
          = source->m_bits[static_cast<size_t>(source->RowIndex(sourceRow)) * source->RowStride() + sourceColumn];
      for (int dy = 0; dy < scale; ++dy) {
        for (int dx = 0; dx < scale; ++dx) {
          uint8_t& target
              = m_hdPixels[static_cast<size_t>((destY + y) * scale + dy) * targetWidth + (destX + x) * scale + dx];
          const uint8_t sourceValue = DetailAt(*source, sourcePixels, sourceColumn, sourceRow, dx, dy, scale);
          const bool keyed = sourceHasDetailKey ? sourceValue != colorKey : logicalSource != colorKey;
          switch (operation) {
            case HighResolutionBlit::Copy:
              target = sourceValue;
              break;
            case HighResolutionBlit::Keyed:
              if (keyed) {
                target = sourceValue;
              }
              break;
            case HighResolutionBlit::RemapDestination:
              if (keyed && source->m_remapTable) {
                target = source->m_remapTable[target];
              }
              break;
            case HighResolutionBlit::Remapped:
              if (keyed && source->m_remapTable) {
                target = source->m_remapTable[sourceValue];
              }
              break;
            case HighResolutionBlit::RemappedOverlay:
              if (source->m_remapTable) {
                if (keyed) {
                  target = source->m_remapTable[sourceValue];
                } else if (hadDestinationDetail) {
                  target = source->m_remapTable[256 + target];
                }
              }
              break;
          }
        }
      }
    }
  }
  if (sourceHasDetailKey) {
    m_hdKeyed = true;
  }
  PublishDetail(*this, destX, destY, width, height);
}

void DibBitmap::FinishHighResolutionFill(int x, int y, int width, int height, uint8_t value)
{
  if (!m_hdPixels.empty()) {
    const int scale = m_hdScale;
    for (int row = y * scale; row < (y + height) * scale; ++row) {
      std::memset(m_hdPixels.data() + static_cast<size_t>(row) * Width() * scale + x * scale, value,
                  static_cast<size_t>(width) * scale);
    }
  }
  PublishDetail(*this, x, y, width, height);
}

void DibBitmap::FinishHighResolutionRemap(int x, int y, int width, int height)
{
  if (!m_hdPixels.empty() && m_remapTable) {
    const int scale = m_hdScale;
    for (int row = y * scale; row < (y + height) * scale; ++row) {
      for (int column = x * scale; column < (x + width) * scale; ++column) {
        uint8_t& pixel = m_hdPixels[static_cast<size_t>(row) * Width() * scale + column];
        pixel = m_remapTable[pixel];
      }
    }
  }
  PublishDetail(*this, x, y, width, height);
}
