#include <new>

#include "character/character.h"
#include "core/game_string.h"
#include "data/dat_record.h"
#include "economy/manu_mgr.h"
#include "economy/side.h"
#include "events/event_helpers.h"
#include "events/event_post.h"
#include "events/game_clock_hooks.h"
#include "fleet/detector.h"
#include "fleet/fleet_iterator.h"
#include "galaxy/game_setup_seed.h"
#include "galaxy/system.h"
#include "gameobj/child_iterator.h"
#include "gameobj/game_obj_iterator.h"
#include "gameobj/game_root.h"
#include "gameobj/object_family.h"
#include "gameobj/object_queries.h"
#include "rules/event_rules.h"
#include "rules/loyalty_params.h"
#include "strategy_ui/advisor_sides.h"

using namespace event_rules;

ObjectKey* GetObjectRecordKey(GameObjNotifier* object, ObjectKey* out); // 0x402d80

namespace
{

constexpr int kEventDeleteDestroyed = 0x372;
constexpr uint32_t kCoruscantId = 0x109;
constexpr uint32_t kCoruscantKey = 0x90000109;
constexpr uint32_t kLukeKey = 0x32000242;
constexpr uint32_t kMonMothmaKey = 0x30000240;
constexpr uint32_t kEmperorKey = 0x35000281;
constexpr uint32_t kVaderKey = 0x34000280;
constexpr uint32_t kControllerMask = 3;
constexpr uint32_t kGarrisonMask = 0x30;
constexpr FamilyRange kSystemFamilies(0x90, 0x98);
constexpr FamilyRange kSectorFamilies(0x80, 0x90);
constexpr FamilyRange kDeathStarFamilies(0x18, 0x1c);
constexpr FamilyRange kStrengthUnitFamilies(0x10, 0x20);

int both(int result, int ok)
{
  return result != 0 && ok != 0 ? 1 : 0;
}

GameObj* asObject(GameObjNotifier* object)
{
  return static_cast<GameObj*>(object);
}

uint32_t capturedFlag(GameObjNotifier* object)
{
  return object ? static_cast<Character*>(object)->m_flagsAc & 1 : 0;
}

} // namespace

// ---- object clean-up --------------------------------------------------------------------------------------------

int PostDeleteIfDestroyedAndEmpty(GameObj* object, NotifyContext* ctx)
{
  int result = 1;
  if ((object->m_stateFlags & kDestroyed) != 0 && (object->m_stateFlags & kLocked) == 0)
  {
    GameObjIterator children(object, 4);
    const int hasChildren = children.HasCurrent();
    if (hasChildren == 0 && object->Virtual16() == 0)
      result = PostEvent(kEventDeleteDestroyed, object, ctx);
  }
  return result;
}

int ReleaseDestroyedObject(GameObj* object, NotifyContext* ctx)
{
  int result = 1;
  if ((object->m_stateFlags & kDestroyed) != 0 && object->Virtual17() != 0)
    result = object->ReleaseIfDestroyed(ctx);
  return result;
}

int RetargetIdleManufacturers(GameObj* object, NotifyContext* ctx)
{
  int ok = 1;
  for (GameObjNotifier* system = GetFirstMasterInFamilies_90_98(); system != nullptr;
       system = asObject(system)->FindNextSibling(&kSystemFamilies, 1))
  {
    ManuMgrIterator managers(system);
    for (managers.Begin(); managers.Current() != nullptr; managers.Advance())
    {
      ManuMgr* manager = static_cast<ManuMgr*>(managers.Current());
      if (manager->m_remainingGameObjCount != 0)
        continue;
      ObjectKey objectKey;
      object->GetKey(&objectKey);
      ObjectKey locationKey;
      ManagerGetLocationKey(manager, &locationKey);
      if (locationKey.m_value == objectKey.m_value)
      {
        ObjectKey systemKey;
        system->GetKey(&systemKey);
        ok = both(manager->SetTargetKey(&systemKey, ctx), ok);
      }
    }
  }
  return ok;
}

