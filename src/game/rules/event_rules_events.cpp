#include "character/character_mgr.h"
#include "data/gnprtb.h"
#include "economy/manu_mgr.h"
#include "economy/side.h"
#include "events/event_helpers.h"
#include "events/event_post.h"
#include "fleet/capship_data.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/system.h"
#include "galaxy/system_rules.h"
#include "galaxy/system_rules_1e.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_root.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"
#include "gameobj/side_bits.h"
#include "mission/mission_rules.h"
#include "rules/event_rules.h"
#include "rules/loyalty_params.h"
#include "rules/rules_mgr.h"
#include "story/mission_mgr.h"

using namespace event_rules;
using events::asIntArg;

// The event handlers (see event_rules.h). Every handler looks the object up by key and then runs its steps in a fixed order;
// a step that fails does not stop the ones after it.

namespace
{

constexpr FamilyRange kSystemFamilies(0x90, 0x98);
constexpr FamilyRange kUnitFamilies(0x10, 0x40);
constexpr FamilyRange kFacilityFamilies(0x20, 0x30);
constexpr FamilyRange kStationFamilies(0x20, 0x22);
constexpr FamilyRange kCharacterFamilies(0x30, 0x40);
constexpr FamilyRange kMajorCharacterFamilies(0x30, 0x38);
constexpr FamilyRange kDeathStarFamilies(0x18, 0x1c);
constexpr FamilyRange kShipFamilies(0x10, 0x40);
constexpr FamilyRange kCapitalShipFamilies(0x14, 0x20);
constexpr FamilyRange kShipsAndFightersFamilies(0x14, 0x1c);
constexpr FamilyRange kFleetsFamilies(0x08, 0x10);
constexpr FamilyRange kMovingFamilies(0x10, 0x40);
constexpr uint32_t kBothSidesMask = 0xc0;
constexpr int kMineFamily = 0x2c;
constexpr int kRefineryFamily = 0x2d;
constexpr int kDestroyedReasonLostLocation = 2;
constexpr int kDestroyedReasonStranded = 5;
constexpr int kEventScrapReason = 0;
constexpr size_t kRootFlagOffset = 0x70;

int both(int result, int ok)
{
  return result != 0 && ok != 0 ? 1 : 0;
}

GameObj* asObject(GameObjNotifier* object)
{
  return static_cast<GameObj*>(object);
}

bool inFamilies(const GameObjNotifier* object, const FamilyRange& range)
{
  const uint32_t family = const_cast<GameObjNotifier*>(object)->GetFamily();
  return family >= range.m_first && family < range.m_second;
}

MissionMgr* missionMgr()
{
  return static_cast<MissionMgr*>(GetGameRootMember_b8());
}
RulesMgr* rulesMgr()
{
  return static_cast<RulesMgr*>(GetGameRootMember_bc());
}
CharacterMgr* characterMgr()
{
  return static_cast<CharacterMgr*>(GetGameRootMember_c0());
}

using ContainerNotify = int (GameObj::*)(intptr_t, int, intptr_t);

int notifyContainer(GameObjNotifier* container, ContainerNotify notify, GameObj* object, int value, NotifyContext* ctx)
{
  return (static_cast<GameObj*>(container)->*notify)(asIntArg(object), value, asIntArg(ctx));
}

constexpr ContainerNotify kNotifyUnitExisting = &GameObj::NotifyUnitExisting;
constexpr ContainerNotify kNotifyUnitUsable = &GameObj::NotifyUnitUsable;
constexpr ContainerNotify kNotifyUnitDamaged = &GameObj::NotifyUnitDamaged;
constexpr ContainerNotify kNotifyOwnerExisting = &GameObj::NotifyOwnerExisting;
constexpr ContainerNotify kNotifyOwnerUsable = &GameObj::NotifyOwnerUsable;

} // namespace

// ---- unit and object events -------------------------------------------------------------------------------------

