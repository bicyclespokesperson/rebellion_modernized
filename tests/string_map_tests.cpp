#include <cstdlib>

#include "core/string_map.h"

namespace
{

void Require(bool condition)
{
  if (!condition) {
    std::abort();
  }
}

int Value(const StringMap& settings, const std::string& key)
{
  int value = 0;
  Require(settings.Find(&key, &value) == 1);
  return value;
}

} // namespace

int main()
{
  const std::string latency("Latency");
  const std::string drop("Drop");
  StringMap settings;
  settings.Add(&latency, 10);
  settings.Add(&latency, 20);
  settings.Add(&drop, 5);
  Require(Value(settings, latency) == 20);

  StringMap copied(settings);
  StringMap assigned;
  assigned = settings;
  Require(Value(copied, latency) == 20);
  Require(Value(assigned, latency) == 20);
  assigned.Set(&latency, 30);
  Require(Value(settings, latency) == 20);
  Require(Value(assigned, latency) == 30);

  const std::string input("unknown 7 -Latency 2147483648 -Drop 9 -Latency missing -Drop 11");
  settings.LoadValues(&input);
  Require(Value(settings, latency) == 20);
  Require(Value(settings, drop) == 11);
  const std::string malformed("Latency 12oops Drop 13");
  settings.LoadValues(&malformed);
  Require(Value(settings, latency) == 20);
  Require(Value(settings, drop) == 13);
  const std::string missing("Latency");
  settings.LoadValues(&missing);
  Require(Value(settings, latency) == 20);

  settings.Set(&latency, -3);
  const std::string words("unknown-Latency-Drop");
  int found = 0;
  Require(settings.FindInText(&words, &found) == 1 && found == -3);
  float floating = 0;
  Require(settings.FindFloat(&words, &floating) == 1 && floating == -3.0f);
  const std::string whole("two words");
  settings.Add(&whole, 42);
  Require(settings.FindInText(&whole, &found) == 1 && found == 42);
  settings.Clear();
  Require(settings.Find(&latency, &found) == 0);
}
