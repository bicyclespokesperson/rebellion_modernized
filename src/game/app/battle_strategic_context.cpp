#include "app/battle_engine_space.h"

#include "app/ai_preferences.h"
#include "app/game_session.h"
#include "data/gnprtb.h"

StrategicBattleContext CurrentStrategicBattleContext()
{
  StrategicBattleContext context;
  context.ruleDifficultyColumn = g_gnprtbDifficulty;
  const GameSession* session = GetGameSession();
  for (int side = 0; side < 2; ++side) {
    PlayerController* controller = session ? session->m_controllers[side] : nullptr;
    context.controllerKinds[side] = controller ? controller->GetKind() : PreferredAiKind(side + 1);
  }
  return context;
}