int NumberUnitName(GameObj* object, NotifyContext* ctx)
{
  int result = 1;
  if (object->Virtual18() != 0 && (object->m_stateFlags & kExisting) != 0)
  {
    DatRecord* record = static_cast<DatRecord*>(object->m_record);
    result = record != nullptr;
    if (record != nullptr)
    {
      std::string* unitName = object->GetName();
      if (CompareStrings(record->m_name, *unitName) == 0)
      {
        GameObjNotifier* sideObject = nullptr;
        GetControlSideObject(object, &sideObject);
        int number = 0;
        if (sideObject != nullptr)
        {
          ObjectKey recordKey;
          GetObjectRecordKey(object, &recordKey);
          static_cast<Side*>(sideObject)->AddObjectRef(&recordKey, &number, ctx);
        }
        std::string text;
        {
          std::string separator(" ");
          text += record->m_name;
          text += separator;
          text.append(std::to_string(number));
        }
        result = object->SetName(&text, ctx);
      }
    }
  }
  return result;
}

int UpdateDeathStarNearby(NotifyContext* ctx)
{
  int ok = 1;
  for (GameObjNotifier* sector = GetFirstMasterInFamilies_80_90(); sector != nullptr;
       sector = asObject(sector)->FindNextSibling(&kSectorFamilies, 1))
  {
    uint32_t found = 0;
    for (GameObjNotifier* deathStar = GetFirstMasterInFamilies_18_1c(); deathStar != nullptr && found == 0;
         deathStar = asObject(deathStar)->FindNextSibling(&kDeathStarFamilies, 1))
    {
      if ((asObject(deathStar)->m_stateFlags & kUsable) == 0)
        continue;
      GameObjNotifier* deathStarSector = nullptr;
      const int located = GetOwnerSector(deathStar, &deathStarSector);
      ok = both(located, ok);
      ok = both(deathStarSector != nullptr, ok);
      if (deathStarSector != nullptr)
      {
        ObjectKey deathStarSectorKey;
        ObjectKey sectorKey;
        deathStarSector->GetKey(&deathStarSectorKey);
        sector->GetKey(&sectorKey);
        found = sectorKey.m_value == deathStarSectorKey.m_value;
      }
    }
    SystemIterator systems(sector, 1);
    for (systems.Begin(); systems.Current() != nullptr; systems.Advance())
      ok = both(static_cast<System*>(systems.Current())->SetDeathStarNearby(found, ctx), ok);
  }
  return ok;
}

// ---- loyalty rules of control changes ---------------------------------------------------------------------------

int ApplySideBitsChangeRules(GameObj* system, SideBits* from, SideBits* to, NotifyContext* ctx)
{
  GameObjNotifier* root = GetGameRootObjectCached();
  int result = root != nullptr;
  if (((from->m_value ^ to->m_value) & kControllerMask) != 0)
  {
    LoyaltyRule rule;
    result = both(BuildControlChangeRule(from, to, &rule), result);
    if (rule.m_amount != 0)
    {
      int first = 1;
      const int ungarrisoned = (from->m_value & kGarrisonMask) == 0 && (to->m_value & kGarrisonMask) == 0;
      if (root != nullptr && ungarrisoned)
        first = static_cast<GameRoot*>(root)->NoteSideControlChange(from, to);
      if (first != 0)
        result = ApplyLoyaltyRuleToParentSector(system, &rule, ctx);
    }
  }
  if (system->m_id == kCoruscantId)
    result = ApplyCoruscantControlChange(system, from, to, ctx);
  return result;
}

int ApplyCoruscantRules(GameObj* system, int value, NotifyContext* ctx)
{
  int result = 1;
  if (system->m_id == kCoruscantId)
    result = ApplyCoruscantValueRules(system, value, ctx) != 0;
  return result;
}

