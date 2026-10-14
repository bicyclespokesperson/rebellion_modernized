#include "events/command_classes2.h"
#include "economy/side.h"
#include "events/command_classes2_decl.h"

#include "character/character_mgr.h"
#include "data/dat_record.h"
#include "data/int_pair_node.h"
#include "economy/manu_mgr.h"
#include "events/event_helpers.h"
#include "events/event_post.h"
#include "fleet/fleet.h"
#include "gameobj/object_lookup.h"
#include "gameobj/object_type_db.h"
#include "mission/mission_external.h"
#include "orders/route_capacity.h"
#include "orders/route_context.h"
#include "rules/event_rules.h"
#include "rules/rules_mgr.h"
#include "strategy_ui/advisor_sides.h"

using events::asIntArg;
using events::checkOnTarget;
using events::executeOnTarget;
using events::kStatusCategory;
using events::kStatusMissingTarget;
using events::kStatusNoDestination;
using events::kStatusNoRoom;
using events::kStatusOk;
using events::kStatusRefused;
using events::succeeded;
using events::succeedWithoutChecks;

MoveCommand::MoveCommand() : Command()
{
}

MoveCommand::MoveCommand(const MoveCommand& other) = default;

MoveCommand::~MoveCommand() = default;

MoveCommand* MoveCommand::Clone()
{
  return NewOrNull<MoveCommand>(*this);
}

int MoveCommand::CheckEnabled(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_01_ff, status,
                       [this, status](GameObj* object)
                       {
                         int ok = object->CheckUnknownImpl(&m_request, status);
                         if (ok && succeeded(*status))
                         {
                           ObjectKey key;
                           ok = CheckMoveEnabled(object->GetKey(&key), status);
                         }
                         return ok;
                       });
}

int MoveCommand::CheckValid(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  GameObjNotifier* object = nullptr;
  int ok = 1;
  if (succeeded(*status))
    ok = GetGameRootMember_bc() != nullptr;
  if (ok && succeeded(*status))
  {
    const StatusCode missing(kStatusCategory, kStatusMissingTarget);
    ok = ResolveMasterInFamilies_01_ff(&m_key, &object, &missing, status);
  }
  GameObjNotifier* destination = nullptr;
  StatusCode destinationStatus(kStatusCategory, kStatusOk);
  if (ok)
  {
    if (succeeded(*status))
    {
      const StatusCode noDestination(kStatusCategory, kStatusNoDestination);
      ok = ResolveMasterInFamilies_01_ff(&m_request.m_target, &destination, &noDestination, &destinationStatus);
      if (!ok)
        return 0;
      if (!destination)
        *status = destinationStatus;
    }
    if (succeeded(*status))
      ok = static_cast<GameObj*>(object)->CheckUnknown(&m_request, status);
    if (ok)
    {
      ObjectKey destinationKey;
      if (succeeded(*status))
        ok = GetRulesMgr()->RulesMgr::CheckRequestMove(static_cast<GameObj*>(object),
                                                       destination->GetKey(&destinationKey), &m_request, status);
      if (ok && succeeded(*status))
      {
        ObjectKey objectKey;
        ObjectKey target;
        ok = CheckMoveValid(object->GetKey(&objectKey), destination->GetKey(&target), status);
      }
    }
  }
  return ok;
}

int MoveCommand::Execute()
{
  NotifyContext ctx(m_context);
  GameObjNotifier* object = nullptr;
  StatusCode status(kStatusCategory, kStatusOk);
  const StatusCode missing(kStatusCategory, kStatusMissingTarget);
  int ok = ResolveMasterInFamilies_01_ff(&m_key, &object, &missing, &status);
  GameObjNotifier* destination = nullptr;
  StatusCode destinationStatus(kStatusCategory, kStatusOk);
  if (ok)
  {
    if (succeeded(status))
    {
      const StatusCode noDestination(kStatusCategory, kStatusNoDestination);
      ok = ResolveMasterInFamilies_01_ff(&m_request.m_target, &destination, &noDestination, &destinationStatus);
      if (!ok)
        return 0;
      if (!destination)
        status = destinationStatus;
    }
    if (ok && succeeded(status))
    {
      ok = CheckValid(&status);
      if (ok && succeeded(status))
      {
        ObjectKey destinationKey;
        ObjectKey objectKey;
        ExecuteMove(object->GetKey(&objectKey), destination->GetKey(&destinationKey), &ctx);
      }
    }
  }
  return 0;
}

