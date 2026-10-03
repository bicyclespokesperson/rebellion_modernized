#pragma once
#include <cstddef>
#include <cstdint>

#include "core/layout.h"

class istream;
class ostream;
class StringCursor;

// Heap-allocated, NUL-terminated string. Used throughout the game (~1500 destructor call sites).
// Most operations free the old buffer and replace it, recording in m_valid whether the new allocation
// succeeded (operator new returns NULL on failure). Lengths are truncated to 16 bits everywhere.
class String {
public:
    String();
    String(const char* text);
    String(const String& other);
    String(int resourceId);
    virtual ~String();

    String& operator=(const String&) = delete;
    int Assign(const String& other);
    void operator=(const char* text);
    void SetFromResource(int resourceId);
    void Clear();

    unsigned short Length() const;
    short GetBufferSize() const;  // Length() + terminator, 0 for an empty (null) string
    char* CopyText() const;       // new heap copy of the text, caller owns it

    String& operator+=(const String& other);
    String operator+(const String& other) const;
    String& AppendSigned(long value);
    String& AppendUnsigned(unsigned long value);
    String& AppendChar(const char& c);
    int AppendSpaces(int count);

    // Insert `other` / the character `c` before character `position` (clamped to the length).
    void Insert(const String& other, int position);
    void InsertChar(const char& c, int position);
    // Remove characters [start, end); does nothing unless 0 <= start < end <= Length().
    void RemoveRange(int start, int end);

    // Byte-wise comparison (null counts as ""): -1, 0 or 1.
    int Compare(const String& other) const;
    int operator==(const String& other) const;
    int operator<(const String& other) const;
    int CompareN(const String& other, size_t count, int caseSensitive) const;
    int StartsWithNonVowel() const;
    int CompareToZeroString() const;

    void ReadFrom(istream* in);
    void WriteTo(ostream* out);
    void Read(istream* in);
    void Write(ostream* out);

    char* m_text;  // +0x04, owned; null for an empty string
    int m_valid;   // +0x08, 0 if allocating m_text failed
};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(String, 0x0c);
REB_ASSERT_OFFSET(String, m_text, 0x04);
REB_ASSERT_OFFSET(String, m_valid, 0x08);
#pragma clang diagnostic pop

// Read position inside a String's text (a char* plus the start, to allow rewinding). Does not own the text.
class StringCursor {
public:
    StringCursor();
    StringCursor(const StringCursor& other);
    explicit StringCursor(const String& text);
    virtual ~StringCursor();

    StringCursor& operator=(const StringCursor& other);
    StringCursor& Attach(const String& text);
    StringCursor& operator++();                // advance, stopping at the terminator
    StringCursor operator++(int);              // returns the position before advancing
    StringCursor operator--(int);              // returns the position before moving back; see BUG note in .cpp
    int Current() const;                       // character at the position, 0 if no text
    void Rewind();
    int HasMore() const;                       // 1 if positioned on a non-terminator character

    const char* m_current;  // +0x04
    const char* m_begin;    // +0x08
};
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winvalid-offsetof"
REB_ASSERT_SIZE(StringCursor, 0x0c);
REB_ASSERT_OFFSET(StringCursor, m_current, 0x04);
REB_ASSERT_OFFSET(StringCursor, m_begin, 0x08);
#pragma clang diagnostic pop

// Length of `text`, or 0 for null.
int SafeStrlen(const char* text);

// *out = heap copy of `text` (null if `text` is null). Returns 0 only if the allocation failed.
int DuplicateString(const char* text, char** out);

// *out = heap concatenation of `first` and `second` (either may be null; null if both are). Returns 0 only
// if the allocation failed.
int ConcatenateStrings(const char* first, const char* second, char** out);

// *out = heap copy of string resource `resourceId` (via the registered loader callback; "" if there is none).
int LoadStringResource(int resourceId, char** out);

// Binary stream format: uint16 length followed by that many bytes (no terminator).
int ReadLengthPrefixedString(istream* in, char** out);
int WriteLengthPrefixedString(ostream* out, const char* text);

// setlocale(LC_ALL, "")
void SetLocaleFromEnvironment();