int ApplyCoruscantControlChange(GameObj* system, SideBits* from, SideBits* to, NotifyContext* ctx)
{
  int ok = UpdateVictoryState(ctx) != 0;
  if (((to->m_value ^ from->m_value) & kControllerMask) != 0)
  {
    LoyaltyRule rule;
    ok = both(BuildControlChangeRuleSimple(from, to, &rule), ok);
    ok = both(ApplyLoyaltyRuleToParentSector(system, &rule, ctx), ok);
  }
  return ok;
}

int ApplyCoruscantValueRules(GameObj*, int value, NotifyContext* ctx)
{
  int result = 1;
  if (value == 0)
    result = UpdateVictoryState(ctx) != 0;
  return result;
}

int ApplyDepartureRules(GameObj*, int value, ObjectKey* system, NotifyContext* ctx)
{
  int ok = 1;
  if (value == 0)
  {
    LoyaltyRule rule;
    ok = BuildDepartureRule(&rule) != 0;
    GameObjNotifier* target = GetMasterInFamilies_01_ff(system);
    if (target != nullptr)
    {
      const uint32_t family = target->GetFamily();
      if (family >= kSystemFamilies.m_first && family < kSystemFamilies.m_second)
        ok = both(ApplyLoyaltyRuleToParentSector(target, &rule, ctx), ok);
    }
    ok = both(UpdateVictoryState(ctx), ok);
  }
  return ok;
}

int ApplyEnrouteRules(GameObj* object, int, ObjectKey* system, NotifyContext* ctx)
{
  int ok = 1;
  if ((object->m_stateFlags & kEnroute) != 0)
  {
    LoyaltyRule rule;
    ok = BuildEnrouteRule(&rule) != 0;
    GameObjNotifier* target = GetMasterInFamilies_01_ff(system);
    if (target != nullptr)
    {
      const uint32_t family = target->GetFamily();
      if (family >= kSystemFamilies.m_first && family < kSystemFamilies.m_second)
        ok = both(ApplyLoyaltyRuleToParentSector(target, &rule, ctx), ok);
    }
  }
  return ok;
}

int ApplyStrengthImbalance(GameObj* object, int side, int valueSide1, int valueSide2, NotifyContext* ctx)
{
  int result = 0;
  if (side == 2 || side == 1)
  {
    int totalSide1 = 0;
    int totalSide2 = 0;
    for (GameObjNotifier* unit = GetFirstMasterInFamilies_10_20(); unit != nullptr;
         unit = asObject(unit)->FindNextSibling(&kStrengthUnitFamilies, 1))
    {
      GameObj* strengthUnit = asObject(unit);
      if ((strengthUnit->m_stateFlags & kExisting) == 0 || (strengthUnit->m_stateFlags & kCompleted) == 0)
        continue;
      const uint32_t unitSide = strengthUnit->ControlSide();
      if (unitSide == 1)
      {
        totalSide1 += static_cast<int>(static_cast<Detector*>(strengthUnit)->GetRecordField58());
      }
      else if (unitSide == 2)
      {
        totalSide2 += static_cast<int>(static_cast<Detector*>(strengthUnit)->GetRecordField58());
      }
    }
    LoyaltyRule rule;
    const int built = BuildStrengthImbalanceRule(totalSide1, totalSide2, side, valueSide1, valueSide2, &rule) != 0;
    result = both(ApplyLoyaltyRuleToParentSector(object, &rule, ctx), built);
  }
  return result;
}

// ---- victory state ----------------------------------------------------------------------------------------------

