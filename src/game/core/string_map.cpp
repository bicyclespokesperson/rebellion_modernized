#include "core/string_map.h"

#include <charconv>
#include <string_view>
#include <system_error>

namespace {

bool IsSeparator(char character) { return character == ' ' || character == '-'; }

std::string_view TextOf(const String* text) {
    return text && text->m_text ? std::string_view(text->m_text) : std::string_view{};
}

std::string_view NextWord(std::string_view text, size_t* position) {
    while (*position < text.size() && IsSeparator(text[*position])) ++*position;
    const size_t start = *position;
    while (*position < text.size() && !IsSeparator(text[*position])) ++*position;
    return text.substr(start, *position - start);
}

bool ReadNumber(std::string_view text, size_t* position, int* value) {
    while (*position < text.size() && IsSeparator(text[*position])) ++*position;
    const size_t start = *position;
    while (*position < text.size() && text[*position] >= '0' && text[*position] <= '9') ++*position;
    if (start == *position) return false;
    if (*position < text.size() && !IsSeparator(text[*position])) return false;

    int parsed = 0;
    const auto result = std::from_chars(text.data() + start, text.data() + *position, parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + *position) return false;
    *value = parsed;
    return true;
}

}  // namespace

int StringMap::Add(const String* key, int value) {
    entries_.insert(entries_.begin(), {std::string(TextOf(key)), value});
    return 1;
}

int StringMap::Set(const String* key, int value) {
    const std::string_view name = TextOf(key);
    for (Entry& entry : entries_) {
        if (entry.key == name) {
            entry.value = value;
            return 1;
        }
    }
    return Add(key, value);
}

int StringMap::Find(const String* key, int* value) const {
    if (!value) return 0;
    const std::string_view name = TextOf(key);
    for (const Entry& entry : entries_) {
        if (entry.key == name) {
            *value = entry.value;
            return 1;
        }
    }
    return 0;
}

int StringMap::FindFloat(const String* key, float* value) const {
    if (!value) return 0;
    int found = 0;
    if (!FindInText(key, &found)) return 0;
    *value = static_cast<float>(found);
    return 1;
}

int StringMap::FindInText(const String* text, int* value) const {
    if (!value) return 0;
    const std::string_view input = TextOf(text);
    size_t position = 0;
    while (position < input.size()) {
        const std::string_view word = NextWord(input, &position);
        if (word.empty()) break;
        for (const Entry& entry : entries_) {
            if (entry.key == word) {
                *value = entry.value;
                return 1;
            }
        }
    }
    return Find(text, value);
}

void StringMap::LoadValues(const String* text) {
    const std::string_view input = TextOf(text);
    size_t position = 0;
    while (position < input.size()) {
        const std::string_view name = NextWord(input, &position);
        if (name.empty()) break;

        bool known = false;
        for (const Entry& entry : entries_) {
            if (entry.key == name) {
                known = true;
                break;
            }
        }
        if (!known) continue;

        int parsed = 0;
        if (ReadNumber(input, &position, &parsed)) {
            for (Entry& entry : entries_) {
                if (entry.key == name) {
                    entry.value = parsed;
                    break;
                }
            }
        }
    }
}

void StringMap::Clear() { entries_.clear(); }
