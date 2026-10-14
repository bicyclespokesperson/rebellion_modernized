#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <vector>

#include "gfx/dib_bitmap.h"
#include "gfx/indexed_sprite_filter.h"
#include "port/minwin_impl/gdi_api.h"

namespace minwin_gdi
{
int DeviceScale() { return 4; }
uint64_t DeviceBitmapRevision(HBITMAP) { return 0; }
bool CopyDeviceBitmapPixels(HBITMAP, DeviceBitmapPixels&) { return false; }
uint64_t SetDeviceBitmapPixels(HBITMAP, int, std::span<const uint8_t>, int, int, int, int) { return 0; }
void InvalidateDeviceBitmap(HBITMAP) {}
}

DibBitmap::DibBitmap(int width, int height, DibBitmap*)
{
  m_storage.resize(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD) + static_cast<size_t>(width) * height);
  m_info = reinterpret_cast<BITMAPINFOHEADER*>(m_storage.data());
  std::memset(m_info, 0, sizeof(BITMAPINFOHEADER));
  m_info->biSize = sizeof(BITMAPINFOHEADER);
  m_info->biWidth = width;
  m_info->biHeight = -height;
  m_info->biBitCount = 8;
  m_bits = m_storage.data() + sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD);
  m_colorCount = 256;
  m_dibSection = nullptr;
  m_remapTable = nullptr;
}
DibBitmap::~DibBitmap() = default;
int DibBitmap::IsLoaded() const { return m_info != nullptr; }
int DibBitmap::Width() const { return m_info->biWidth; }
int DibBitmap::Height() const { return m_info->biHeight; }
uint16_t DibBitmap::BitCount() const { return m_info->biBitCount; }
uint32_t DibBitmap::Compression() const { return m_info->biCompression; }
uint32_t DibBitmap::HeaderSize() const { return m_info->biSize; }
int DibBitmap::RowStride() const { return Width(); }
int DibBitmap::RowIndex(int y) const { return y; }
HBITMAP DibBitmap::DibSectionHandle() const { return nullptr; }
void DibBitmap::CacheTransparentColor() {}
void DibBitmap::NotifyPixelsChanged() { m_hdPixels.clear(); m_hdScale = 1; }
uint32_t DibBitmap::Blit24(DibBitmap*, int, int, int, int, int, int) { std::abort(); }
void DibBitmap::BlitKeyed24(DibBitmap*, int, int, uint32_t, int, int, int, int) { std::abort(); }
void DibBitmap::BlitKeyedRgb24(DibBitmap*, int, int, Rgb24, int, int, int, int) { std::abort(); }

