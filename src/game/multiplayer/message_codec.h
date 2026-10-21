#pragma once

#include <cstdint>
#include <vector>

class MessageBase;

class MessageCodec
{
  public:
  MessageCodec();
  virtual ~MessageCodec();
  MessageCodec(const MessageCodec&) = delete;
  MessageCodec& operator=(const MessageCodec&) = delete;

  int Deserialize(MessageBase** out);
  int Serialize(MessageBase* message);
  void SetData(const void* data, uint32_t size);

  int m_valid = 1;
  uint32_t m_size = 0;
  char* m_buffer = nullptr;

  private:
  void RefreshView();

  std::vector<uint8_t> m_bytes;
};
