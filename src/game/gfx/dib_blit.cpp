// DibBitmap software blitter: plain and colour-keyed copies, rectangle fills and colour remapping.
// `this` is always the source bitmap; the destination is the first parameter.
#include <cstdlib>
#include <cstring>
#include <new>

#include "core/reb_x87.h"
#include "gfx/dib_bitmap.h"

namespace
{
constexpr int kBitsPerPixelRgb = 24;
constexpr int kColorTableOffset = sizeof(BITMAPINFOHEADER);
constexpr int kRemapTableEntries = 0x100;
constexpr int kRemapScratchWidth = 0x100;
constexpr int kRemapScratchHeight = 2;

// Direction in which successive visual rows move through memory: +1 for top-down bitmaps, -1 otherwise.
int RowDirection(int signedHeight)
{
  return signedHeight < 0 ? 1 : -1;
}

int AbsoluteHeight(int signedHeight)
{
  return signedHeight < 0 ? -signedHeight : signedHeight;
}

float BytesPerPixel(int bitsPerPixel)
{
  switch (bitsPerPixel)
  {
    case 1:
      return 0.125f;
    case 4:
      return 0.5f;
    case 24:
      return 3.0f;
    case 32:
      return 4.0f;
    default:
      return 1.0f;
  }
}

int TruncatedProduct(int count, float scale)
{
  return reb_ftol(static_cast<double>(count) * scale);
}

int SquaredDistance(const uint8_t* a, const uint8_t* b)
{
  const int blue = a[0] - b[0];
  const int green = a[1] - b[1];
  const int red = a[2] - b[2];
  return blue * blue + green * green + red * red;
}
} // namespace

uint32_t DibBitmap::Blit(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height)
{
  const int srcWidth = Width();
  if (srcWidth < srcX)
    return 0;
  if (!dest->IsLoaded())
    return 0;
  if (dest->Compression() != 0)
    return 0;
  if (dest->BitCount() == kBitsPerPixelRgb)
  {
    return Blit24(dest, destX, destY, srcX, srcY, width, height);
  }

  const uint8_t* srcBits = m_bits;
  SyncHighResolutionFromGdi();
  dest->SyncHighResolutionFromGdi();
  uint8_t* destBits = dest->m_bits;
  const int destStride = dest->RowStride();
  const int srcStride = RowStride();
  const int destSignedHeight = dest->Height();
  const int destWidth = dest->Width();
  const int destDirection = RowDirection(destSignedHeight);
  const int destHeight = AbsoluteHeight(destSignedHeight);
  const int srcSignedHeight = Height();
  const int srcDirection = RowDirection(srcSignedHeight);
  const int srcHeight = AbsoluteHeight(srcSignedHeight);

  if (srcY > srcHeight)
    return 0;

  if (width == 0)
  {
    if (destX < 0)
      destX = 0;
    width = destWidth - destX;
  }
  if (height == 0)
  {
    if (destY < 0)
      destY = 0;
    height = destHeight - destY;
  }
  if (destX < 0)
  {
    width += destX;
    srcX -= destX;
    destX = 0;
  }
  if (destY < 0)
  {
    height += destY;
    srcY -= destY;
    destY = 0;
  }
  if (srcX < 0)
  {
    width += srcX;
    if (width <= 0)
      return 0;
    destX -= srcX;
    srcX = 0;
  }
  if (srcY < 0)
  {
    height += srcY;
    if (height <= 0)
      return 0;
    destY -= srcY;
    srcY = 0;
  }
  if (height > srcHeight - srcY)
    height = srcHeight - srcY;
  if (width > srcWidth - srcX)
    width = srcWidth - srcX;
  if (destX >= destWidth || destY >= destHeight || width <= 0 || height <= 0)
    return 0;
  if (width > destWidth - destX)
    width = destWidth - destX;
  if (height > destHeight - destY)
    height = destHeight - destY;

  const int destRow = dest->RowIndex(destY);
  const int srcRow = RowIndex(srcY);
  const float bytesPerPixel = BytesPerPixel(BitCount());

  if (height > 0)
  {
    const auto rowBytes = static_cast<size_t>(TruncatedProduct(width, bytesPerPixel));
    const ptrdiff_t destAdvance = static_cast<ptrdiff_t>(destStride) * destDirection;
    const ptrdiff_t srcAdvance = static_cast<ptrdiff_t>(srcStride) * srcDirection;
    uint8_t* destRowPtr =
      destBits + static_cast<ptrdiff_t>(destStride) * destRow + TruncatedProduct(destX, bytesPerPixel);
    const uint8_t* srcRowPtr =
      srcBits + static_cast<ptrdiff_t>(srcStride) * srcRow + TruncatedProduct(srcX, bytesPerPixel);
    for (int row = height; row != 0; --row)
    {
      std::memcpy(destRowPtr, srcRowPtr, rowBytes);
      destRowPtr += destAdvance;
      srcRowPtr += srcAdvance;
    }
  }
  dest->CacheTransparentColor();
  dest->FinishHighResolutionBlit(this, destX, destY, srcX, srcY, width, height, HighResolutionBlit::Copy);
  return height > 0;
}