namespace
{
void Require(bool condition)
{
  if (!condition)
    std::abort();
}

std::vector<uint8_t> Detail(int width, int height, uint8_t value)
{
  return std::vector<uint8_t>(static_cast<size_t>(width) * height * 4, value);
}

void VerifySameIndexCopyAndKeyedComposition()
{
  DibBitmap source(4, 2, nullptr);
  DibBitmap destination(4, 2, nullptr);
  std::memset(source.m_bits, 7, 8);
  std::memset(destination.m_bits, 7, 8);
  auto detail = Detail(4, 2, 7);
  detail[0] = 1; detail[1] = 2; detail[8] = 3; detail[9] = 4;
  Require(source.SetHighResolutionPixels(2, detail));
  Require(source.Blit(&destination, 0, 0, 0, 0, 1, 1));
  Require(destination.m_hdPixels[0] == 1 && destination.m_hdPixels[1] == 2);
  Require(destination.m_hdPixels[8] == 3 && destination.m_hdPixels[9] == 4);

  source.m_bits[0] = 0;
  destination.m_hdPixels[0] = 19;
  Require(source.BlitKeyed(&destination, 0, 0, 0, 0, 0, 1, 1));
  Require(destination.m_hdPixels[0] == 19);
  source.m_bits[0] = 7;
  Require(source.BlitKeyed(&destination, 0, 0, 0, 0, 0, 1, 1));
  Require(destination.m_hdPixels[0] == 1);

  Require(destination.FillRect(1, 0, 1, 1, 5));
  Require(destination.m_hdPixels[0] == 1 && destination.m_hdPixels[2] == 5 && destination.m_hdPixels[10] == 5);

  detail[2] = 12;
  detail[3] = 13;
  Require(source.SetHighResolutionPixels(2, detail));
  Require(source.Blit(&destination, -1, 1, 0, 0, 2, 1));
  Require(destination.m_hdPixels[4 * 4] == 12 && destination.m_hdPixels[4 * 4 + 1] == 13);

  DibBitmap legacy(4, 2, nullptr);
  std::memset(legacy.m_bits, 7, 8);
  Require(legacy.Blit(&destination, 0, 0, 0, 0, 1, 1));
  Require(destination.m_hdPixels[0] == 7 && destination.m_hdPixels[1] == 7 &&
          destination.m_hdPixels[8] == 7 && destination.m_hdPixels[9] == 7);
}

void VerifyRemapsUseDevicePixels()
{
  DibBitmap source(4, 2, nullptr);
  DibBitmap destination(4, 2, nullptr);
  source.m_bits[0] = 7;
  auto detail = Detail(4, 2, 7);
  detail[0] = 1; detail[1] = 2; detail[8] = 3; detail[9] = 4;
  Require(source.SetHighResolutionPixels(2, detail));
  std::array<uint8_t, 512> remap{};
  for (int i = 0; i < 256; ++i)
  {
    remap[i] = static_cast<uint8_t>(i + 10);
    remap[256 + i] = static_cast<uint8_t>(i + 20);
  }
  source.m_remapTable = remap.data();
  Require(source.BlitRemapped(&destination, 0, 0, 0, 0, 1, 1));
  Require(destination.m_hdPixels[0] == 11 && destination.m_hdPixels[1] == 12);

  destination.m_hdPixels[0] = 1;
  destination.m_hdPixels[1] = 2;
  Require(source.BlitRemapDestination(&destination, 0, 0, 0, 0, 1, 1));
  Require(destination.m_hdPixels[0] == 11 && destination.m_hdPixels[1] == 12);

  source.m_bits[0] = 0;
  destination.m_hdPixels[0] = 1;
  Require(source.BlitRemappedOverlay(&destination, 0, 0, 0, 0, 1, 1));
  Require(destination.m_hdPixels[0] == 21);
}

void VerifyThreefoldDetail()
{
  DibBitmap bitmap(4, 2, nullptr);
  std::vector<uint8_t> pixels(4 * 2 * 9, 6);
  pixels[0] = 3;
  Require(bitmap.SetHighResolutionPixels(3, pixels));
  Require(bitmap.FillRect(1, 0, 1, 1, 4));
  Require(bitmap.m_hdPixels[0] == 3);
  Require(bitmap.m_hdPixels[3] == 4);
  Require(bitmap.m_hdPixels[4 * 3 + 3] == 4);
}

void VerifyIndexedSpriteFiltering()
{
  DibBitmap sprite(3, 1, nullptr);
  sprite.m_colorKey = 0;
  sprite.m_bits[0] = 0;
  sprite.m_bits[1] = 1;
  sprite.m_bits[2] = 3;
  auto* colors = reinterpret_cast<uint8_t*>(sprite.m_info) + sprite.m_info->biSize;
  colors[1 * 4 + 0] = 16;
  colors[1 * 4 + 1] = 16;
  colors[1 * 4 + 2] = 16;
  colors[2 * 4 + 0] = 128;
  colors[2 * 4 + 1] = 128;
  colors[2 * 4 + 2] = 128;
  colors[3 * 4 + 0] = 240;
  colors[3 * 4 + 1] = 240;
  colors[3 * 4 + 2] = 240;
  colors[4 * 4 + 0] = 16;
  colors[4 * 4 + 1] = 16;
  colors[4 * 4 + 2] = 16;
  IndexedSpriteFilter filter;
  Require(filter.EnsureDetail(sprite));
  Require(sprite.m_hdScale == 4);
  for (int y = 0; y < 4; ++y)
  {
    for (int x = 0; x < 4; ++x)
      Require(sprite.m_hdPixels[y * 12 + x] == 0);
    for (int x = 4; x < 12; ++x)
      Require(sprite.m_hdPixels[y * 12 + x] != 0);
    Require(sprite.m_hdPixels[y * 12 + 4] == 1);
    Require(sprite.m_hdPixels[y * 12 + 7] == 2);
  }
  const auto original = sprite.m_hdPixels;
  Require(filter.EnsureDetail(sprite) && sprite.m_hdPixels == original);
  sprite.m_bits[2] = 4;
  sprite.NotifyPixelsChanged();
  Require(filter.EnsureDetail(sprite));
  Require(sprite.m_hdPixels[11] == 4);
  Require(sprite.m_bits[0] == 0 && sprite.m_bits[1] == 1 && sprite.m_bits[2] == 4);

  DibBitmap destination(3, 1, nullptr);
  Require(sprite.BlitKeyed(&destination, 0, 0, 0, 0, 0, 3, 1));
  Require(destination.m_hdPixels[0] == 0 && destination.m_hdPixels[4] == 1);
  Require(destination.m_hdPixels[11] == 4);

  DibBitmap uniform(2, 1, nullptr);
  uniform.m_colorKey = 0;
  uniform.m_bits[0] = 5;
  uniform.m_bits[1] = 5;
  auto* uniformColors = reinterpret_cast<uint8_t*>(uniform.m_info) + uniform.m_info->biSize;
  uniformColors[5 * 4] = 7;
  uniformColors[5 * 4 + 1] = 7;
  uniformColors[5 * 4 + 2] = 7;
  uniformColors[6 * 4] = 8;
  uniformColors[6 * 4 + 1] = 8;
  uniformColors[6 * 4 + 2] = 8;
  Require(filter.EnsureDetail(uniform));
  for (uint8_t pixel : uniform.m_hdPixels)
    Require(pixel == 5);

  const std::vector<uint8_t> authored(2 * 1 * 16, 6);
  Require(uniform.SetHighResolutionPixels(4, authored));
  Require(filter.EnsureDetail(uniform) && uniform.m_hdPixels == authored);
}

void MeasureIndexedSpriteFiltering()
{
  DibBitmap sprite(106, 133, nullptr);
  sprite.m_colorKey = 0;
  auto* colors = reinterpret_cast<uint8_t*>(sprite.m_info) + sprite.m_info->biSize;
  for (int i = 0; i < 256; ++i)
  {
    colors[i * 4] = static_cast<uint8_t>(i);
    colors[i * 4 + 1] = static_cast<uint8_t>(i);
    colors[i * 4 + 2] = static_cast<uint8_t>(i);
  }
  for (int i = 0; i < 106 * 133; ++i)
    sprite.m_bits[i] = static_cast<uint8_t>(1 + i % 255);
  IndexedSpriteFilter filter;
  const auto start = std::chrono::steady_clock::now();
  Require(filter.EnsureDetail(sprite));
  const auto first = std::chrono::steady_clock::now();
  Require(filter.EnsureDetail(sprite));
  const auto cached = std::chrono::steady_clock::now();
  sprite.NotifyPixelsChanged();
  Require(filter.EnsureDetail(sprite));
  const auto nextFrame = std::chrono::steady_clock::now();
  std::printf("indexed_sprite_filter initial=%lldms cached=%lldus next_frame=%lldms\n",
              static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(first - start).count()),
              static_cast<long long>(std::chrono::duration_cast<std::chrono::microseconds>(cached - first).count()),
              static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(nextFrame - cached).count()));
}
} // namespace

int main()
{
  VerifyIndexedSpriteFiltering();
  MeasureIndexedSpriteFiltering();
  VerifySameIndexCopyAndKeyedComposition();
  VerifyRemapsUseDevicePixels();
  VerifyThreefoldDetail();
}
