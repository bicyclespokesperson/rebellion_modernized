#include "rules/planet_assault.h"

#include "ai/ai_common.h"
#include "character/character.h"
#include "data/random.h"
#include "economy/defense_facility.h"
#include "economy/facility.h"
#include "fleet/capship.h"
#include "fleet/fleet.h"
#include "fleet/fleet_iterator.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "galaxy/system_rules_1e.h"
#include "mission/mission.h"
#include "mission/mission_rules.h"

namespace
{

constexpr uint32_t kUsableFilter = 1;
constexpr uint32_t kFleetEndOfAssaultFilter = 4;
constexpr int kDestroyedByAssault = 0xe;
constexpr int kCommanderNumber = 3;
constexpr uint32_t kAssaultFleetFlag = 0x80;

int both(int result, int ok)
{
  return result != 0 && ok != 0 ? 1 : 0;
}

int destroyTroop(GameObj* object, NotifyContext* ctx)
{
  return object->ChangeDestroyedReason(kDestroyedByAssault, ctx);
}

int commanderLeadership(GameObjNotifier* commander)
{
  return commander != nullptr ? static_cast<Character*>(commander)->GetLeadership() : 0;
}

} // namespace

// Record getters shared (ICF) between the unit classes; the garrison and attacking troops use them for their odds.

// ---- visitors and walker ----------------------------------------------------------------------------------------

TroopVisitor::TroopVisitor(AssaultWalker* walker)
    : m_walker(walker)
{
}
TroopVisitor::~TroopVisitor() = default;

AssaultWalker::AssaultWalker()
    : m_unused4(1)
    , m_system(nullptr)
    , m_attackerSide(0)
    , m_attackersAtStart(0)
    , m_defendersAtStart(0)
    , m_attackersLeft(0)
    , m_defendersLeft(0)
    , m_rounds(0)
{
}
AssaultWalker::~AssaultWalker() = default;

int AssaultWalker::ForEachAttackerTroop(TroopVisitor* visitor, NotifyContext* ctx)
{
  int ok = 1;
  uint32_t stop = 0;
  if (m_system == nullptr) {
    return ok;
  }
  FleetIterator fleets(m_system, kUsableFilter, m_attackerSide);
  fleets.Begin();
  while (fleets.Current() != nullptr && stop == 0) {
    auto* fleet = static_cast<Fleet*>(fleets.Current());
    fleets.Advance();
    if ((fleet->m_fleetFlags & kAssaultFleetFlag) != 0) {
      CapShipIterator ships(fleet, kUsableFilter, m_attackerSide);
      ships.Begin();
      while (ok != 0 && ships.Current() != nullptr && stop == 0) {
        GameObjNotifier* ship = ships.Current();
        ships.Advance();
        TroopRegIterator troops(ship, kUsableFilter, m_attackerSide);
        troops.Begin();
        while (ok != 0 && troops.Current() != nullptr && stop == 0) {
          GameObjNotifier* troop = troops.Current();
          troops.Advance();
          ok = visitor->Visit(static_cast<GameObj*>(troop), static_cast<GameObj*>(ship), fleet, &stop, ctx);
        }
      }
    }
    if (ok == 0) {
      break;
    }
  }
  return ok;
}

int AssaultWalker::ForEachDefenderTroop(TroopVisitor* visitor, NotifyContext* ctx)
{
  int ok = 1;
  uint32_t stop = 0;
  if (m_system == nullptr) {
    return ok;
  }
  TroopRegIterator troops(m_system, kUsableFilter, m_system->ControlSide());
  troops.Begin();
  while (troops.Current() != nullptr && stop == 0) {
    GameObjNotifier* troop = troops.Current();
    troops.Advance();
    ok = visitor->Visit(static_cast<GameObj*>(troop), nullptr, nullptr, &stop, ctx);
    if (ok == 0) {
      break;
    }
  }
  return ok;
}

TroopCounter::TroopCounter(AssaultWalker* walker)
    : TroopVisitor(walker)
    , m_count(0)
{
}
TroopCounter::~TroopCounter() = default;

int TroopCounter::Visit(GameObj*, GameObj*, GameObj*, uint32_t* stop, NotifyContext*)
{
  ++m_count;
  *stop = 0;
  return 1;
}

TroopPicker::TroopPicker(AssaultWalker* walker)
    : TroopVisitor(walker)
    , m_index(0)
    , m_target(0)
    , m_picked(nullptr)
{
}
TroopPicker::~TroopPicker() = default;

int TroopPicker::Visit(GameObj* troop, GameObj*, GameObj*, uint32_t* stop, NotifyContext*)
{
  *stop = 0;
  const int present = troop != nullptr;
  if (present) {
    *stop = m_index == m_target;
    if (*stop != 0) {
      m_picked = troop;
      return present;
    }
    ++m_index;
  }
  return present;
}

TroopReparenter::TroopReparenter(AssaultWalker* walker)
    : TroopVisitor(walker)
    , m_index(0)
    , m_target(0)
{
}
TroopReparenter::~TroopReparenter() = default;