MoveCommand* CreateMoveCommand()
{
  return NewOrNull<MoveCommand>();
}

RenameCommand::RenameCommand() : Command(), m_name()
{
}

RenameCommand::RenameCommand(const RenameCommand& other) = default;

RenameCommand::~RenameCommand() = default;

RenameCommand* RenameCommand::Clone()
{
  return NewOrNull<RenameCommand>(*this);
}

void RenameCommand::Read(istream* in)
{
  ReadCommand(in);
  ReadLegacyString(in, m_name);
}

void RenameCommand::Write(ostream* out)
{
  WriteCommand(out);
  WriteLegacyString(out, m_name);
}

int RenameCommand::CheckEnabled(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_01_ff, status,
                       [this, status](GameObj* object)
                       {
                         return object->CheckRenameImpl(&m_request, status);
                       });
}

int RenameCommand::CheckValid(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_01_ff, status,
                       [this, status](GameObj* object)
                       {
                         return object->CheckRename(&m_request, asIntArg(&m_name), status);
                       });
}

int RenameCommand::Execute()
{
  return executeOnTarget(this, ResolveMasterInFamilies_01_ff,
                         [this](GameObj* object, NotifyContext* ctx)
                         {
                           object->TryRename(&m_request, asIntArg(&m_name), ctx);
                         });
}

RenameCommand* CreateRenameCommand()
{
  return NewOrNull<RenameCommand>();
}

ScrapCommand::ScrapCommand() : Command()
{
}

ScrapCommand::ScrapCommand(const ScrapCommand& other) = default;

ScrapCommand::~ScrapCommand() = default;

ScrapCommand* ScrapCommand::Clone()
{
  return NewOrNull<ScrapCommand>(*this);
}

int ScrapCommand::CheckEnabled(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_01_ff, status,
                       [this, status](GameObj* object)
                       {
                         return object->CheckRequestImpl(&m_request, status);
                       });
}

int ScrapCommand::CheckValid(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_01_ff, status,
                       [this, status](GameObj* object)
                       {
                         return object->CheckRequest(&m_request, status);
                       });
}

int ScrapCommand::Execute()
{
  return executeOnTarget(this, ResolveMasterInFamilies_01_ff,
                         [this](GameObj* object, NotifyContext* ctx)
                         {
                           object->DestroyIfAllowed(&m_request, ctx);
                         });
}

ScrapCommand* CreateScrapCommand()
{
  return NewOrNull<ScrapCommand>();
}

ReadyForDeleteCommand::ReadyForDeleteCommand() : Command()
{
}

ReadyForDeleteCommand::ReadyForDeleteCommand(const ReadyForDeleteCommand& other) = default;

ReadyForDeleteCommand::~ReadyForDeleteCommand() = default;

ReadyForDeleteCommand* ReadyForDeleteCommand::Clone()
{
  return NewOrNull<ReadyForDeleteCommand>(*this);
}

int ReadyForDeleteCommand::CheckEnabled(StatusCode* status)
{
  return succeedWithoutChecks(status);
}

int ReadyForDeleteCommand::CheckValid(StatusCode* status)
{
  return succeedWithoutChecks(status);
}

int ReadyForDeleteCommand::Execute()
{
  NotifyContext ctx(m_context);
  auto* object = static_cast<GameObj*>(GetObjectInFamilies_01_ff(m_request.m_side, &m_key));
  if (object)
    object->ReleaseIfDestroyed(&ctx);
  return 0;
}

