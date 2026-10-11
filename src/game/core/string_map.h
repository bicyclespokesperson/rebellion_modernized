#pragma once

#include <string>
#include <vector>

#include "core/game_string.h"

// Name -> int dictionary. Duplicate keys are retained; lookup returns the newest entry.
// Text values use spaces and dashes as legacy separators.
class StringMap {
public:
    StringMap() = default;
    StringMap(const StringMap& other) = default;
    StringMap& operator=(const StringMap& other) = default;

    int Add(const String* key, int value);
    int Set(const String* key, int value);
    int Find(const String* key, int* value) const;
    int FindFloat(const String* key, float* value) const;
    int FindInText(const String* text, int* value) const;
    void LoadValues(const String* text);
    void Clear();

private:
    struct Entry {
        std::string key;
        int value;
    };

    std::vector<Entry> entries_;
};
