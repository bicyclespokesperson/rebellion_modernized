#include "ai/ai_class_records.h"
#include "ai/ai_records.h"
#include "ai/ai_side_controller.h"
#include "ai/ai_system_record.h"
#include "ai/ai_task_family.h"
#include "ai/ai_task_types.h"
#include "ai/ai_task_util.h"
#include "core/checked_new.h"
#include "fleet/troop_reg.h"
#include "galaxy/system.h"
#include "gameobj/object_lookup.h"

using namespace aitask;

namespace
{

constexpr FamilyParams kParams = {0,
                                  0,
                                  0x14,
                                  0xd4,
                                  0xd4,
                                  0x5c,
                                  0x40,
                                  0x13,
                                  0x400003,
                                  5,
                                  0x400000,
                                  0xbc,
                                  0x8000,
                                  0x20,
                                  0xc,
                                  10,
                                  FacilityKind::kManufacturing28,
                                  0x200000,
                                  FacilityKind::kManufacturing2A,
                                  0x3800000,
                                  false,
                                  true,
                                  false,
                                  false};

constexpr int kScrapOrder = 0x200;
constexpr int kMoveOrder = 0x201;
constexpr int kStageTargets = 0x3ec;
constexpr int kStageMoves = 0x3ef;
constexpr int kStageClasses = 0x3f0;
constexpr uint32_t kSystemClaimed = 0x400000;
constexpr int kUnreachableCost = 100000;

int TroopStrength(GameObjNotifier* troop)
{
  return static_cast<TroopReg*>(troop)->GetDefenseStrength();
}

struct TroopMoveParams {
  bool byRecord;          // the count comes from the system record (A, B), else from the system's figures (C, D)
  uint32_t ownerFlag;     // A, B: the side flag that enables the order
  uint32_t sectorX3c;     // A, B
  uint32_t systemRequire; // A, B: required system flags; C, D: required sector flags of the first pick
  uint32_t unitSector;    // C, D: sector flags
  uint32_t unitSystem;    // C, D: system flags
  uint32_t nearRequire;   // the sector near the unit: required third mask
  uint32_t supplyRequire; // the supply system in it: required third mask
  bool fallback;
  uint32_t fallbackSector;
  uint32_t fallbackSystem;
  uint32_t classBit; // regiments of classes without this flag are skipped (0 for none)
};

constexpr TroopMoveParams kMoveA
    = {true, 0x20, 0x2000000, 0x20001, 0, 0, 0x1000, 0x8001, true, 0x280000, 0x18000, 0x2000};
constexpr TroopMoveParams kMoveB
    = {true, 0x10, 0x1000000, 0x10001, 0, 0, 0x800, 0x4001, true, 0x180000, 0x10010000, 0x1000};
constexpr TroopMoveParams kMoveC = {false, 0, 0, 0, 0x40010, 0x2001, 0, 1, false, 0, 0, 0};
constexpr TroopMoveParams kMoveD
    = {false, 0, 0, 0, 0x40000, 0x2000, 0x1000, 0x8001, true, 0x280000, 0x20010000, 0x2000};

OrderBase* BuildTroopMove(AiTask6* task, const TroopMoveParams& p)
{
  ObjectKeyValueSet team;
  OrderBase* order = nullptr;
  ObjectKey scratch;
  int count = 0;
  AiSideController* owner = task->m_owner;
  AiRankedSelector& selector = task->m_selector;
  task->m_manager.Reset();
  task->m_unit.Reset();

  auto pickSupplySystem = [&]() {
    selector.Add(owner->RankSectorsByDistance(&task->m_unit, 0x80000, 0, p.nearRequire, 0, 0, 0, 1));
    task->m_manager = *selector.PickBest(&scratch);
    selector.Reset();
    selector.Add(owner->RankSystemsOfSector(&task->m_manager, 0x10000, 0, p.supplyRequire, 2, 0, 0x30000, 0x13, 2));
    task->m_manager = *selector.PickBest(&scratch);
    selector.Reset();
    if (p.fallback && !IsSystemKey(task->m_manager)) {
      selector.Add(owner->RankSectorsByDistance(&task->m_unit, p.fallbackSector, 0, 0, 0, 0, 0, 1));
      task->m_manager = *selector.PickBest(&scratch);
      selector.Reset();
      selector.Add(owner->RankSystemsOfSector(&task->m_manager, p.fallbackSystem, 0, 1, 2, 0, 0x30000, 0x13, 2));
      task->m_manager = *selector.PickBest(&scratch);
      selector.Reset();
    }
  };
  auto collect = [&]() {
    if (!IsSystemKey(task->m_manager)) {
      return;
    }
    AiSystemEval* eval = owner->m_systems.FindEval(task->m_manager);
    GameObjNotifier* system = GetObjectInFamilies_90_98(task->m_view, &task->m_manager);
    if (!system || !eval) {
      return;
    }
    bool done = false;
    if (eval->Stat(0x94) < count) {
      count = eval->Stat(0x94);
    }
    TroopRegIterator troops(system, 1);
    do {
      if (count < 1) {
        break;
      }
      GameObjNotifier* best = nullptr;
      int cheapest = 1000000;
      for (troops.Begin(); troops.Current(); troops.Advance()) {
        GameObjNotifier* troop = troops.Current();
        bool eligible = true;
        if (p.classBit != 0) {
          ObjectKey recordKey;
          GetObjectRecordKey(troop, &recordKey);
          eligible = (owner->m_classes.Find(&recordKey)->m_flags.m_value & p.classBit) != 0;
        }
        if (eligible && TroopStrength(troop) < cheapest) {
          ObjectKey key;
          GetObjectKey(troop, &key);
          if (team.Find(&key) == nullptr) {
            cheapest = TroopStrength(troop);
            best = troop;
          }
        }
      }
      if (!best) {
        done = true;
      } else {
        ObjectKey key;
        GetObjectKey(best, &key);
        auto* node = NewOrNull<ObjectKeyValueNode>(key, 0);
        if (node) {
          team.Add(node);
        }
        --count;
      }
    } while (!done);
  };

  if (p.byRecord) {
    if ((owner->m_flags.Get() & p.ownerFlag) != 0) {
      selector.Add(owner->RankSectorsByX3c(0, 0, p.sectorX3c, 0, 0, 0, 2));
      task->m_unit = *selector.PickBest(&scratch);
      selector.Reset();
      selector.Add(owner->RankSystemsOfSector(&task->m_unit, 0, 0, p.systemRequire, 2, 0, 0, 0x10, 2));
      task->m_unit = *selector.PickBest(&scratch);
      selector.Reset();
      if (IsSystemKey(task->m_unit)) {
        AiSystemRecord* record = owner->m_systemRecords.FindByTarget(&task->m_unit);
        if (record) {
          count = record->ForwardToPosture();
        }
      }
      if (0 < count) {
        pickSupplySystem();
      }
      collect();
    }
  } else {
    selector.Add(owner->RankSectors(p.unitSector, 0, 0, 0, 0, 0, 0x1e, 2));
    task->m_unit = *selector.PickBest(&scratch);
    selector.Reset();
    selector.Add(owner->RankSystemsOfSector(&task->m_unit, p.unitSystem, 0, 1, 2, 0, 0, 0x15, 2));
    task->m_unit = *selector.PickBest(&scratch);
    selector.Reset();
    if (IsSystemKey(task->m_unit)) {
      AiSystemEval* eval = owner->m_systems.FindEval(task->m_unit);
      if (eval) {
        count = eval->Stat(0x9c);
      }
      pickSupplySystem();
      collect();
    }
  }
  if (team.First()) {
    order = static_cast<OrderBase*>(CreateMessageByKind(kMoveOrder));
    if (order) {
      order->m_side = task->m_view;
      order->SetTeam(&team);
      order->SetTarget(&task->m_unit);
    }
  }
  return order;
}

} // namespace