ReadyForDeleteCommand* CreateReadyForDeleteCommand()
{
  return NewOrNull<ReadyForDeleteCommand>();
}

RootCommand240::RootCommand240() : Command()
{
}

RootCommand240::RootCommand240(const RootCommand240& other) = default;

RootCommand240::~RootCommand240() = default;

RootCommand240* RootCommand240::Clone()
{
  return NewOrNull<RootCommand240>(*this);
}

int RootCommand240::IsChecked()
{
  int result = 0;
  CharacterMgr* manager = GetCharacterManager();
  if (manager)
  {
    ObjectKey key = m_key;
    manager->ForwardMatchesRequestedCommandKind(&key, events::asIntArg(&m_request), &result);
  }
  return result;
}

int RootCommand240::CheckEnabled(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusRefused);
  CharacterMgr* manager = GetCharacterManager();
  int ok = 0;
  if (manager)
  {
    ObjectKey key = m_key;
    ok = manager->ForwardCheckActivity3(&key, &m_request, status);
  }
  return ok;
}

int RootCommand240::CheckValid(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusRefused);
  CharacterMgr* manager = GetCharacterManager();
  int ok = 0;
  if (manager)
  {
    ObjectKey key = m_key;
    ok = manager->ForwardCheckActivityStatus3(&key, &m_request, status);
  }
  return ok;
}

int RootCommand240::Execute()
{
  NotifyContext ctx(m_context);
  CharacterMgr* manager = GetCharacterManager();
  if (manager)
  {
    ObjectKey key = m_key;
    manager->AssignCommand(&key, &m_request, &ctx);
  }
  return 0;
}

RootCommand240* CreateRootCommand240()
{
  return NewOrNull<RootCommand240>();
}

RootCommand242::RootCommand242() : Command()
{
}

RootCommand242::RootCommand242(const RootCommand242& other) = default;

RootCommand242::~RootCommand242() = default;

RootCommand242* RootCommand242::Clone()
{
  return NewOrNull<RootCommand242>(*this);
}

int RootCommand242::CheckEnabled(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusRefused);
  CharacterMgr* manager = GetCharacterManager();
  int ok = 0;
  if (manager)
  {
    ObjectKey key = m_key;
    ok = manager->ForwardCheckActivity4(&key, &m_request, status);
  }
  return ok;
}

int RootCommand242::CheckValid(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusRefused);
  CharacterMgr* manager = GetCharacterManager();
  int ok = 0;
  if (manager)
  {
    ObjectKey key = m_key;
    ok = manager->ForwardCheckActivityStatus4(&key, &m_request, status);
  }
  return ok;
}

int RootCommand242::Execute()
{
  NotifyContext ctx(m_context);
  CharacterMgr* manager = GetCharacterManager();
  if (manager)
  {
    ObjectKey key = m_key;
    manager->ReportTarget(&key, &m_request, &ctx);
  }
  return 0;
}

RootCommand242* CreateRootCommand242()
{
  return NewOrNull<RootCommand242>();
}

DestinationCommand::DestinationCommand() : Command(), m_pairs()
{
}

DestinationCommand::DestinationCommand(const DestinationCommand& other) = default;

DestinationCommand::~DestinationCommand() = default;

DestinationCommand* DestinationCommand::Clone()
{
  return NewOrNull<DestinationCommand>(*this);
}

void DestinationCommand::Read(istream* in)
{
  ReadCommand(in);
  m_pairs.Load(in);
}

void DestinationCommand::Write(ostream* out)
{
  WriteCommand(out);
  m_pairs.Save(out);
}

void DestinationCommand::Checksum(Crc32* crc)
{
  ChecksumCommand(crc);
  m_pairs.Checksum(crc);
}

int DestinationCommand::CheckEnabled(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<ManuMgr*>(object)->CheckPlain2(&m_request, status);
                       });
}

