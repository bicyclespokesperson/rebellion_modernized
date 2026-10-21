#pragma once

#include "port/native/lan_transport.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace reb::native::lan
{

class Writer
{
public:
  void U32(uint32_t value)
  {
    if (!good_) return;
    bytes_.push_back(static_cast<uint8_t>(value));
    bytes_.push_back(static_cast<uint8_t>(value >> 8));
    bytes_.push_back(static_cast<uint8_t>(value >> 16));
    bytes_.push_back(static_cast<uint8_t>(value >> 24));
  }

  void I32(int32_t value) { U32(static_cast<uint32_t>(value)); }

  void U64(uint64_t value)
  {
    if (!good_) return;
    U32(static_cast<uint32_t>(value));
    U32(static_cast<uint32_t>(value >> 32));
  }

  void String(std::string_view value, size_t max = 1024)
  {
    if (!good_ || value.size() > max || value.size() > std::numeric_limits<uint32_t>::max()) {
      good_ = false;
      return;
    }
    U32(static_cast<uint32_t>(value.size()));
    bytes_.insert(bytes_.end(), value.begin(), value.end());
  }

  void Blob(std::span<const uint8_t> value, size_t max = kMaxMessageSize)
  {
    if (!good_ || value.size() > max || value.size() > std::numeric_limits<uint32_t>::max()) {
      good_ = false;
      return;
    }
    U32(static_cast<uint32_t>(value.size()));
    bytes_.insert(bytes_.end(), value.begin(), value.end());
  }

  bool Good() const { return good_; }
  const std::vector<uint8_t>& Bytes() const { return bytes_; }
  std::vector<uint8_t> TakeBytes() { return std::exchange(bytes_, {}); }

private:
  std::vector<uint8_t> bytes_;
  bool good_ = true;
};

class Reader
{
public:
  explicit Reader(std::span<const uint8_t> bytes) : bytes_(bytes) {}

  uint32_t U32()
  {
    if (!Reserve(4)) return 0;
    const uint32_t value = uint32_t(bytes_[offset_]) |
                           (uint32_t(bytes_[offset_ + 1]) << 8) |
                           (uint32_t(bytes_[offset_ + 2]) << 16) |
                           (uint32_t(bytes_[offset_ + 3]) << 24);
    offset_ += 4;
    return value;
  }

  int32_t I32() { return std::bit_cast<int32_t>(U32()); }

  uint64_t U64()
  {
    const uint64_t low = U32();
    const uint64_t high = U32();
    return Good() ? low | (high << 32) : 0;
  }

  std::string String(size_t max = 1024)
  {
    const uint32_t length = U32();
    if (!good_ || length > max || !Reserve(length)) {
      good_ = false;
      return {};
    }
    const char* data = reinterpret_cast<const char*>(bytes_.data() + offset_);
    offset_ += length;
    return std::string(data, length);
  }

  std::span<const uint8_t> Blob(size_t max = kMaxMessageSize)
  {
    const uint32_t length = U32();
    if (!good_ || length > max || !Reserve(length)) {
      good_ = false;
      return {};
    }
    const auto blob = bytes_.subspan(offset_, length);
    offset_ += length;
    return blob;
  }

  bool Good() const { return good_; }
  bool Done() const { return good_ && offset_ == bytes_.size(); }

private:
  bool Reserve(size_t length)
  {
    if (!good_ || length > bytes_.size() - offset_) {
      good_ = false;
      return false;
    }
    return true;
  }

  std::span<const uint8_t> bytes_;
  size_t offset_ = 0;
  bool good_ = true;
};

inline uint64_t Fingerprint(std::span<const uint8_t> bytes)
{
  uint64_t hash = 14695981039346656037ull;
  for (uint8_t byte : bytes) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

} // namespace reb::native::lan
