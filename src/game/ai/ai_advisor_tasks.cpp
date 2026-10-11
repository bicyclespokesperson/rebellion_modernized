#include "ai/ai_advisor_tasks.h"

#include "ai/ai_common.h"
#include "core/checked_new.h"
#include "core/iostream_lib.h"
#include "core/stream_io.h"
#include "economy/side.h"
#include "fleet/fleet.h"
#include "galaxy/system.h"
#include "gameobj/game_obj.h"
#include "gameobj/object_lookup.h"
#include "orders/order_base.h"
#include "orders/orders_210_216.h"
#include "strategy_ui/advisor_droid.h"
#include "strategy_ui/game_ui_facade.h"

using ai::KeyInFamilies;

namespace
{

constexpr int kScrapOrder = 0x200;
constexpr int kDestinationOrder = 0x214;
constexpr int kManufactureOrderBase = 0x210;
constexpr uint32_t kTaskWantsToRun = 1;
constexpr uint32_t kTaskDone = 2;
constexpr uint32_t kTaskHandOff = 0x10000000;

bool IsSector(const ObjectKey& key)
{
  return KeyInFamilies(key, 0x80, 0x90) != 0;
}
bool IsSystem(const ObjectKey& key)
{
  return KeyInFamilies(key, 0x90, 0x98) != 0;
}
bool IsKeySet(const ObjectKey& key)
{
  return (key.m_value & 0xff000000) != 0;
}

// Order 0x214 (or 0x210-0x212) for the manager `manager`, given to `side`. The order stays unset up to its team when the
// team's node could not be made.
OrderBase* NewOrderForManager(int orderKind, int side, const ObjectKey& manager, const ObjectKey* target)
{
  ObjectKeyValueSet team;
  OrderBase* order = static_cast<OrderBase*>(CreateMessageByKind(orderKind));
  if (order)
  {
    ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(manager, 0);
    if (node)
    {
      team.Add(node);
      order->m_side = side;
      order->SetTeam(&team);
      if (target)
        order->SetTarget(target);
    }
  }
  return order;
}

ObjectKey KeyOfObject(GameObjNotifier* object)
{
  ObjectKey key;
  GetObjectKey(object, &key);
  return key;
}

} // namespace

// ---- AdvisorTaskBase ----------------------------------------------------------------------------------------------------

AdvisorTaskBase::AdvisorTaskBase(int id, int view) : AiTaskNode(id)
{
  m_finished = 0;
  m_count = 0;
  m_x24 = 0;
  m_analysis = nullptr;
  m_view = view;
}

AdvisorTaskBase::~AdvisorTaskBase()
{
  m_analysis = nullptr;
}

void AdvisorTaskBase::Read(istream* in)
{
  AiTaskNode::Read(in);
  ReadValue32(in, &m_view);
  ReadValue32(in, &m_finished);
  ReadValue32(in, &m_count);
  ReadValue32(in, &m_x24);
}

void AdvisorTaskBase::Write(ostream* out)
{
  AiTaskNode::Write(out);
  WriteValue32(out, &m_view);
  WriteValue32(out, &m_finished);
  WriteValue32(out, &m_count);
  WriteValue32(out, &m_x24);
}

void AdvisorTaskBase::Evaluate()
{
  if (m_flags.Get() != 1 && m_flags.Get() != 2)
  {
    m_flags.m_value = 1;
    m_finished = 0;
  }
}

// ---- AdvisorTask20 ------------------------------------------------------------------------------------------------------

AdvisorTask20::AdvisorTask20(int view) : AdvisorTaskBase(0x14, view), m_selector()
{
  m_item.m_value = 0;
  m_x34 = 0;
  m_state = 0;
}

AdvisorTask20::~AdvisorTask20() = default;

void AdvisorTask20::Read(istream* in)
{
  AdvisorTaskBase::Read(in);
  ReadValue32(in, &m_x34);
  ReadValue32(in, &m_state);
  m_supply.Read(in);
  m_manager.Read(in);
  m_selector.Read(in);
  m_item.Read(in);
}