int DibBitmap::BlitKeyed(DibBitmap* dest,
                         int destX,
                         int destY,
                         uint8_t colorKey,
                         int srcX,
                         int srcY,
                         int width,
                         int height)
{
  const int srcWidth = Width();
  if (!IsLoaded())
    return 1;
  if (dest->BitCount() == kBitsPerPixelRgb)
  {
    BlitKeyed24(dest, destX, destY, colorKey, srcX, srcY, width, height);
    return 1;
  }
  if (BitCount() == kBitsPerPixelRgb)
    return 0;
  if (dest->Compression() != 0)
    return 0;

  const uint8_t* srcBits = m_bits;
  SyncHighResolutionFromGdi();
  dest->SyncHighResolutionFromGdi();
  uint8_t* destBits = dest->m_bits;
  const int destSignedHeight = dest->Height();
  const int destWidth = dest->Width();
  const int destStride = dest->RowStride();
  const int srcStride = RowStride();

  if (destY < 0)
  {
    srcY -= destY;
    if (height != 0)
      height -= destY;
    destY = 0;
  }
  if (destX < 0)
  {
    srcX -= destX;
    if (width != 0)
      width -= destX;
    destX = 0;
  }
  if (srcX < 0)
  {
    width += srcX;
    destX -= srcX;
    srcX = 0;
  }
  if (srcY < 0)
  {
    height += srcY;
    destY -= srcY;
    srcY = 0;
  }
  const int destDirection = RowDirection(destSignedHeight);
  const int destHeight = AbsoluteHeight(destSignedHeight);
  const int srcSignedHeight = Height();
  const int srcDirection = RowDirection(srcSignedHeight);
  const int srcHeight = AbsoluteHeight(srcSignedHeight);
  if (width == 0)
    width = srcWidth - srcX;
  if (height == 0)
    height = srcHeight - srcY;
  if (srcX >= srcWidth || srcY >= srcHeight || destX >= destWidth || destY >= destHeight)
    return 1;
  if (width > srcWidth - srcX)
    width = srcWidth - srcX;
  if (height > srcHeight - srcY)
    height = srcHeight - srcY;
  if (height > destHeight - destY)
    height = destHeight - destY;
  if (width > destWidth - destX)
    width = destWidth - destX;
  if (height <= 0)
    return 1;
  const int destRow = dest->RowIndex(destY);
  const int srcRow = RowIndex(srcY);

  const ptrdiff_t destAdvance = static_cast<ptrdiff_t>(destStride) * destDirection;
  const ptrdiff_t srcAdvance = static_cast<ptrdiff_t>(srcStride) * srcDirection;
  uint8_t* destRowPtr = destBits + static_cast<ptrdiff_t>(destStride) * destRow + destX;
  const uint8_t* srcRowPtr = srcBits + static_cast<ptrdiff_t>(srcStride) * srcRow + srcX;
  for (int row = height; row != 0; --row)
  {
    for (int column = 0; column < width; ++column)
    {
      if (srcRowPtr[column] != colorKey)
        destRowPtr[column] = srcRowPtr[column];
    }
    destRowPtr += destAdvance;
    srcRowPtr += srcAdvance;
  }
  dest->FinishHighResolutionBlit(this, destX, destY, srcX, srcY, width, height,
                                 HighResolutionBlit::Keyed, colorKey);
  return 1;
}

