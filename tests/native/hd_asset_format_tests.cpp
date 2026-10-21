#include "gfx/hd_asset_format.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{
void Require(bool condition)
{
  if (!condition) {
    std::abort();
  }
}

void Write(std::vector<uint8_t>& bytes, size_t offset, uint64_t value, size_t count = 4)
{
  for (size_t i = 0; i < count; ++i) {
    bytes[offset + i] = static_cast<uint8_t>(value >> (i * 8));
  }
}
} // namespace

int main()
{
  const std::vector<uint8_t> original = {'h', 'e', 'l', 'l', 'o'};
  const auto fingerprint = reb::hd::SourceFingerprint(original);
  Require(fingerprint == 0xa430d84680aabd0bull);
  std::vector<uint8_t> valid = {'R', 'E', 'B', 'H', 'D', '0', '1', 0};
  valid.resize(32 + 2 * 3 * 16, 1);
  Write(valid, 8, 2);
  Write(valid, 12, 3);
  Write(valid, 16, 4);
  Write(valid, 20, 96);
  Write(valid, 24, fingerprint, 8);
  reb::hd::IndexedAsset result;
  Require(reb::hd::DecodeAsset(valid, fingerprint, 2, 3, 2, result));
  Require(result.scale == 4 && result.pixels.size() == 96 && !result.keyedByDetailPixel);
  valid[6] = '2';
  Require(reb::hd::DecodeAsset(valid, fingerprint, 2, 3, 2, result));
  Require(result.keyedByDetailPixel);
  valid[6] = '1';
  Require(!reb::hd::DecodeAsset(valid, fingerprint + 1, 2, 3, 2, result));
  Require(result.scale == 0 && result.pixels.empty());
  Require(!reb::hd::DecodeAsset(valid, fingerprint, 3, 2, 2, result));
  Require(!reb::hd::DecodeAsset(valid, fingerprint, 2, 3, 1, result));
  for (size_t length = 0; length < valid.size(); ++length) {
    Require(!reb::hd::DecodeAsset(std::span(valid).first(length), fingerprint, 2, 3, 2, result));
  }
  auto invalid = valid;
  invalid.push_back(0);
  Require(!reb::hd::DecodeAsset(invalid, fingerprint, 2, 3, 2, result));
  invalid = valid;
  Write(invalid, 16, 3);
  Require(!reb::hd::DecodeAsset(invalid, fingerprint, 2, 3, 2, result));
  invalid = valid;
  Write(invalid, 8, 0x7fffffff);
  Write(invalid, 12, 0x7fffffff);
  Require(!reb::hd::DecodeAsset(invalid, fingerprint, 0x7fffffff, 0x7fffffff, 2, result));
  std::puts("HD asset format validation passed");
}