AiTask6::AiTask6(int view)
    : AiStagedTask(6, view)
    , m_selector()
    , m_manager()
    , m_unit()
    , m_system()
    , m_classKey()
    , m_targets()
    , m_dropped()
{
  m_classKey.m_value = 0;
  m_stage = 0;
  m_step = 0;
  m_count = 0;
  m_cap = 0;
  m_limit = 0;
  m_round = 1;
  m_classFlags = 0;
  m_classMode = 0;
  m_classOrder = 0;
  m_targets.Clear();
  m_dropped.Clear();
  m_x24 = 0x16;
}

AiTask6::~AiTask6() = default;

FamilyFields AiTask6::Fields()
{
  return {m_owner,   m_view,     m_x24,       &m_selector, &m_targets, &m_dropped,
          &m_system, &m_manager, &m_classKey, &m_count,    &m_cap,     &m_limit};
}

void AiTask6::Read(istream* in)
{
  AiTaskBase::Read(in);
  ReadValue32(in, &m_stage);
  ReadValue32(in, &m_step);
  ReadValue32(in, &m_count);
  ReadValue32(in, &m_cap);
  ReadValue32(in, &m_limit);
  ReadValue32(in, &m_round);
  ReadValue32(in, &m_classFlags);
  ReadValue32(in, &m_classMode);
  ReadValue32(in, &m_classOrder);
  m_selector.Read(in);
  m_manager.Read(in);
  m_unit.Read(in);
  m_system.Read(in);
  m_targets.Load(in);
  m_dropped.Load(in);
}

