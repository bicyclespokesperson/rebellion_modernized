#include "core/memory_stream.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>

namespace reb::io
{

class MemoryWriter::Buffer final : public streambuf
{
  public:
  explicit Buffer(size_t maxBytes)
      : m_maxBytes(std::min(maxBytes, static_cast<size_t>(std::numeric_limits<int>::max())))
  {
  }

  std::span<const uint8_t> Bytes() const { return m_bytes; }

  std::vector<uint8_t> TakeBytes()
  {
    m_position = 0;
    return std::exchange(m_bytes, {});
  }

  int Write(const char* text, int count) override
  {
    if (count <= 0) return 0;
    const size_t length = static_cast<size_t>(count);
    if (length > m_maxBytes - m_position) return 0;
    const size_t end = m_position + length;
    if (end > m_bytes.size()) m_bytes.resize(end);
    std::memcpy(m_bytes.data() + m_position, text, length);
    m_position = end;
    return count;
  }

  int Overflow(int value) override
  {
    if (value == -1) return 0;
    const char character = static_cast<char>(value);
    return Write(&character, 1) == 1 ? static_cast<unsigned char>(character) : -1;
  }

  int SeekPosition(long position, int mode) override
  {
    return SeekOffset(position, iostream_flags::kSeekBeg, mode);
  }

  int SeekOffset(long offset, int direction, int mode) override
  {
    if ((mode & iostream_flags::kOut) == 0) return -1;
    size_t origin;
    switch (direction) {
      case iostream_flags::kSeekBeg: origin = 0; break;
      case iostream_flags::kSeekCur: origin = m_position; break;
      case iostream_flags::kSeekEnd: origin = m_bytes.size(); break;
      default: return -1;
    }
    if (offset < 0) {
      const auto magnitude = static_cast<unsigned long>(-(offset + 1)) + 1UL;
      if (magnitude > origin) return -1;
      m_position = origin - magnitude;
    } else {
      const auto forward = static_cast<unsigned long>(offset);
      if (forward > m_maxBytes - origin) return -1;
      m_position = origin + forward;
    }
    return static_cast<int>(m_position);
  }

  int Sync() override { return 0; }

  private:
  std::vector<uint8_t> m_bytes;
  size_t m_position = 0;
  size_t m_maxBytes;
};

class MemoryReader::Buffer final : public streambuf
{
  public:
  explicit Buffer(std::span<const uint8_t> bytes)
      : m_bytes(bytes)
  {
    if (Valid()) SetPosition(0);
    else SetEndSentinel();
  }

  bool Valid() const { return m_bytes.size() <= static_cast<size_t>(std::numeric_limits<int>::max()); }
  size_t Remaining() const { return m_bytes.size() - Position(); }

  int Read(char* destination, int count) override
  {
    if (count <= 0) return 0;
    const size_t available = Remaining();
    const size_t length = std::min(static_cast<size_t>(count), available);
    if (length == 0) return 0;
    std::memcpy(destination, m_gptr, length);
    m_gptr += length;
    return static_cast<int>(length);
  }

  int Underflow() override
  {
    if (!m_atEndSentinel && m_gptr < m_egptr) return static_cast<unsigned char>(*m_gptr);
    // Legacy sbumpc increments gptr even after Underflow reports EOF. Keep that increment in our own storage.
    SetEndSentinel();
    return -1;
  }

  int Putback(int value) override
  {
    if (m_atEndSentinel || m_gptr == m_eback) return -1;
    if (value != -1 && static_cast<unsigned char>(m_gptr[-1]) != static_cast<unsigned char>(value)) return -1;
    --m_gptr;
    return static_cast<unsigned char>(*m_gptr);
  }

  int SeekPosition(long position, int mode) override
  {
    return SeekOffset(position, iostream_flags::kSeekBeg, mode);
  }

  int SeekOffset(long offset, int direction, int mode) override
  {
    if (!Valid() || (mode & iostream_flags::kIn) == 0) return -1;
    size_t origin;
    switch (direction) {
      case iostream_flags::kSeekBeg: origin = 0; break;
      case iostream_flags::kSeekCur: origin = Position(); break;
      case iostream_flags::kSeekEnd: origin = m_bytes.size(); break;
      default: return -1;
    }
    size_t position;
    if (offset < 0) {
      const auto magnitude = static_cast<unsigned long>(-(offset + 1)) + 1UL;
      if (magnitude > origin) return -1;
      position = origin - magnitude;
    } else {
      const auto forward = static_cast<unsigned long>(offset);
      if (forward > m_bytes.size() - origin) return -1;
      position = origin + forward;
    }
    SetPosition(position);
    return static_cast<int>(position);
  }

  int Sync() override { return 0; }

  private:
  size_t Position() const
  {
    if (m_atEndSentinel) return m_bytes.size();
    return static_cast<size_t>(m_gptr - m_eback);
  }

  void SetPosition(size_t position)
  {
    if (m_bytes.empty()) {
      SetEndSentinel();
      return;
    }
    m_atEndSentinel = false;
    auto* first = reinterpret_cast<char*>(const_cast<uint8_t*>(m_bytes.data()));
    m_eback = first;
    m_gptr = first + position;
    m_egptr = first + m_bytes.size();
  }

  void SetEndSentinel()
  {
    m_atEndSentinel = true;
    m_eback = m_endSentinel;
    m_gptr = m_endSentinel;
    m_egptr = m_endSentinel;
  }

  std::span<const uint8_t> m_bytes;
  char m_endSentinel[2]{};
  bool m_atEndSentinel = false;
};

MemoryWriter::MemoryWriter(size_t maxBytes)
    : m_buffer(std::make_unique<Buffer>(maxBytes))
{
  ostream_ctor(m_stream.Out(), m_buffer.get(), 0);
  m_stream.Ios()->m_delbuf = 0;
}

MemoryWriter::~MemoryWriter() = default;

bool MemoryWriter::Good() const { return m_stream.Ios()->m_state == 0; }
std::span<const uint8_t> MemoryWriter::Bytes() const { return m_buffer->Bytes(); }
std::vector<uint8_t> MemoryWriter::TakeBytes() { return m_buffer->TakeBytes(); }

MemoryReader::MemoryReader(std::span<const uint8_t> bytes)
    : m_buffer(std::make_unique<Buffer>(bytes))
{
  istream_ctor(m_stream.In(), m_buffer.get(), 0);
  m_stream.Ios()->m_delbuf = 0;
  if (!m_buffer->Valid()) m_stream.Ios()->m_state |= iostream_flags::kFail;
}

MemoryReader::~MemoryReader() = default;

bool MemoryReader::Good() const { return m_stream.Ios()->m_state == 0; }
bool MemoryReader::AtEnd() const { return m_buffer->Remaining() == 0; }
size_t MemoryReader::Remaining() const { return m_buffer->Remaining(); }

} // namespace reb::io