int ApplyEvent300(ObjectKey* key, ObjectKey* from, ObjectKey* to, int value, NotifyContext* ctx)
{
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  if (object == nullptr)
    return 1;
  MissionMgr* missions = missionMgr();
  int ok = missions != nullptr;
  if (missions != nullptr)
    ok = both(missions->UpdateMissionsForObject(key, asIntArg(from), to, value, ctx), ok);
  CharacterMgr* characters = characterMgr();
  ok = both(characters != nullptr, ok);
  if (characters != nullptr)
    ok = both(characters->OnFacilityOrPersonMoved(key, from, to, value, ctx), ok);
  if ((object->m_stateFlags & kDestroyed) != 0)
    ok = both(RetargetIdleManufacturers(object, ctx), ok);
  if ((object->m_stateFlags & kCompleted) == 0 && object->m_builderKey.IsSet())
  {
    GameObjNotifier* owner = GetMasterInFamilies_a0_b0(&object->m_builderKey);
    ok = both(owner != nullptr, ok);
    if (owner != nullptr)
      ok = both(static_cast<ManuMgr*>(owner)->HandleProductEvent(key, asIntArg(from), asIntArg(to), value, ctx), ok);
  }
  GameObj* fromObject = asObject(GetMasterInFamilies_01_ff(from));
  ok = both(fromObject != nullptr, ok);
  if (fromObject != nullptr)
    ok = both(notifyContainer(fromObject, kNotifyUnitDamaged, object, value, ctx), ok);
  if (fromObject != nullptr)
    ok = both(notifyContainer(fromObject, kNotifyUnitExisting, object, value, ctx), ok);
  GameObj* toObject = nullptr;
  if (to->IsSet())
    toObject = asObject(GetMasterInFamilies_01_ff(to));
  if (toObject != nullptr)
    ok = both(notifyContainer(toObject, kNotifyOwnerExisting, object, value, ctx), ok);
  ok = both(NumberUnitName(object, ctx), ok);
  if (inFamilies(object, kUnitFamilies))
  {
    GameObjNotifier* sideObject = nullptr;
    ok = both(GetControlSideObject(object, &sideObject), ok);
    if (sideObject != nullptr)
    {
      ok = both(static_cast<Side*>(sideObject)->RecomputeMaintenance(ctx), ok);
      if ((object->m_stateFlags & kDestroyed) != 0)
        ok = both(static_cast<Side*>(sideObject)->RefundScrappedUnit(object, ctx), ok);
    }
    if (inFamilies(object, kFacilityFamilies) && inFamilies(object, kStationFamilies))
      ok = both(ApplyDepartureRules(object, value, to, ctx), ok);
  }
  if (inFamilies(object, kSystemFamilies))
    ok = both(ApplyCoruscantRules(object, value, ctx), ok);
  return both(PostDeleteIfDestroyedAndEmpty(object, ctx), ok);
}

int ApplyEvent301(ObjectKey* key, ObjectKey* from, ObjectKey* to, int value, NotifyContext* ctx)
{
  int result = 1;
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  if (object == nullptr)
    return result;
  MissionMgr* missions = missionMgr();
  int ok = missions != nullptr;
  if (missions != nullptr)
    ok = both(missions->UpdateMissionsForObjective(key, asIntArg(from), asIntArg(to), value, ctx), ok);
  CharacterMgr* characters = characterMgr();
  ok = both(characters != nullptr, ok);
  if (characters != nullptr)
    ok = both(characters->OnEmperorMoved(key, from, to, value, ctx), ok);
  GameObj* fromObject = asObject(GetMasterInFamilies_01_ff(from));
  result = both(fromObject != nullptr, ok);
  if (fromObject != nullptr)
    result = both(notifyContainer(fromObject, kNotifyUnitUsable, object, value, ctx), result);
  GameObj* toObject = nullptr;
  if (to->IsSet())
    toObject = asObject(GetMasterInFamilies_01_ff(to));
  if (toObject != nullptr)
    result = both(notifyContainer(toObject, kNotifyOwnerUsable, object, value, ctx), result);
  if (inFamilies(object, kUnitFamilies))
  {
    GameObjNotifier* sideObject = nullptr;
    result = both(GetControlSideObject(object, &sideObject), result);
    if (sideObject != nullptr)
      result = both(static_cast<Side*>(sideObject)->RecomputeMaintenance(ctx), result);
    if (inFamilies(object, kFacilityFamilies) && inFamilies(object, kStationFamilies))
      result = both(ApplyEnrouteRules(object, value, to, ctx), result);
    if (inFamilies(object, kDeathStarFamilies))
      result = both(UpdateDeathStarNearby(ctx), result);
  }
  return result;
}

