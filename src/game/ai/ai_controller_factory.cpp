#include "ai/ai_controller_factory.h"

#include <cstring>

#include "ai/ai_player.h"

int AiControllerKindFromName(const char* name)
{
  return name && std::strcmp(name, "classic") == 0 ? kPlayerAi : 0;
}

const char* AiControllerName(int kind)
{
  return kind == kPlayerAi ? "classic" : nullptr;
}

PlayerController* CreateAiController(int kind, int side)
{
  if (kind != kPlayerAi)
    return nullptr;
  return side == 0 ? static_cast<PlayerController*>(new AiPlayer()) : new AiPlayer(side);
}
