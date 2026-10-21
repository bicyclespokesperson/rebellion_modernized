#include <cstring>

#include "gfx/dib_bitmap.h"

namespace
{
constexpr int kBitsPerPixelRgb = 24;
constexpr int kBitsPerPixelPalette = 8;
constexpr int kColorTableOffset = sizeof(BITMAPINFOHEADER);
constexpr uint32_t kOpaqueKeyWhite = 0xff;

int RowDirection(int signedHeight)
{
  return signedHeight < 0 ? 1 : -1;
}

int AbsoluteHeight(int signedHeight)
{
  return signedHeight < 0 ? -signedHeight : signedHeight;
}

const uint8_t* paletteEntry(const DibBitmap* bitmap, uint8_t index)
{
  return reinterpret_cast<const uint8_t*>(bitmap->m_info) + bitmap->HeaderSize() + index * 4;
}
} // namespace

Rgb24::Rgb24(uint8_t red, uint8_t green, uint8_t blue)
{
  r = red;
  g = green;
  b = blue;
}

Rgb24::Rgb24(uint32_t rgbQuad)
{
  r = static_cast<uint8_t>(rgbQuad >> 16);
  g = static_cast<uint8_t>(rgbQuad >> 8);
  b = static_cast<uint8_t>(rgbQuad);
}

int Rgb24::DiffersFrom(uint32_t rgbQuad)
{
  return static_cast<uint8_t>(rgbQuad >> 16) != r || static_cast<uint8_t>(rgbQuad >> 8) != g
         || static_cast<uint8_t>(rgbQuad) != b;
}

void DibBitmap::BlitKeyed24(DibBitmap* dest, int destX, int destY, uint32_t paletteIndexOrColor, int srcX, int srcY,
                            int width, int height)
{
  uint32_t key = kOpaqueKeyWhite;
  if (BitCount() == kBitsPerPixelPalette) {
    key = *reinterpret_cast<const uint32_t*>(paletteEntry(this, static_cast<uint8_t>(paletteIndexOrColor)));
  }
  key &= 0xffffff;
  Rgb24 keyColor(key);
  BlitKeyedRgb24(dest, destX, destY, keyColor, srcX, srcY, width, height);
}

