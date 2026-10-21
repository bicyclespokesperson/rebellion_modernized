#include "multiplayer/message_codec.h"

#include <limits>
#include <memory>
#include <span>
#include <utility>

#include "core/memory_stream.h"
#include "core/stream_io.h"
#include "orders/order_base.h"

namespace
{
constexpr size_t kMaxMessageBytes = 64U * 1024U * 1024U;
}

MessageCodec::MessageCodec() = default;
MessageCodec::~MessageCodec() = default;

void MessageCodec::RefreshView()
{
  m_size = static_cast<uint32_t>(m_bytes.size());
  m_buffer = m_bytes.empty() ? nullptr : reinterpret_cast<char*>(m_bytes.data());
}

int MessageCodec::Serialize(MessageBase* message)
{
  m_bytes.clear();
  RefreshView();
  m_valid = 0;
  if (!message) return 0;

  reb::io::MemoryWriter writer(kMaxMessageBytes);
  const int kind = message->GetKind();
  StreamWrite32(writer.Out(), &kind);
  const bool written = message->Write(writer.Out()) != 0;
  if (!written || !writer.Good() || writer.Bytes().size() < sizeof(kind)
      || writer.Bytes().size() > std::numeric_limits<uint32_t>::max()) return 0;

  m_bytes = writer.TakeBytes();
  RefreshView();
  m_valid = 1;
  return 1;
}

void MessageCodec::SetData(const void* data, uint32_t size)
{
  if (!data || size < sizeof(int) || size > kMaxMessageBytes) {
    m_bytes.clear();
    RefreshView();
    m_valid = 0;
    return;
  }
  const auto* first = static_cast<const uint8_t*>(data);
  std::vector<uint8_t> replacement(first, first + size);
  m_bytes = std::move(replacement);
  RefreshView();
  m_valid = 1;
}

int MessageCodec::Deserialize(MessageBase** out)
{
  if (!out) return 0;
  *out = nullptr;
  if (!m_valid || m_bytes.size() < sizeof(int)) return 0;

  reb::io::MemoryReader reader{std::span<const uint8_t>(m_bytes)};
  int kind = 0;
  StreamRead32(reader.In(), &kind);
  if (!reader.Good()) return 0;

  std::unique_ptr<MessageBase> message(static_cast<MessageBase*>(CreateOrderByKind(kind)));
  if (!message || !message->Read(reader.In()) || !reader.Good() || !reader.AtEnd()) return 0;
  *out = message.release();
  return 1;
}
