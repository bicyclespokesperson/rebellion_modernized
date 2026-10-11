#include "core/reb_abi.h"
#include "core/game_string.h"
#include "core/icf_stubs.h"

#include <cstdio>
#include <cstring>
#include <locale.h>
#include <new>

#include "core/stream_io.h"
#include "iostream/iostream.h"


extern char g_stringResourceBuffer[0x100];
extern void* g_loadStringCallback;
extern char g_signedNumberBuffer[];
extern char g_unsignedNumberBuffer[];
extern const char *g_vowelCharacters;

namespace {

// operator new returns NULL on failure here, but the compiler assumes the standard non-null contract and
// would delete the null checks; the empty asm hides the value from it.
char* allocateChars(size_t size) {
    void* memory = operator new(size);
    asm volatile("" : "+r"(memory));
    return static_cast<char*>(memory);
}

int terminatorBytes() { return static_cast<uint16_t>(ReturnOneShort()); }

int length16(const char* text) { return static_cast<uint16_t>(SafeStrlen(text)); }

void appendRaw(char* dest, const char* src) { std::memcpy(dest + std::strlen(dest), src, std::strlen(src) + 1); }

const char* orEmpty(const char* text) { return text ? text : ""; }

void advance(StringCursor& cursor, int count) {
    for (; count != 0; --count) ++cursor;
}

}  // namespace

String::String() : m_text(nullptr), m_valid(1) {}

String::String(const char* text) { m_valid = DuplicateString(text, &m_text); }

String::String(const String& other) { m_valid = DuplicateString(other.m_text, &m_text); }

String::String(const ResourceStringId* resource) { m_valid = LoadStringResource(resource, &m_text); }

String::~String() { operator delete(m_text); }

int SafeStrlen(const char* text) { return text ? static_cast<int>(std::strlen(text)) : 0; }

int DuplicateString(const char* text, char** out) {
    *out = nullptr;
    if (!text) return 1;
    // The original truncates both terms to 16 bits.
    const uint16_t length = static_cast<uint16_t>(SafeStrlen(text));
    const uint16_t terminator = static_cast<uint16_t>(ReturnOneShort());
    char* copy = allocateChars(length + terminator);
    *out = copy;
    if (!copy) return 0;
    std::memcpy(copy, text, std::strlen(text) + 1);
    return 1;
}

int ConcatenateStrings(const char* first, const char* second, char** out) {
    *out = nullptr;
    if (!first && !second) return 1;
    char* result = allocateChars(length16(first) + length16(second) + terminatorBytes());
    *out = result;
    if (!result) return 0;
    *result = '\0';
    if (first) appendRaw(result, first);
    if (second) appendRaw(result, second);
    return 1;
}

int LoadStringResource(const ResourceStringId* resource, char** out) {
    *out = nullptr;
    g_stringResourceBuffer[0] = '\0';
    if (g_loadStringCallback) {
        using LoadStringCallback = void(REB_CDECL*)(const ResourceStringId* resource, int bufferSize, char* buffer);
        reinterpret_cast<LoadStringCallback>(g_loadStringCallback)(resource, sizeof(g_stringResourceBuffer),
                                                                   g_stringResourceBuffer);
    }
    return DuplicateString(g_stringResourceBuffer, out);
}

int ReadLengthPrefixedString(istream* in, char** out) {
    *out = nullptr;
    // The original reads into the low half of its own stream-pointer argument, so a failed read leaves that as the length.
    uint16_t length = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(in));
    StreamRead16(in, &length);
    char* buffer = allocateChars(terminatorBytes() + length);
    if (!buffer) return 0;
    istream_read(in, buffer, length);
    buffer[length] = '\0';
    *out = buffer;
    return 1;
}

int WriteLengthPrefixedString(ostream* out, const char* text) {
    const uint16_t length = static_cast<uint16_t>(SafeStrlen(text));
    StreamWrite16(out, &length);
    ostream_write(out, text, length);
    return 1;
}

void SetLocaleFromEnvironment() { setlocale(LC_ALL, ""); }

int String::Assign(const String& other) {
    if (m_text != other.m_text) {
        operator delete(m_text);
        m_valid = DuplicateString(other.m_text, &m_text);
    }
    return m_valid;
}

void String::operator=(const char* text) {
    operator delete(m_text);
    m_valid = DuplicateString(text, &m_text);
}

void String::SetFromResource(const ResourceStringId* resource) {
    operator delete(m_text);
    m_valid = LoadStringResource(resource, &m_text);
}

void String::Clear() { *this = ""; }

unsigned short String::Length() const {
    if (!m_text) return 0;
    return static_cast<unsigned short>(std::strlen(m_text));
}

short String::GetBufferSize() const {
    if (!m_text) return 0;
    return static_cast<short>(static_cast<int16_t>(SafeStrlen(m_text)) + ReturnOneShort());
}

char* String::CopyText() const {
    char* copy;
    DuplicateString(m_text, &copy);
    return copy;
}

String& String::operator+=(const String& other) {
    char* combined;
    m_valid = ConcatenateStrings(m_text, other.m_text, &combined);
    operator delete(m_text);
    m_text = combined;
    return *this;
}

String String::operator+(const String& other) const {
    String sum;
    sum.m_valid = ConcatenateStrings(m_text, other.m_text, &sum.m_text);
    // The original copy-constructs the result from the temporary (no named-return optimisation).
    return String(sum);
}

String& String::AppendSigned(long value) {
    std::sprintf(g_signedNumberBuffer, "%ld", value);
    char* combined;
    m_valid = ConcatenateStrings(m_text, g_signedNumberBuffer, &combined);
    operator delete(m_text);
    m_text = combined;
    return *this;
}

