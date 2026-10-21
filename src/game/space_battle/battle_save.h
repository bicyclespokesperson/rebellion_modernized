#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "space_battle/battle_state.h"

// Versioned binary encodings for mid-battle saves and replays (tactical_combat.md §11.4). Same-platform stable:
// values are written in host byte order and floats bit for bit.
namespace reb::battle
{

// Version 2 adds planetary batteries (§6.13), version 3 whether each squadron has launched; older blocks still read.
inline constexpr uint32_t kStateVersion = 3;

// The dynamic state only: profiles are derived again from the snapshot, cosmetic effects are dropped. An older
// `version` leaves out what it did not have (the compatibility tests write version 1).
void WriteState(const BattleState& state, std::vector<uint8_t>& out, uint32_t version = kStateVersion);
// Reads into a state already initialised from the same snapshot and options; false on a malformed or foreign block.
bool ReadState(BattleState& state, std::span<const uint8_t> data, size_t* consumed);

// A replay or bug report: the commands of a battle, in order (Battle::CommandLog()).
std::vector<uint8_t> EncodeCommands(std::span<const Command> commands);
std::optional<std::vector<Command>> DecodeCommands(std::span<const uint8_t> data);

uint64_t HashBytes(std::span<const uint8_t> data);

} // namespace reb::battle