void AiTask6::Write(ostream* out)
{
  AiTaskBase::Write(out);
  WriteValue32(out, &m_stage);
  WriteValue32(out, &m_step);
  WriteValue32(out, &m_count);
  WriteValue32(out, &m_cap);
  WriteValue32(out, &m_limit);
  WriteValue32(out, &m_round);
  WriteValue32(out, &m_classFlags);
  WriteValue32(out, &m_classMode);
  WriteValue32(out, &m_classOrder);
  m_selector.Write(out);
  m_manager.Write(out);
  m_unit.Write(out);
  m_system.Write(out);
  m_targets.Save(out);
  m_dropped.Save(out);
}

OrderBase* AiTask6::Step()
{
  if (m_flags.Get() != 1) {
    m_stage = 0;
    m_step = 0;
    m_finished = 1;
    return nullptr;
  }
  if (m_stage == kStageTargets) {
    return StepTargets();
  }
  if (m_stage == kStageMoves) {
    return StepMoves();
  }
  if (m_stage != kStageClasses) {
    m_stage = kStageMoves;
    m_step = 0;
    return nullptr;
  }
  return StepClasses();
}

OrderBase* AiTask6::StepMoves()
{
  OrderBase* order = nullptr;
  switch (m_step) {
    case 1:
      order = BuildTroopMoveOrderA();
      if (order == nullptr) {
        m_step = 2;
        return nullptr;
      }
      break;
    case 2:
      order = BuildTroopMoveOrderB();
      if (order == nullptr) {
        m_step = 3;
        return nullptr;
      }
      break;
    case 3:
      order = BuildTroopMoveOrderC();
      if (order == nullptr) {
        m_step = 4;
        return nullptr;
      }
      break;
    case 4:
      order = BuildTroopMoveOrderD();
      break;
    default:
      m_step = 1;
      return nullptr;
  }
  m_step = 0;
  m_stage = kStageClasses;
  m_finished = 1;
  return order;
}

OrderBase* AiTask6::StepClasses()
{
  OrderBase* order = nullptr;
  switch (m_step) {
    case 5:
      m_step = ChooseManager() != 0 ? 6 : 0xb;
      return nullptr;
    case 6:
      m_step = ChooseKind() != 0 ? 7 : 0xb;
      return nullptr;
    case 7:
      m_step = ComputeClassCount();
      if (m_step != 0) {
        return nullptr;
      }
      m_stage = kStageTargets;
      m_finished = 1;
      return nullptr;
    case 8:
      order = BuildTroopScrapOrder();
      break;
    default:
      m_step = 5;
      return nullptr;
    case 0xb:
      if (ResetCount() == 0) {
        m_step = 0;
        m_stage = kStageTargets;
        return nullptr;
      }
      m_step = 8;
      return nullptr;
    case 0xc:
      {
        OrderBase* destination = BuildFleetDestinationOrder(&m_unit);
        if (destination) {
          m_step = 0xd;
          m_finished = 1;
          ++m_priority;
          return destination;
        }
        break;
      }
    case 0xd:
      order = BuildManufactureOrder212();
      break;
  }
  m_step = 0;
  m_stage = kStageTargets;
  m_finished = 1;
  return order;
}

OrderBase* AiTask6::StepTargets()
{
  OrderBase* order = nullptr;
  switch (m_step) {
    case 5:
      m_owner->m_bitsA &= ~0x20u;
      if (m_owner->m_bitB == 0x20 && PickSupplySystem() != 0) {
        m_owner->AdvanceBitCursor();
        m_step = 9;
        return nullptr;
      }
      m_step = 0xe;
      return nullptr;
    case 6:
      if (SelectTarget() != 0) {
        m_step = 5;
        return nullptr;
      }
      break;
    case 7:
      m_step = ComputeFacilityCount();
      if (m_step != 0) {
        return nullptr;
      }
      m_stage = kStageMoves;
      m_finished = 1;
      return nullptr;
    default:
      m_step = 6;
      return nullptr;
    case 9:
      order = BuildScrapOrder();
      if (order) {
        m_finished = 1;
        ++m_priority;
      }
      m_step = 7;
      return order;
    case 10:
      order = BuildCapitalOrder();
      break;
    case 0xc:
      {
        OrderBase* destination = BuildDestinationOrder(&m_system);
        if (destination) {
          m_step = 0xd;
          m_finished = 1;
          ++m_priority;
          return destination;
        }
        break;
      }
    case 0xd:
      order = BuildManufactureOrder210();
      break;
    case 0xe:
      AddTargetSystem();
      RefreshTargets();
      break;
  }
  m_step = 0;
  m_stage = kStageMoves;
  m_finished = 1;
  return order;
}