int DestinationCommand::CheckValid(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  GameObjNotifier* manager = nullptr;
  int ok = 1;
  if (succeeded(*status))
  {
    const StatusCode missing(kStatusCategory, kStatusMissingTarget);
    ok = ResolveMasterInFamilies_a0_b0(&m_key, &manager, &missing, status);
  }
  GameObjNotifier* destination = nullptr;
  StatusCode destinationStatus(kStatusCategory, kStatusOk);
  if (!ok)
    return 0;
  if (succeeded(*status))
  {
    const StatusCode noDestination(kStatusCategory, kStatusNoDestination);
    ok = ResolveMasterInFamilies_01_ff(&m_request.m_target, &destination, &noDestination, &destinationStatus);
    if (!ok)
      return 0;
    if (!destination)
      *status = destinationStatus;
  }
  if (!ok || !succeeded(*status))
    return ok;
  ok = static_cast<ManuMgr*>(manager)->CheckRequestedObject(&m_request, events::asIntArg(&m_pairs), status);
  if (!ok || !succeeded(*status) || static_cast<ManuMgr*>(manager)->m_remainingGameObjCount == 0)
    return ok;
  RouteContext route;
  if (succeeded(*status))
  {
    int mode = 3;
    ok = route.InitFromRequest(&m_request, manager->ControlSide(), &mode);
    if (ok)
    {
      ObjectKey destinationKey;
      ok = route.AddContainerLegs(destination->GetKey(&destinationKey));
      if (ok)
        ok = route.FreeCarriedParents(static_cast<GameObj*>(manager), status);
    }
  }
  if (!ok)
  {
    *status = StatusCode(kStatusCategory, kStatusRefused);
  }
  else
  {
    for (const auto& entry : m_pairs.Entries())
    {
      if (!ok || !succeeded(*status))
        break;
      auto* node = entry.get();
      CapacityScratch slot;
      if (ok && succeeded(*status))
      {
        auto* pair = static_cast<ObjectKeyPairNode*>(node);
        ok = route.CheckUnitArrival(&pair->m_second, &pair->m_key, status, &slot);
      }
    }
    if (!ok)
      *status = StatusCode(kStatusCategory, kStatusRefused);
  }
  return ok;
}

int DestinationCommand::Execute()
{
  return executeOnTarget(this, ResolveMasterInFamilies_a0_b0,
                         [this](GameObj* object, NotifyContext* ctx)
                         {
                           static_cast<ManuMgr*>(object)->ReparentProducts(&m_request, &m_pairs, ctx);
                         });
}

DestinationCommand* CreateDestinationCommand()
{
  return NewOrNull<DestinationCommand>();
}

ManagerRenameCommand::ManagerRenameCommand() : Command(), m_name()
{
}

ManagerRenameCommand::ManagerRenameCommand(const ManagerRenameCommand& other) = default;

ManagerRenameCommand::~ManagerRenameCommand() = default;

ManagerRenameCommand* ManagerRenameCommand::Clone()
{
  return NewOrNull<ManagerRenameCommand>(*this);
}

void ManagerRenameCommand::Read(istream* in)
{
  ReadCommand(in);
  ReadLegacyString(in, m_name);
}

void ManagerRenameCommand::Write(ostream* out)
{
  WriteCommand(out);
  WriteLegacyString(out, m_name);
}

int ManagerRenameCommand::CheckEnabled(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<ManuMgr*>(object)->CheckProductActionImpl(&m_request, status);
                       });
}

int ManagerRenameCommand::CheckValid(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<ManuMgr*>(object)->CheckProductAction(&m_request, events::asIntArg(&m_name),
                                                                                  status);
                       });
}

int ManagerRenameCommand::Execute()
{
  return executeOnTarget(this, ResolveMasterInFamilies_a0_b0,
                         [this](GameObj* object, NotifyContext* ctx)
                         {
                           static_cast<ManuMgr*>(object)->RenameProduct(&m_request, &m_name, ctx);
                         });
}