int ApplyEvent31d(ObjectKey* key, ObjectKey* from, ObjectKey*, int value, NotifyContext* ctx)
{
  int result = 1;
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  if (object != nullptr)
  {
    GameObj* fromObject = asObject(GetMasterInFamilies_01_ff(from));
    result = fromObject != nullptr;
    if (fromObject != nullptr)
    {
      if (notifyContainer(fromObject, kNotifyUnitDamaged, object, value, ctx) != 0 && result != 0)
        return 1;
      result = 0;
    }
  }
  return result;
}

int ApplyEvent302(ObjectKey* key, int valueA, int valueB, NotifyContext* ctx)
{
  int result = 1;
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  if (object == nullptr)
    return result;
  MissionMgr* missions = missionMgr();
  int ok = missions != nullptr;
  if (missions != nullptr)
    ok = both(missions->UpdateMissionsForTargetOrObjective(key, valueA, valueB, ctx), ok);
  result = both(RetargetIdleManufacturers(object, ctx), ok);
  if (inFamilies(object, kUnitFamilies))
  {
    GameObjNotifier* sideA = nullptr;
    GameObjNotifier* sideB = nullptr;
    const int first = GetSideObjectForIndex(valueA, &sideA);
    const int second = GetSideObjectForIndex(valueB, &sideB);
    result = both(second, both(first, result));
    if (sideA != nullptr)
      result = both(static_cast<Side*>(sideA)->RecomputeMaintenance(ctx), result);
    if (sideB != nullptr)
      result = both(static_cast<Side*>(sideB)->RecomputeMaintenance(ctx), result);
  }
  return result;
}

int ApplyEvent318(ObjectKey* key, int valueA, int valueB, NotifyContext* ctx)
{
  int result = 1;
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  if (object != nullptr)
    result = object->ApplyAllToViewOf(valueA, valueB, ctx) != 0;
  return result;
}

