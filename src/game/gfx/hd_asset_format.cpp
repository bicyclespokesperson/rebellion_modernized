#include "gfx/hd_asset_format.h"

#include <algorithm>
#include <array>

namespace reb::hd
{
namespace
{
constexpr std::array<uint8_t, 8> kMagicV1 = {'R', 'E', 'B', 'H', 'D', '0', '1', 0};
constexpr std::array<uint8_t, 8> kMagicV2 = {'R', 'E', 'B', 'H', 'D', '0', '2', 0};
constexpr size_t kHeaderSize = 32;

uint64_t ReadLittleEndian(std::span<const uint8_t> bytes)
{
  uint64_t value = 0;
  for (size_t i = 0; i < bytes.size(); ++i) {
    value |= static_cast<uint64_t>(bytes[i]) << (8 * i);
  }
  return value;
}
} // namespace

uint64_t SourceFingerprint(std::span<const uint8_t> source)
{
  uint64_t value = 14695981039346656037ull;
  for (uint8_t byte : source) {
    value ^= byte;
    value *= 1099511628211ull;
  }
  return value;
}

bool DecodeAsset(std::span<const uint8_t> bytes, uint64_t fingerprint, int width, int height, int colorCount,
                 IndexedAsset& result)
{
  result = {};
  if (width <= 0 || height <= 0 || colorCount < 1 || colorCount > 256 || bytes.size() < kHeaderSize
      || (!std::equal(kMagicV1.begin(), kMagicV1.end(), bytes.begin())
          && !std::equal(kMagicV2.begin(), kMagicV2.end(), bytes.begin()))) {
    return false;
  }
  const auto sourceWidth = ReadLittleEndian(bytes.subspan(8, 4));
  const auto sourceHeight = ReadLittleEndian(bytes.subspan(12, 4));
  const auto scale = ReadLittleEndian(bytes.subspan(16, 4));
  const auto byteCount = ReadLittleEndian(bytes.subspan(20, 4));
  if (sourceWidth != static_cast<uint64_t>(width) || sourceHeight != static_cast<uint64_t>(height)
      || (scale != 2 && scale != 4) || ReadLittleEndian(bytes.subspan(24, 8)) != fingerprint) {
    return false;
  }
  if (sourceWidth > kMaximumPixelBytes / scale / scale / sourceHeight) {
    return false;
  }
  const auto expectedBytes = sourceWidth * sourceHeight * scale * scale;
  if (byteCount != expectedBytes || bytes.size() - kHeaderSize != expectedBytes) {
    return false;
  }
  const auto pixels = bytes.subspan(kHeaderSize);
  if (colorCount < 256
      && std::any_of(
          pixels.begin(), pixels.end(),
          [colorCount](uint8_t pixel) {
            return pixel >= colorCount;
          })) {
    return false;
  }
  result.scale = static_cast<int>(scale);
  result.keyedByDetailPixel = std::equal(kMagicV2.begin(), kMagicV2.end(), bytes.begin());
  result.pixels.assign(pixels.begin(), pixels.end());
  return true;
}
} // namespace reb::hd
