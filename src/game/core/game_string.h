#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

class istream;
class ostream;

struct ResourceStringId {
  int16_t m_id;
  int16_t m_group;
};

using StringLoader = int (*)(const ResourceStringId*, int, char*);
void SetStringLoader(StringLoader loader);

std::string LoadStringResource(const ResourceStringId* resource);
std::string StringFromText(const char* text);
int CompareStrings(const std::string& first, const std::string& second);
int CompareStringsN(const std::string& first, const std::string& second, size_t count, bool caseSensitive);
bool StartsWithNonVowel(const std::string& text);
void InsertString(std::string& text, const std::string& other, int position);
void InsertCharacter(std::string& text, char character, int position);
void RemoveStringRange(std::string& text, int start, int end);
int AppendSpaces(std::string& text, int count);
bool ReadLegacyString(istream* in, std::string& text);
bool WriteLegacyString(ostream* out, const std::string& text);

// Bounded borrow of a String's text; mutation of the owner invalidates attached cursors.
class StringCursor
{
  public:
  StringCursor() = default;
  explicit StringCursor(const std::string& text);
  StringCursor& Attach(const std::string& text);
  StringCursor& operator++();
  StringCursor operator++(int);
  StringCursor operator--(int);
  int Current() const;
  void Rewind();
  int HasMore() const;

  const char* m_current = nullptr;
  const char* m_begin = nullptr;
};

int SafeStrlen(const char* text);
void SetLocaleFromEnvironment();