int DibBitmap::DrawKeyed(DibBitmap* dest, int destX, int destY)
{
  if (!IsLoaded())
    return 0;
  if (BitCount() == 8)
    return BlitKeyed(dest, destX, destY, m_colorKey, 0, 0, 0, 0);
  BlitKeyedRgb24(dest, destX, destY, m_colorKeyRgb, 0, 0, 0, 0);
  return 1;
}

int DibBitmap::FillRect(int x, int y, int width, int height, uint8_t value)
{
  const int stride = RowStride();
  if (Compression() != 0)
    return 0;
  const int imageHeight = AbsoluteHeight(Height());
  if (width <= 0 || height <= 0 || x >= Width() || y >= imageHeight)
    return 0;
  if (x < 0)
  {
    width += x;
    x = 0;
  }
  if (y < 0)
  {
    height += y;
    y = 0;
  }
  if (y + height > imageHeight)
    height = imageHeight - y;
  if (x + width > Width())
    width = Width() - x;
  if (height > 0 && width > 0)
  {
    SyncHighResolutionFromGdi();
    for (int row = 0; row < height; ++row)
    {
      uint8_t* rowPtr = m_bits + static_cast<ptrdiff_t>(stride) * RowIndex(row + y) + x;
      std::memset(rowPtr, value, static_cast<size_t>(width));
    }
    FinishHighResolutionFill(x, y, width, height, value);
  }
  CacheTransparentColor();
  return height > 0 && width > 0;
}

int DibBitmap::IsPixelOpaque(int x, int y)
{
  const int height = Height();
  if (Compression() != 0)
    return 0;
  const int imageHeight = AbsoluteHeight(height);
  if (m_bits && y >= 0 && y < imageHeight && x >= 0 && x < Width())
  {
    const uint8_t pixel = m_bits[static_cast<ptrdiff_t>(RowStride()) * RowIndex(y) + x];
    if (pixel != m_colorKey)
      return 1;
  }
  return 0;
}

uint8_t* DibBitmap::BuildRemapTable(uint32_t (*transform)(uint32_t))
{
  m_remapTable = nullptr;
  if (BitCount() != 8)
    return m_remapTable;

  void* scratchMemory = operator new(sizeof(DibBitmap));
  DibBitmap* scratch =
    scratchMemory ? new (scratchMemory) DibBitmap(kRemapScratchWidth, kRemapScratchHeight, this) : nullptr;
  if (!scratch)
    return m_remapTable;

  const uint8_t* original = reinterpret_cast<const uint8_t*>(m_info) + kColorTableOffset;
  uint8_t* transformed = reinterpret_cast<uint8_t*>(scratch->m_info) + kColorTableOffset;
  for (int i = 0; i < m_colorCount; ++i)
  {
    uint32_t entry;
    std::memcpy(&entry, original + i * 4, sizeof(entry));
    entry = transform(entry);
    std::memcpy(transformed + i * 4, &entry, sizeof(entry));
  }

  m_remapTable = static_cast<uint8_t*>(operator new(kRemapTableEntries));
  if (m_remapTable)
  {
    for (int i = 0; i < kRemapTableEntries; ++i)
    {
      const uint8_t* wanted = transformed + i * 4;
      const uint8_t* current = original + i * 4;
      auto nearest = static_cast<uint8_t>(i);
      if (std::memcmp(current, wanted, 3) != 0)
      {
        int bestDistance = SquaredDistance(current, wanted);
        for (uint32_t candidate = 0; candidate < static_cast<uint32_t>(m_colorCount);)
        {
          const int distance = SquaredDistance(original + candidate * 4, wanted);
          if (distance < bestDistance)
          {
            nearest = static_cast<uint8_t>(candidate);
            bestDistance = distance;
          }
          if (distance == 0)
            break;
          candidate = (candidate + 1) & 0xffff;
        }
      }
      m_remapTable[i] = nearest;
    }
  }
  scratch->~DibBitmap();
  operator delete(scratch);
  return m_remapTable;
}

void DibBitmap::SetRemapTable(uint8_t* table)
{
  m_remapTable = table;
}

