#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "battle_ui/battle_source.h"

// A fabricated battle for UI work while the simulator is being written, and for screen tests: two small fleets that
// close, fire bolts, flash shields, explode, tractor and withdraw. It obeys move, attack and hold orders roughly. It
// is deterministic for a seed but follows none of the design's combat rules.
namespace reb::battle_ui
{

class FakeBattle : public BattleSource
{
  public:
  // `fleets` copies of each side's fleet, side by side, for testing the screen with larger battles.
  explicit FakeBattle(uint32_t seed = 1, int fleets = 1);

  bool Submit(const battle::Command& command) override;
  void Step() override;
  bool Finished() const override;
  int64_t Tick() const override
  {
    return m_tick;
  }
  void SetAiControlled(battle::Side side, bool ai) override;
  void BuildViewModel(float alpha, battle::ViewModel& out) const override;
  battle::ViewUnitDetail Detail(battle::UnitIndex unit) const override;

  // An Imperial Death Star behind the Empire's fleet, with a charged superlaser (for the HUD's Death Star controls).
  void AddDeathStar(battle::Vec2 position);

  bool AiControlled(battle::Side side) const
  {
    return m_ai[static_cast<int>(side)];
  }
  const std::vector<battle::Command>& Commands() const
  {
    return m_commands;
  }

  private:
  struct Unit {
    battle::ViewUnit view;
    battle::Vec2 previous;
    float previousHeading = 0.0f;
    std::string name;
    std::string className;
    battle::WeaponGrid weapons;
    float speed = 0.0f;    // m/s
    float turnRate = 0.0f; // rad/s
    float hull = 0.0f;
    float hullMax = 0.0f;
    float shield = 0.0f;
    float shieldMax = 0.0f;
    float range = 0.0f; // m, longest weapon
    float preferredRange = 0.0f;
    float fireInterval = 0.0f;
    float cooldown = 0.0f;
    bool broadside = false;
    std::vector<battle::UnitIndex> attackOrder;
    std::optional<battle::Vec2> moveTo;
    std::optional<float> moveFacing;
    bool hold = false;
    battle::UnitIndex tractorTarget = battle::kNoUnit;
    float spoolSeconds = 0.0f;
    // Per-unit doctrine overrides (§5.1): the fields in `overrides` replace the group's.
    battle::ShipDoctrine shipOverride;
    battle::FlightDoctrine flightOverride;
    battle::DoctrineOverrideMask overrides = 0;
  };
  struct Shot {
    battle::ViewShot view;
    battle::UnitIndex target = battle::kNoUnit;
    float damage = 0.0f;
    float duration = 0.0f;
    float age = 0.0f;
  };
  struct Effect {
    battle::ViewEffect view;
    float duration = 0.0f;
  };

  void AddFleets(int fleet, float dy);
  void AddShip(battle::Side side, int32_t typeIndex, battle::GroupId group, std::string name, std::string className,
               battle::Vec2 position, float radius);
  void AddSquadron(battle::Side side, int32_t typeIndex, battle::GroupId group, std::string name, battle::Vec2 position,
                   int craft);
  void AddGroup(battle::Side side, bool flight, std::string name);
  bool Active(const Unit& unit) const;
  battle::UnitIndex ChooseTarget(battle::UnitIndex self) const;
  void SteerAndMove(Unit& unit, battle::UnitIndex target);
  void Fire(battle::UnitIndex self, battle::UnitIndex target);
  void Damage(battle::UnitIndex unit, float amount, battle::Vec2 at);
  void UpdateTractors();
  void UpdateWithdrawals();
  void AddEffect(battle::ViewEffectKind kind, battle::Vec2 position, float size, float duration,
                 battle::UnitIndex unit = battle::kNoUnit);
  void Event(battle::Side side, std::string text, battle::UnitIndex unit = battle::kNoUnit);
  // Doctrine, regrouping, signals, deployment and the Death Star: applied at once, as the HUD expects even while
  // paused. nullopt leaves the command to Submit's movement and attack handling.
  std::optional<bool> SubmitHudOrder(const battle::Command& command);
  std::vector<battle::UnitIndex> CommandUnits(const battle::Command& command) const;
  battle::GroupId NewGroup(battle::Side side, bool flight, battle::GroupId copyFrom);
  float Random(); // [0, 1)
  float Seconds() const;

  std::vector<Unit> m_units;
  std::vector<battle::ViewGroup> m_groups;
  std::vector<Shot> m_shots;
  std::vector<Effect> m_effects;
  std::vector<battle::ViewEvent> m_events;
  std::vector<battle::Command> m_commands;
  std::array<float, 2> m_initialStrength{};
  std::array<bool, 2> m_ai{true, true};
  std::array<int, 2> m_signalsReady{2, 2};
  float m_superlaserCharge = -1.0f; // negative: no Death Star
  std::array<battle::ViewDeployZone, 2> m_zones{};
  uint64_t m_rng = 0;
  int64_t m_tick = 0;
};

} // namespace reb::battle_ui
