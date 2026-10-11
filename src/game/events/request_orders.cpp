#include "events/request_orders.h"

#include "core/checked_new.h"
#include "events/event_helpers.h"
#include "fleet/capship.h"
#include "fleet/capship_subclasses.h"
#include "fleet/fleet.h"
#include "fleet/troop_reg.h"
#include "gameobj/game_hooks.h"
#include "gameobj/object_queries.h"
#include "mission/mission_events.h"
#include "orders/order_checks.h"
#include "orders/route_capacity.h"
#include "orders/route_context.h"

// Collects what the team of a request depends on and the people on board (0x5187f0, combat_strat module).
int CollectTeamSystems(ActivityRequest* request, ObjectKeyValueSet* team, ObjectKeyValueSet* out, StatusCode* status);

namespace
{

constexpr int kStatusCategory = 1;
constexpr int kStatusOk = -1;
constexpr int kStatusRefused = 1;
constexpr int kStatusTooManyCommands = 0x15;
constexpr int kFleetStatusCategory = 8;
constexpr int kNoRegiments = 5;
constexpr int kNoUsableFleet = 7;
constexpr int kNoShips = 6;
constexpr int kNoDeathStar = 8;
constexpr int kRequestKindDeathStar = 0x223;
constexpr uint8_t kFleetFlagCreated = 1;
constexpr uint8_t kFleetFlagState58Bit5 = 0x20;
constexpr uint32_t kFirstFleetFamily = 8;
constexpr uint32_t kLastFleetFamily = 0xf;
constexpr uint32_t kCreatedStateFilter = 1;

bool isOk(const StatusCode* status)
{
  return status->m_second == static_cast<uint32_t>(kStatusOk);
}

void setStatus(StatusCode* status, int category, int code)
{
  *status = StatusCode(category, code);
}

bool isFleetKey(const ObjectKey& key)
{
  return key.Family() >= kFirstFleetFamily && key.Family() <= kLastFleetFamily;
}

ObjectKey keyOfNode(AvlNode* node)
{
  ObjectKey key;
  static_cast<ObjectKeyNode*>(node)->GetKey(&key);
  return key;
}

Command* firstCommand(CommandQueue* commands)
{
  return commands->FirstCommand();
}

CommandQueue* newCommandQueue()
{
  return NewOrNull<CommandQueue>();
}

} // namespace

// ---- Order250 -----------------------------------------------------------------------------------------------------

Order250::Order250() : OrderBase()
{
}

Order250::~Order250() = default;

Command* Order250::CreateCommand()
{
  return NewOrNull<MissionActivityCommand>();
}

void* CreateOrder250()
{
  return NewOrNull<Order250>();
}

// ---- PayloadMessage -----------------------------------------------------------------------------------------------

PayloadMessage::PayloadMessage() : SidedMessage(), m_delay(0), m_valueA(0), m_valueB(0)
{
}

PayloadMessage::~PayloadMessage() = default;

int PayloadMessage::Read(istream* in)
{
  int result = SidedMessage::Read(in);
  if (result != 0)
  {
    StreamRead32(in, &m_delay);
    StreamRead32(in, &m_valueA);
    StreamRead32(in, &m_valueB);
  }
  return result;
}

int PayloadMessage::Write(ostream* out)
{
  int result = SidedMessage::Write(out);
  if (result != 0)
  {
    StreamWrite32(out, &m_delay);
    StreamWrite32(out, &m_valueA);
    StreamWrite32(out, &m_valueB);
  }
  return result;
}

int PayloadMessage::CheckEnabled(StatusCode* status)
{
  return events::succeedWithoutChecks(status);
}

int PayloadMessage::CheckValid(StatusCode* status)
{
  return events::succeedWithoutChecks(status);
}

int PayloadMessage::Execute(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  PayloadEvent3f0* event = NewOrNull<PayloadEvent3f0>(m_valueA, m_valueB, m_side);
  if (event)
  {
    ObjectKey sideKey;
    sideKey.m_value = m_side == 1 ? 0xf300000fu : 0xf3000010u;
    event->m_key = sideKey;
    CallGameHook2AD8(events::asIntArg(event), m_delay);
  }
  return 1;
}