OrderBase* AiTask6::BuildTroopMoveOrderA()
{
  return BuildTroopMove(this, kMoveA);
}
OrderBase* AiTask6::BuildTroopMoveOrderB()
{
  return BuildTroopMove(this, kMoveB);
}
OrderBase* AiTask6::BuildTroopMoveOrderC()
{
  return BuildTroopMove(this, kMoveC);
}
OrderBase* AiTask6::BuildTroopMoveOrderD()
{
  return BuildTroopMove(this, kMoveD);
}

int AiTask6::ChooseManager()
{
  int result = 0;
  bool found = false;
  m_cap = 0;
  m_owner->m_key14.Reset();
  auto position = m_targets.Entries().begin();
  while (position != m_targets.Entries().end() && !found) {
    auto* node = static_cast<ObjectKeyNode*>((*position).get());
    ++position;
    m_manager = KeyOfNode(node);
    if (!IsSystemKey(m_manager)) {
      ObjectKey key = KeyOfNode(node);
      m_targets.RemoveKey(&key);
      continue;
    }
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
    const bool stale = !eval || (eval->m_flags30.m_value & 1) == 0 || (eval->m_flags28.m_value & kSystemClaimed) == 0;
    if (stale) {
      if (eval) {
        eval->m_flags28.m_value &= ~kSystemClaimed;
        eval->MarkChanged();
      }
      ObjectKey key = KeyOfNode(node);
      m_targets.RemoveKey(&key);
      continue;
    }
    if (0 < eval->Stat(0x5c) && (eval->m_flags28.m_value & 0x400) != 0 && (eval->m_flags28.m_value & 3) == 0) {
      m_cap = eval->Stat(0x5c);
      m_owner->m_key14 = m_manager;
      found = true;
    }
  }
  if (!found) {
    m_manager = m_owner->m_key18;
    if (IsSystemKey(m_manager)) {
      AiSystemEval* eval = m_owner->m_systems.FindEval(m_manager);
      if (eval && (eval->m_flags30.m_value & 1) != 0 && (eval->m_flags28.m_value & kSystemClaimed) != 0
          && 0 < eval->Stat(0x5c) && (eval->m_flags28.m_value & 0x400) != 0 && (eval->m_flags28.m_value & 3) == 0) {
        m_cap = eval->Stat(0x5c);
        found = true;
      }
    }
    if (!found) {
      m_manager.Reset();
      return 0;
    }
  }
  auto* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, &m_manager));
  if (system) {
    GameObjNotifier* manager = system->FindManufacturingManagerByKind(2);
    if (manager) {
      ObjectKey key;
      GetObjectKey(manager, &key);
      m_manager = key;
      result = 1;
    }
  }
  return result;
}