int ApplyEvent320(ObjectKey* key, ObjectKey* keyC, ObjectKey* keyD, NotifyContext* ctx)
{
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  if (object == nullptr)
    return 1;
  MissionMgr* missions = missionMgr();
  int ok = missions != nullptr;
  if (missions != nullptr)
    ok = both(missions->RunStoryPipelineForStoryKey(key, asIntArg(keyC), asIntArg(keyD), ctx), ok);
  CharacterMgr* characters = characterMgr();
  ok = both(characters != nullptr, ok);
  if (characters != nullptr)
    ok = both(characters->OnLocationChanged(key, keyC, keyD, ctx), ok);
  if ((object->m_stateFlags & kCompleted) == 0 && object->m_builderKey.IsSet())
  {
    GameObjNotifier* owner = GetMasterInFamilies_a0_b0(&object->m_builderKey);
    ok = both(owner != nullptr, ok);
    if (owner != nullptr)
      ok = both(static_cast<ManuMgr*>(owner)->OnProductChanged(key, asIntArg(keyC), asIntArg(keyD), ctx), ok);
  }
  GameObj* fromObject = asObject(GetMasterInFamilies_01_ff(keyC));
  ok = both(fromObject != nullptr, ok);
  GameObj* toObject = asObject(GetMasterInFamilies_01_ff(keyD));
  ok = both(toObject != nullptr, ok);
  RulesMgr* rules = rulesMgr();
  ok = both(rules != nullptr, ok);
  if (rules != nullptr)
    ok = both(rules->BeginTravel(object, keyC, keyD, ctx), ok);
  if ((object->m_stateFlags & kExisting) != 0)
  {
    if (fromObject != nullptr)
      ok = both(notifyContainer(fromObject, kNotifyUnitExisting, object, 0, ctx), ok);
    if (toObject != nullptr)
      ok = both(notifyContainer(toObject, kNotifyUnitExisting, object, 1, ctx), ok);
  }
  if ((object->m_stateFlags & kUsable) != 0)
  {
    if (fromObject != nullptr)
      ok = both(notifyContainer(fromObject, kNotifyUnitUsable, object, 0, ctx), ok);
    if (toObject != nullptr)
      ok = both(notifyContainer(toObject, kNotifyUnitUsable, object, 1, ctx), ok);
  }
  if ((object->m_stateFlags & kDamaged) != 0)
  {
    if (fromObject != nullptr)
      ok = both(notifyContainer(fromObject, kNotifyUnitDamaged, object, 0, ctx), ok);
    if (toObject != nullptr)
      ok = both(notifyContainer(toObject, kNotifyUnitDamaged, object, 1, ctx), ok);
  }
  GameObjNotifier* fromOwner = nullptr;
  GameObjNotifier* toOwner = nullptr;
  ok = both(GetOwnerObject(fromObject, &fromOwner), ok);
  ok = both(GetOwnerObject(toObject, &toOwner), ok);
  bool sameOwner = false;
  if (fromOwner != nullptr && toOwner != nullptr)
  {
    ObjectKey toOwnerKey;
    ObjectKey fromOwnerKey;
    toOwner->GetKey(&toOwnerKey);
    fromOwner->GetKey(&fromOwnerKey);
    sameOwner = fromOwnerKey.m_value == toOwnerKey.m_value;
  }
  if (!sameOwner)
  {
    if ((object->m_stateFlags & kExisting) != 0)
    {
      if (fromOwner != nullptr)
        ok = both(notifyContainer(fromOwner, kNotifyOwnerExisting, object, 0, ctx), ok);
      if (toOwner != nullptr)
        ok = both(notifyContainer(toOwner, kNotifyOwnerExisting, object, 1, ctx), ok);
    }
    if ((object->m_stateFlags & kUsable) != 0)
    {
      if (fromOwner != nullptr)
        ok = both(notifyContainer(fromOwner, kNotifyOwnerUsable, object, 0, ctx), ok);
      if (toOwner != nullptr)
        ok = both(notifyContainer(toOwner, kNotifyOwnerUsable, object, 1, ctx), ok);
    }
  }
  GameObjNotifier* afterFrom = fromObject;
  if (ok != 0 && inFamilies(object, kUnitFamilies) && inFamilies(object, kCapitalShipFamilies) &&
      inFamilies(object, kShipsAndFightersFamilies) && inFamilies(object, kDeathStarFamilies))
  {
    GameObjNotifier* fromSector = nullptr;
    GameObjNotifier* toSector = nullptr;
    const int first = GetOwnerSector(fromObject, &fromSector);
    const int second = GetOwnerSector(toObject, &toSector);
    ok = second != 0 && first != 0;
    bool sameSector = false;
    if (fromSector != nullptr && toSector != nullptr)
    {
      ObjectKey toSectorKey;
      ObjectKey fromSectorKey;
      toSector->GetKey(&toSectorKey);
      fromSector->GetKey(&fromSectorKey);
      sameSector = fromSectorKey.m_value == toSectorKey.m_value;
    }
    if (!sameSector)
      ok = both(UpdateDeathStarNearby(ctx), ok);
  }
  if (afterFrom != nullptr)
    ok = both(PostDeleteIfDestroyedAndEmpty(asObject(afterFrom), ctx), ok);
  return both(ReleaseDestroyedObject(object, ctx), ok);
}

int ApplyEvent308(ObjectKey* key, int value, NotifyContext* ctx)
{
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  int ok = object != nullptr;
  if (object == nullptr)
    return ok;
  if (!inFamilies(object, kUnitFamilies))
    return ok;
  ObjectKey* builderKey = &object->m_builderKey;
  if (builderKey->IsSet())
  {
    GameObjNotifier* owner = GetMasterInFamilies_a0_b0(builderKey);
    ok = both(owner != nullptr, ok);
    if (owner != nullptr)
      ok = both(static_cast<ManuMgr*>(owner)->MoveOverflowToCompleted(key, value, ctx), ok);
  }
  if (inFamilies(object, kCharacterFamilies))
  {
    MissionMgr* missions = missionMgr();
    ok = both(missions != nullptr, ok);
    if (missions == nullptr)
      return ok;
    if (missions->OnCharacterEvent(key, value, ctx) != 0 && ok != 0)
      return 1;
  }
  else
  {
    RulesMgr* rules = rulesMgr();
    ok = both(rules != nullptr, ok);
    if (rules != nullptr && builderKey->IsSet())
    {
      ObjectKey objectKey;
      object->GetKey(&objectKey);
      ok = both(rules->BeginTravel(object, builderKey, &objectKey, ctx), ok);
    }
    if ((object->m_stateFlags & kEnrouteActive) != 0)
      return ok;
    if (rules->EnforceLocation(object, ctx) != 0 && ok != 0)
      return 1;
  }
  return 0;
}

