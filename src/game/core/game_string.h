#pragma once
#include <cstdint>

// Heap-allocated, NUL-terminated string. Used throughout the game (~1500 destructor call sites).
class String {
public:
    String();
    String(const char* text);
    virtual ~String();

    char* m_text;  // +0x04, owned; null for an empty string
    int m_valid;   // +0x08, 0 if allocating m_text failed
};
static_assert(sizeof(String) == 0x0c);

// Length of `text`, or 0 for null.
int SafeStrlen(const char* text);

// *out = heap copy of `text` (null if `text` is null). Returns 0 only if the allocation failed.
int DuplicateString(const char* text, char** out);