int AiTask6::ChooseKind()
{
  m_count = 0;
  m_classFlags = 0;
  m_classMode = 0;
  m_classOrder = 0;
  m_flags28.m_value &= 0xfffffff0;
  using Pick = void (AiTask6::*)();
  static const Pick kRounds[4][5] = {
      {&AiTask6::PickBySectorDistanceA, &AiTask6::PickByFleetDistance, &AiTask6::PickBySectorDistanceB,
       &AiTask6::PickByParentSystem, &AiTask6::PickBySectorRank},
      {&AiTask6::PickByFleetDistance, &AiTask6::PickBySectorDistanceB, &AiTask6::PickBySectorDistanceB,
       &AiTask6::PickByParentSystem, &AiTask6::PickBySectorRank},
      {&AiTask6::PickByParentSystem, &AiTask6::PickBySectorRank, &AiTask6::PickBySectorDistanceA,
       &AiTask6::PickByFleetDistance, &AiTask6::PickBySectorDistanceB},
      {&AiTask6::PickBySectorRank, &AiTask6::PickBySectorDistanceA, &AiTask6::PickByFleetDistance,
       &AiTask6::PickBySectorDistanceB, &AiTask6::PickByParentSystem},
  };
  static const Pick kRound2Tail = &AiTask6::PickBySectorDistanceA;
  const int round = m_round;
  if (round >= 1 && round <= 4) {
    const Pick* sequence = kRounds[round - 1];
    (this->*sequence[0])();
    for (int index = 1; index < 5; ++index) {
      if (!IsSystemKey(m_unit)) {
        (this->*sequence[index])();
      }
    }
    if (round == 2 && !IsSystemKey(m_unit)) {
      (this->*kRound2Tail)();
    }
    m_round = round == 4 ? 1 : round + 1;
  } else {
    m_round = 1;
  }
  int result = 0;
  if (IsSystemKey(m_unit) && m_owner->m_systems.FindEval(m_unit)) {
    result = 1;
  }
  if ((m_flags28.m_value & 0xf) == 0xf && (m_owner->m_flags.Get() & 0x800000) != 0 && 0 < Need()) {
    if (m_owner->m_budget[8] < 0xc) {
      m_owner->RaiseBudget(8);
    }
    if (m_owner->m_budget[7] < 6) {
      m_owner->RaiseBudget(7);
    }
    if (m_owner->m_budget[6] < 4) {
      m_owner->RaiseBudget(6);
    }
  }
  return result;
}

int AiTask6::ComputeClassCount()
{
  int result = 0;
  const int need = Need();
  if (need < 1) {
    if (need < 0) {
      result = 8;
    }
  } else {
    const int classes = m_owner->ChooseClass(&m_classKey, 0x29, m_classFlags, m_classMode, m_classOrder);
    if ((m_classKey.m_value & 0xff000000) != 0 && classes != 0) {
      const int total = classes * m_count;
      if (total - need != 0 && need <= total) {
        m_count = need / classes;
      }
    }
    if (0 < m_count) {
      if (m_cap < m_count) {
        m_count = m_cap;
      }
      m_owner->m_key14.Reset();
      result = 0xc;
    }
  }
  if ((m_owner->m_flags.Get() & 0x400000) != 0 && need < 1) {
    if (9 < m_owner->m_budget[8]) {
      m_owner->LowerBudget(8);
    }
    if (3 < m_owner->m_budget[7]) {
      m_owner->LowerBudget(7);
    }
    if (0 < m_owner->m_budget[6]) {
      m_owner->LowerBudget(6);
    }
  }
  return result;
}

int AiTask6::ResetCount()
{
  m_count = 0;
  Need();
  return 0;
}

OrderBase* AiTask6::BuildTroopScrapOrder()
{
  ObjectKeyValueSet team;
  OrderBase* order = nullptr;
  ObjectKey scratch;
  m_selector.Add(m_owner->RankSectors(0x80000, 0, 0, 0, 0, 0, 0x1f, 2));
  m_unit = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0x10000, 0, 1, 0, 0, 0, 0x13, 2));
  m_unit = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  GameObjNotifier* system = IsSystemKey(m_unit) ? GetObjectInFamilies_90_98(m_view, &m_unit) : nullptr;
  if (system) {
    int cheapest = kUnreachableCost;
    GameObjNotifier* best = nullptr;
    TroopRegIterator troops(system, 1);
    for (troops.Begin(); troops.Current(); troops.Advance()) {
      if (TroopStrength(troops.Current()) < cheapest) {
        cheapest = TroopStrength(troops.Current());
        best = troops.Current();
      }
    }
    if (best) {
      ObjectKey key;
      GetObjectKey(best, &key);
      auto* node = NewOrNull<ObjectKeyValueNode>(key, 0);
      if (node) {
        team.Add(node);
      }
    }
  }
  if (team.First()) {
    order = NewTeamOrder(kScrapOrder, m_view, &team);
  }
  return order;
}

void AiTask6::PickByFleetDistance()
{
  ObjectKey scratch;
  m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x400000, 0, 0, 0, 0, 0, 2));
  m_unit = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0x20000, 0, 1, 3, 0, 0, 0x15, 2));
  m_unit = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
  if (!eval) {
    m_flags28.m_value |= 8;
    return;
  }
  m_count = eval->Stat(0x9c);
  m_classFlags = 0x2000;
  if ((eval->m_flags28.m_value & 0x83e00000) != 0) {
    m_classMode = 0x2000;
    m_classOrder = 1;
  } else {
    m_classMode = 1;
    m_classOrder = 2;
  }
}