String& String::AppendUnsigned(unsigned long value) {
    std::sprintf(g_unsignedNumberBuffer, "%lu", value);
    char* combined;
    m_valid = ConcatenateStrings(m_text, g_unsignedNumberBuffer, &combined);
    operator delete(m_text);
    m_text = combined;
    return *this;
}

String& String::AppendChar(const char& c) {
    // BUG(original): the allocation is not checked, and m_valid is not updated.
    char* grown = allocateChars((GetBufferSize() & 0xffff) + 4);
    *grown = '\0';
    if (m_text) {
        std::memcpy(grown, m_text, std::strlen(m_text) + 1);
        operator delete(m_text);
    }
    std::strncat(grown, &c, 1);
    m_text = grown;
    return *this;
}

int String::AppendSpaces(int count) {
    // BUG(original): the allocation is not checked.
    char* spaces = allocateChars(terminatorBytes() + 1 * count);
    *spaces = '\0';
    for (; count > 0; --count) appendRaw(spaces, " ");
    char* combined;
    m_valid = ConcatenateStrings(m_text, spaces, &combined);
    operator delete(spaces);
    operator delete(m_text);
    m_text = combined;
    return m_valid;
}

void String::Insert(const String& other, int position) {
    StringCursor tail(*this);
    if (position > Length()) position = Length();
    if (other.Length() == 0) return;
    char* result = allocateChars(length16(other.m_text) + length16(m_text) + terminatorBytes());
    if (!result) return;
    *result = '\0';
    if (m_text) std::strncat(result, m_text, position);
    appendRaw(result, other.m_text);
    if (tail.m_current) {
        advance(tail, position);
        appendRaw(result, tail.m_current);
    }
    operator delete(m_text);
    m_text = result;
}

void String::InsertChar(const char& c, int position) {
    StringCursor tail(*this);
    if (position > Length()) position = Length();
    char* result = allocateChars(length16(m_text) + terminatorBytes() + 2);
    if (!result) return;
    *result = '\0';
    if (m_text) std::strncat(result, m_text, position);
    std::strncat(result, &c, 1);
    if (tail.m_current) {
        advance(tail, position);
        appendRaw(result, tail.m_current);
    }
    operator delete(m_text);
    m_text = result;
}

void String::RemoveRange(int start, int end) {
    const int length = Length();
    StringCursor tail(*this);
    if (start < 0 || start >= length || end <= start || end > length) return;
    // BUG(original): the allocation is not checked.
    char* result = allocateChars(length16(m_text) + terminatorBytes());
    *result = '\0';
    if (start > 0) std::strncat(result, m_text, start);
    advance(tail, end);
    appendRaw(result, tail.m_current);
    operator delete(m_text);
    m_text = result;
}

int String::Compare(const String& other) const {
    const unsigned char* a = reinterpret_cast<const unsigned char*>(orEmpty(m_text));
    const unsigned char* b = reinterpret_cast<const unsigned char*>(orEmpty(other.m_text));
    while (*a == *b) {
        if (*a == 0) return 0;
        ++a;
        ++b;
    }
    return *a < *b ? -1 : 1;
}

int String::operator==(const String& other) const { return Compare(other) == 0; }

int String::operator<(const String& other) const { return Compare(other) < 0; }

int String::CompareN(const String& other, size_t count, int caseSensitive) const {
    if (caseSensitive) return std::strncmp(orEmpty(m_text), orEmpty(other.m_text), count);
    return _strnicmp(orEmpty(m_text), orEmpty(other.m_text), count);
}

int String::StartsWithNonVowel() const { return std::strchr(g_vowelCharacters, *m_text) == nullptr; }

int String::CompareToZeroString() const {
    String zero("0");
    return Compare(zero);
}

void String::ReadFrom(istream* in) {
    operator delete(m_text);
    m_valid = ReadLengthPrefixedString(in, &m_text);
}

void String::WriteTo(ostream* out) { m_valid = WriteLengthPrefixedString(out, m_text); }

void String::Read(istream* in) { ReadFrom(in); }

void String::Write(ostream* out) { WriteTo(out); }

StringCursor::StringCursor() : m_current(nullptr), m_begin(nullptr) {}

StringCursor::StringCursor(const StringCursor& other) : m_current(other.m_current), m_begin(other.m_begin) {}

StringCursor::StringCursor(const String& text) : m_current(text.m_text), m_begin(text.m_text) {}

StringCursor::~StringCursor() {}

StringCursor& StringCursor::operator=(const StringCursor& other) {
    m_current = other.m_current;
    m_begin = other.m_begin;
    return *this;
}

StringCursor& StringCursor::Attach(const String& text) {
    m_begin = m_current = text.m_text;
    return *this;
}

StringCursor& StringCursor::operator++() {
    if (m_current && *m_current) ++m_current;
    return *this;
}

StringCursor StringCursor::operator++(int) {
    StringCursor previous(*this);
    ++*this;
    return StringCursor(previous);
}

StringCursor StringCursor::operator--(int) {
    StringCursor previous(*this);
    if (m_current && m_current != m_begin) {
        // BUG(original): moving back from a position below m_begin yields null instead of staying put.
        m_current = reinterpret_cast<uintptr_t>(m_begin) < reinterpret_cast<uintptr_t>(m_current) ? m_current - 1 : nullptr;
    }
    return StringCursor(previous);
}

int StringCursor::Current() const { return m_current ? *m_current : 0; }

void StringCursor::Rewind() { m_current = m_begin; }

int StringCursor::HasMore() const { return m_current && *m_current ? 1 : 0; }

char g_stringResourceBuffer[256]{};
char g_signedNumberBuffer[32]{};
char g_unsignedNumberBuffer[32]{};
const char *g_vowelCharacters = nullptr;
void * g_loadStringCallback{};
