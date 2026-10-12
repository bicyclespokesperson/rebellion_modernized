#pragma once

class PlayerController;

// Controller kinds are persisted in game saves. Keep a distinct, stable kind for each future AI.
int AiControllerKindFromName(const char* name);
const char* AiControllerName(int kind);
PlayerController* CreateAiController(int kind, int side);