void AdvisorTask20::Write(ostream* out)
{
  AdvisorTaskBase::Write(out);
  WriteValue32(out, &m_x34);
  WriteValue32(out, &m_state);
  m_supply.Write(out);
  m_manager.Write(out);
  m_selector.Write(out);
  m_item.Write(out);
}

OrderBase* AdvisorTask20::Step()
{
  if (m_flags.Get() == 1)
    return Advance();
  m_x34 = 0;
  m_state = 0;
  m_finished = 1;
  return nullptr;
}

OrderBase* AdvisorTask20::Advance()
{
  OrderBase* result = nullptr;
  switch (m_state)
  {
    case 1:
      if (PickManager())
      {
        m_state = 2;
        return nullptr;
      }
      break;
    case 2:
      if (PickSupplySystem())
      {
        m_state = 3;
        return nullptr;
      }
      break;
    case 3:
      result = BuildDestinationOrder();
      if (result)
      {
        m_state = 4;
        m_finished = 1;
        ++m_count;
        return result;
      }
      result = nullptr;
      break;
    case 4:
      result = BuildManufactureOrder();
      break;
    default:
      m_state = 1;
      return nullptr;
  }
  m_state = 0;
  m_finished = 1;
  return result;
}

int AdvisorTask20::PickManager()
{
  int result = 0;
  ObjectKey scratch;
  m_selector.Add(m_analysis->RankSectors(0x44000, 0, 0, 0, 10, 2));
  m_manager = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(m_analysis->RankSystemsOfSector(&m_manager, 0x204, 0, 0x1000, 0, 7, 2));
  m_manager = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  if (IsSystem(m_manager))
  {
    System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, &m_manager));
    if (system)
    {
      GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(1);
      if (manager)
      {
        m_manager = KeyOfObject(manager);
        result = 1;
      }
    }
  }
  return result;
}

int AdvisorTask20::PickSupplySystem()
{
  AiGalaxyAnalysis* analysis = m_analysis;
  ObjectKey scratch;
  uint32_t sectorMask;
  uint32_t systemMask;
  if (analysis->Total(0xa8) < analysis->Total(0xac))
  {
    sectorMask = 0x180;
    systemMask = 0x60;
    m_item.m_value = 0x2c000001;
  }
  else
  {
    systemMask = 0x20;
    sectorMask = 0x80;
    m_item.m_value = 0x2d000002;
  }
  m_selector.Add(analysis->RankSectorsByDistance(&m_manager, sectorMask, 0, 0, 0, 2));
  m_supply = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  systemMask |= 4;
  m_selector.Add(analysis->RankSystemsOfSectorByDistance(&m_supply, &m_manager, systemMask, 0, 2, 0, 2));
  m_selector.Add(analysis->RankSystemsOfSector(&m_supply, systemMask, 0, 2, 0, 5, 2));
  m_selector.Add(analysis->RankSystemsOfSector(&m_supply, systemMask, 0, 2, 0, 6, 2));
  m_selector.Add(analysis->RankSystemsOfSector(&m_supply, systemMask, 0, 2, 0, 7, 2));
  m_supply = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  return IsSystem(m_supply) ? 1 : 0;
}

OrderBase* AdvisorTask20::BuildDestinationOrder()
{
  OrderBase* order = nullptr;
  if (IsSystem(m_supply) && KeyInFamilies(m_manager, 0xa0, 0xa2))
    order = NewOrderForManager(kDestinationOrder, m_view, m_manager, &m_supply);
  return order;
}

OrderBase* AdvisorTask20::BuildManufactureOrder()
{
  OrderBase* order = nullptr;
  if (KeyInFamilies(m_manager, 0xa0, 0xa2))
  {
    ObjectKeyValueSet team;
    order = static_cast<OrderBase*>(CreateMessageByKind(kManufactureOrderBase));
    if (order)
    {
      ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(m_manager, 0);
      if (node)
      {
        team.Add(node);
        order->m_side = m_view;
        order->SetTeam(&team);
        ManufactureOrderBase* manufacture = static_cast<ManufactureOrderBase*>(order);
        manufacture->m_item.m_value = m_item.m_value;
        manufacture->m_count = 1;
      }
    }
  }
  return order;
}

// ---- AdvisorTask21 ------------------------------------------------------------------------------------------------------