void AiTask6::PickBySectorDistanceA()
{
  ObjectKey scratch;
  m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x800000, 0, 0, 0, 0, 0, 2));
  m_unit = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0x2000, 0, 1, 0x4000003, 0, 0, 0x15, 2));
  m_unit = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
  if (eval) {
    m_count = eval->Stat(0x9c);
    m_classOrder = 1;
    m_classFlags = 0x2000;
    m_classMode = 0x2000;
    return;
  }
  m_flags28.m_value |= 1;
}

void AiTask6::PickBySectorRank()
{
  ObjectKey scratch;
  m_selector.Add(m_owner->RankSectors(0x40000000, 0, 0, 0, 0, 0, 0x1e, 2));
  m_unit = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0x2000, 0, 1, 3, 0, 0, 0x15, 2));
  m_unit = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
  if (eval) {
    m_count = eval->Stat(0x9c);
    m_classFlags = 0x2000;
    m_classMode = 0x2000;
    m_classOrder = 1;
    return;
  }
  m_flags28.m_value |= 2;
}

void AiTask6::PickByParentSystem()
{
  GameObjNotifier* object = GetObjectInFamilies_01_ff(m_view, &m_manager);
  ObjectKey key;
  for (; object; object = object->m_parent) {
    GetObjectKey(object, &key);
    if (IsSystemKey(key)) {
      break;
    }
  }
  GetObjectKey(object, &key);
  if (IsSystemKey(key)) {
    ObjectKey system;
    GetObjectKey(object, &system);
    m_unit = system;
    m_count = m_cap;
    m_classFlags = 0x1000;
    m_classMode = 0x1000;
    m_classOrder = 1;
  }
}

void AiTask6::PickBySectorDistanceB()
{
  ObjectKey scratch;
  m_selector.Add(m_owner->RankSectorsByDistance(&m_manager, 0x40000, 0, 0, 0, 0, 0, 2));
  m_unit = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_owner->RankSystemsOfSector(&m_unit, 0x2000, 0, 1, 3, 0, 0, 0x15, 2));
  m_unit = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  if (IsSystemKey(m_unit)) {
    AiSystemEval* eval = m_owner->m_systems.FindEval(m_unit);
    if (eval) {
      m_classFlags = 0x2000;
      m_count = eval->Stat(0x9c);
      m_classMode = 1;
      m_classOrder = 2;
    }
  } else {
    m_flags28.m_value |= 4;
  }
}

int AiTask6::SelectTarget()
{
  return aitask::SelectTarget(Fields(), kParams);
}

int AiTask6::PickSupplySystem()
{
  return aitask::PickSupplySystemNear(Fields(), kParams);
}

OrderBase* AiTask6::BuildScrapOrder()
{
  return aitask::BuildScrapOrder(Fields(), kParams);
}

int AiTask6::ComputeFacilityCount()
{
  return aitask::ComputeCount(Fields(), kParams);
}

OrderBase* AiTask6::BuildCapitalOrder()
{
  return aitask::BuildCapitalOrder(Fields(), kParams, FacilityKind::kManufacturing29);
}

OrderBase* AiTask6::BuildFleetDestinationOrder(ObjectKey* target)
{
  return aitask::BuildDestinationOrder(Fields(), target, 0xa4);
}

OrderBase* AiTask6::BuildManufactureOrder212()
{
  return aitask::BuildManufactureOrder(Fields(), 0x212, 0xa4);
}

OrderBase* AiTask6::BuildDestinationOrder(ObjectKey* target)
{
  return aitask::BuildDestinationOrder(Fields(), target, 0xa0);
}

OrderBase* AiTask6::BuildManufactureOrder210()
{
  return aitask::BuildManufactureOrder(Fields(), 0x210, 0xa0);
}

void AiTask6::AddTargetSystem()
{
  aitask::AddTargetSystem(Fields(), kParams);
}

int AiTask6::Need()
{
  AiSideController* owner = m_owner;
  int need = ((((m_x24 * owner->Stat(0x184)) / 100) * 0x50) / 100 - owner->Stat(0x250)) - owner->Stat(0x1d8);
  if (owner->Stat(0x184) - owner->Stat(0x188) < need) {
    need = 0;
  }
  return need;
}

int AiTask6::NeedForTargets()
{
  return aitask::NeedForTargets(Fields(), kParams);
}

void AiTask6::RefreshTargets()
{
  aitask::RefreshTargets(Fields(), kParams);
}