int ApplyRoleStateEvent(ObjectKey* key, int value, NotifyContext* ctx)
{
  int result = 1;
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  if (object != nullptr && (object->m_stateFlags & kDestroyed) == 0 && inFamilies(object, kCharacterFamilies))
  {
    MissionMgr* missions = missionMgr();
    result = missions != nullptr;
    if (missions != nullptr)
    {
      if (missions->OnPersonStateChanged(key, value, ctx) != 0 && result != 0)
        return 1;
      result = 0;
    }
  }
  return result;
}

int ApplyEvent306(ObjectKey* key, int value, NotifyContext* ctx)
{
  int result = 1;
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  if (object == nullptr || (object->m_stateFlags & kDestroyed) != 0)
    return result;
  if (!inFamilies(object, kCharacterFamilies))
  {
    RulesMgr* rules = rulesMgr();
    result = rules != nullptr;
    uint32_t notHyperdrive = 1;
    if (rules != nullptr)
      result = both(rules->ResumeTrip(object, &notHyperdrive, ctx), result);
    if (notHyperdrive == 0)
      return result;
    if (object->SetEnrouteActive(0, ctx) != 0 && result != 0)
      return 1;
  }
  else
  {
    MissionMgr* missions = missionMgr();
    if (missions == nullptr)
      return 0;
    if (missions->UpdatePersonMission(key, value, ctx) != 0)
      return 1;
  }
  return 0;
}

int ApplyEvent316(ObjectKey* key, int value, NotifyContext* ctx)
{
  int result = 1;
  if (value == 0)
  {
    GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
    if (PostDeleteIfDestroyedAndEmpty(object, ctx) != 0 && object != nullptr)
      return 1;
    result = 0;
  }
  return result;
}

int ApplyEvent307(ObjectKey* key, int value, NotifyContext* ctx)
{
  GameObj* object = asObject(GetMasterInFamilies_01_ff(key));
  if (object == nullptr)
    return 1;
  if ((object->m_stateFlags & kDestroyed) != 0)
    return 1;
  if (inFamilies(object, kCharacterFamilies) && (object->m_stateFlags & kCompleted) != 0)
  {
    MissionMgr* missions = missionMgr();
    if (missions == nullptr)
      return 0;
    return missions->OnPersonStateChanged(key, value, ctx) != 0 ? 1 : 0;
  }
  if (value == 0)
    return 1;
  RulesMgr* rules = rulesMgr();
  int ok = rules != nullptr;
  ObjectKey destinationKey;
  ObjectKey parentKey;
  ObjectKey placedKey;
  if ((object->m_stateFlags & kCompleted) == 0)
  {
    GameObjNotifier* location = nullptr;
    GameObjNotifier* builderLocation = nullptr;
    ok = both(rules->GetOwnerObject(object, &location), ok);
    GameObjNotifier* builder = GetMasterInFamilies_01_ff(&object->m_builderKey);
    ok = both(builder != nullptr, ok);
    if (rules != nullptr && builder != nullptr)
      ok = both(rules->GetOwnerObject(builder, &builderLocation), ok);
    if (location != nullptr && builderLocation != nullptr)
    {
      ObjectKey locationKey;
      ObjectKey builderLocationKey;
      builderLocation->GetKey(&builderLocationKey);
      location->GetKey(&locationKey);
      if (locationKey.m_value != builderLocationKey.m_value)
      {
        builderLocation->GetKey(&builderLocationKey);
        new (&destinationKey) ObjectKey(builderLocationKey);
      }
    }
  }
  else if ((object->m_stateFlags & kEnrouteActive) != 0)
  {
    ok = both(object->GetDataKey(&destinationKey), ok);
    ok = both(object->GetParentKey(&parentKey), ok);
    if (destinationKey.m_value == parentKey.m_value)
    {
      destinationKey.Reset();
      ObjectKey cleared;
      ok = both(object->SetDataKey(&cleared), ok);
    }
  }
  ObjectKey ownerKeyValue;
  GetOwnerKeyValue(object, &ownerKeyValue);
  const bool ownerIsSystem =
    ownerKeyValue.Family() >= kSystemFamilies.m_first && ownerKeyValue.Family() < kSystemFamilies.m_second;
  int result = ownerIsSystem && ok != 0;
  if (rules != nullptr)
  {
    if (ownerIsSystem)
    {
      StatusCode status;
      GetOwnerKeyValue(object, &ownerKeyValue);
      result = both(rules->FindDestination(object, &ownerKeyValue, &destinationKey, 0, 1, &placedKey, &status), result);
      if (status.m_second != 0xffffffff)
        result = both(object->ChangeDestroyedReason(kDestroyedReasonLostLocation, ctx), result);
    }
    if ((object->m_stateFlags & kDestroyed) == 0 && (object->m_stateFlags & kCompleted) != 0)
    {
      uint32_t sameOwner = 1;
      ObjectKey objectKey;
      object->GetKey(&objectKey);
      result = both(rules->AreKeysInSameOwner(&objectKey, &placedKey, &sameOwner), result);
      if (sameOwner == 0 && object->GetTransitSpeed((object->m_stateFlags & kEnrouteActive) != 0) == 0)
        result = both(object->ChangeDestroyedReason(kDestroyedReasonStranded, ctx), result);
    }
    if ((object->m_stateFlags & kDestroyed) == 0)
    {
      ObjectKey objectKey;
      object->GetKey(&objectKey);
      result = both(rules->MoveUnitTo(&objectKey, &placedKey, ctx), result);
    }
  }
  return result;
}

