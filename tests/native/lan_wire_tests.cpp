#include "port/native/lan_wire.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

void Require(bool condition, const char* message)
{
  if (!condition) {
    std::fprintf(stderr, "%s\n", message);
    std::abort();
  }
}

} // namespace

int main()
{
  using reb::native::lan::Reader;
  using reb::native::lan::Writer;

  const std::array<uint8_t, 4> payload{0, 127, 128, 255};
  Writer writer;
  writer.U32(0x12345678);
  writer.I32(-1234567);
  writer.U64(0x0123456789abcdefull);
  writer.String("hello");
  writer.Blob(payload);
  Require(writer.Good(), "valid values rejected");
  const auto& bytes = writer.Bytes();
  Require(bytes[0] == 0x78 && bytes[1] == 0x56 && bytes[2] == 0x34 && bytes[3] == 0x12,
          "integer byte order is wrong");

  Reader reader(bytes);
  Require(reader.U32() == 0x12345678, "U32 round trip failed");
  Require(reader.I32() == -1234567, "I32 round trip failed");
  Require(reader.U64() == 0x0123456789abcdefull, "U64 round trip failed");
  Require(reader.String() == "hello", "string round trip failed");
  Require(std::ranges::equal(reader.Blob(), payload), "blob round trip failed");
  Require(reader.Good() && reader.Done(), "reader did not consume payload");
  Require(reader.U32() == 0 && !reader.Good() && !reader.Done(), "truncated integer was accepted");

  for (size_t length = 0; length < bytes.size(); ++length) {
    Reader truncated(std::span<const uint8_t>(bytes.data(), length));
    truncated.U32();
    truncated.I32();
    truncated.U64();
    truncated.String();
    truncated.Blob();
    Require(!truncated.Done(), "truncated payload was accepted");
  }

  Writer tooLong;
  tooLong.String("abcd", 3);
  tooLong.U32(1);
  Require(!tooLong.Good() && tooLong.Bytes().empty(), "writer string limit was ignored");
  tooLong = Writer{};
  tooLong.Blob(payload, 3);
  Require(!tooLong.Good() && tooLong.Bytes().empty(), "writer blob limit was ignored");

  Reader limitedString(bytes);
  limitedString.U32(); limitedString.I32(); limitedString.U64();
  Require(limitedString.String(4).empty() && !limitedString.Good(), "reader string limit was ignored");
  Reader limitedBlob(bytes);
  limitedBlob.U32(); limitedBlob.I32(); limitedBlob.U64(); limitedBlob.String();
  Require(limitedBlob.Blob(3).empty() && !limitedBlob.Good(), "reader blob limit was ignored");

  std::vector<uint8_t> trailing = bytes;
  trailing.push_back(0);
  Reader trailingReader(trailing);
  trailingReader.U32(); trailingReader.I32(); trailingReader.U64();
  trailingReader.String(); trailingReader.Blob();
  Require(trailingReader.Good() && !trailingReader.Done(), "trailing bytes were accepted");

  const std::array<uint8_t, 4> oversizedLength{0xff, 0xff, 0xff, 0xff};
  Reader oversized(oversizedLength);
  Require(oversized.Blob().empty() && !oversized.Good(), "oversized blob was accepted");
  Require(reb::native::lan::Fingerprint({}) == 14695981039346656037ull,
          "empty fingerprint changed");
  const std::array<uint8_t, 3> abc{'a', 'b', 'c'};
  Require(reb::native::lan::Fingerprint(abc) == 0xe71fa2190541574bull,
          "fingerprint changed");
}