int TroopReparenter::Visit(GameObj* troop, GameObj*, GameObj*, uint32_t* stop, NotifyContext* ctx)
{
  *stop = 0;
  const int present = troop != nullptr;
  if (present) {
    *stop = m_index == m_target;
    if (*stop == 0) {
      ++m_index;
      return troop->SetParent(m_walker->m_system, ctx);
    }
  }
  return present;
}

AssaultCombat::AssaultCombat(AssaultWalker* walker)
    : TroopVisitor(walker)
{
}
AssaultCombat::~AssaultCombat() = default;

int AssaultCombat::Visit(GameObj* troop, GameObj* ship, GameObj* fleet, uint32_t* stop, NotifyContext* ctx)
{
  *stop = 0;
  GameObj* defender = nullptr;
  int ok = troop != nullptr && ship != nullptr && fleet != nullptr;
  if (ok == 0) {
    return 0;
  }
  AssaultWalker* walker = m_walker;
  const int pick = GameRandom(walker->m_defendersAtStart - 1);
  if (pick < walker->m_defendersLeft) {
    TroopPicker picker(walker);
    picker.m_target = pick;
    ok = walker->ForEachDefenderTroop(&picker, ctx);
    if (ok != 0) {
      defender = picker.m_picked;
      ok = defender != nullptr;
    }
  }
  if (ok == 0) {
    return 0;
  }
  if (defender == nullptr) {
    return ok;
  }
  ++walker->m_rounds;
  int defenderLeadership = 0;
  int attackerLeadership = 0;
  GameObjNotifier* commander = nullptr;
  int found = walker->m_system->FindCharacterByCommandKind(kCommanderNumber, &commander);
  if (found != 0 && commander != nullptr) {
    defenderLeadership = commanderLeadership(commander);
  }
  if (found != 0) {
    commander = nullptr;
    found = static_cast<Fleet*>(fleet)->FindShipByNumber(kCommanderNumber, &commander);
    if (found != 0 && commander != nullptr) {
      attackerLeadership = commanderLeadership(commander);
    }
  }
  unsigned attackerLost = 0;
  unsigned defenderLost = 0;
  int result = 0;
  if (found != 0) {
    const int attackerRating = static_cast<TroopReg*>(troop)->GetAttackStrength();
    const int defenderRating = static_cast<TroopReg*>(defender)->GetDefenseStrength();
    result = ComputeCompareFlags0200(attackerRating, attackerLeadership, defenderRating, defenderLeadership,
                                     &attackerLost, &defenderLost);
  }
  if (result == 0) {
    return 0;
  }
  if (defenderLost == 0 && attackerLost == 0) {
    return result;
  }
  if (defenderLost != 0) {
    --walker->m_defendersLeft;
    result = destroyTroop(defender, ctx);
  } else {
    --walker->m_attackersLeft;
    result = destroyTroop(troop, ctx);
  }
  *stop = walker->m_defendersLeft == 0 || walker->m_attackersLeft == 0;
  return result;
}

// ---- the resolver -----------------------------------------------------------------------------------------------

AssaultResolver::AssaultResolver()
    : m_unused4(1)
    , m_walker()
{
}
AssaultResolver::~AssaultResolver() = default;

int AssaultResolver::Run(GameObjNotifier* system, ActivityRequest*, NotifyContext* ctx)
{
  int consistent = 1;
  uint32_t attackerSide = 0;
  FleetIterator fleets(system, 1);
  fleets.Begin();
  do {
    if (fleets.Current() == nullptr) {
      break;
    }
    if ((static_cast<Fleet*>(fleets.Current())->m_fleetFlags >> 7 & 1) != 0) {
      if (attackerSide == 0) {
        attackerSide = fleets.Current()->ControlSide();
      } else {
        consistent = attackerSide == fleets.Current()->ControlSide();
      }
    }
    fleets.Advance();
  } while (consistent != 0);
  m_walker.m_system = static_cast<System*>(system);
  m_walker.m_attackerSide = attackerSide;
  m_walker.m_defenderSide = system->ControlSide();
  return RunPhases(ctx);
}

int AssaultResolver::RunPhases(NotifyContext* ctx)
{
  if (m_walker.m_system != nullptr && m_walker.m_attackerSide != 0) {
    int ok = StartAssault(ctx) != 0;
    ok = both(DefensesFire(ctx), ok);
    ok = both(FightTroops(ctx), ok);
    ok = both(DamageSystem(ctx), ok);
    ok = both(Occupy(ctx), ok);
    ok = both(EndAssault(ctx), ok);
    if (ok != 0) {
      return 1;
    }
  }
  return 0;
}