// ---- system events ----------------------------------------------------------------------------------------------

int ApplySystemEvent31b(ObjectKey* key, SideBits* from, SideBits* to, NotifyContext* ctx)
{
  int result = 1;
  GameObjNotifier* system = nullptr;
  if (key->Family() >= kSystemFamilies.m_first && key->Family() < kSystemFamilies.m_second)
    system = GetMasterInFamilies_90_98(key);
  if (system != nullptr)
  {
    CharacterMgr* characters = characterMgr();
    if (characters != nullptr)
      result = characters->OnSystemControlChanged(key, &from->m_value, &to->m_value, ctx) != 0;
    if (ApplySideBitsChangeRules(asObject(system), from, to, ctx) != 0 && result != 0)
      return 1;
    result = 0;
  }
  return result;
}

int ApplySystemEvent303(ObjectKey* key, int value, NotifyContext* ctx)
{
  MissionMgr* missions = missionMgr();
  int ok = missions != nullptr;
  if (missions != nullptr)
    ok = both(missions->UpdateMissionsAtLocation50to60(key, value, ctx), ok);
  int system = 0;
  if (ok != 0)
    system = GetMasterInFamilies_90_98(key) != nullptr;
  GameObjNotifier* sideObject = nullptr;
  int result = both(GetSideObjectForIndex(1, &sideObject), system);
  if (sideObject != nullptr)
    result = both(static_cast<Side*>(sideObject)->RecomputeMaintenance(ctx), result);
  result = both(GetSideObjectForIndex(2, &sideObject), result);
  if (sideObject != nullptr)
  {
    if (static_cast<Side*>(sideObject)->RecomputeMaintenance(ctx) != 0 && result != 0)
      return 1;
    result = 0;
  }
  return result;
}

int ApplySystemEvent304(ObjectKey* key, int value, NotifyContext* ctx)
{
  MissionMgr* missions = missionMgr();
  int ok = missions != nullptr;
  if (missions != nullptr)
    ok = both(missions->OnSystemEvent(key, value, ctx), ok);
  if (value != 0)
  {
    GameRoot* root = static_cast<GameRoot*>(GetGameRootObjectCached());
    ok = both(root != nullptr, ok);
    if (root != nullptr)
      root->m_unk70 = 1;
  }
  return ok;
}

int ApplySystemEvent309(ObjectKey* key, int value, NotifyContext* ctx)
{
  MissionMgr* missions = missionMgr();
  int ok = missions != nullptr;
  if (missions != nullptr)
  {
    if (missions->OnSystemEventNoop(asIntArg(key), value, asIntArg(ctx)) != 0 && ok != 0)
      return 1;
    ok = 0;
  }
  return ok;
}

int ApplySystemEvent310(ObjectKey* key, int value, NotifyContext* ctx)
{
  MissionMgr* missions = missionMgr();
  int ok = missions != nullptr;
  if (missions != nullptr)
  {
    if (missions->UpdateMissionsAtSystemA(key, value, ctx) != 0 && ok != 0)
      return 1;
    ok = 0;
  }
  return ok;
}