AdvisorTask21::AdvisorTask21(int view) : AdvisorTaskBase(0x15, view), m_selector()
{
  m_item.m_value = 0;
  m_x34 = 0;
  m_units = 0;
  m_state = 0;
}

AdvisorTask21::~AdvisorTask21() = default;

void AdvisorTask21::Read(istream* in)
{
  AdvisorTaskBase::Read(in);
  ReadValue32(in, &m_x34);
  ReadValue32(in, &m_units);
  ReadValue32(in, &m_state);
  m_target.Read(in);
  m_site.Read(in);
  m_selector.Read(in);
  m_item.Read(in);
}

void AdvisorTask21::Write(ostream* out)
{
  AdvisorTaskBase::Write(out);
  WriteValue32(out, &m_x34);
  WriteValue32(out, &m_units);
  WriteValue32(out, &m_state);
  m_target.Write(out);
  m_site.Write(out);
  m_selector.Write(out);
  m_item.Write(out);
}

OrderBase* AdvisorTask21::Step()
{
  if (m_flags.Get() == 1)
    return Advance();
  m_x34 = 0;
  m_state = 0;
  m_finished = 1;
  return nullptr;
}

void AdvisorTask20::Refused(int*, OrderBase* order)
{
  m_finished = 1;
  m_x34 = 0;
  m_state = 0;
  m_count = 0;
  if (order)
    delete order;
}

void AdvisorTask21::Refused(int* status, OrderBase* order)
{
  m_finished = 1;
  m_x34 = 0;
  m_state = 0;
  m_count = 0;
  if (order)
    delete order;
}

OrderBase* AdvisorTask21::Advance()
{
  OrderBase* result = nullptr;
  switch (m_state)
  {
    case 1:
      if (PickSite())
      {
        m_state = 3;
        return nullptr;
      }
      break;
    case 2:
      if (ChooseTarget())
      {
        m_state = 1;
        return nullptr;
      }
      break;
    case 3:
      if (ComputeCount())
      {
        m_state = 4;
        return nullptr;
      }
      break;
    case 4:
      result = BuildDestinationOrder();
      if (result)
      {
        m_state = 5;
        m_finished = 1;
        ++m_count;
        return result;
      }
      result = nullptr;
      break;
    case 5:
      result = BuildManufactureOrder();
      break;
    default:
      m_state = 2;
      return nullptr;
  }
  m_state = 0;
  m_finished = 1;
  return result;
}

int AdvisorTask21::ChooseTarget()
{
  ObjectKey scratch;
  AiGalaxyAnalysis* analysis = m_analysis;
  m_selector.Add(analysis->RankSectors(0x21000000, 0, 0, 0, 0x19, 1));
  m_target = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(analysis->RankSystemsOfSector(&m_target, 0x10000005, 0, 2, 0, 0x14, 1));
  m_target = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  if (!IsSystem(m_target))
  {
    m_selector.Add(analysis->RankSectors(0x1000000, 0, 0, 0, 2, 2));
    m_target = *m_selector.PickBest(&scratch);
    m_selector.Reset();
    m_selector.Add(analysis->RankSystemsOfSector(&m_target, 0x10000004, 0, 2, 0, 0x14, 2));
    m_target = *m_selector.PickBest(&scratch);
    m_selector.Reset();
  }
  return IsSystem(m_target) ? 1 : 0;
}

int AdvisorTask21::PickSite()
{
  ObjectKey scratch;
  AiGalaxyAnalysis* analysis = m_analysis;
  m_selector.Add(analysis->RankSectorsByDistance(&m_target, 0x22000, 0, 0, 0, 2));
  m_site = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  m_selector.Add(analysis->RankSystemsOfSectorByDistance(&m_site, &m_target, 0x104, 0, 0x800, 0, 2));
  m_site = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  if (IsSystem(m_site))
  {
    System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, &m_site));
    if (system)
    {
      GameObjNotifier* manager = system->FindManufacturingManagerBySlot123(2);
      if (manager)
      {
        m_site = KeyOfObject(manager);
        return 1;
      }
    }
  }
  return 0;
}