int UpdateVictoryState(NotifyContext* ctx)
{
  GameRoot* root = static_cast<GameRoot*>(GetGameRootObjectCached());
  const ObjectKey lukeKey(ObjectKey::Make(kLukeKey >> 24, kLukeKey & 0xffffff));
  GameObjNotifier* luke = GetMasterInFamilies_30_3c(&lukeKey);
  const ObjectKey monMothmaKey(ObjectKey::Make(kMonMothmaKey >> 24, kMonMothmaKey & 0xffffff));
  GameObjNotifier* monMothma = GetMasterInFamilies_30_3c(&monMothmaKey);
  const ObjectKey emperorKey(ObjectKey::Make(kEmperorKey >> 24, kEmperorKey & 0xffffff));
  GameObjNotifier* emperor = GetMasterInFamilies_30_3c(&emperorKey);
  const ObjectKey vaderKey(ObjectKey::Make(kVaderKey >> 24, kVaderKey & 0xffffff));
  GameObjNotifier* vader = GetMasterInFamilies_30_3c(&vaderKey);
  const ObjectKey coruscantKey(ObjectKey::Make(kCoruscantKey >> 24, kCoruscantKey & 0xffffff));
  GameObjNotifier* coruscant = GetMasterInFamilies_90_98(&coruscantKey);
  GameObjNotifier* allianceHq = GetFirstMasterInFamilies_20_22();

  int available = 1;
  if (coruscant == nullptr || root == nullptr)
    available = 0;
  else if (root->m_settings.m_field08 == 0 &&
           (luke == nullptr || monMothma == nullptr || emperor == nullptr || vader == nullptr))
    available = 0;

  uint32_t flags = 0;
  int allianceHqDestroyed = 0;
  uint32_t lukeCaptured = 0;
  uint32_t monMothmaCaptured = 0;
  int coruscantAlliance = 0;
  uint32_t emperorCaptured = 0;
  uint32_t vaderCaptured = 0;
  uint32_t coruscantDestroyed = 0;
  int oneSide = 0;
  int otherSide = 0;
  int either = 0;
  if (available != 0)
  {
    allianceHqDestroyed = allianceHq == nullptr || (asObject(allianceHq)->m_stateFlags & kDestroyed) != 0;
    lukeCaptured = capturedFlag(luke);
    monMothmaCaptured = capturedFlag(monMothma);
    coruscantAlliance = (asObject(coruscant)->m_flags & 0xc0) == 0x40;
    emperorCaptured = capturedFlag(emperor);
    vaderCaptured = capturedFlag(vader);
    coruscantDestroyed = (asObject(coruscant)->m_stateFlags >> 3) & 1;
    const int mode = root->m_settings.m_field08;
    int left = 0;
    int right = 0;
    if (mode == 0)
    {
      left = lukeCaptured != 0 && monMothmaCaptured != 0 && allianceHqDestroyed;
      right = emperorCaptured != 0 && vaderCaptured != 0 && (coruscantAlliance || coruscantDestroyed != 0);
    }
    else if (mode == 1)
    {
      left = allianceHqDestroyed;
      right = coruscantAlliance || coruscantDestroyed != 0;
    }
    else
    {
      available = 0;
      left = 0;
    }
    if (available != 0)
    {
      const uint32_t previous = static_cast<uint32_t>(root->m_unk74);
      if (right != 0 && left != 0)
      {
        if ((previous & 0x1000000) == 0)
        {
          if (((previous >> 0x14) & 1) == 0)
          {
            right = 1;
            left = 0;
          }
          else
          {
            right = 0;
            left = 1;
          }
        }
        else
        {
          right = 1;
          left = 0;
        }
      }
      oneSide = left;
      otherSide = right;
      either = right != 0 || left != 0;
    }
  }
  if (available != 0)
  {
    flags = allianceHqDestroyed ? 1 : 0;
    if (lukeCaptured != 0)
      flags |= 0x10;
    if (monMothmaCaptured != 0)
      flags |= 0x20;
    if (coruscantAlliance)
      flags |= 0x100;
    if (coruscantDestroyed != 0)
      flags |= 0x200;
    if (emperorCaptured != 0)
      flags |= 0x1000;
    if (vaderCaptured != 0)
      flags |= 0x2000;
    if (oneSide)
      flags |= 0x100000;
    if (otherSide)
      flags |= 0x1000000;
    if (either)
      flags |= 0x10000000;
  }
  if (available != 0 && static_cast<uint32_t>(root->m_unk74) != flags)
  {
    root->m_unk74 = static_cast<int>(flags);
    root->m_ctx194 = *ctx;
  }
  return available;
}

