#include "battle_ui/auto_pause.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace reb::battle_ui
{
namespace
{

using battle::UnitIndex;

constexpr std::array kHullThresholds{0.25f, 0.5f, 0.75f};
constexpr float kShieldsGone = 0.0001f;

// The hull or shield fraction below which the group's doctrine withdraws a ship; negative when it never does on its
// own. `shields` tells which of the two the rule watches.
float WithdrawThreshold(battle::WithdrawRule rule, bool& shields)
{
  shields = false;
  switch (rule) {
    case battle::WithdrawRule::HullBelow25:
      return 0.25f;
    case battle::WithdrawRule::HullBelow50:
      return 0.5f;
    case battle::WithdrawRule::HullBelow75:
      return 0.75f;
    case battle::WithdrawRule::ShieldsCollapse:
      shields = true;
      return kShieldsGone;
    case battle::WithdrawRule::Never:
    case battle::WithdrawRule::OnOrderOnly:
      break;
  }
  return -1.0f;
}

} // namespace

std::vector<AutoPauseHit> AutoPauseWatcher::Update(const battle::ViewModel& view, battle::Side player,
                                                   const UnitFactsLookup& facts, const WithdrawRuleLookup& withdrawRule)
{
  std::vector<AutoPauseHit> hits;
  const bool started = m_started;
  m_started = true;
  std::vector<Seen> seen(m_seen.size());
  for (size_t i = 0; i < seen.size(); ++i) {
    seen[i].everSeen = m_seen[i].everSeen;
  }
  int reinforcements = 0;
  UnitIndex firstReinforcement = battle::kNoUnit;
  for (const battle::ViewUnit& unit : view.units) {
    if (unit.unit < 0) {
      continue;
    }
    if (static_cast<size_t>(unit.unit) >= seen.size()) {
      seen.resize(static_cast<size_t>(unit.unit) + 1);
    }
    const Seen before = static_cast<size_t>(unit.unit) < m_seen.size() ? m_seen[unit.unit] : Seen{};
    Seen& now = seen[unit.unit];
    now.present = true;
    now.everSeen = true;
    now.gone = (unit.flags & battle::kViewDestroyed) != 0;
    now.hull = unit.hullFraction;
    now.shield = unit.shieldFraction;
    if (!started) {
      continue;
    }
    const bool own = unit.side == player;
    if (!own && !before.everSeen && !now.gone && view.tick > 0) {
      ++reinforcements;
      firstReinforcement = firstReinforcement == battle::kNoUnit ? unit.unit : firstReinforcement;
    }
    if (!own || !before.present || before.gone) {
      continue;
    }
    if (now.gone && facts(unit.unit).carrier) {
      hits.push_back({AutoPauseEvent::CarrierLost, unit.unit, "Carrier " + facts(unit.unit).name + " lost"});
      continue;
    }
    if (now.gone || unit.kind != battle::ViewUnitKind::CapitalShip) {
      continue;
    }
    const bool hullCrossed = std::ranges::any_of(kHullThresholds, [&](float t) {
      return before.hull >= t && now.hull < t;
    });
    const bool shieldsCollapsed = before.shield >= kShieldsGone && now.shield < kShieldsGone;
    if (!hullCrossed && !shieldsCollapsed) {
      continue;
    }
    bool shields = false;
    const float threshold = WithdrawThreshold(withdrawRule(unit.unit), shields);
    const float before01 = shields ? before.shield : before.hull;
    const float now01 = shields ? now.shield : now.hull;
    if (threshold > 0.0f && before01 >= threshold && now01 < threshold) {
      hits.push_back({AutoPauseEvent::BelowWithdrawThreshold, unit.unit,
                      facts(unit.unit).name + " is below its withdrawal threshold"});
    }
  }
  if (reinforcements > 0) {
    hits.push_back({AutoPauseEvent::Reinforcements, firstReinforcement,
                    reinforcements == 1 ? "Enemy reinforcements: " + facts(firstReinforcement).name
                                        : "Enemy reinforcements: " + std::to_string(reinforcements) + " units"});
  }

  std::vector<UnitIndex> wells;
  for (const battle::ViewWell& well : view.wells) {
    if (!well.active || well.side == player) {
      continue;
    }
    wells.push_back(well.projector);
    if (started && std::ranges::find(m_activeWells, well.projector) == m_activeWells.end()) {
      hits.push_back(
          {AutoPauseEvent::GravityWell, well.projector, "Gravity well active: " + facts(well.projector).name});
    }
  }
  m_activeWells = std::move(wells);

  if (started && m_superlaser >= 0.0f && m_superlaser < 1.0f && view.superlaserCharge >= 1.0f) {
    hits.push_back({AutoPauseEvent::SuperlaserCharged, battle::kNoUnit, "Death Star superlaser charged"});
  }
  m_superlaser = view.superlaserCharge;
  m_seen = std::move(seen);
  return hits;
}

bool AutoPauseWatcher::PhaseDue(float battleSeconds, int intervalSeconds)
{
  if (intervalSeconds <= 0) {
    m_phaseInterval = 0;
    return false;
  }
  const float interval = static_cast<float>(intervalSeconds);
  if (intervalSeconds != m_phaseInterval) {
    m_phaseInterval = intervalSeconds;
    m_nextPhase = (std::floor(battleSeconds / interval) + 1.0f) * interval;
    return false;
  }
  if (battleSeconds + 1e-3f < m_nextPhase) {
    return false;
  }
  m_nextPhase = (std::floor(battleSeconds / interval + 1e-4f) + 1.0f) * interval;
  return true;
}

} // namespace reb::battle_ui
