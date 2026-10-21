#pragma once

#include <span>

class PlayerController;

struct AiControllerDefinition {
  int kind;
  const char* name;
  const char* displayName;
};

std::span<const AiControllerDefinition> AiControllerDefinitions();

// Controller kinds are persisted in game saves. Keep a distinct, stable kind for each future AI.
int AiControllerKindFromName(const char* name);
const char* AiControllerName(int kind);
PlayerController* CreateAiController(int kind, int side);
