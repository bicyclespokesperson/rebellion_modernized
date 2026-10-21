#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "iostream/iostream.h"

namespace reb::io
{

class MemoryWriter
{
  public:
  static constexpr size_t kDefaultMaxBytes = 64U * 1024U * 1024U;

  explicit MemoryWriter(size_t maxBytes = kDefaultMaxBytes);
  ~MemoryWriter();
  MemoryWriter(const MemoryWriter&) = delete;
  MemoryWriter& operator=(const MemoryWriter&) = delete;

  ostream* Out() { return m_stream.Out(); }
  bool Good() const;
  std::span<const uint8_t> Bytes() const;
  std::vector<uint8_t> TakeBytes();

  private:
  class Buffer;
  std::unique_ptr<Buffer> m_buffer;
  ostrstream m_stream;
};

class MemoryReader
{
  public:
  explicit MemoryReader(std::span<const uint8_t> bytes);
  ~MemoryReader();
  MemoryReader(const MemoryReader&) = delete;
  MemoryReader& operator=(const MemoryReader&) = delete;

  istream* In() { return m_stream.In(); }
  bool Good() const;
  bool AtEnd() const;
  size_t Remaining() const;

  private:
  class Buffer;
  std::unique_ptr<Buffer> m_buffer;
  istrstream m_stream;
};

} // namespace reb::io