int AdvisorTask21::ComputeCount()
{
  m_units = 0;
  int result = 0;
  const int shortfall = m_analysis->Total(0x80) - m_analysis->Total(0x84);
  if (0 < shortfall)
  {
    AiSystemScan* scan = m_analysis->FindSystemScan(&m_target);
    uint32_t mode;
    int descending;
    if (!scan)
    {
      mode = 0;
      descending = 0;
    }
    else if ((scan->m_flagsA.m_value & 1) == 0)
    {
      mode = 0x2000;
      descending = 1;
    }
    else
    {
      mode = 1;
      descending = 2;
    }
    m_units = 1;
    const int perUnit = m_analysis->ChooseFromSide(&m_item, 0x29, 0x2000, mode, descending);
    if (IsKeySet(m_item) && perUnit != 0)
    {
      const int total = perUnit * m_units;
      if (total - shortfall != 0 && shortfall <= total)
        m_units = shortfall / perUnit;
    }
    if (0 < m_units)
      result = 1;
  }
  return result;
}

OrderBase* AdvisorTask21::BuildDestinationOrder()
{
  OrderBase* order = nullptr;
  if (IsSystem(m_target) && KeyInFamilies(m_site, 0xa4, 0xa6))
    order = NewOrderForManager(kDestinationOrder, m_view, m_site, &m_target);
  return order;
}

OrderBase* AdvisorTask21::BuildManufactureOrder()
{
  OrderBase* order = nullptr;
  if (KeyInFamilies(m_site, 0xa4, 0xa6))
  {
    ObjectKeyValueSet team;
    order = static_cast<OrderBase*>(CreateMessageByKind(0x212));
    if (order)
    {
      ObjectKeyValueNode* node = NewOrNull<ObjectKeyValueNode>(m_site, 0);
      if (node)
      {
        team.Add(node);
        order->m_side = m_view;
        order->SetTeam(&team);
        ManufactureOrderBase* manufacture = static_cast<ManufactureOrderBase*>(order);
        manufacture->m_item.m_value = m_item.m_value;
        manufacture->m_count = m_units;
      }
    }
  }
  return order;
}

// ---- AdvisorTask23 ------------------------------------------------------------------------------------------------------

AdvisorTask23::AdvisorTask23(int view) : AdvisorTaskBase(0x17, view), m_selector()
{
  m_advisor = nullptr;
  m_phase = 0;
}

AdvisorTask23::~AdvisorTask23()
{
  m_advisor = nullptr;
}

void AdvisorTask23::Read(istream* in)
{
  AdvisorTaskBase::Read(in);
  ReadValue32(in, &m_phase);
  ReadValue32(in, &m_kind);
  m_target.Read(in);
  m_site.Read(in);
  m_selector.Read(in);
}

void AdvisorTask23::Write(ostream* out)
{
  AdvisorTaskBase::Write(out);
  WriteValue32(out, &m_phase);
  WriteValue32(out, &m_kind);
  m_target.Write(out);
  m_site.Write(out);
  m_selector.Write(out);
}

void AdvisorTask23::Start(int kind)
{
  m_kind = kind;
  m_target.Reset();
  m_site.Reset();
}

namespace
{

void FinishTask(AdvisorTask23* task)
{
  task->m_kind = 0;
  task->m_target.Reset();
  task->m_site.Reset();
  task->m_phase = 0;
  task->m_finished = 1;
  task->m_flags.m_value = kTaskDone;
}

} // namespace

void AdvisorTask23::SetTarget(const ObjectKey* target)
{
  const uint32_t family = target->Family();
  const bool pickable =
    KeyInFamilies(*target, 0x90, 0x98) || KeyInFamilies(*target, 8, 0x10) || KeyInFamilies(*target, 0x14, 0x1c);
  if (pickable)
  {
    if (m_kind == 3 && KeyInFamilies(*target, 0x90, 0x98))
    {
      System* system = static_cast<System*>(GetObjectInFamilies_90_98(m_view, target));
      if (system && static_cast<int>(system->ControlSide()) != m_view && CountFleets(system, m_view, 1) == 0)
      {
        OrderBase* order = static_cast<OrderBase*>(CreateMessageByKind(kDestinationOrder));
        if (order)
        {
          order->m_side = m_view;
          order->SetTarget(target);
          StatusCode status(1, 0x24);
          m_advisor->HandleRefusedOrder(order, &status);
          delete order;
        }
        FinishTask(this);
        return;
      }
    }
    m_target = *target;
    m_flags.m_value = 0;
  }
  else if (family == 0)
  {
    FinishTask(this);
  }
  else
  {
    StatusCode status(1, 0x27);
    m_advisor->HandleRefusedOrder(nullptr, &status);
    FinishTask(this);
  }
}