int PublishVictoryState()
{
  int result = 1;
  GameObjNotifier* side1 = GetGameRootSideMemberThunk(1);
  GameObjNotifier* side2 = GetGameRootSideMemberThunk(2);
  GameRoot* root = static_cast<GameRoot*>(GetGameRootObjectCached());
  if (side1 != nullptr && side2 != nullptr && root != nullptr)
  {
    NotifyContext ctx(root->m_ctx194);
    if (root->IsSetupComplete())
      UpdateVictoryState(&ctx);
    result = 0;
    ctx.m_flag04 = 1;
    ctx.m_flag08 = 1;
    const int first = static_cast<Side*>(side1)->SetVictoryConditions(root->m_unk74, &ctx);
    const int second = static_cast<Side*>(side2)->SetVictoryConditions(root->m_unk74, &ctx);
    if (second != 0 && first != 0)
      result = 1;
  }
  return result;
}

int GameRoot::NoteSideControlChange(SideBits* from, SideBits*)
{
  const int tick = CallGameHook2B04();
  if (tick != m_unk78[0])
  {
    m_unk78[0] = CallGameHook2B04();
    m_unk78[1] = 0;
    m_unk78[2] = 0;
    m_unk78[3] = 0;
    m_unk78[4] = 0;
  }
  int* firstCounter = nullptr;
  int* secondCounter = nullptr;
  const uint32_t side = from->m_value & kControllerMask;
  if (side == 2)
  {
    firstCounter = &m_unk78[3];
  }
  else if (side == 1)
  {
    firstCounter = &m_unk78[1];
  }
  if (side == 2)
  {
    secondCounter = &m_unk78[4];
  }
  else if (side == 1)
  {
    secondCounter = &m_unk78[2];
  }
  int fresh = 1;
  if (firstCounter != nullptr && *firstCounter > 0)
    fresh = 0;
  if (secondCounter != nullptr && *secondCounter > 0)
    fresh = 0;
  if (fresh != 0)
  {
    if (firstCounter != nullptr)
      ++*firstCounter;
    if (secondCounter != nullptr)
      ++*secondCounter;
  }
  return fresh;
}

// ---- look-ups ---------------------------------------------------------------------------------------------------

int GetOwnerObject(GameObjNotifier* object, GameObjNotifier** out)
{
  *out = nullptr;
  ObjectKey key;
  int result = object->GetOwnerKey(&key);
  if (result != 0 && key.OrNull() != nullptr)
  {
    GameObjNotifier* owner = GetMasterInFamilies_01_ff(&key);
    result = owner != nullptr;
    *out = owner;
  }
  return result;
}

int GetOwnerSystem(GameObjNotifier* object, GameObjNotifier** out)
{
  *out = nullptr;
  ObjectKey key;
  int result = object->GetOwnerKey(&key);
  if (result != 0 && key.OrNull() != nullptr)
  {
    const uint32_t family = key.Family();
    const int isSystem = family >= kSystemFamilies.m_first && family < kSystemFamilies.m_second;
    if (isSystem)
    {
      GameObjNotifier* owner = GetMasterInFamilies_90_98(&key);
      result = owner != nullptr;
      *out = owner;
    }
  }
  return result;
}

int GetOwnerSector(GameObjNotifier* object, GameObjNotifier** out)
{
  *out = nullptr;
  GameObjNotifier* system = nullptr;
  int result = GetOwnerSystem(object, &system);
  if (result == 0 || system == nullptr)
    return result;
  GameObjNotifier* parent = system->m_parent;
  result = parent != nullptr;
  if (result == 0)
    return result;
  const uint32_t family = parent->GetFamily();
  result = family >= kSectorFamilies.m_first && family < kSectorFamilies.m_second;
  if (result != 0)
    *out = parent;
  return result;
}