int ApplySystemEvent30f(ObjectKey* key, int value, NotifyContext* ctx)
{
  MissionMgr* missions = missionMgr();
  int ok = missions != nullptr;
  if (missions != nullptr)
  {
    if (missions->UpdateMissionsAtSystemB(key, value, ctx) != 0 && ok != 0)
      return 1;
    ok = 0;
  }
  return ok;
}

int ApplySystemEvent311(ObjectKey* key, int value, NotifyContext* ctx)
{
  GameObjNotifier* system = GetMasterInFamilies_90_98(key);
  int ok = system != nullptr;
  if (system != nullptr && value != 0)
  {
    if (static_cast<System*>(system)->RecomputeBlockadeAndBattlePending(ctx) != 0 && ok != 0)
      return 1;
    ok = 0;
  }
  return ok;
}

int ApplySystemEvent305(ObjectKey* key, int value, NotifyContext* ctx)
{
  MissionMgr* missions = missionMgr();
  int ok = missions != nullptr;
  if (missions != nullptr)
    ok = both(missions->UpdateMissionsAtLocation53(key, value, ctx), ok);
  CharacterMgr* characters = characterMgr();
  ok = both(characters != nullptr, ok);
  if (characters != nullptr)
    ok = both(characters->OnSystemOwnerChanged(key, value, ctx), ok);
  RulesMgr* rules = rulesMgr();
  ok = both(rules != nullptr, ok);
  System* system = static_cast<System*>(GetMasterInFamilies_90_98(key));
  ok = both(system != nullptr, ok);
  if (system != nullptr && value != 0)
    ok = both(RetargetIdleManufacturers(system, ctx), ok);
  if (system != nullptr && value != 0)
  {
    UnitIterator units(system, 3);
    for (units.Begin(); units.Current() != nullptr; units.Advance())
    {
      GameObj* unit = asObject(units.Current());
      if ((unit->m_stateFlags & kCompleted) == 0 || (unit->m_stateFlags & kEnroute) != 0)
        ok = both(rules->EnforceLocation(unit, ctx), ok);
    }
  }
  GameObjNotifier* sideObject = nullptr;
  ok = both(GetSideObjectForIndex(1, &sideObject), ok);
  if (sideObject != nullptr)
    ok = both(static_cast<Side*>(sideObject)->RecomputeMaintenance(ctx), ok);
  int result = both(GetSideObjectForIndex(2, &sideObject), ok);
  if (sideObject != nullptr)
    result = both(static_cast<Side*>(sideObject)->RecomputeMaintenance(ctx), result);
  if (system != nullptr && value == 0)
  {
    uint32_t side = system->GetDataWord60();
    result = side != 3 && result != 0;
    if (side != 3)
    {
      if ((system->m_stateFlags & kDestroyed) == 0 && side != system->ControlSide())
        side = 2 - (side != 1);
      const int valueSide2 = system->SumWords48And54(2);
      const int valueSide1 = system->SumWords48And54(1);
      result = both(ApplyStrengthImbalance(system, side, valueSide1, valueSide2, ctx), result);
    }
  }
  return result;
}

int ApplySystemEvent31bUnused();

int ApplyEvent30a(ObjectKey* key, int valueA, int valueB, NotifyContext* ctx)
{
  GameObjNotifier* person = GetMasterInFamilies_30_3c(key);
  MissionMgr* missions = missionMgr();
  int ok = missions != nullptr && person != nullptr;
  if (missions != nullptr)
    ok = both(missions->OnPersonArrived(key, valueA, valueB, ctx), ok);
  CharacterMgr* characters = characterMgr();
  int result = characters != nullptr && ok != 0;
  if (characters != nullptr)
    result = both(characters->OnEmperorChanged(key, valueA, valueB, ctx), result);
  if (person != nullptr && inFamilies(person, kMajorCharacterFamilies))
    result = both(UpdateVictoryState(ctx), result);
  return result;
}

