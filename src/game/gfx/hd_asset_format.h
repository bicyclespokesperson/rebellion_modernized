#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace reb::hd
{
constexpr uint64_t kMaximumPixelBytes = 64u * 1024u * 1024u;

struct IndexedAsset {
  int scale = 0;
  bool keyedByDetailPixel = false;
  std::vector<uint8_t> pixels;
};

uint64_t SourceFingerprint(std::span<const uint8_t> source);
bool DecodeAsset(std::span<const uint8_t> bytes, uint64_t fingerprint, int width, int height, int colorCount,
                 IndexedAsset& result);
} // namespace reb::hd
