#include "core/game_string.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <locale.h>

#include "core/stream_io.h"
#include "iostream/iostream.h"

extern const char* g_vowelCharacters;

namespace
{

StringLoader stringLoader = nullptr;

std::string LoadResource(const ResourceStringId* resource)
{
  if (!resource)
    return {};
  std::array<char, 256> buffer{};
  if (stringLoader && !stringLoader(resource, static_cast<int>(buffer.size()), buffer.data()))
    return {};
  buffer.back() = '\0';
  return buffer.data();
}

size_t InsertPosition(int position, size_t length)
{
  return position <= 0 ? 0 : std::min(static_cast<size_t>(position), length);
}

} // namespace

void SetStringLoader(StringLoader loader)
{
  stringLoader = loader;
}

std::string LoadStringResource(const ResourceStringId* resource)
{
  return LoadResource(resource);
}
std::string StringFromText(const char* text)
{
  return text ? text : "";
}

int SafeStrlen(const char* text)
{
  return text ? static_cast<int>(std::strlen(text)) : 0;
}
void SetLocaleFromEnvironment()
{
  setlocale(LC_ALL, "");
}

int AppendSpaces(std::string& text, int count)
{
  if (count > 0)
    text.append(static_cast<size_t>(count), ' ');
  return 1;
}

void InsertString(std::string& text, const std::string& other, int position)
{
  text.insert(InsertPosition(position, text.size()), other);
}

void InsertCharacter(std::string& text, char character, int position)
{
  if (character != '\0')
    text.insert(InsertPosition(position, text.size()), 1, character);
}

void RemoveStringRange(std::string& text, int start, int end)
{
  if (start < 0 || end <= start || static_cast<size_t>(end) > text.size())
    return;
  text.erase(static_cast<size_t>(start), static_cast<size_t>(end - start));
}

int CompareStrings(const std::string& first, const std::string& second)
{
  const int comparison = first.compare(second);
  return (comparison > 0) - (comparison < 0);
}

int CompareStringsN(const std::string& first, const std::string& second, size_t count, bool caseSensitive)
{
  if (caseSensitive)
    return std::strncmp(first.c_str(), second.c_str(), count);
  return _strnicmp(first.c_str(), second.c_str(), count);
}

bool StartsWithNonVowel(const std::string& text)
{
  if (text.empty())
    return false;
  const char* vowels = g_vowelCharacters ? g_vowelCharacters : "aeiouAEIOU";
  return std::strchr(vowels, text.front()) == nullptr;
}

bool ReadLegacyString(istream* in, std::string& text)
{
  uint16_t length = 0;
  StreamRead16(in, &length);
  if (in->m_gcount != sizeof(length) || iostream_access::IosOf(in)->m_state != 0)
    return false;
  std::string replacement(length, '\0');
  if (length != 0)
  {
    istream_read(in, replacement.data(), length);
    if (in->m_gcount != length || iostream_access::IosOf(in)->m_state != 0)
      return false;
  }
  text = std::move(replacement);
  return true;
}

bool WriteLegacyString(ostream* out, const std::string& text)
{
  if (text.size() > std::numeric_limits<uint16_t>::max())
  {
    iostream_access::SetState(iostream_access::IosOf(out), iostream_flags::kFail);
    return false;
  }
  const auto length = static_cast<uint16_t>(text.size());
  StreamWrite16(out, &length);
  if (length != 0)
    ostream_write(out, text.data(), length);
  return iostream_access::IosOf(out)->m_state == 0;
}

StringCursor::StringCursor(const std::string& text)
{
  Attach(text);
}

StringCursor& StringCursor::Attach(const std::string& text)
{
  m_begin = m_current = text.c_str();
  return *this;
}

StringCursor& StringCursor::operator++()
{
  if (m_current && *m_current)
    ++m_current;
  return *this;
}

StringCursor StringCursor::operator++(int)
{
  StringCursor previous(*this);
  ++*this;
  return previous;
}

StringCursor StringCursor::operator--(int)
{
  StringCursor previous(*this);
  if (m_current && m_current > m_begin)
    --m_current;
  return previous;
}

int StringCursor::Current() const
{
  return m_current ? static_cast<unsigned char>(*m_current) : 0;
}
void StringCursor::Rewind()
{
  m_current = m_begin;
}
int StringCursor::HasMore() const
{
  return m_current && *m_current ? 1 : 0;
}

const char* g_vowelCharacters = nullptr;