int AssaultResolver::StartAssault(NotifyContext* ctx)
{
  int ok = m_walker.m_system->SetAssault(1, ctx) != 0;
  {
    TroopCounter attackers(&m_walker);
    ok = both(m_walker.ForEachAttackerTroop(&attackers, ctx), ok);
    m_walker.m_attackersAtStart = attackers.m_count;
    m_walker.m_attackersLeft = attackers.m_count;
  }
  TroopCounter defenders(&m_walker);
  ok = both(m_walker.ForEachDefenderTroop(&defenders, ctx), ok);
  m_walker.m_defendersAtStart = defenders.m_count;
  m_walker.m_defendersLeft = defenders.m_count;
  return ok;
}

int AssaultResolver::DefensesFire(NotifyContext* ctx)
{
  int ok = 1;
  uint32_t done = 0;
  DefenseFacilityIterator facilities(m_walker.m_system, 1);
  facilities.Begin();
  while (facilities.Current() != nullptr && done == 0) {
    GameObjNotifier* facility = facilities.Current();
    int hit = 0;
    ok = RollChanceFromGnprtb0200(static_cast<DefenseFacility*>(facility)->GetAttackStrength(), &hit);
    if (ok != 0 && hit != 0) {
      TroopPicker picker(&m_walker);
      picker.m_target = GameRandom(m_walker.m_attackersAtStart - 1);
      ok = m_walker.ForEachAttackerTroop(&picker, ctx);
      GameObj* victim = picker.m_picked;
      if (ok != 0 && victim != nullptr) {
        --m_walker.m_attackersLeft;
        done = m_walker.m_attackersLeft == 0;
        ok = destroyTroop(victim, ctx);
      }
    }
    facilities.Advance();
    if (ok == 0) {
      break;
    }
  }
  return ok;
}

int AssaultResolver::FightTroops(NotifyContext* ctx)
{
  int ok = 1;
  if (m_walker.m_defendersLeft != 0 && m_walker.m_attackersLeft != 0) {
    AssaultCombat combat(&m_walker);
    ok = m_walker.ForEachAttackerTroop(&combat, ctx);
  }
  return ok;
}

int AssaultResolver::DamageSystem(NotifyContext* ctx)
{
  int hits = 0;
  int ok = CountSuccesses0205(m_walker.m_rounds, &hits) != 0;
  FacilityIterator facilities(m_walker.m_system, 1);
  AllFacilityIterator allFacilities(m_walker.m_system, 1);
  ObjectKey firstKey;
  allFacilities.Begin();
  if (allFacilities.Current() != nullptr) {
    ObjectKey key;
    firstKey = *GetObjectKey(allFacilities.Current(), &key);
  }
  int done = 0;
  for (int index = 0; ok != 0 && index < hits && done == 0; ++index) {
    const int facilityCount = facilities.Count();
    const int allFacilityCount = allFacilities.Count();
    int facilityHit = 0;
    int pick = 0;
    int energyHit = 0;
    int rawHit = 0;
    ok = RollIdentifyOutcome(facilityCount, allFacilityCount, m_walker.m_system->m_energy,
                             m_walker.m_system->m_rawMaterial, &facilityHit, &pick, &energyHit, &rawHit);
    if (ok == 0) {
      continue;
    }
    if (energyHit != 0) {
      ok = m_walker.m_system->SetEnergy(m_walker.m_system->m_energy - 1, ctx);
    } else if (rawHit != 0) {
      ok = m_walker.m_system->SetRawMaterial(m_walker.m_system->m_rawMaterial - 1, ctx);
    } else if (facilityHit != 0) {
      GameObjNotifier* target = nullptr;
      int seen = 0;
      for (facilities.Begin(); facilities.Current() != nullptr && target == nullptr; facilities.Advance()) {
        ObjectKey key;
        const uint32_t candidate = GetObjectKey(facilities.Current(), &key)->m_value;
        if (firstKey.m_value == candidate) {
          continue;
        }
        if (seen == pick) {
          target = facilities.Current();
        } else {
          ++seen;
        }
      }
      ok = target != nullptr;
      if (ok != 0) {
        ok = static_cast<GameObj*>(target)->ChangeDestroyedReason(kDestroyedByAssault, ctx);
      }
    } else {
      done = 1;
    }
  }
  return ok;
}

int AssaultResolver::Occupy(NotifyContext* ctx)
{
  int ok = 1;
  if (m_walker.m_attackersLeft != 0 && m_walker.m_defendersLeft == 0) {
    const int garrison = ComputeSupportCost(m_walker.m_attackerSide, 0, 0, 0, 0);
    TroopReparenter mover(&m_walker);
    mover.m_target = garrison;
    ok = m_walker.ForEachAttackerTroop(&mover, ctx);
  }
  return ok;
}

int AssaultResolver::EndAssault(NotifyContext* ctx)
{
  const int ok = m_walker.m_system->SetAssault(0, ctx) != 0;
  FleetIterator fleets(m_walker.m_system, kFleetEndOfAssaultFilter, m_walker.m_attackerSide);
  for (fleets.Begin(); fleets.Current() != nullptr; fleets.Advance()) {
    static_cast<Fleet*>(fleets.Current())->SetAssault(0, ctx);
  }
  return ok;
}