OrderBase* AdvisorTask23::Step()
{
  if (m_flags.Get() != 1)
  {
    m_kind = 0;
    m_phase = 0;
    m_finished = 1;
    m_flags.m_value = kTaskDone;
    return nullptr;
  }
  if (m_kind != 0)
    return Advance();
  m_kind = 0;
  m_finished = 1;
  m_flags.m_value = kTaskDone;
  return nullptr;
}

void AdvisorTask23::Refused(int* status, OrderBase* order)
{
  m_kind = 0;
  m_phase = 0;
  m_finished = 1;
  m_flags.m_value = kTaskDone;
  m_count = 0;
  m_advisor->HandleRefusedOrder(order, reinterpret_cast<StatusCode*>(status));
  if (order)
    delete order;
}

OrderBase* AdvisorTask23::Advance()
{
  switch (m_phase)
  {
    case 1:
      if (!IsKeySet(m_target))
      {
        m_finished = 1;
        return nullptr;
      }
      if (PickSite())
      {
        m_phase = 2;
        return nullptr;
      }
      break;
    case 2:
    {
      OrderBase* order = BuildDestinationOrder();
      if (order)
      {
        m_phase = 3;
        m_finished = 1;
        ++m_count;
        return order;
      }
      break;
    }
    case 3:
    {
      ManufactureOrderBase* order = static_cast<ManufactureOrderBase*>(BuildManufactureOrder(OrderKind()));
      if (order)
      {
        OpenWindow(order);
        g_humanUiFlags.m_value |= kTaskHandOff;
        m_advisor->SetRefusedOrder(order);
        m_phase = 0;
        m_kind = 0;
        m_finished = 1;
        m_flags.m_value = kTaskDone;
        return nullptr;
      }
      break;
    }
    default:
      m_phase = 1;
      return nullptr;
  }
  m_phase = 0;
  m_kind = 0;
  m_finished = 1;
  m_flags.m_value = kTaskDone;
  return nullptr;
}

int AdvisorTask23::PickSite()
{
  int result = 0;
  ObjectKey scratch;
  uint32_t mask;
  GetFacilityMask(&mask);
  m_selector.Add(m_analysis->RankSectorsByDistance(&m_target, mask | 0x10, 0, 0, 0, 2));
  m_site = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  GetSystemMask(&mask);
  m_selector.Add(m_analysis->RankSystemsOfSectorByDistance(&m_site, &m_target, mask | 4, 0, 0, 0, 2));
  m_site = *m_selector.PickBest(&scratch);
  m_selector.Reset();
  if (IsSystem(m_site))
  {
    GameObjNotifier* system = GetObjectInFamilies_90_98(m_view, &m_site);
    if (system)
    {
      GameObjNotifier* manager = FindManager(system);
      if (manager)
      {
        m_site = KeyOfObject(manager);
        result = 1;
      }
    }
  }
  if (result == 0)
  {
    StatusCode status(0xa0, 3);
    m_advisor->HandleRefusedOrder(nullptr, &status);
  }
  return result;
}

OrderBase* AdvisorTask23::BuildDestinationOrder()
{
  OrderBase* order = nullptr;
  if (IsKeySet(m_target) && IsKeySet(m_site))
    order = NewOrderForManager(kDestinationOrder, m_view, m_site, &m_target);
  return order;
}

OrderBase* AdvisorTask23::BuildManufactureOrder(int kind)
{
  OrderBase* order = nullptr;
  if (KeyInFamilies(m_site, 0xa0, 0xb0))
    order = NewOrderForManager(kind, m_view, m_site, nullptr);
  return order;
}

