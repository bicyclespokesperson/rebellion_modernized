#pragma once
#include "strategy_ui/game_ui_facade.h"

// The session's own random generator (g_sessionRandom, 0x6b14d8), distinct from the gameplay generator in data/random.h.
// The AI draws from it to break ties and pick among equals; the draw order is part of the AI's behaviour.
//
// Returns a value in [0, limit) (see RandomGenerator::Next), or 0 when there is no game session.
inline int SessionRandomNext(int limit) { return UiSessionRandom(limit); }  // 0x41cd80
