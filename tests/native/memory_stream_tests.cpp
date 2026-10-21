#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "core/game_string.h"
#include "core/iostream_lib.h"
#include "core/memory_stream.h"
#include "core/stream_io.h"

namespace
{

void Require(bool condition)
{
  if (!condition) std::abort();
}

void WriteLegacySequence(ostream* out)
{
  WritePositionMarker(out);
  const uint8_t byte = 0xe7;
  const uint16_t word = 0x3412;
  const uint32_t dword = 0x89abcdef;
  StreamWrite8(out, &byte);
  StreamWrite16(out, &word);
  StreamWrite32(out, &dword);
  ostream_operator_ls_char(out, 'Z');
  Require(WriteLegacyString(out, "old save string"));
  WritePositionMarker(out);
}

void TestLegacyBytes()
{
  ScopedStrstream legacy;
  WriteLegacySequence(legacy.get()->Out());
  Require(iostream_access::State(legacy.get()->Out()) == 0);
  const auto length = static_cast<size_t>(ostream_tellp(legacy.get()->Out()));
  const auto* source = reinterpret_cast<const uint8_t*>(legacy.get()->Ios()->m_bp->m_base);

  reb::io::MemoryWriter writer;
  WriteLegacySequence(writer.Out());
  Require(writer.Good());
  Require(writer.Bytes().size() == length);
  Require(std::memcmp(writer.Bytes().data(), source, length) == 0);

  reb::io::MemoryReader reader(writer.Bytes());
  Require(CheckPositionMarker(reader.In()));
  uint8_t byte = 0;
  uint16_t word = 0;
  uint32_t dword = 0;
  StreamRead8(reader.In(), &byte);
  StreamRead16(reader.In(), &word);
  StreamRead32(reader.In(), &dword);
  Require(byte == 0xe7 && word == 0x3412 && dword == 0x89abcdef);
  Require(istream_get(reader.In()) == 'Z');
  std::string text;
  Require(ReadLegacyString(reader.In(), text) && text == "old save string");
  Require(CheckPositionMarker(reader.In()));
  Require(reader.Good() && reader.AtEnd() && reader.Remaining() == 0);
}

void TestBackpatchAndBounds()
{
  reb::io::MemoryWriter writer(512 * 1024);
  const uint32_t placeholder = 0;
  StreamWrite32(writer.Out(), &placeholder);
  std::vector<uint8_t> payload(256 * 1024);
  for (size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<uint8_t>(i * 37);
  ostream_write(writer.Out(), payload.data(), static_cast<int>(payload.size()));
  Require(writer.Good() && writer.Bytes().size() == payload.size() + sizeof(placeholder));
  ostream_seekp_pos(writer.Out(), 0);
  const uint32_t payloadLength = static_cast<uint32_t>(payload.size());
  StreamWrite32(writer.Out(), &payloadLength);
  Require(writer.Good() && writer.Bytes().size() == payload.size() + sizeof(placeholder));
  Require(std::equal(payload.begin(), payload.end(), writer.Bytes().begin() + 4));

  const size_t oldSize = writer.Bytes().size();
  ostream_seekp_pos(writer.Out(), static_cast<long>(oldSize + 8));
  ostream_operator_ls_char(writer.Out(), 'X');
  Require(writer.Good() && writer.Bytes().size() == oldSize + 9);
  Require(std::all_of(writer.Bytes().begin() + oldSize, writer.Bytes().end() - 1,
                      [](uint8_t value) { return value == 0; }));
  Require(writer.Bytes().back() == 'X');

  auto bytes = writer.TakeBytes();
  Require(bytes.size() == oldSize + 9 && writer.Bytes().empty());
  reb::io::MemoryReader reader(bytes);
  uint32_t restoredLength = 0;
  StreamRead32(reader.In(), &restoredLength);
  Require(reader.Good() && restoredLength == payload.size());
  Require(reader.Remaining() == payload.size() + 9);
  istream_seekg_off(reader.In(), -1, iostream_flags::kSeekEnd);
  Require(reader.Good() && istream_get(reader.In()) == 'X' && reader.AtEnd());

  reb::io::MemoryWriter bounded(4);
  StreamWrite32(bounded.Out(), &payloadLength);
  ostream_operator_ls_char(bounded.Out(), 'X');
  Require(!bounded.Good() && bounded.Bytes().size() == 4);
  reb::io::MemoryWriter negativeSeek;
  ostream_seekp_off(negativeSeek.Out(), -1, iostream_flags::kSeekBeg);
  Require(!negativeSeek.Good());
}

void TestMalformedInput()
{
  const std::array<uint8_t, 3> truncated{0x12, 0x34, 0x56};
  reb::io::MemoryReader reader(truncated);
  uint32_t value = 0;
  StreamRead32(reader.In(), &value);
  Require(!reader.Good() && reader.AtEnd() && reader.Remaining() == 0);

  reb::io::MemoryReader invalidSeek(truncated);
  istream_seekg_pos(invalidSeek.In(), -1);
  Require(!invalidSeek.Good() && invalidSeek.Remaining() == truncated.size());

  reb::io::MemoryReader end(truncated);
  istream_seekg_pos(end.In(), static_cast<long>(truncated.size()));
  Require(end.Good() && end.AtEnd());
  Require(istream_get(end.In()) == -1);
  Require(!end.Good());
  // The old stream helper calls Underflow again at EOF; repeated calls stay within the sentinel.
  end.In()->State()->m_state = 0;
  Require(istream_get(end.In()) == -1);
}

} // namespace

int main()
{
  TestLegacyBytes();
  TestBackpatchAndBounds();
  TestMalformedInput();
}
