#include "core/game_string.h"

#include <cstring>
#include <new>

int16_t FUN_00479ac0();  // returns 1: bytes reserved for the terminator

String::String() : m_text(nullptr), m_valid(1) {}

String::String(const char* text) { m_valid = DuplicateString(text, &m_text); }

String::~String() { operator delete(m_text); }

int SafeStrlen(const char* text) { return text ? static_cast<int>(std::strlen(text)) : 0; }

int DuplicateString(const char* text, char** out) {
    *out = nullptr;
    if (!text) return 1;
    // The original truncates both terms to 16 bits.
    const uint16_t length = static_cast<uint16_t>(SafeStrlen(text));
    const uint16_t terminator = static_cast<uint16_t>(FUN_00479ac0());
    char* copy = static_cast<char*>(operator new(length + terminator));
    *out = copy;
    if (!copy) return 0;
    std::memcpy(copy, text, std::strlen(text) + 1);
    return 1;
}
