#pragma once

// The session's own random generator (g_sessionRandom, 0x6b14d8), distinct from the gameplay generator in data/random.h.
// The AI draws from it to break ties and pick among equals; the draw order is part of the AI's behaviour.
//
// Returns a value in [0, limit) (see RandomGenerator::Next), or 0 when there is no game session.
int SessionRandomNext(int limit);  // 0x41cd80