void DibBitmap::BlitKeyedRgb24(DibBitmap* dest, int destX, int destY, Rgb24 colorKey, int srcX, int srcY, int width,
                               int height)
{
  uint8_t* destBits = dest->m_bits;
  const int destSignedHeight = dest->Height();
  const int destWidth = dest->Width();
  const int destStride = dest->RowStride();
  const uint8_t* srcBits = m_bits;
  const int srcSignedHeight = Height();
  const int srcWidth = Width();
  const int srcStride = RowStride();
  const int bitCount = BitCount();

  if (destY < 0) {
    srcY -= destY;
    if (height != 0) {
      height -= destY;
    }
    destY = 0;
  }
  if (destX < 0) {
    srcX -= destX;
    if (width != 0) {
      width -= destX;
    }
    destX = 0;
  }
  const int destHeight = AbsoluteHeight(destSignedHeight);
  const int destDirection = RowDirection(destSignedHeight);
  const int srcHeight = AbsoluteHeight(srcSignedHeight);
  const int srcDirection = RowDirection(srcSignedHeight);
  if (width == 0) {
    width = srcWidth - srcX;
  }
  if (height == 0) {
    height = srcHeight - srcY;
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
  if (width <= 0 || height <= 0 || srcX >= srcWidth || srcY >= srcHeight || destX >= destWidth || destY >= destHeight) {
    return;
  }
  if (width > srcWidth - srcX) {
    width = srcWidth - srcX;
  }
  if (height > srcHeight - srcY) {
    height = srcHeight - srcY;
  }
  if (destHeight - destY < height) {
    height = destHeight - destY;
  }
  if (destWidth - destX < width) {
    width = destWidth - destX;
  }
  if (width <= 0 || height <= 0) {
    return;
  }
  const int destRow = dest->RowIndex(destY);
  const int srcRow = RowIndex(srcY);

  if (bitCount == kBitsPerPixelRgb) {
    int destRowOffset = destRow * destStride;
    int srcRowOffset = srcRow * srcStride;
    for (int row = 0; row < height; ++row) {
      const uint8_t* source = srcBits + srcRowOffset + srcX * 3;
      uint8_t* target = destBits + destRowOffset + destX * 3;
      for (int column = 0; column < width; ++column) {
        if (source[0] != colorKey.b || source[1] != colorKey.g || source[2] != colorKey.r) {
          target[0] = source[0];
          target[1] = source[1];
          target[2] = source[2];
        }
        source += 3;
        target += 3;
      }
      srcRowOffset += srcDirection * srcStride;
      destRowOffset += destDirection * destStride;
    }
  } else {
    int srcRowOffset = srcRow * srcStride;
    uint8_t* targetRow = destBits + destX * 3 + destRow * destStride;
    for (int row = 0; row < height; ++row) {
      const uint8_t* source = srcBits + srcRowOffset + srcX;
      uint8_t* target = targetRow;
      for (int column = 0; column < width; ++column) {
        const uint8_t* entry = paletteEntry(this, *source);
        if (colorKey.DiffersFrom(*reinterpret_cast<const uint32_t*>(entry))) {
          target[0] = entry[0];
          target[1] = entry[1];
          target[2] = entry[2];
        }
        ++source;
        target += 3;
      }
      srcRowOffset += srcDirection * srcStride;
      targetRow += destDirection * destStride;
    }
  }
}

uint32_t DibBitmap::Blit24(DibBitmap* dest, int destX, int destY, int srcX, int srcY, int width, int height)
{
  uint8_t* destBits = dest->m_bits;
  const int destWidth = dest->Width();
  const int destSignedHeight = dest->Height();
  const int destStride = dest->RowStride();
  const uint8_t* srcBits = m_bits;
  const int srcWidth = Width();
  const int srcStride = RowStride();
  const int bitCount = BitCount();
  const int destDirection = RowDirection(destSignedHeight);
  const int destHeight = AbsoluteHeight(destSignedHeight);
  const int srcSignedHeight = Height();
  const int srcDirection = RowDirection(srcSignedHeight);
  const int srcHeight = AbsoluteHeight(srcSignedHeight);

  if (srcY > srcHeight) {
    return 0;
  }
  if (width == 0) {
    if (destX < 0) {
      destX = 0;
    }
    width = destWidth - destX;
  }
  if (height == 0) {
    if (destY < 0) {
      destY = 0;
    }
    height = destHeight - destY;
  }
  if (destX < 0) {
    srcX -= destX;
    width += destX;
    destX = 0;
  }
  if (destY < 0) {
    srcY -= destY;
    height += destY;
    destY = 0;
  }
  if (srcX < 0) {
    width += srcX;
    destX -= srcX;
    if (width < 1) {
      return 0;
    }
    srcX = 0;
  }
  if (srcY < 0) {
    height += srcY;
    destY -= srcY;
    if (height < 1) {
      return 0;
    }
    srcY = 0;
  }
  if (width <= 0 || height <= 0 || srcX >= srcWidth || srcY >= srcHeight || destX >= destWidth || destY >= destHeight) {
    return 0;
  }
  if (srcHeight - srcY < height) {
    height = srcHeight - srcY;
  }
  if (srcWidth - srcX < width) {
    width = srcWidth - srcX;
  }
  if (destHeight - destY < height) {
    height = destHeight - destY;
  }
  if (destWidth - destX < width) {
    width = destWidth - destX;
  }
  if (width <= 0 || height <= 0) {
    return 0;
  }
  const int destRow = dest->RowIndex(destY);
  const int srcRow = RowIndex(srcY);
  if (bitCount == kBitsPerPixelRgb) {
    uint8_t* target = destBits + destX * 3 + destRow * destStride;
    const uint8_t* source = srcBits + srcX * 3 + srcRow * srcStride;
    for (int row = 0; row < height; ++row) {
      std::memcpy(target, source, static_cast<size_t>(width) * 3);
      target += destStride * destDirection;
      source += srcStride * srcDirection;
    }
  } else {
    const uint8_t* source = srcBits + srcRow * srcStride + srcX;
    uint8_t* target = destBits + destX * 3 + destRow * destStride;
    for (int row = 0; row < height; ++row) {
      const uint8_t* sourcePixel = source;
      uint8_t* targetPixel = target;
      for (int column = 0; column < width; ++column) {
        const uint8_t* entry = paletteEntry(this, *sourcePixel++);
        targetPixel[0] = entry[0];
        targetPixel[1] = entry[1];
        targetPixel[2] = entry[2];
        targetPixel += 3;
      }
      source += srcStride * srcDirection;
      target += destStride * destDirection;
    }
  }
  return height > 0;
}