int AdvisorTask23::OrderKind()
{
  if (m_kind == 1)
    return 0x210;
  if (m_kind == 2)
    return 0x211;
  if (m_kind == 3)
    return 0x212;
  return 0;
}

void AdvisorTask23::GetFacilityMask(uint32_t* out)
{
  if (m_kind == 1)
    *out = 0x4000;
  else if (m_kind == 2)
    *out = 0x1000;
  else if (m_kind == 3)
    *out = 0x2000;
  else
    *out = 0;
}

void AdvisorTask23::GetSystemMask(uint32_t* out)
{
  if (m_kind == 1)
    *out = 0x200;
  else if (m_kind == 2)
    *out = 0x80;
  else if (m_kind == 3)
    *out = 0x100;
  else
    *out = 0;
}

GameObjNotifier* AdvisorTask23::FindManager(GameObjNotifier* system)
{
  System* typed = static_cast<System*>(system);
  if (m_kind == 1)
    return typed->FindManufacturingManagerBySlot123(1);
  if (m_kind == 2)
    return typed->FindManufacturingManagerBySlot123(0);
  if (m_kind == 3)
    return typed->FindManufacturingManagerBySlot123(2);
  return nullptr;
}

void AdvisorTask23::OpenWindow(ManufactureOrderBase* order)
{
  if (m_kind == 1)
  {
    UiOpenManufactureWindow(order, 0x2a);
  }
  else if (m_kind == 2)
  {
    UiOpenManufactureWindow(order, 0x28);
  }
  else if (m_kind == 3)
  {
    UiOpenManufactureWindow(order, 0x29);
  }
  else if (order)
  {
    delete order;
  }
}

// ---- AdvisorTaskManager -------------------------------------------------------------------------------------------------

AdvisorTaskManager::AdvisorTaskManager()
{
  m_current = nullptr;
  m_cursor = nullptr;
  m_last = nullptr;
}

AdvisorTaskManager::~AdvisorTaskManager()
{
  m_current = nullptr;
  m_cursor = nullptr;
  m_last = nullptr;
  Clear();
}

void AdvisorTaskManager::Clear()
{
  m_tasks.Clear();
}

uint32_t AdvisorTaskManager::AddWithId(AdvisorTaskBase* task)
{
  return m_tasks.AddWithId(task);
}

AdvisorTaskBase* AdvisorTaskManager::FindById(uint32_t id) const
{
  return m_tasks.FindById(id);
}

void AdvisorTaskManager::Save(ostream* out)
{
  WriteValue32(out, &m_view);
  int16_t count = Count();
  WriteValue16(out, &count);
  for (const auto& entry : Entries())
  {
    AdvisorTaskBase* task = entry.second.get();
    uint32_t id = task->m_id;
    WriteValue32(out, &id);
    task->Write(out);
  }
  uint32_t id = m_current ? m_current->m_id : 0;
  WriteValue32(out, &id);
  id = m_cursor ? m_cursor->m_id : 0;
  WriteValue32(out, &id);
  id = m_last ? m_last->m_id : 0;
  WriteValue32(out, &id);
}