ManagerRenameCommand* CreateManagerRenameCommand()
{
  return NewOrNull<ManagerRenameCommand>();
}

ManagerCommand213::ManagerCommand213() : Command()
{
}

ManagerCommand213::ManagerCommand213(const ManagerCommand213& other) = default;

ManagerCommand213::~ManagerCommand213() = default;

ManagerCommand213* ManagerCommand213::Clone()
{
  return NewOrNull<ManagerCommand213>(*this);
}

int ManagerCommand213::CheckEnabled(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<ManuMgr*>(object)->CheckHasProductImpl(&m_request, status);
                       });
}

int ManagerCommand213::CheckValid(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<ManuMgr*>(object)->CheckHasProduct(&m_request, status);
                       });
}

int ManagerCommand213::Execute()
{
  return executeOnTarget(this, ResolveMasterInFamilies_a0_b0,
                         [this](GameObj* object, NotifyContext* ctx)
                         {
                           static_cast<ManuMgr*>(object)->CancelProducts(&m_request, ctx);
                         });
}

ManagerCommand213* CreateManagerCommand213()
{
  return NewOrNull<ManagerCommand213>();
}

ManagerCommand216::ManagerCommand216() : Command(), m_value(0)
{
}

ManagerCommand216::ManagerCommand216(const ManagerCommand216& other) = default;

ManagerCommand216::~ManagerCommand216() = default;

ManagerCommand216* ManagerCommand216::Clone()
{
  return NewOrNull<ManagerCommand216>(*this);
}

void ManagerCommand216::Read(istream* in)
{
  ReadCommand(in);
  ReadValue32(in, &m_value);
}

void ManagerCommand216::Write(ostream* out)
{
  WriteCommand(out);
  WriteValue32(out, &m_value);
}

void ManagerCommand216::Checksum(Crc32* crc)
{
  ChecksumCommand(crc);
  crc->AddInt(m_value);
}

int ManagerCommand216::IsChecked()
{
  int result = 0;
  GameObjNotifier* manager = nullptr;
  StatusCode status(kStatusCategory, kStatusOk);
  const StatusCode missing(kStatusCategory, kStatusMissingTarget);
  const int ok = ResolveMasterInFamilies_a0_b0(&m_key, &manager, &missing, &status);
  if (ok && succeeded(status))
    static_cast<ManuMgr*>(manager)->GetReserved(events::asIntArg(&m_request), &result);
  return result;
}

int ManagerCommand216::CheckEnabled(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<ManuMgr*>(object)->CheckPlainImpl(&m_request, m_value, status);
                       });
}

int ManagerCommand216::CheckValid(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<ManuMgr*>(object)->CheckPlain(&m_request, m_value, status);
                       });
}

int ManagerCommand216::Execute()
{
  return executeOnTarget(this, ResolveMasterInFamilies_a0_b0,
                         [this](GameObj* object, NotifyContext* ctx)
                         {
                           static_cast<ManuMgr*>(object)->ReserveChecked(&m_request, m_value, ctx);
                         });
}

ManagerCommand216* CreateManagerCommand216()
{
  return NewOrNull<ManagerCommand216>();
}

ManufactureCommand::ManufactureCommand() : Command(), m_items()
{
}

ManufactureCommand::ManufactureCommand(const ManufactureCommand& other) = default;

ManufactureCommand::~ManufactureCommand() = default;

ManufactureCommand* ManufactureCommand::Clone()
{
  return NewOrNull<ManufactureCommand>(*this);
}

void ManufactureCommand::Read(istream* in)
{
  ReadCommand(in);
  m_items.Load(in);
}

void ManufactureCommand::Write(ostream* out)
{
  WriteCommand(out);
  m_items.Save(out);
}

