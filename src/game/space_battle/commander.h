#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include "space_battle/commands.h"

// A commander controls one side of a battle through Commands only, exactly as the player does (§9): it reads the
// battle (Battle::State() for the full picture) and appends commands for its side. The battle runs one for every
// AI-controlled side at the start of each tick, before that tick's commands are applied, and logs what it submits,
// so a replay with both sides human-controlled re-simulates the battle from the command log alone.
namespace reb::battle
{

class Battle;

class Commander
{
  public:
  virtual ~Commander() = default;
  // `out` receives commands for `side`; their tick should be battle.Tick().
  virtual void Update(const Battle& battle, Side side, std::vector<Command>& out) = 0;
  // Mid-battle saves carry each commander's private state (none by default).
  virtual void Save(std::vector<uint8_t>& out) const
  {
  }
  virtual bool Restore(std::span<const uint8_t> data)
  {
    return data.empty();
  }
};

enum class Temperament : uint8_t { Aggressive, Balanced, Cautious };
enum class CommanderDifficulty : uint8_t { Low, Normal, High };

struct CommanderParams {
  Temperament temperament = Temperament::Balanced;
  CommanderDifficulty difficulty = CommanderDifficulty::Normal;
};

class DefaultCommander final : public Commander
{
  public:
  explicit DefaultCommander(CommanderParams params = {}) : m_params(params) {}
  void Update(const Battle& battle, Side side, std::vector<Command>& out) override;
  void Save(std::vector<uint8_t>& out) const override;
  bool Restore(std::span<const uint8_t> data) override;

  private:
  struct Preservation {
    UnitIndex ship = kNoUnit;
    int64_t since = 0;
  };
  void PreserveShips(const Battle& battle, Side side, std::vector<Command>& out);
  CommanderParams m_params;
  bool m_initialized = false;
  bool m_observing = false;
  int64_t m_nextThink = 0;
  int64_t m_withdrawTick = -1;
  std::vector<Preservation> m_preserving;
};

std::unique_ptr<Commander> MakeBattleCommander(CommanderParams params = {});

} // namespace reb::battle