void DibBitmap::ApplyRemapTable(int16_t x, int16_t y, int16_t width, int16_t height)
{
  const int stride = RowStride();
  if (Compression() != 0 || !m_remapTable)
    return;
  const int imageHeight = AbsoluteHeight(Height());
  if (x < 0 || y < 0 || x >= Width() || y >= imageHeight)
    return;
  if (height + y > imageHeight)
    height = static_cast<int16_t>(imageHeight - y);
  if (width + x > Width())
    width = static_cast<int16_t>(Width() - x);
  if (height <= 0 || width <= 0)
    return;
  SyncHighResolutionFromGdi();
  for (int row = 0; row < height; ++row)
  {
    uint8_t* rowPtr = m_bits + static_cast<ptrdiff_t>(stride) * RowIndex(y + row) + x;
    for (int column = 0; column < width; ++column)
      rowPtr[column] = m_remapTable[rowPtr[column]];
  }
  FinishHighResolutionRemap(x, y, width, height);
}

int DibBitmap::BlitRemapDestination(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height)
{
  const int srcWidth = Width();
  if (!IsLoaded())
    return 1;
  if (!m_remapTable)
    return 0;
  const uint8_t colorKey = m_colorKey;
  if (dest->Compression() != 0)
    return 0;

  const uint8_t* srcBits = m_bits;
  SyncHighResolutionFromGdi();
  dest->SyncHighResolutionFromGdi();
  uint8_t* destBits = dest->m_bits;
  const int destSignedHeight = dest->Height();
  const int destWidth = dest->Width();
  const int destStride = dest->RowStride();
  const int srcStride = RowStride();

  if (destY < 0)
  {
    srcY -= destY;
    if (height != 0)
      height -= destY;
    destY = 0;
  }
  if (destX < 0)
  {
    srcX -= destX;
    if (width != 0)
      width -= destX;
    destX = 0;
  }
  const int destDirection = RowDirection(destSignedHeight);
  const int destHeight = AbsoluteHeight(destSignedHeight);
  const int srcSignedHeight = Height();
  const int srcDirection = RowDirection(srcSignedHeight);
  const int srcHeight = AbsoluteHeight(srcSignedHeight);
  const int destRow = dest->RowIndex(destY);
  const int srcRow = RowIndex(srcY);

  if (width == 0)
    width = srcWidth - srcX;
  if (height == 0)
    height = srcHeight - srcY;
  if (height > destHeight - destY)
    height = destHeight - destY;
  if (width > destWidth - destX)
    width = destWidth - destX;

  if (height > 0)
  {
    const ptrdiff_t destAdvance = static_cast<ptrdiff_t>(destStride) * destDirection;
    const ptrdiff_t srcAdvance = static_cast<ptrdiff_t>(srcStride) * srcDirection;
    uint8_t* destRowPtr = destBits + static_cast<ptrdiff_t>(destStride) * destRow + destX;
    const uint8_t* srcRowPtr = srcBits + static_cast<ptrdiff_t>(srcStride) * srcRow + srcX;
    for (int row = height; row != 0; --row)
    {
      for (int column = 0; column < width; ++column)
      {
        if (srcRowPtr[column] != colorKey)
          destRowPtr[column] = m_remapTable[destRowPtr[column]];
      }
      destRowPtr += destAdvance;
      srcRowPtr += srcAdvance;
    }
  }
  dest->CacheTransparentColor();
  dest->FinishHighResolutionBlit(this, destX, destY, srcX, srcY, width, height,
                                 HighResolutionBlit::RemapDestination, colorKey);
  return 1;
}