int GetControlSideObject(GameObjNotifier* object, GameObjNotifier** out)
{
  *out = nullptr;
  int result = 1;
  const uint32_t controlSide = object->ControlSide();
  const int index = controlSide == 1 ? 1 : (controlSide == 2 ? 2 : 0);
  if (index != 0)
  {
    GameObjNotifier* sideObject = GetGameRootSideMemberThunk(index);
    result = sideObject != nullptr;
    *out = sideObject;
  }
  return result;
}

int GetSideObjectForIndex(int side, GameObjNotifier** out)
{
  *out = nullptr;
  int result = 1;
  const int index = side == 1 ? 1 : (side == 2 ? 2 : 0);
  if (index != 0)
  {
    GameObjNotifier* sideObject = GetGameRootSideMemberThunk(index);
    result = sideObject != nullptr;
    *out = sideObject;
  }
  return result;
}

// ---- applying loyalty rules -------------------------------------------------------------------------------------

int ApplyLoyaltyRuleToAllSectors(LoyaltyRule* rule, NotifyContext* ctx)
{
  int ok = 1;
  for (GameObjNotifier* sector = GetFirstMasterInFamilies_80_90(); sector != nullptr;
       sector = static_cast<GameObj*>(sector)->FindNextSibling(&family::kSectors, 1))
    ok = both(ApplyLoyaltyRuleToSector(sector, rule, ctx), ok);
  return ok;
}

int ApplyLoyaltyRuleToParentSector(GameObjNotifier* object, LoyaltyRule* rule, NotifyContext* ctx)
{
  GameObjNotifier* sector = nullptr;
  int result = static_cast<GameObj*>(object)->GetParent(&sector) != 0;
  if (sector != nullptr)
    result = both(ApplyLoyaltyRuleToSector(sector, rule, ctx), result);
  return result;
}

// ---- the systems a team is in -----------------------------------------------------------------------------------

int CollectTeamSystems(ActivityRequest* request, ObjectKeyValueSet* team, ObjectKeyValueSet* out, StatusCode* status)
{
  out->Clear();
  *status = StatusCode(1, 0xffffffff);
  ObjectKeyValueSet dependencies;
  int ok = CollectKeyDependencies(request, team, &dependencies, status);
  if (ok != 0)
  {
    if (status->m_second == 0xffffffff)
    {
      for (auto position = dependencies.Entries().begin(); position != dependencies.Entries().end(); ++position)
      {
        auto* node = (*position).get();
        if (ok == 0)
        {
          *status = StatusCode(1, 1);
          return ok;
        }
        if (status->m_second != 0xffffffff)
          break;
        ObjectKey fleetKey;
        static_cast<ObjectKeyNode*>(node)->GetKey(&fleetKey);
        GameObjNotifier* fleet = GetMasterInFamilies_08_10(&fleetKey);
        if (fleet == nullptr)
        {
          *status = StatusCode(1, 0x12);
        }
        else
        {
          if (status->m_second == 0xffffffff && (asObject(fleet)->m_stateFlags & kDestroyed) != 0)
            *status = StatusCode(1, 0x12);
          if (status->m_second == 0xffffffff)
          {
            GameObjNotifier* system = fleet->m_parent;
            const uint32_t family = system->GetFamily();
            ok = family >= kSystemFamilies.m_first && family < kSystemFamilies.m_second;
            if (system != nullptr)
            {
              ObjectKey systemKey;
              system->GetKey(&systemKey);
              if (out->Find(&systemKey) == nullptr)
              {
                system->GetKey(&systemKey);
                ok = out->AddKeyValue(&systemKey, 0);
              }
            }
          }
        }
      }
    }
    if (ok != 0)
      return ok;
  }
  *status = StatusCode(1, 1);
  return ok;
}
