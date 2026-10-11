#include "multiplayer/message_codec.h"

#include <cstring>
#include <new>

#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"
#include "orders/order_base.h"

namespace
{

constexpr int kInitialCapacity = 10;
constexpr int kOutputMode = 2;

uint32_t BytesWritten(OStrStream* stream)
{
  return iostream_access::WrittenBytes(StreambufOf(stream->Out()));
}

} // namespace

MessageCodec::MessageCodec()
: m_valid(1),
  m_input(nullptr),
  m_output(nullptr),
  m_capacity(0),
  m_size(0),
  m_buffer(nullptr)
{
  m_valid = Reserve(kInitialCapacity, 0);
}

MessageCodec::~MessageCodec()
{
  Release();
}

int MessageCodec::Deserialize(MessageBase** out)
{
  *out = nullptr;
  int found = m_valid;
  if (found)
  {
    ClearIosState(m_input->In());
    istream_seekg_pos(m_input->In(), 0);
    int kind;
    StreamRead32(m_input->In(), &kind);
    *out = static_cast<MessageBase*>(CreateOrderByKind(kind));
    found = *out != nullptr;
    if (found)
    {
      (*out)->Read(m_input->In());
      return found;
    }
  }
  if (*out)
  {
    delete *out;
    *out = nullptr;
  }
  return found;
}

int MessageCodec::Serialize(MessageBase* message)
{
  int ok = message != nullptr;
  if (ok)
    ok = m_valid;
  while (ok)
  {
    ClearIosState(m_output->Out());
    ostream_seekp_pos(m_output->Out(), 0);
    int kind = message->GetKind();
    StreamWrite32(m_output->Out(), &kind);
    message->Write(m_output->Out());
    m_size = BytesWritten(m_output);
    ok = m_size != 0;
    if (!ok)
      break;
    const bool filled = m_size == static_cast<uint32_t>(m_capacity);
    if (filled)
      ok = Reserve(m_capacity * 2, 0);
    if (!ok || !filled)
      break;
  }
  return ok;
}

void MessageCodec::SetData(const void* data, uint32_t size)
{
  int ok = data != nullptr;
  if (ok)
    ok = m_valid;
  if (ok)
    ok = Reserve(size, 0);
  if (ok)
  {
    memcpy(m_buffer, data, size);
    m_size = size;
  }
}

int MessageCodec::Release()
{
  if (m_input)
  {
    delete m_input;
    m_input = nullptr;
  }
  if (m_output)
  {
    delete m_output;
    m_output = nullptr;
  }
  if (m_buffer)
  {
    ::operator delete(m_buffer);
    m_buffer = nullptr;
  }
  return 1;
}

int MessageCodec::Reserve(int capacity, int keepContents)
{
  int ok = 1;
  if (capacity <= m_capacity)
    return ok;
  char* replacement = static_cast<char*>(::operator new(capacity));
  bool failed = replacement == nullptr;
  ok = !failed;
  if (ok)
  {
    if (keepContents)
    {
      if (m_size != 0)
        memcpy(replacement, m_buffer, m_size);
    }
    else
    {
      m_size = 0;
    }
    ok = Release();
    if (ok)
    {
      m_capacity = capacity;
      m_buffer = replacement;
      replacement = nullptr;
    }
  }
  if (ok)
  {
    m_input = new (std::nothrow) IStrStream();
    if (m_input)
      istrstream_ctor_str_len(m_input, m_buffer, m_capacity, 1);
    failed = m_input == nullptr;
    ok = !failed;
    if (ok)
    {
      m_output = new (std::nothrow) OStrStream();
      if (m_output)
        ostrstream_ctor_buf(m_output, m_buffer, m_capacity, kOutputMode, 1);
      failed = m_output == nullptr;
      ok = !failed;
    }
  }
  if (replacement)
    ::operator delete(replacement);
  m_valid = ok;
  return ok;
}