int DibBitmap::BlitRemapped(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height)
{
  const int srcWidth = Width();
  const uint8_t colorKey = m_colorKey;
  if (!IsLoaded())
    return 1;
  if (!m_remapTable)
    return 0;
  if (dest->Compression() != 0)
    return 0;

  const uint8_t* srcBits = m_bits;
  SyncHighResolutionFromGdi();
  dest->SyncHighResolutionFromGdi();
  uint8_t* destBits = dest->m_bits;
  const int destSignedHeight = dest->Height();
  const int destWidth = dest->Width();
  const int destStride = dest->RowStride();
  const int srcStride = RowStride();

  if (destY < 0)
  {
    srcY -= destY;
    if (height != 0)
      height -= destY;
    destY = 0;
  }
  if (destX < 0)
  {
    srcX -= destX;
    if (width != 0)
      width -= destX;
    destX = 0;
  }
  const int destDirection = RowDirection(destSignedHeight);
  const int destHeight = AbsoluteHeight(destSignedHeight);
  const int srcSignedHeight = Height();
  const int srcDirection = RowDirection(srcSignedHeight);
  const int srcHeight = AbsoluteHeight(srcSignedHeight);
  const int destRow = dest->RowIndex(destY);
  const int srcRow = RowIndex(srcY);

  if (width == 0)
    width = srcWidth - srcX;
  if (height == 0)
    height = srcHeight - srcY;
  if (height > destHeight - destY)
    height = destHeight - destY;
  if (width > destWidth - destX)
    width = destWidth - destX;

  if (height > 0)
  {
    const ptrdiff_t destAdvance = static_cast<ptrdiff_t>(destStride) * destDirection;
    const ptrdiff_t srcAdvance = static_cast<ptrdiff_t>(srcStride) * srcDirection;
    uint8_t* destRowPtr = destBits + static_cast<ptrdiff_t>(destStride) * destRow + destX;
    const uint8_t* srcRowPtr = srcBits + static_cast<ptrdiff_t>(srcStride) * srcRow + srcX;
    for (int row = height; row != 0; --row)
    {
      for (int column = 0; column < width; ++column)
      {
        if (srcRowPtr[column] != colorKey)
          destRowPtr[column] = m_remapTable[srcRowPtr[column]];
      }
      destRowPtr += destAdvance;
      srcRowPtr += srcAdvance;
    }
  }
  dest->CacheTransparentColor();
  dest->FinishHighResolutionBlit(this, destX, destY, srcX, srcY, width, height,
                                 HighResolutionBlit::Remapped, colorKey);
  return 1;
}

int DibBitmap::BlitRemappedOverlay(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height)
{
  const int srcWidth = Width();
  if (!IsLoaded())
    return 1;
  if (!m_remapTable)
    return 0;
  const uint8_t colorKey = m_colorKey;
  if (dest->Compression() != 0)
    return 0;

  const uint8_t* srcBits = m_bits;
  SyncHighResolutionFromGdi();
  dest->SyncHighResolutionFromGdi();
  uint8_t* destBits = dest->m_bits;
  const int destSignedHeight = dest->Height();
  const int destWidth = dest->Width();
  const int destStride = dest->RowStride();
  const int srcStride = RowStride();

  if (destY < 0)
  {
    srcY -= destY;
    if (height != 0)
      height -= destY;
    destY = 0;
  }
  if (destX < 0)
  {
    srcX -= destX;
    if (width != 0)
      width -= destX;
    destX = 0;
  }
  const int destDirection = RowDirection(destSignedHeight);
  const int destHeight = AbsoluteHeight(destSignedHeight);
  const int srcSignedHeight = Height();
  const int srcDirection = RowDirection(srcSignedHeight);
  const int srcHeight = AbsoluteHeight(srcSignedHeight);
  const int destRow = dest->RowIndex(destY);
  const int srcRow = RowIndex(srcY);

  if (width == 0)
    width = srcWidth - srcX;
  if (height == 0)
    height = srcHeight - srcY;
  if (height > destHeight - destY)
    height = destHeight - destY;
  if (width > destWidth - destX)
    width = destWidth - destX;

  if (height > 0)
  {
    const ptrdiff_t destAdvance = static_cast<ptrdiff_t>(destStride) * destDirection;
    const ptrdiff_t srcAdvance = static_cast<ptrdiff_t>(srcStride) * srcDirection;
    uint8_t* destRowPtr = destBits + static_cast<ptrdiff_t>(destStride) * destRow + destX;
    const uint8_t* srcRowPtr = srcBits + static_cast<ptrdiff_t>(srcStride) * srcRow + srcX;
    for (int row = height; row != 0; --row)
    {
      for (int column = 0; column < width; ++column)
      {
        const uint8_t source = srcRowPtr[column];
        destRowPtr[column] =
          source == colorKey ? m_remapTable[kRemapTableEntries + destRowPtr[column]] : m_remapTable[source];
      }
      destRowPtr += destAdvance;
      srcRowPtr += srcAdvance;
    }
  }
  dest->CacheTransparentColor();
  dest->FinishHighResolutionBlit(this, destX, destY, srcX, srcY, width, height,
                                 HighResolutionBlit::RemappedOverlay, colorKey);
  return 1;
}
