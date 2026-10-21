#pragma once

#include <cstdint>
#include <map>
#include <vector>

#include "ai/ai_player.h"

class Fleet;
class System;

// "Claude Admiral": a theatre commander layered over the classic staff. The staff keeps running the economy,
// research, diplomacy, missions and garrisons; the commander owns most of the warships and runs one concentrated
// strike group through gather -> strike -> siege cycles, picking targets by value, distance and a combat estimate
// of attack x durability. Sieges bombard defenses down and assault with embarked troops; the Empire also scouts
// for the hidden Alliance headquarters and uses a Death Star on it when it has one.
class ClaudeAiPlayer final : public AiPlayer
{
  public:
  ClaudeAiPlayer() = default;
  explicit ClaudeAiPlayer(int side)
      : AiPlayer(side)
  {
  }

  int GetKind() override
  {
    return kPlayerAiClaude;
  }
  void Read(istream* in) override;
  void Write(ostream* out) override;
  int StepPlayer() override;
  void BeginVote(int id, ObjectKey* key) override;

  protected:
  bool AcceptTaskOrder(OrderBase* order) override;

  public:
  struct Force {
    double attack = 0;
    double hull = 0;
    double Value() const
    {
      return attack * hull;
    }
    void Add(const Force& other)
    {
      attack += other.attack;
      hull += other.hull;
    }
  };

  struct FleetInfo {
    ObjectKey key;
    Fleet* fleet = nullptr;
    System* system = nullptr;
    ObjectKey systemKey;
    bool enroute = false;
    Force force;
    int ships = 0;
    int bombard = 0;
    int troops = 0;
    int troopRoom = 0;
    int hangarRoom = 0;
    bool deathStar = false;
  };

  struct SystemInfo {
    ObjectKey key;
    System* system = nullptr;
    int controller = 0;
    Force enemyFleets;
    double planetFighters = 0;
    int defenders = 0;
    int defenderDefense = 0;
    int batteries = 0;
    int shields = 0;
    int shieldStrength = 0;
    int productionFacilities = 0;
    int resourceFacilities = 0;
    int freeEnergy = 0;
    bool populated = false;
    bool explored = false;
    bool headquarters = false;
    bool ownFleetPresent = false;
    bool blockadedByUs = false;
  };

  enum Phase : int { kGather = 0, kStrike = 1, kSiege = 2 };

  private:
  void Command();
  void Survey();
  void RefreshClaims();
  void ChooseFlagship();
  void RunStrikeGroup();
  void RunGather(FleetInfo& flagship);
  void RunSiege(FleetInfo& flagship, SystemInfo& target);
  void RunScout();
  void HideHeadquarters();
  int DaysToEnemy(const SystemInfo& info) const;
  struct SideCount {
    double readyAttack = 0;
    double attack = 0;
    double hull = 0;
    int ships = 0;
    int squadrons = 0;
  };
  void CountForces(System* system, SideCount* own, SideCount* enemy);
  void ReportBattles();
  bool PickTarget(FleetInfo& flagship);
  double TargetValue(const SystemInfo& info) const;
  Force EnemyForceAt(const SystemInfo& info) const;
  bool IntelIsStale(const SystemInfo& info) const;
  Force PotentialForce() const;
  int TroopsNeeded(const SystemInfo& info) const;
  bool CanBreakShields(const SystemInfo& info, int bombard) const;
  void CallJoiners(const FleetInfo& flagship, const ObjectKey& rally);
  enum class Cargo { kTroops, kFighters };
  std::vector<ObjectKey> SpareCargo(const SystemInfo& info, Cargo cargo) const;
  int Embark(FleetInfo& flagship, Cargo cargo, int wanted, bool summon);
  ObjectKey NearestOwnSystem(const ObjectKey& from, bool needTroops) const;
  int TravelDaysBetween(const ObjectKey& from, const ObjectKey& to) const;
  bool IsObjective(const SystemInfo& info) const;
  bool IsOwnCapital(const SystemInfo& info) const;
  const SystemInfo* OwnCapital() const;
  bool IsClaimed(const ObjectKey& fleet) const;
  bool IsBenched(const ObjectKey& unit, int refusals) const;
  // True (and restarts the period) when `days` have passed since *lastDay: the commander does not run every day.
  bool Every(int* lastDay, int days) const;
  bool OrderTouchesClaimedFleet(OrderBase* order) const;
  bool Issue(int kind, std::vector<ObjectKey> team, const ObjectKey* target, const char* what);
  void Trace(const char* format, ...) const;
  int Enemy() const
  {
    return m_side == 1 ? 2 : 1;
  }
  FleetInfo* FindFleet(const ObjectKey& key);
  SystemInfo* FindSystem(const ObjectKey& key);
  void SetPhase(Phase phase);
  void AbandonTarget(int days);

  // Rebuilt by Survey() each planning day; not saved.
  std::vector<FleetInfo> m_fleets;
  std::map<uint32_t, SystemInfo> m_systems;
  std::vector<uint32_t> m_claimed;
  int m_ordersThisDay = 0;
  int m_surveyDay = -1;
  int m_strikeFailures = 0;
  int m_lastStatusDay = -100;
  struct BattleProbe {
    int day = 0;
    SideCount own;
    SideCount enemy;
  };
  std::map<uint32_t, BattleProbe> m_battles;
  struct Refusals {
    int count = 0;
    int day = 0;
  };
  std::map<uint32_t, Refusals> m_refusals;

  // Saved state.
  ObjectKey m_flagship;
  ObjectKey m_target;
  ObjectKey m_scout;
  ObjectKey m_scoutTarget;
  ObjectKey m_knownHeadquarters;
  int m_phase = kGather;
  int m_phaseDay = 0;
  int m_lastSiegeOrderDay = -100;
  int m_lastPlanDay = -1;
  int m_lastScoutLossDay = -100;
  int m_lastHeadquartersMoveDay = -1000;
  int m_lastHeadquartersCheckDay = -100;
  std::map<uint32_t, int> m_enemySeenDay;
  std::map<uint32_t, int> m_capturedDay;
  std::map<uint32_t, int> m_avoidUntil;
  std::map<uint32_t, int> m_visitedDay;
  int m_strongestEnemyAttack = 0;
  int m_strongestEnemyHull = 0;
};