int ApplyEvent315(ObjectKey* key, int, int level, NotifyContext* ctx)
{
  const int isFirst = level == 1;
  const int isThird = level == 3;
  int smuggled = 0;
  GameObjNotifier* facility = GetMasterInFamilies_28_30(key);
  int ok = (level >= 1 && level <= 4 && facility != nullptr) ? 1 : 0;
  if (facility == nullptr || level < 1 || level > 4 || (!isFirst && !isThird))
    return ok;
  GameObjNotifier* ownSide = nullptr;
  GameObjNotifier* otherSide = nullptr;
  if ((facility->m_flags & kBothSidesMask) != kBothSidesMask)
  {
    const uint32_t controlSide = facility->ControlSide();
    const int index = controlSide == 1 ? 1 : (controlSide == 2 ? 2 : 0);
    ownSide = GetGameRootSideMemberThunk(index);
    otherSide = GetGameRootSideMemberThunk(2 - (index != 1));
  }
  if (ownSide == nullptr || otherSide == nullptr)
    return ok;
  ProductionFacility* production = reinterpret_cast<ProductionFacility*>(facility);
  const uint32_t family = key->Family();
  if (family == kMineFamily)
  {
    if (isFirst)
      ok = both(MarkFacilityWaiting(production, ctx), ok);
    if (isThird)
    {
      const int rolled = both(RollSmuggling(facility, &smuggled), ok);
      Side* side = static_cast<Side*>(smuggled == 0 ? ownSide : otherSide);
      const int released = side->ReleaseRawMaterial(production, ctx);
      return released != 0 && rolled != 0 ? 1 : 0;
    }
  }
  else if (family == kRefineryFamily)
  {
    if (isFirst)
      ok = both(static_cast<Side*>(ownSide)->AcquireRawMaterial(production, ctx), ok);
    if (isThird)
    {
      const int rolled = both(RollSmuggling(facility, &smuggled), ok);
      Side* side = static_cast<Side*>(smuggled == 0 ? ownSide : otherSide);
      const int released = side->ReleaseRefinedMaterial(production, ctx);
      return released != 0 && rolled != 0 ? 1 : 0;
    }
  }
  else if (family >= 0x28 && family < 0x2c)
  {
    if (isFirst)
      ok = both(static_cast<Side*>(ownSide)->AcquireRefinedMaterial(production, ctx), ok);
    if (isThird)
      ok = both(static_cast<Side*>(ownSide)->ReleaseFacilityToManager(production, ctx), ok);
  }
  return ok;
}

int RollSmuggling(GameObjNotifier* facility, int* smuggled)
{
  *smuggled = 0;
  GameObjNotifier* system = nullptr;
  const int located = GetOwnerSystem(facility, &system);
  const int ok = located != 0 && system != nullptr;
  if (ok != 0)
  {
    int percent = static_cast<System*>(system)->m_smugglingPercent;
    if (percent < 0)
      percent = -percent;
    if (percent != 0 && PercentChance(percent) != 0)
    {
      *smuggled = 1;
      return ok;
    }
    *smuggled = 0;
  }
  return ok;
}

// ---- the move command's helpers ---------------------------------------------------------------------------------

int CheckMoveEnabled(const ObjectKey* object, StatusCode* status)
{
  *status = StatusCode(1, 1);
  RulesMgr* rules = rulesMgr();
  int ok = rules != nullptr;
  if (rules != nullptr)
  {
    if (rules->CheckMoveEnabled(object, status) != 0 && ok != 0)
      return 1;
    ok = 0;
  }
  return ok;
}

int CheckMoveValid(const ObjectKey* object, const ObjectKey* destination, StatusCode* status)
{
  *status = StatusCode(1, 1);
  RulesMgr* rules = rulesMgr();
  int ok = rules != nullptr;
  if (rules != nullptr)
  {
    if (rules->CheckMoveValid(object, destination, status) != 0 && ok != 0)
      return 1;
    ok = 0;
  }
  return ok;
}

int ExecuteMove(const ObjectKey* object, const ObjectKey* destination, NotifyContext* ctx)
{
  RulesMgr* rules = rulesMgr();
  int ok = rules != nullptr;
  if (rules != nullptr)
  {
    if (rules->MoveUnitTo(object, const_cast<ObjectKey*>(destination), ctx) != 0 && ok != 0)
      return 1;
    ok = 0;
  }
  return ok;
}

int BindRuleTables(int difficulty)
{
  SetGnprtbDifficulty(difficulty);
  BindGnprtbDefault();
  BindGnprtb02();
  BindGnprtb06();
  BindGnprtb08();
  BindGeneralParameters();
  BindGnprtb0C();
  BindGnprtb0E();
  BindGnprtb10();
  BindGnprtb12();
  BindGnprtb14();
  BindGnprtb18();
  BindGnprtb1C();
  BindGnprtb1E();
  return 1;
}
