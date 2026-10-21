#include "ai/ai_controller_factory.h"

#include <cstring>

#include "ai/ai_player.h"
#include "ai/claude_ai_player.h"
#include "ai/codex_ai_player.h"

std::span<const AiControllerDefinition> AiControllerDefinitions()
{
  static constexpr AiControllerDefinition definitions[] = {
      {kPlayerAi, "classic", "Classic (game default)"},
      {kPlayerAiCodex, "codex", "Codex Strategist"},
      {kPlayerAiClaude, "claude", "Claude Admiral"},
  };
  return definitions;
}

int AiControllerKindFromName(const char* name)
{
  for (const auto& definition : AiControllerDefinitions()) {
    if (name && std::strcmp(name, definition.name) == 0) {
      return definition.kind;
    }
  }
  return 0;
}

const char* AiControllerName(int kind)
{
  for (const auto& definition : AiControllerDefinitions()) {
    if (kind == definition.kind) {
      return definition.name;
    }
  }
  return nullptr;
}

PlayerController* CreateAiController(int kind, int side)
{
  if (kind == kPlayerAi) {
    return side == 0 ? static_cast<PlayerController*>(new AiPlayer()) : new AiPlayer(side);
  }
  if (kind == kPlayerAiCodex) {
    return side == 0 ? static_cast<PlayerController*>(new CodexAiPlayer()) : new CodexAiPlayer(side);
  }
  if (kind == kPlayerAiClaude) {
    return side == 0 ? static_cast<PlayerController*>(new ClaudeAiPlayer()) : new ClaudeAiPlayer(side);
  }
  return nullptr;
}
