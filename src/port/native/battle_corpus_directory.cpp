#include "app/battle_engine.h"

#include <cstdlib>

std::optional<std::string> BattleCorpusDirectory()
{
  const char* directory = std::getenv("REB_BATTLE_CORPUS");
  if (!directory || !*directory) {
    return std::nullopt;
  }
  return std::string(directory);
}