void* CreatePayloadMessage()
{
  return NewOrNull<PayloadMessage>();
}

// ---- Order270 -----------------------------------------------------------------------------------------------------

Order270::Order270() : OrderBase()
{
}

Order270::~Order270() = default;

int Order270::GetKind()
{
  return 0x270;
}

Command* Order270::CreateCommand()
{
  return NewOrNull<MoveCommand>();
}

int Order270::CheckDestination(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  ActivityRequest request;
  BuildRequest(&request);
  int ok = OrderBase::CheckDestination(status);
  uint32_t side = 0;
  if (ok != 0 && isOk(status))
    ok = GetCommandSide(m_commands, &side);

  RouteContext route;
  if (ok != 0)
  {
    if (isOk(status))
    {
      int mode = 3;
      ok = route.InitFromRequest(&request, static_cast<int>(side), &mode);
    }
    if (ok != 0)
    {
      if (isOk(status))
        ok = route.AddContainerLegs(&request.m_target);
      if (ok != 0)
      {
        if (isOk(status))
        {
          for (auto commandPosition = m_commands->Entries().begin(); commandPosition != m_commands->Entries().end();)
          {
            Command* command = static_cast<Command*>((commandPosition++)->get());
            if (!(true && ok != 0 && isOk(status)))
              break;

            ObjectKey tightest;
            CapacityScratch slot;
            ObjectKey unit = command->m_key;
            ok = route.CheckMove(&unit, &request.m_target, status, &slot, &tightest);
            if (ok != 0 && isOk(status))
              command->m_request.m_target = tightest;
          }
        }
        if (ok != 0)
          return ok;
      }
    }
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return ok;
}

void* CreateOrder270()
{
  return NewOrNull<Order270>();
}

// ---- RootCommandOrder and its leaves ------------------------------------------------------------------------------

RootCommandOrder::RootCommandOrder() : OrderBase()
{
}

RootCommandOrder::~RootCommandOrder() = default;

int RootCommandOrder::IsChecked()
{
  const int ownsQueue = static_cast<int>(m_unk1c);
  int checked = OrderBase::IsChecked();
  if (ownsQueue != 0 && checked != 0)
  {
    if (static_cast<uint16_t>(m_commands->Count()) > 1)
      checked = 0;
  }
  return checked;
}

int RootCommandOrder::CheckEnabled(StatusCode* status)
{
  const int ownsQueue = static_cast<int>(m_unk1c);
  *status = StatusCode(kStatusCategory, kStatusOk);
  OrderBase::CheckEnabled(status);
  if (ownsQueue != 0 && isOk(status))
  {
    if (static_cast<uint16_t>(m_commands->Count()) > 1)
      setStatus(status, kStatusCategory, kStatusTooManyCommands);
  }
  return isOk(status);
}

Command* RootCommandOrder::CreateCommand()
{
  return NewOrNull<RootCommand240>();
}

RootOrder260::RootOrder260() : RootCommandOrder()
{
}
RootOrder260::~RootOrder260() = default;
int RootOrder260::Read(istream* in)
{
  return OrderBase::Read(in);
}
int RootOrder260::Write(ostream* out)
{
  return OrderBase::Write(out);
}
void* CreateRootOrder260()
{
  return NewOrNull<RootOrder260>();
}

RootOrder261::RootOrder261() : RootCommandOrder()
{
}
RootOrder261::~RootOrder261() = default;
int RootOrder261::Read(istream* in)
{
  return OrderBase::Read(in);
}
int RootOrder261::Write(ostream* out)
{
  return OrderBase::Write(out);
}
void* CreateRootOrder261()
{
  return NewOrNull<RootOrder261>();
}

RootOrder262::RootOrder262() : RootCommandOrder()
{
}
RootOrder262::~RootOrder262() = default;
int RootOrder262::Read(istream* in)
{
  return OrderBase::Read(in);
}
int RootOrder262::Write(ostream* out)
{
  return OrderBase::Write(out);
}
void* CreateRootOrder262()
{
  return NewOrNull<RootOrder262>();
}

RootOrder263::RootOrder263() : RootCommandOrder()
{
}
RootOrder263::~RootOrder263() = default;
int RootOrder263::Read(istream* in)
{
  return OrderBase::Read(in);
}
int RootOrder263::Write(ostream* out)
{
  return OrderBase::Write(out);
}
void* CreateRootOrder263()
{
  return NewOrNull<RootOrder263>();
}

// ---- Order268 -----------------------------------------------------------------------------------------------------

Order268::Order268() : OrderBase()
{
}

Order268::~Order268() = default;

Command* Order268::CreateCommand()
{
  return NewOrNull<RootCommand242>();
}

void* CreateOrder268()
{
  return NewOrNull<Order268>();
}

// ---- Order234 -----------------------------------------------------------------------------------------------------

Order234::Order234() : OrderBase(), m_queue(nullptr)
{
  int owns = 0;
  if (m_unk1c != 0)
  {
    m_queue = newCommandQueue();
    owns = m_queue != nullptr;
  }
  m_unk1c = static_cast<uint32_t>(owns);
}

Order234::~Order234()
{
  if (m_queue)
  {
    delete m_queue;
    m_queue = nullptr;
  }
}

int Order234::CheckEnabled(StatusCode* status)
{
  int ownsQueue = static_cast<int>(m_unk1c);
  *status = StatusCode(kStatusCategory, kStatusOk);
  ActivityRequest request;
  BuildRequest(&request);
  OrderBase::CheckEnabled(status);
  if (ownsQueue != 0)
  {
    if (isOk(status))
      ownsQueue = CheckCommandsEnabled(&request, m_queue, status);
    if (ownsQueue != 0)
    {
      if (isOk(status))
        ownsQueue = CheckFleetsCanInvade(&request, GetTeam(), status);
      if (ownsQueue != 0)
        return isOk(status);
    }
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return isOk(status);
}

int Order234::CheckValid(StatusCode* status)
{
  int ownsQueue = static_cast<int>(m_unk1c);
  *status = StatusCode(kStatusCategory, kStatusOk);
  ActivityRequest request;
  BuildRequest(&request);
  OrderBase::CheckValid(status);
  if (ownsQueue != 0)
  {
    if (isOk(status))
      ownsQueue = CheckCommandsValid(&request, m_queue, status);
    if (ownsQueue != 0)
      return isOk(status);
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return isOk(status);
}

int Order234::Execute(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  int ok = OrderBase::Execute(status);
  if (ok != 0)
  {
    if (isOk(status))
    {
      ActivityRequest request;
      BuildRequest(&request);
      ok = ExecuteCommands(&request, m_queue);
    }
    if (ok != 0)
      return ok;
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return 0;
}

Command* Order234::CreateCommand()
{
  return NewOrNull<FleetCommand221>();
}

int Order234::CheckCommand(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  ActivityRequest request;
  BuildRequest(&request);
  int ok = OrderBase::CheckCommand(status);
  if (ok != 0)
  {
    if (isOk(status))
    {
      SystemCommand225 prototype;
      ActivityRequest commandRequest;
      BuildRequest(&commandRequest);
      prototype.SetRequest(&commandRequest);
      ObjectKeyValueSet dependencies;
      ok = CollectTeamSystems(&commandRequest, GetTeam(), &dependencies, status);
      if (ok != 0 && isOk(status))
        ok = CheckCommandForTeam(&prototype, &dependencies, m_queue, status);
    }
    if (ok != 0)
      return ok;
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return ok;
}

int Order234::CheckDestination(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  ActivityRequest request;
  BuildRequest(&request);
  int ok = OrderBase::CheckDestination(status);
  if (ok == 0)
    setStatus(status, kStatusCategory, kStatusRefused);
  return ok;
}

void* CreateOrder234()
{
  return NewOrNull<Order234>();
}

// ---- FleetCommandOrder and its leaves -----------------------------------------------------------------------------

FleetCommandOrder::FleetCommandOrder() : OrderBase(), m_queue(nullptr)
{
  int owns = 0;
  if (m_unk1c != 0)
  {
    m_queue = newCommandQueue();
    owns = m_queue != nullptr;
  }
  m_unk1c = static_cast<uint32_t>(owns);
}

FleetCommandOrder::~FleetCommandOrder()
{
  if (m_queue)
  {
    delete m_queue;
    m_queue = nullptr;
  }
}

int FleetCommandOrder::CheckEnabled(StatusCode* status)
{
  int ownsQueue = static_cast<int>(m_unk1c);
  *status = StatusCode(kStatusCategory, kStatusOk);
  ActivityRequest request;
  BuildRequest(&request);
  OrderBase::CheckEnabled(status);
  if (ownsQueue != 0)
  {
    if (isOk(status))
      ownsQueue = CheckCommandsEnabled(&request, m_queue, status);
    if (ownsQueue != 0)
    {
      if (isOk(status))
        ownsQueue = CheckFleetsCanBeUsed(&request, GetTeam(), status);
      if (ownsQueue != 0)
        return isOk(status);
    }
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return isOk(status);
}

int FleetCommandOrder::CheckValid(StatusCode* status)
{
  int ownsQueue = static_cast<int>(m_unk1c);
  *status = StatusCode(kStatusCategory, kStatusOk);
  ActivityRequest request;
  BuildRequest(&request);
  OrderBase::CheckValid(status);
  if (ownsQueue != 0)
  {
    if (isOk(status))
      ownsQueue = CheckCommandsValid(&request, m_queue, status);
    if (ownsQueue != 0)
      return isOk(status);
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return isOk(status);
}

int FleetCommandOrder::Execute(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  int ok = OrderBase::Execute(status);
  if (ok != 0)
  {
    if (isOk(status))
    {
      ActivityRequest request;
      BuildRequest(&request);
      ok = ExecuteCommands(&request, m_queue);
    }
    if (ok != 0)
      return ok;
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return 0;
}

Command* FleetCommandOrder::CreateCommand()
{
  return NewOrNull<FleetCommand220>();
}

int FleetCommandOrder::CheckCommand(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  int ok = OrderBase::CheckCommand(status);
  if (ok != 0)
  {
    if (isOk(status))
    {
      SystemCommand224 prototype;
      ActivityRequest commandRequest;
      BuildRequest(&commandRequest);
      prototype.SetRequest(&commandRequest);
      ObjectKeyValueSet dependencies;
      ok = CollectTeamSystems(&commandRequest, GetTeam(), &dependencies, status);
      if (ok != 0 && isOk(status))
        ok = CheckCommandForTeam(&prototype, &dependencies, m_queue, status);
    }
    if (ok != 0)
      return ok;
  }
  setStatus(status, kStatusCategory, kStatusRefused);
  return ok;
}

int FleetCommandOrder::CheckDestination(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  int ok = OrderBase::CheckDestination(status);
  if (ok == 0)
    setStatus(status, kStatusCategory, kStatusRefused);
  return ok;
}

Order221::Order221() : FleetCommandOrder()
{
}
Order221::~Order221() = default;
int Order221::Read(istream* in)
{
  return OrderBase::Read(in);
}
int Order221::Write(ostream* out)
{
  return OrderBase::Write(out);
}
void* CreateOrder221()
{
  return NewOrNull<Order221>();
}

Order223::Order223() : FleetCommandOrder()
{
}
Order223::~Order223() = default;
int Order223::Read(istream* in)
{
  return OrderBase::Read(in);
}
int Order223::Write(ostream* out)
{
  return OrderBase::Write(out);
}
void* CreateOrder223()
{
  return NewOrNull<Order223>();
}

Order222::Order222() : FleetCommandOrder()
{
}
Order222::~Order222() = default;
int Order222::Read(istream* in)
{
  return OrderBase::Read(in);
}
int Order222::Write(ostream* out)
{
  return OrderBase::Write(out);
}
void* CreateOrder222()
{
  return NewOrNull<Order222>();
}

Order220::Order220() : FleetCommandOrder()
{
}
Order220::~Order220() = default;
int Order220::Read(istream* in)
{
  return OrderBase::Read(in);
}
int Order220::Write(ostream* out)
{
  return OrderBase::Write(out);
}
void* CreateOrder220()
{
  return NewOrNull<Order220>();
}

// ---- the fleet checks ---------------------------------------------------------------------------------------------

int CheckFleetsCanInvade(ActivityRequest* request, ObjectKeyValueSet* team, StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  ObjectKeyValueSet dependencies;
  const int ok = CollectKeyDependencies(request, team, &dependencies, status);
  bool anyUsableFleet = false;
  int regimentsAboard = 0;
  if (ok != 0 && isOk(status))
  {
    for (auto position = dependencies.Entries().begin();
         position != dependencies.Entries().end() && (regimentsAboard == 0); ++position)
    {
      AvlNode* node = static_cast<AvlNode*>(position->get());
      ObjectKey fleetKey = keyOfNode(node);
      if (!isFleetKey(fleetKey))
        continue;
      ObjectKey lookupKey = keyOfNode(node);
      GameObjNotifier* fleet = GetMasterInFamilies_08_10(&lookupKey);
      if (fleet && (static_cast<GameObj*>(fleet)->m_stateFlags & kFleetFlagCreated) != 0 &&
          (static_cast<Fleet*>(fleet)->m_fleetFlags & kFleetFlagState58Bit5) != 0)
      {
        anyUsableFleet = true;
        CapShipIterator ships(fleet, kCreatedStateFilter);
        for (ships.Begin(); regimentsAboard == 0 && ships.Current(); ships.Advance())
        {
          TroopRegIterator regiments(ships.Current(), kCreatedStateFilter);
          regimentsAboard = regiments.HasCurrent();
        }
      }
    }
  }
  if (ok != 0)
  {
    if (isOk(status) && !anyUsableFleet)
      setStatus(status, kFleetStatusCategory, kNoUsableFleet);
    if (ok != 0 && isOk(status) && regimentsAboard == 0)
      setStatus(status, kFleetStatusCategory, kNoRegiments);
  }
  return ok;
}

int CheckFleetsCanBeUsed(ActivityRequest* request, ObjectKeyValueSet* team, StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  bool anyShips = false;
  int deathStarAboard = 0;
  bool stop = false;
  const bool needsDeathStar = request->m_kind == kRequestKindDeathStar;
  ObjectKeyValueSet dependencies;
  const int ok = CollectKeyDependencies(request, team, &dependencies, status);
  if (ok != 0 && isOk(status))
  {
    for (const auto& entry : dependencies.Entries())
    {
      if (stop)
        break;
      AvlNode* node = entry.get();
      ObjectKey fleetKey = keyOfNode(node);
      if (isFleetKey(fleetKey))
      {
        ObjectKey lookupKey = keyOfNode(node);
        GameObjNotifier* fleet = GetMasterInFamilies_08_10(&lookupKey);
        if (fleet && (static_cast<GameObj*>(fleet)->m_stateFlags & kFleetFlagCreated) != 0)
        {
          anyShips = static_cast<Fleet*>(fleet)->SumUnitStat119() != 0;
          DeathStarIterator deathStars(fleet, kCreatedStateFilter);
          deathStarAboard = deathStars.HasCurrent();
          if (!anyShips || (needsDeathStar && deathStarAboard == 0))
          {
            stop = false;
          }
          else
          {
            stop = true;
          }
        }
      }
    }
  }
  if (ok != 0 && isOk(status) && !anyShips)
    setStatus(status, kFleetStatusCategory, kNoShips);
  if (ok != 0 && isOk(status) && needsDeathStar && deathStarAboard == 0)
    setStatus(status, kFleetStatusCategory, kNoDeathStar);
  return ok;
}