void AdvisorTaskManager::Load(istream* in, AiGalaxyAnalysis* analysis)
{
  Clear();
  ReadValue32(in, &m_view);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return;
  int16_t count;
  ReadValue16(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || count < 0)
  {
    iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
    return;
  }
  while (count > 0)
  {
    int type = 0;
    ReadValue32(in, &type);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
      return;
    AdvisorTaskBase* task = MakeTask(type);
    if (!task)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    task->SetAnalysis(analysis);
    task->Read(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || AddWithId(task) == 0)
    {
      delete task;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    --count;
  }
  uint32_t id;
  ReadValue32(in, &id);
  m_current = static_cast<AdvisorTaskBase*>(FindById(id));
  ReadValue32(in, &id);
  m_cursor = static_cast<AdvisorTaskBase*>(FindById(id));
  ReadValue32(in, &id);
  m_last = static_cast<AdvisorTaskBase*>(FindById(id));
}

uint32_t AdvisorTaskManager::CreateTask(int type, AiGalaxyAnalysis* analysis)
{
  uint32_t id = 0;
  if (FindById(type))
    return id;
  AdvisorTaskBase* task = MakeTask(type);
  if (task)
  {
    task->SetAnalysis(analysis);
    task->m_flags.m_value = 0;
    id = AddWithId(task);
  }
  return id;
}

AdvisorTaskBase* AdvisorTaskManager::SelectNext()
{
  m_last = PickBest();
  if (m_last != nullptr)
    return m_last;
  auto position = PositionOf(m_current);
  if (position == Entries().end() || ++position == Entries().end())
    position = Entries().begin();
  m_current = position == Entries().end() ? nullptr : position->second.get();
  AdvisorTaskBase* start = m_current;
  bool wrapped = false;
  int wants = m_current->m_flags.Get();
  while (wants != 1)
  {
    if (wrapped)
    {
      m_last = nullptr;
      return m_last;
    }
    if (++position == Entries().end())
      position = Entries().begin();
    m_current = position == Entries().end() ? nullptr : position->second.get();
    wrapped = start == m_current;
    wants = m_current->m_flags.Get();
  }
  if (!wrapped)
  {
    m_last = m_current;
    return m_current;
  }
  m_last = nullptr;
  return m_last;
}

AdvisorTaskBase* AdvisorTaskManager::MakeTask(int type)
{
  switch (type)
  {
    case 0x14:
      return NewOrNull<AdvisorTask20>(m_view);
    case 0x15:
      return NewOrNull<AdvisorTask21>(m_view);
    case 0x17:
      return NewOrNull<AdvisorTask23>(m_view);
  }
  return nullptr;
}

AdvisorTaskBase* AdvisorTaskManager::PickBest()
{
  AdvisorTaskBase* best = nullptr;
  uint32_t bestCount = 0;
  auto position = PositionOf(m_cursor);
  if (position == Entries().end() || ++position == Entries().end())
    position = Entries().begin();
  m_cursor = position == Entries().end() ? nullptr : position->second.get();
  AdvisorTaskBase* task = m_cursor;
  while (task)
  {
    if (task->m_flags.Get() == 1 && bestCount < static_cast<uint32_t>(task->m_count))
    {
      bestCount = task->m_count;
      best = task;
    }
    if (++position == Entries().end())
      position = Entries().begin();
    task = position == Entries().end() ? nullptr : position->second.get();
    if (task == m_cursor)
      task = nullptr;
  }
  return best;
}

// ---- AdvisorEntryTree ---------------------------------------------------------------------------------------------------

AdvisorEntryTree::AdvisorEntryTree() = default;

AdvisorEntryTree::~AdvisorEntryTree()
{
  Clear();
}

void AdvisorEntryTree::Clear()
{
  m_entries.Clear();
}

uint32_t AdvisorEntryTree::AddWithId(DroidScript* entry)
{
  return m_entries.AddWithId(entry);
}

void AdvisorEntryTree::Save(ostream* out)
{
  if (!WriteCount32(out, Count()))
    return;
  for (const auto& entry : Entries())
    entry.second->Write(out);
}

void AdvisorEntryTree::Load(istream* in)
{
  uint32_t count = 0;
  ReadValue32(in, &count);
  if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0)
    return;
  for (uint32_t index = 0; index < count; ++index)
  {
    DroidScript* entry = NewOrNull<DroidScript>();
    if (!entry)
    {
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
    entry->Read(in);
    if ((iostream_access::IosOf(in)->m_state & iostream_flags::kFail) != 0 || AddWithId(entry) == 0)
    {
      delete entry;
      iostream_access::SetState(iostream_access::IosOf(in), iostream_flags::kFail);
      return;
    }
  }
}

PersistentInt g_humanUiFlags = PersistentInt(0);
const GUID g_iidDirectPlay2A = {0x133efe41, 0x32dc, 0x11d0, {0x9c, 0xfb, 0x00, 0xa0, 0xc9, 0x0a, 0x43, 0xcb}};
int g_advisorFlag0{};
int g_advisorFlushAll{};
int g_advisorMaySpeak{};
int g_advisorNagCount{};
int g_advisorPendingArg{};
int g_advisorPendingMode{};
uint32_t g_advisorLastRun{};
uint32_t g_advisorNow{};