void ManufactureCommand::Checksum(Crc32* crc)
{
  ChecksumCommand(crc);
  m_items.Checksum(crc);
}

int ManufactureCommand::CheckEnabled(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_a0_b0, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<ManuMgr*>(object)->CheckFacilityAvailableImpl(&m_request, status);
                       });
}

int ManufactureCommand::CheckValid(StatusCode* status)
{
  *status = StatusCode(kStatusCategory, kStatusOk);
  GameObjNotifier* manager = nullptr;
  int ok = 1;
  if (succeeded(*status))
  {
    const StatusCode missing(kStatusCategory, kStatusMissingTarget);
    ok = ResolveMasterInFamilies_a0_b0(&m_key, &manager, &missing, status);
  }
  if (ok)
  {
    if (succeeded(*status))
    {
      ok = static_cast<ManuMgr*>(manager)->CheckFacilityAvailable(&m_request, events::asIntArg(&m_items), status);
      if (ok && succeeded(*status))
      {
        RouteContext route;
        ObjectKey managerKey;
        if (succeeded(*status))
        {
          int mode = 3;
          ok = route.InitFromRequest(&m_request, manager->ControlSide(), &mode);
          if (ok)
          {
            ok = route.AddContainerLegs(ManagerGetLocationKey(manager, &managerKey));
            if (ok)
              ok = route.FreeCarriedParents(static_cast<GameObj*>(manager), status);
          }
        }
        ObjectKey previous;
        previous.m_value = 0;
        int total = 0;
        int count = 0;
        for (auto position = m_items.Entries().begin();
             position != m_items.Entries().end() && (ok && succeeded(*status)); ++position)
        {
          auto* node = static_cast<IntPairNode*>((*position).get());
          CapacityScratch slot;
          ObjectKey unit;
          unit.m_value = static_cast<uint32_t>(node->m_value);
          if (ok)
          {
            if (succeeded(*status))
              ok = route.CheckFamilyArrival(&unit, &node->m_key, status, &slot);
            if (ok && succeeded(*status))
            {
              if (unit.m_value != previous.m_value)
              {
                previous = unit;
                count = 0;
                if (previous.Family() >= 0x10 && previous.Family() < 0x40)
                {
                  auto* record = static_cast<SdRecord*>(FindDataRecord(AsLookupKey(&previous)));
                  ok = record != nullptr;
                  if (ok)
                    count = static_cast<int>(record->GetField4c());
                }
              }
              if (ok)
                total += count;
            }
          }
        }
        if (ok && succeeded(*status))
        {
          Side* sideObject = nullptr;
          ok = GetControlSideObject(manager, reinterpret_cast<GameObjNotifier**>(&sideObject));
          if (total != 0 && sideObject->m_maintState.m_first - sideObject->m_maintState.m_second < total)
            *status = StatusCode(kStatusNoRoom, kStatusRefused);
        }
      }
    }
    if (ok)
      return ok;
  }
  *status = StatusCode(kStatusCategory, kStatusRefused);
  return 0;
}

int ManufactureCommand::Execute()
{
  return executeOnTarget(this, ResolveMasterInFamilies_a0_b0,
                         [this](GameObj* object, NotifyContext* ctx)
                         {
                           static_cast<ManuMgr*>(object)->ReplaceQueue(&m_request, &m_items, ctx);
                         });
}

ManufactureCommand* CreateManufactureCommand()
{
  return NewOrNull<ManufactureCommand>();
}

FleetCommand221::FleetCommand221() : Command()
{
}

FleetCommand221::FleetCommand221(const FleetCommand221& other) = default;

FleetCommand221::~FleetCommand221() = default;

FleetCommand221* FleetCommand221::Clone()
{
  return NewOrNull<FleetCommand221>(*this);
}

int FleetCommand221::CheckEnabled(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_08_10, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<Fleet*>(object)->CheckAssaultImpl(&m_request, status);
                       });
}

