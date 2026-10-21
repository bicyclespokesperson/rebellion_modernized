#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include "core/game_string.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"

namespace
{

int LoadTestResource(const ResourceStringId*, int bufferSize, char* buffer)
{
  constexpr char kText[] = "resource text";
  if (bufferSize < static_cast<int>(sizeof(kText))) {
    return 0;
  }
  std::memcpy(buffer, kText, sizeof(kText));
  return 1;
}

void Require(bool condition)
{
  if (!condition) {
    std::abort();
  }
}

void Rewind(ScopedStrstream& stream)
{
  Require(istream_seekg_pos(stream.get()->In(), 0) == stream.get()->In());
}

void VerifyLongEditingAndAliasing()
{
  std::string longText;
  Require(AppendSpaces(longText, 70000) == 1);
  Require(longText.size() == 70000);
  Require(longText.size() + 1 == 70001);

  std::string value("abcdef");
  value = value.substr(2);
  Require(value == "cdef");

  value += value;
  Require(value == "cdefcdef");
  InsertString(value, value, 4);
  Require(value == "cdefcdefcdefcdef");
  InsertString(value, "!", -5);
  InsertCharacter(value, '?', 999);
  Require(value == "!cdefcdefcdefcdef?");

  const size_t beforeSpaces = value.size();
  Require(AppendSpaces(value, -4) == 1);
  Require(value.size() == beforeSpaces);
  RemoveStringRange(value, -1, 1);
  RemoveStringRange(value, 1, 999);
  Require(value.size() == beforeSpaces);
}

void VerifyCopiesAndResourceLifetime()
{
  std::string original("copied");
  std::string copied(original);
  std::string assigned;
  assigned = original;
  assigned = assigned;
  std::string assignedThroughApi;
  assignedThroughApi = original;
  assignedThroughApi = assignedThroughApi;

  original = "changed";
  Require(copied == "copied");
  Require(assigned == "copied");
  Require(assignedThroughApi == "copied");

  SetStringLoader(LoadTestResource);
  const ResourceStringId resource{1, 2};
  std::string loaded = LoadStringResource(&resource);
  SetStringLoader(nullptr);
  Require(loaded == "resource text");
}

void VerifyTruncatedReadPreservesValue()
{
  {
    ScopedStrstream stream;
    const char truncatedLength = 3;
    ostream_write(stream.get()->Out(), &truncatedLength, 1);
    Rewind(stream);

    std::string value("kept");
    Require(!ReadLegacyString(stream.get()->In(), value));
    Require(value == "kept");
  }

  {
    ScopedStrstream stream;
    const uint16_t length = 3;
    const char body[] = {'x', 'y'};
    StreamWrite16(stream.get()->Out(), &length);
    ostream_write(stream.get()->Out(), body, sizeof(body));
    Rewind(stream);

    std::string value("kept");
    Require(!ReadLegacyString(stream.get()->In(), value));
    Require(value == "kept");
  }
}

void VerifyMaximumLengthRoundTrip()
{
  const std::string body(65535, 'x');
  std::string value(body);
  Require(value.size() == 65535);

  ScopedStrstream stream;
  Require(WriteLegacyString(stream.get()->Out(), value));
  Rewind(stream);

  std::string restored("old");
  Require(ReadLegacyString(stream.get()->In(), restored));
  Require(restored.size() == body.size());
  Require(restored == body);
}

void VerifyOversizedWriteEmitsNoBytes()
{
  std::string value;
  Require(AppendSpaces(value, 65536) == 1);

  ScopedStrstream stream;
  Require(!WriteLegacyString(stream.get()->Out(), value));
  Require((stream.get()->Ios()->m_state & iostream_flags::kFail) != 0);

  const auto* buffer = iostream_access::BufferOf(stream.get());
  Require(buffer->m_pptr == buffer->m_pbase);
}

} // namespace

int main()
{
  VerifyLongEditingAndAliasing();
  VerifyCopiesAndResourceLifetime();
  VerifyTruncatedReadPreservesValue();
  VerifyMaximumLengthRoundTrip();
  VerifyOversizedWriteEmitsNoBytes();
}
