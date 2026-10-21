#pragma once

#include <string>
#include <vector>

#include "core/game_string.h"

// Name -> int dictionary. Duplicate keys are retained; lookup returns the newest entry.
// Text values use spaces and dashes as legacy separators.
class StringMap
{
  public:
  StringMap() = default;
  StringMap(const StringMap& other) = default;
  StringMap& operator=(const StringMap& other) = default;

  int Add(const std::string* key, int value);
  int Set(const std::string* key, int value);
  int Find(const std::string* key, int* value) const;
  int FindFloat(const std::string* key, float* value) const;
  int FindInText(const std::string* text, int* value) const;
  void LoadValues(const std::string* text);
  void Clear();

  private:
  struct Entry {
    std::string key;
    int value;
  };

  std::vector<Entry> entries_;
};