int FleetCommand221::CheckValid(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_08_10, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<Fleet*>(object)->CheckAssault(&m_request, status);
                       });
}

int FleetCommand221::Execute()
{
  return executeOnTarget(this, ResolveMasterInFamilies_08_10,
                         [this](GameObj* object, NotifyContext* ctx)
                         {
                           static_cast<Fleet*>(object)->StartAssault(events::asIntArg(&m_request), ctx);
                         });
}

FleetCommand221* CreateFleetCommand221()
{
  return NewOrNull<FleetCommand221>();
}

FleetCommand220::FleetCommand220() : Command()
{
}

FleetCommand220::FleetCommand220(const FleetCommand220& other) = default;

FleetCommand220::~FleetCommand220() = default;

FleetCommand220* FleetCommand220::Clone()
{
  return NewOrNull<FleetCommand220>(*this);
}

int FleetCommand220::CheckEnabled(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_08_10, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<Fleet*>(object)->CheckBombardImpl(&m_request, status);
                       });
}

int FleetCommand220::CheckValid(StatusCode* status)
{
  return checkOnTarget(this, ResolveMasterInFamilies_08_10, status,
                       [this, status](GameObj* object)
                       {
                         return static_cast<Fleet*>(object)->CheckBombard(&m_request, status);
                       });
}

int FleetCommand220::Execute()
{
  return executeOnTarget(this, ResolveMasterInFamilies_08_10,
                         [this](GameObj* object, NotifyContext* ctx)
                         {
                           static_cast<Fleet*>(object)->StartBombard(events::asIntArg(&m_request), ctx);
                         });
}

FleetCommand220* CreateFleetCommand220()
{
  return NewOrNull<FleetCommand220>();
}

PayloadEvent3f0::PayloadEvent3f0() : TimedEvent(), m_valueA(0), m_valueB(0), m_valueC(0)
{
}

PayloadEvent3f0::PayloadEvent3f0(const PayloadEvent3f0& other) = default;

PayloadEvent3f0::~PayloadEvent3f0() = default;

PayloadEvent3f0* PayloadEvent3f0::Clone()
{
  return NewOrNull<PayloadEvent3f0>(*this);
}

void PayloadEvent3f0::Read(istream* in)
{
  ReadTimed(in);
  ReadValue32(in, &m_valueA);
  ReadValue32(in, &m_valueB);
  ReadValue32(in, &m_valueC);
}

void PayloadEvent3f0::Write(ostream* out)
{
  WriteTimed(out);
  WriteValue32(out, &m_valueA);
  WriteValue32(out, &m_valueB);
  WriteValue32(out, &m_valueC);
}

void PayloadEvent3f0::Checksum(Crc32* crc)
{
  ChecksumTimed(crc);
  crc->AddInt(m_valueA);
  crc->AddInt(m_valueB);
  crc->AddInt(m_valueC);
}

int PayloadEvent3f0::CheckEnabled(StatusCode* status)
{
  return succeedWithoutChecks(status);
}

int PayloadEvent3f0::CheckValid(StatusCode* status)
{
  return succeedWithoutChecks(status);
}

int PayloadEvent3f0::Execute()
{
  GameObjNotifier* target = nullptr;
  StatusCode status(kStatusCategory, kStatusOk);
  const StatusCode missing(kStatusCategory, kStatusMissingTarget);
  ResolveMasterInFamilies_01_ff(&m_key, &target, &missing, &status);
  if (target)
  {
    NotifyContext ctx(m_context);
    PostPayloadFourInts(1, target, 0, 0, m_valueA, m_valueB, &ctx);
  }
  return 0;
}

PayloadEvent3f0::PayloadEvent3f0(int valueA, int valueB, int valueC)
: TimedEvent(),
  m_valueA(valueA),
  m_valueB(valueB),
  m_valueC(valueC)
{
}

PayloadEvent3f0* CreatePayloadEvent3f0()
{
  return NewOrNull<PayloadEvent3f0>();
}
